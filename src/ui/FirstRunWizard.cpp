#include "FirstRunWizard.h"
#include "../core/HotkeyManager.h"
#include "../core/HotkeySequence.h"
#include "../core/TranslationManager.h"
#include "ThemeColors.h"
#ifdef Q_OS_LINUX
#include "../core/LinuxDependencySelection.h"
#include "../core/LinuxDesktopIntegration.h"
#include "../core/LinuxGnomeShortcutInstaller.h"
#include "../core/LinuxPortalGlobalShortcuts.h"
#endif

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QGroupBox>
#include <QFont>
#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QScrollArea>
#include <QStyle>
#include <QSignalBlocker>
#include <utility>
#ifdef Q_OS_LINUX
#include <QCheckBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QLocale>
#include <QDBusInterface>
#include <QDBusReply>
#endif

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QDialog(parent)
{
#ifdef Q_OS_WIN
    // Strip maximize button to prevent Windows 11 ARM64 Snap Layouts crash.
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
#endif
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setupUi();
    loadDefaults();
    connect(m_langCombo, &QComboBox::currentIndexChanged, this, &FirstRunWizard::onLanguageChanged);

    // Measure after defaults have revealed/hidden platform-specific rows. Keep
    // the footer visible on small screens and let only the form scroll.
    ensurePolished();
    QScreen *targetScreen = parent ? parent->screen()
                                   : QGuiApplication::screenAt(QCursor::pos());
    if (!targetScreen)
        targetScreen = screen();
    const QSize available = targetScreen->availableGeometry().size();
    const QSize maximumClient(qMax(1, available.width() - 32),
                              qMax(1, available.height() - 48));
    setMinimumSize(QSize(480, 320).boundedTo(maximumClient));
    const int initialWidth = qMin(640, maximumClient.width());
    auto *scrollArea = findChild<QScrollArea *>(QStringLiteral("firstRunScrollArea"));
    const QMargins margins = layout()->contentsMargins();
    const int contentWidth = initialWidth - margins.left() - margins.right()
        - style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    QLayout *formLayout = scrollArea->widget()->layout();
    const int formHeight = formLayout->hasHeightForWidth()
        ? formLayout->heightForWidth(qMax(1, contentWidth))
        : formLayout->sizeHint().height();
    const int footerHeight = layout()->sizeHint().height() - scrollArea->sizeHint().height();
    resize(initialWidth, qBound(minimumHeight(), formHeight + footerHeight,
                               maximumClient.height()));
}

FirstRunWizard::~FirstRunWizard() {}

bool FirstRunWizard::shouldShow()
{
    QSettings s("EShot", "EShot");
#ifdef Q_OS_LINUX
    return linuxSetupShouldShow(s.contains(QStringLiteral("linuxSetupCompleted")),
                                s.value(QStringLiteral("linuxSetupCompleted"), false).toBool());
#else
    return !s.value("wizardCompleted", false).toBool();
#endif
}

void FirstRunWizard::addTranslatedText(std::function<void()> apply)
{
    apply();
    m_retranslations.append(std::move(apply));
}

void FirstRunWizard::retranslateUi()
{
    for (const auto &apply : std::as_const(m_retranslations))
        apply();
}

void FirstRunWizard::onLanguageChanged()
{
    // Preview the picked language right away; Finish saves it.
    TranslationManager::setLanguage(languageFromCode(m_langCombo->currentData().toString()), false);
    retranslateUi();
    onHotkeyChanged();
}

TranslationManager::Language FirstRunWizard::languageFromCode(const QString &code)
{
    static const char *codes[] = {"tr", "en", "de", "fr", "es", "ja", "zh", "ru"};
    for (int i = 0; i < TranslationManager::LangCount; ++i) {
        if (code == QLatin1String(codes[i]))
            return static_cast<TranslationManager::Language>(i);
    }
    return TranslationManager::English;
}

void FirstRunWizard::setupUi()
{
    addTranslatedText([this]() { setWindowTitle(TranslationManager::wizardTitle()); });
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("firstRunScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("firstRunScrollContent"));
    QVBoxLayout *mainLayout = new QVBoxLayout(content);
    mainLayout->setSpacing(12);
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea, 1);

    QFont titleFont;
    titleFont.setPointSize(16);
    titleFont.setBold(true);

    QLabel *titleLabel = new QLabel();
    titleLabel->setFont(titleFont);
    addTranslatedText([titleLabel]() { titleLabel->setText(TranslationManager::wizardTitle()); });
    mainLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel();
    descLabel->setWordWrap(true);
    ThemeColors::setStyleSheet(descLabel, QStringLiteral("color: @muted;"));
    addTranslatedText([descLabel]() { descLabel->setText(TranslationManager::wizardDesc()); });
    mainLayout->addWidget(descLabel);

#ifdef Q_OS_LINUX
    const QString currentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QString sessionDesktop = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    const LinuxDesktopEnvironment startupDesktop = LinuxDesktopIntegration::detect(
        currentDesktop, sessionDesktop);
    const LinuxDesktopSupportLevel supportLevel = LinuxDesktopIntegration::startupSupportLevel(
        startupDesktop, qEnvironmentVariable("XDG_SESSION_TYPE"));
    if (supportLevel != LinuxDesktopSupportLevel::Supported) {
        auto *supportWarning = new QLabel();
        supportWarning->setWordWrap(true);
        ThemeColors::setStyleSheet(supportWarning,
            supportLevel == LinuxDesktopSupportLevel::Unsupported
                ? "background: rgba(220, 53, 69, 0.16); color: @bannerErrorText; "
                  "border: 1px solid rgba(220, 53, 69, 0.60); border-radius: 6px; "
                  "padding: 8px; font-size: 12px;"
                : "background: rgba(255, 193, 7, 0.14); color: @bannerText; "
                  "border: 1px solid rgba(255, 193, 7, 0.45); border-radius: 6px; "
                  "padding: 8px; font-size: 12px;");
        addTranslatedText([supportWarning, supportLevel, currentDesktop, sessionDesktop]() {
            if (supportLevel == LinuxDesktopSupportLevel::Unsupported) {
                QString detectedName = currentDesktop.trimmed();
                if (detectedName.isEmpty())
                    detectedName = sessionDesktop.trimmed();
                if (detectedName.isEmpty())
                    detectedName = TranslationManager::tr("desktopUnknown");
                supportWarning->setText(TranslationManager::tr("desktopUnsupportedWarning")
                                            .arg(detectedName.toHtmlEscaped()));
            } else {
                supportWarning->setText(TranslationManager::tr("desktopLimitedWarning"));
            }
        });
        mainLayout->addWidget(supportWarning);
    }
    if (isAppImageManagedExternally(qEnvironmentVariable("APPIMAGE"), QDir::homePath())) {
        auto *managerWarning = new QLabel();
        addTranslatedText([managerWarning]() {
            managerWarning->setText(TranslationManager::tr("appImageManagerWarning"));
        });
        managerWarning->setWordWrap(true);
        ThemeColors::setStyleSheet(managerWarning,
            "background: rgba(255, 193, 7, 0.14); color: @bannerText; "
            "border: 1px solid rgba(255, 193, 7, 0.45); border-radius: 6px; "
            "padding: 8px; font-size: 12px;");
        mainLayout->addWidget(managerWarning);
    }
#endif

    QGroupBox *langGroup = new QGroupBox();
    addTranslatedText([langGroup]() { langGroup->setTitle(TranslationManager::language()); });
    QVBoxLayout *langLayout = new QVBoxLayout(langGroup);
    m_langCombo = new QComboBox();
    m_langCombo->addItem(QString::fromUtf8("Türkçe"), "tr");
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("Deutsch", "de");
    m_langCombo->addItem(QString::fromUtf8("Français"), "fr");
    m_langCombo->addItem(QString::fromUtf8("Español"), "es");
    m_langCombo->addItem(QString::fromUtf8("日本語"), "ja");
    m_langCombo->addItem(QString::fromUtf8("中文"), "zh");
    m_langCombo->addItem(QString::fromUtf8("Русский"), "ru");
    addTranslatedText([this]() { m_langCombo->setToolTip(TranslationManager::tr("wizardLanguageTip")); });
    langLayout->addWidget(m_langCombo);
    mainLayout->addWidget(langGroup);

    QGroupBox *hkGroup = new QGroupBox();
    addTranslatedText([hkGroup]() { hkGroup->setTitle(TranslationManager::hotkeyTitle()); });
    QVBoxLayout *hkLayout = new QVBoxLayout(hkGroup);
    QLabel *hkDesc = new QLabel();
    addTranslatedText([hkDesc]() { hkDesc->setText(TranslationManager::wizardHotkeyDesc()); });
    hkDesc->setWordWrap(true);
    hkLayout->addWidget(hkDesc);
    m_hotkeyEdit = new QKeySequenceEdit();
    m_hotkeyEdit->setMinimumHeight(38);
    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged,
            this, &FirstRunWizard::onHotkeyChanged);
    hkLayout->addWidget(m_hotkeyEdit);

    m_hotkeyStatusLabel = new QLabel();
    m_hotkeyStatusLabel->setStyleSheet("font-size: 12px;");
    hkLayout->addWidget(m_hotkeyStatusLabel);

    m_printScreenConflictLabel = new QLabel();
    addTranslatedText([this]() {
        m_printScreenConflictLabel->setText(TranslationManager::printScreenConflictMessage());
    });
    m_printScreenConflictLabel->setWordWrap(true);
    ThemeColors::setStyleSheet(m_printScreenConflictLabel,
        "background: rgba(255, 193, 7, 0.14); color: @bannerText; "
        "border: 1px solid rgba(255, 193, 7, 0.45); border-radius: 6px; "
        "padding: 7px; font-size: 12px;");
    hkLayout->addWidget(m_printScreenConflictLabel);

    m_printScreenFixButton = new QPushButton();
    addTranslatedText([this]() {
        m_printScreenFixButton->setText(TranslationManager::printScreenConflictFix());
    });
    connect(m_printScreenFixButton, &QPushButton::clicked,
            this, &FirstRunWizard::onDisableWindowsPrintScreenSnipping);
    hkLayout->addWidget(m_printScreenFixButton);
#ifdef Q_OS_LINUX
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    auto *activatePrintButton = new QPushButton();
    addTranslatedText([activatePrintButton, desktop]() {
        activatePrintButton->setText(TranslationManager::tr("usePrintScreenForEshot"));
        activatePrintButton->setToolTip(desktop == LinuxDesktopEnvironment::Kde
            ? TranslationManager::tr("usePrintScreenKdeTip")
            : TranslationManager::tr("usePrintScreenTip"));
    });
    connect(activatePrintButton, &QPushButton::clicked,
            this, &FirstRunWizard::onActivateLinuxPrintScreen);
    hkLayout->addWidget(activatePrintButton);
#endif
    mainLayout->addWidget(hkGroup);

    QGroupBox *pathGroup = new QGroupBox();
    addTranslatedText([pathGroup]() { pathGroup->setTitle(TranslationManager::saveDir()); });
    QHBoxLayout *pathLayout = new QHBoxLayout(pathGroup);
    m_savePathEdit = new QLineEdit();
    QPushButton *browseBtn = new QPushButton();
    addTranslatedText([browseBtn]() { browseBtn->setText(TranslationManager::browse()); });
    connect(browseBtn, &QPushButton::clicked, this, &FirstRunWizard::onBrowse);
    pathLayout->addWidget(m_savePathEdit);
    pathLayout->addWidget(browseBtn);
    mainLayout->addWidget(pathGroup);

#ifdef Q_OS_LINUX
    QGroupBox *depsGroup = new QGroupBox();
    QVBoxLayout *depsLayout = new QVBoxLayout(depsGroup);
    QLabel *depsHint = new QLabel();
    depsHint->setWordWrap(true); depsLayout->addWidget(depsHint);
    m_linuxFfmpegCheck = new QCheckBox();
    m_linuxOcrCheck = new QCheckBox(QStringLiteral("Tesseract OCR"));
    m_linuxDesktopCheck = new QCheckBox();
    m_linuxAppImageIntegrationCheck = new QCheckBox();
    addTranslatedText([this, depsGroup, depsHint]() {
        depsGroup->setTitle(TranslationManager::tr("linuxOptionalFeatures"));
        depsHint->setText(TranslationManager::tr("linuxOptionalFeaturesHint"));
        m_linuxFfmpegCheck->setText(TranslationManager::tr("linuxFeatureFfmpeg"));
        m_linuxDesktopCheck->setText(TranslationManager::tr("linuxFeaturePortal"));
        m_linuxAppImageIntegrationCheck->setText(TranslationManager::tr("linuxFeatureAppMenu"));
        m_linuxFfmpegCheck->setToolTip(TranslationManager::tr("linuxFeatureFfmpegTip"));
        m_linuxOcrCheck->setToolTip(TranslationManager::tr("linuxFeatureOcrTip"));
        m_linuxDesktopCheck->setToolTip(TranslationManager::tr("linuxFeaturePortalTip"));
        m_linuxAppImageIntegrationCheck->setToolTip(TranslationManager::tr("linuxFeatureAppMenuTip"));
    });
    m_linuxFfmpegCheck->setChecked(true); m_linuxOcrCheck->setChecked(true);
    m_linuxDesktopCheck->setChecked(defaultLinuxPortalSelection(qEnvironmentVariable("XDG_SESSION_TYPE")));
    const bool offerAppImageIntegration = shouldOfferAppImageIntegration(
        qEnvironmentVariable("APPIMAGE"));
    m_linuxAppImageIntegrationCheck->setVisible(offerAppImageIntegration);
    m_linuxAppImageIntegrationCheck->setChecked(offerAppImageIntegration);
    depsLayout->addWidget(m_linuxFfmpegCheck); depsLayout->addWidget(m_linuxOcrCheck); depsLayout->addWidget(m_linuxDesktopCheck); depsLayout->addWidget(m_linuxAppImageIntegrationCheck);
    QGridLayout *languages = new QGridLayout();
    const auto names = ocrLanguageDisplayNames();
    const auto defaults = defaultOcrLanguageCodes(QLocale::system().name());
    int languageIndex = 0;
    for (const QString &code : supportedOcrLanguageCodes()) { auto *check = new QCheckBox(names.value(code)); check->setProperty("ocrCode", code); check->setChecked(defaults.contains(code)); addTranslatedText([check, name = names.value(code)]() { check->setToolTip(TranslationManager::tr("ocrLanguageDataTip").arg(name)); }); languages->addWidget(check, languageIndex / 2, languageIndex % 2); m_linuxLanguageChecks << check; ++languageIndex; }
    depsLayout->addLayout(languages);
    connect(m_linuxOcrCheck, &QCheckBox::toggled, depsGroup, [this](bool enabled) { for (auto *check : m_linuxLanguageChecks) check->setEnabled(enabled); });
    QPushButton *skipDeps = new QPushButton();
    addTranslatedText([skipDeps]() {
        skipDeps->setText(TranslationManager::tr("linuxSkipOptionalSetup"));
        skipDeps->setToolTip(TranslationManager::tr("linuxSkipOptionalSetupTip"));
    });
    connect(skipDeps, &QPushButton::clicked, depsGroup, [this] { m_linuxExplicitSkip = true; m_linuxFfmpegCheck->setChecked(false); m_linuxOcrCheck->setChecked(false); m_linuxDesktopCheck->setChecked(false); m_linuxAppImageIntegrationCheck->setChecked(false); m_linuxInstallStatus->setText(TranslationManager::tr("linuxSetupWillBeSkipped")); });
    depsLayout->addWidget(skipDeps);
    m_skipDepsButton = skipDeps;
    m_linuxInstallStatus = new QLabel(); m_linuxInstallStatus->setWordWrap(true); depsLayout->addWidget(m_linuxInstallStatus);
    mainLayout->addWidget(depsGroup);
#endif

    // The scroll widget fills its viewport. Spare height belongs below the
    // form, otherwise QVBoxLayout spreads it into labels and group boxes.
    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *finishBtn = new QPushButton();
    addTranslatedText([finishBtn]() { finishBtn->setText(TranslationManager::wizardFinish()); });
#ifdef Q_OS_LINUX
    m_finishButton = finishBtn;
#endif
    finishBtn->setDefault(true);
    finishBtn->setStyleSheet(R"(
        QPushButton { background-color: #0078D4; color: white; border: none;
                      padding: 8px 24px; border-radius: 4px; font-weight: bold; }
        QPushButton:hover { background-color: #1a8cff; }
    )");
    connect(finishBtn, &QPushButton::clicked, this, &FirstRunWizard::onFinish);
    btnLayout->addWidget(finishBtn);
    outerLayout->addLayout(btnLayout);
}

void FirstRunWizard::loadDefaults()
{
    QSettings s("EShot", "EShot");

    // Match the onboarding language chosen by startup (the system language on
    // a first run). Picking another one retranslates the wizard right away;
    // Finish saves the choice.
    const int langInt = static_cast<int>(TranslationManager::currentLanguage());
    static const char* langCodes[] = {"tr","en","de","fr","es","ja","zh","ru"};
    QString lang = (langInt >= 0 && langInt < TranslationManager::LangCount) ? langCodes[langInt] : "en";
    int li = m_langCombo->findData(lang);
    if (li >= 0) {
        const QSignalBlocker blocker(m_langCombo);
        m_langCombo->setCurrentIndex(li);
    }

    UINT savedMod = static_cast<UINT>(s.value("hotkeyModifiers", 0).toUInt());
    UINT savedVKey = static_cast<UINT>(s.value("hotkeyVKey", VK_SNAPSHOT).toUInt());
    // The wizard historically shows Print for a zero key code, while the shared
    // converter returns an empty sequence for it.
    m_hotkeyEdit->setKeySequence(savedVKey == 0 ? QKeySequence(Qt::Key_Print)
                                                : win32ToKeySequence(savedMod, savedVKey));

    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (defaultPath.isEmpty())
        defaultPath = QDir::homePath();
    defaultPath = QDir(defaultPath).filePath("EShot");
    m_savePathEdit->setText(s.value("savePath", defaultPath).toString());
#ifdef Q_OS_LINUX
    m_linuxFfmpegCheck->setChecked(s.value("linuxSetupFfmpeg", true).toBool());
    m_linuxOcrCheck->setChecked(s.value("linuxSetupOcr", true).toBool());
    m_linuxDesktopCheck->setChecked(s.value("linuxSetupPortal",
        defaultLinuxPortalSelection(qEnvironmentVariable("XDG_SESSION_TYPE"))).toBool());
    const bool offerAppImageIntegration = shouldOfferAppImageIntegration(
        qEnvironmentVariable("APPIMAGE"));
    m_linuxAppImageIntegrationCheck->setVisible(offerAppImageIntegration);
    m_linuxAppImageIntegrationCheck->setChecked(
        offerAppImageIntegration
        && s.value("linuxSetupAppImageIntegration", true).toBool());
    const QStringList selectedLanguages = s.value("linuxSetupOcrLanguages", defaultOcrLanguageCodes(QLocale::system().name())).toStringList();
    for (auto *check : m_linuxLanguageChecks) check->setChecked(selectedLanguages.contains(check->property("ocrCode").toString()));
#endif

    onHotkeyChanged();
}

void FirstRunWizard::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, TranslationManager::saveDir(), m_savePathEdit->text());
    if (!dir.isEmpty()) m_savePathEdit->setText(dir);
}

void FirstRunWizard::onHotkeyChanged()
{
    UINT mod = 0, vk = 0;
    const bool ok = keySequenceToWin32(m_hotkeyEdit->keySequence(), mod, vk);
    if (ok) {
        m_hotkeyStatusLabel->setText(QStringLiteral("OK: %1").arg(m_hotkeyEdit->keySequence().toString(QKeySequence::NativeText)));
        ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @success; font-size: 12px;"));
    } else {
        m_hotkeyStatusLabel->setText(TranslationManager::hotkeyInvalid());
        ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @warning; font-size: 12px;"));
    }
    updatePrintScreenConflictUi();
}

void FirstRunWizard::updatePrintScreenConflictUi()
{
    UINT mod = 0, vk = 0;
    const bool hotkeyOk = keySequenceToWin32(m_hotkeyEdit->keySequence(), mod, vk);
    const bool showWarning = hotkeyOk
        && HotkeyManager::isPlainPrintScreen(mod, vk)
        && HotkeyManager::isWindowsPrintScreenSnippingEnabled();

    m_printScreenConflictLabel->setVisible(showWarning);
    m_printScreenFixButton->setVisible(showWarning);
}

void FirstRunWizard::onDisableWindowsPrintScreenSnipping()
{
    if (!HotkeyManager::setWindowsPrintScreenSnippingEnabled(false)) {
        QMessageBox::warning(this, QStringLiteral("EShot"), TranslationManager::errTitle());
        return;
    }
    HotkeyManager::instance().reRegisterCaptureHotkey(
        HotkeyManager::instance().captureModifiers(),
        HotkeyManager::instance().captureVirtualKey());
    updatePrintScreenConflictUi();
}

#ifdef Q_OS_LINUX
void FirstRunWizard::onActivateLinuxPrintScreen()
{
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    if (desktop == LinuxDesktopEnvironment::Other) {
        m_hotkeyStatusLabel->setText(TranslationManager::tr("printScreenDesktopsOnly"));
        ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @warning; font-size: 12px;"));
        return;
    }

    m_hotkeyEdit->setKeySequence(QKeySequence(Qt::Key_Print));
    if (desktop == LinuxDesktopEnvironment::Gnome) {
        if (HotkeyManager::instance().linuxPortalShortcutsAvailable()) {
            if (!HotkeyManager::instance().reRegisterCaptureHotkey(0, VK_SNAPSHOT)) {
                m_hotkeyStatusLabel->setText(TranslationManager::tr("gnomePrintScreenDenied"));
                ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @warning; font-size: 12px;"));
                return;
            }
            if (shouldOfferAppImageIntegration(qEnvironmentVariable("APPIMAGE")))
                m_linuxAppImageIntegrationCheck->setChecked(true);
            m_hotkeyStatusLabel->setText(TranslationManager::tr("gnomePrintScreenChoose"));
            ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @success; font-size: 12px;"));
            return;
        }

        const bool offerAppImageIntegration = shouldOfferAppImageIntegration(
            qEnvironmentVariable("APPIMAGE"));
        if (offerAppImageIntegration)
            m_linuxAppImageIntegrationCheck->setChecked(true);
        const QString integrated = QDir::home().filePath(
            QStringLiteral(".local/opt/EShot/EShot.AppImage"));
        const QString executable = LinuxGnomeShortcutInstaller::preferredExecutable(
            qEnvironmentVariable("APPIMAGE"), QCoreApplication::applicationFilePath(),
            offerAppImageIntegration ? integrated : QString());
        const auto installed = LinuxGnomeShortcutInstaller::installPrintScreen(
            LinuxGnomeShortcutInstaller::captureCommand(executable));
        if (!installed.success) {
            m_hotkeyStatusLabel->setText(TranslationManager::tr("gnomePrintScreenFailed").arg(installed.error));
            ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @warning; font-size: 12px;"));
            return;
        }
        m_hotkeyStatusLabel->setText(TranslationManager::tr("gnomePrintScreenActivated"));
        ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @success; font-size: 12px;"));
        return;
    }

    QDBusInterface globalAccel(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QDBusConnection::sessionBus());
    const QStringList spectacleLaunchId = {
        QStringLiteral("org.kde.spectacle.desktop"),
        QStringLiteral("_launch"),
        QStringLiteral("Spectacle"),
        QStringLiteral("Launch Spectacle")
    };
    const QDBusReply<QList<int>> currentReply = globalAccel.call(
        QStringLiteral("shortcut"), spectacleLaunchId);
    if (!globalAccel.isValid() || !currentReply.isValid()) {
        m_hotkeyStatusLabel->setText(TranslationManager::tr("kdeShortcutsReadFailed"));
        ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @warning; font-size: 12px;"));
        return;
    }

    const QList<int> originalShortcuts = currentReply.value();
    const QList<int> shortcutsForEshot = kdeShortcutsAfterEshotPrintScreenRegistration(
        originalShortcuts, true);
    const QDBusReply<void> setReply = globalAccel.call(
        QStringLiteral("setForeignShortcut"),
        spectacleLaunchId,
        QVariant::fromValue(shortcutsForEshot));
    if (!setReply.isValid()) {
        m_hotkeyStatusLabel->setText(TranslationManager::tr("kdeShortcutChangeDenied"));
        ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @warning; font-size: 12px;"));
        return;
    }

    if (!HotkeyManager::instance().reRegisterCaptureHotkey(0, VK_SNAPSHOT)) {
        const QDBusReply<void> restoreReply = globalAccel.call(
            QStringLiteral("setForeignShortcut"),
            spectacleLaunchId,
            QVariant::fromValue(kdeShortcutsAfterEshotPrintScreenRegistration(
                originalShortcuts, false)));
        const QString recovery = restoreReply.isValid()
            ? TranslationManager::tr("kdeSpectacleRestored")
            : TranslationManager::tr("kdeSpectacleRestoreFailed");
        m_hotkeyStatusLabel->setText(
            TranslationManager::tr("kdePrintScreenRegisterFailed").arg(recovery));
        ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @warning; font-size: 12px;"));
        return;
    }

    if (shouldOfferAppImageIntegration(qEnvironmentVariable("APPIMAGE")))
        m_linuxAppImageIntegrationCheck->setChecked(true);
    m_hotkeyStatusLabel->setText(TranslationManager::tr("kdePrintScreenActivated"));
    ThemeColors::setStyleSheet(m_hotkeyStatusLabel, QStringLiteral("color: @success; font-size: 12px;"));
}
#endif

void FirstRunWizard::onFinish()
{
    QString savePath = m_savePathEdit->text().trimmed();
    if (savePath.isEmpty()) {
        savePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (savePath.isEmpty())
            savePath = QDir::homePath();
        savePath = QDir(savePath).filePath("EShot");
    }
    if (!savePath.isEmpty()) {
        QDir dir(savePath);
        if (!dir.exists() && !dir.mkpath(".")) {
            QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::errSaveDir() + savePath);
            return;
        }
    }

    UINT modifiers = 0, vkey = 0;
    if (!keySequenceToWin32(m_hotkeyEdit->keySequence(), modifiers, vkey)) {
        QMessageBox::warning(this, TranslationManager::errInvalidHotkeyTitle(), TranslationManager::errInvalidHotkey());
        return;
    }

    bool deferHotkeyRegistration = false;
#ifdef Q_OS_LINUX
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    deferHotkeyRegistration = LinuxDesktopIntegration::deferFirstRunHotkeyRegistration(
        desktop);
    const bool willIntegrate = m_linuxAppImageIntegrationCheck->isChecked()
        && shouldOfferAppImageIntegration(qEnvironmentVariable("APPIMAGE"));
    if (desktop == LinuxDesktopEnvironment::Gnome
        && !LinuxPortalGlobalShortcuts::desktopPortalAvailable()
        && !willIntegrate) {
        const QString integrated = QDir::home().filePath(
            QStringLiteral(".local/opt/EShot/EShot.AppImage"));
        const QString executable = LinuxGnomeShortcutInstaller::preferredExecutable(
            qEnvironmentVariable("APPIMAGE"), QCoreApplication::applicationFilePath(),
            willIntegrate ? integrated : QString());
        const QString binding = LinuxGnomeShortcutInstaller::acceleratorFromPortableSequence(
            m_hotkeyEdit->keySequence().toString(QKeySequence::PortableText));
        const auto installed = LinuxGnomeShortcutInstaller::installCaptureShortcut(
            LinuxGnomeShortcutInstaller::captureCommand(executable), binding);
        if (!installed.success) {
            QMessageBox::warning(
                this, TranslationManager::errInvalidHotkeyTitle(),
                TranslationManager::tr("gnomeCaptureShortcutFailed")
                    + installed.error);
            return;
        }
    }
#endif

    if (!deferHotkeyRegistration
        && !HotkeyManager::instance().reRegisterCaptureHotkey(modifiers, vkey)) {
        QString message = TranslationManager::hotkeyMayBeInUse();
#ifdef Q_OS_LINUX
        if (desktop == LinuxDesktopEnvironment::Kde
            && HotkeyManager::isPlainPrintScreen(modifiers, vkey)) {
            message = TranslationManager::tr("kdePrintScreenTakenBySpectacle").arg(TranslationManager::tr("usePrintScreenForEshot"));
        }
#endif
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            message);
        return;
    }

    const TranslationManager::Language langEnum = languageFromCode(m_langCombo->currentData().toString());

    TranslationManager::setLanguage(langEnum);

    QSettings s("EShot", "EShot");
    s.setValue("language", static_cast<int>(langEnum));
    s.setValue("hotkeyModifiers", modifiers);
    s.setValue("hotkeyVKey", vkey);
    s.setValue("savePath", savePath);
#ifdef Q_OS_LINUX
    QStringList selectedLanguages;
    for (auto *check : m_linuxLanguageChecks) if (check->isChecked()) selectedLanguages << check->property("ocrCode").toString();
    s.setValue("linuxSetupFfmpeg", m_linuxFfmpegCheck->isChecked());
    s.setValue("linuxSetupOcr", m_linuxOcrCheck->isChecked());
    s.setValue("linuxSetupPortal", m_linuxDesktopCheck->isChecked());
    s.setValue("linuxSetupAppImageIntegration", m_linuxAppImageIntegrationCheck->isChecked());
    s.setValue("linuxSetupOcrLanguages", selectedLanguages);
#else
    s.setValue("wizardCompleted", true);
#endif
    s.sync();

#ifdef Q_OS_LINUX
    startLinuxDependencyInstaller();
#else
    accept();
#endif
}

void FirstRunWizard::reject()
{
#ifdef Q_OS_LINUX
    const bool running = m_linuxInstallerProcess
        && m_linuxInstallerProcess->state() != QProcess::NotRunning;
    if (!linuxDependencyDialogMayClose(running)) {
        if (m_linuxInstallStatus)
            m_linuxInstallStatus->setText(TranslationManager::installRunningWaitToClose());
        return;
    }
#endif
    QDialog::reject();
}

#ifdef Q_OS_LINUX
static QString linuxInstallerPath()
{
    const QString appDir = qEnvironmentVariable("ESHOT_APPDIR");
    if (!appDir.isEmpty()) return QDir(appDir).filePath("usr/lib/eshot/install-runtime-deps.sh");
    return QDir(QCoreApplication::applicationDirPath()).filePath("../lib/eshot/install-runtime-deps.sh");
}

// install-runtime-deps.sh prints this marker when GNOME Shell needs a new
// login to load the AppIndicator extension it just installed (tray icon).
static void showLinuxReloginHintIfNeeded(QProcess *process, QWidget *parent)
{
    if (!process || !process->readAllStandardOutput().contains("ESHOT_SETUP_RELOGIN_REQUIRED"))
        return;
    QMessageBox::information(parent, QStringLiteral("EShot"),
                             TranslationManager::tr("gnomeTrayReloginHint"));
}

static bool selectedLinuxCapabilitiesAvailable(bool ffmpeg, bool ocr, bool integration)
{
    if (ffmpeg && QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty()) return false;
    if (ocr && QStandardPaths::findExecutable(QStringLiteral("tesseract")).isEmpty()) return false;
    if (integration) {
        const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME",
            QDir::home().filePath(QStringLiteral(".local/share")));
        if (!QFileInfo::exists(QDir(dataHome).filePath(QStringLiteral("applications/io.github.benoks.EShot.desktop")))) return false;
    }
    return true;
}

static bool scheduleIntegratedAppRestart()
{
    const QString dataHome = qEnvironmentVariable(
        "XDG_DATA_HOME", QDir::home().filePath(QStringLiteral(".local/share")));
    const QString desktopFile = QDir(dataHome).filePath(
        QStringLiteral("applications/io.github.benoks.EShot.desktop"));
    const QString installedAppImage = QDir::home().filePath(
        QStringLiteral(".local/opt/EShot/EShot.AppImage"));
    if (!QFileInfo::exists(desktopFile) || !QFileInfo::exists(installedAppImage))
        return false;

    const QString script = QStringLiteral(
        "pid=\"$1\"; desktop=\"$2\"; app=\"$3\"; "
        "while kill -0 \"$pid\" 2>/dev/null; do sleep 0.1; done; "
        "if command -v kioclient6 >/dev/null 2>&1; then exec kioclient6 exec \"$desktop\"; fi; "
        "exec \"$app\" --silent");
    return QProcess::startDetached(
        QStringLiteral("/bin/sh"),
        {QStringLiteral("-c"), script, QStringLiteral("eshot-restart"),
         QString::number(QCoreApplication::applicationPid()), desktopFile, installedAppImage});
}

void FirstRunWizard::startLinuxDependencyInstaller()
{
    QStringList languages;
    for (auto *check : m_linuxLanguageChecks) if (check->isChecked()) languages << check->property("ocrCode").toString();
    QStringList args = linuxDependencyArguments(m_linuxFfmpegCheck->isChecked(), m_linuxOcrCheck->isChecked(), languages, m_linuxDesktopCheck->isChecked());
    const bool integrate = m_linuxAppImageIntegrationCheck->isChecked()
        && shouldOfferAppImageIntegration(qEnvironmentVariable("APPIMAGE"));
    if (integrate) args << QStringLiteral("--integrate-appimage");
    auto markLinuxSetupCompleted = [] {
        QSettings settings("EShot", "EShot");
        settings.setValue(QStringLiteral("wizardCompleted"), true);
        settings.setValue(QStringLiteral("linuxSetupCompleted"), true);
        settings.sync();
    };
    if (m_linuxExplicitSkip) { markLinuxSetupCompleted(); accept(); return; }
    if (args.isEmpty() && !integrate) { markLinuxSetupCompleted(); accept(); return; }
    const QString script = linuxInstallerPath();
    if (!QFileInfo::exists(script)) { m_linuxInstallStatus->setText(TranslationManager::tr("installerScriptMissingWizard")); return; }
    m_finishButton->setEnabled(false);
    if (m_skipDepsButton)
        m_skipDepsButton->setEnabled(false);
    m_linuxInstallStatus->setText(TranslationManager::tr("installingSelectedDependencies"));
    m_linuxInstallerProcess = new QProcess(this);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("ESHOT_LANGUAGE"), TranslationManager::langCode());
    m_linuxInstallerProcess->setProcessEnvironment(environment);
    const QString program = QStringLiteral("bash");
    const QStringList processArgs = QStringList{script} + args;
    qInfo() << "[FirstRunWizard] Starting optional setup:" << program << processArgs << "packages/options; no credentials are logged";
    connect(m_linuxInstallerProcess, &QProcess::started, this, [this] { m_linuxInstallStatus->setText(TranslationManager::tr("installingOptionalComponents")); });
    connect(m_linuxInstallerProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_linuxInstallerProcess) return;
        m_finishButton->setEnabled(true);
        if (m_skipDepsButton)
            m_skipDepsButton->setEnabled(true);
        m_linuxInstallStatus->setText(TranslationManager::tr("optionalSetupStartFailed"));
        m_linuxInstallerProcess->deleteLater(); m_linuxInstallerProcess = nullptr;
    });
    connect(m_linuxInstallerProcess, &QProcess::finished, this, [this, markLinuxSetupCompleted](int exitCode, QProcess::ExitStatus status) {
        if (!m_linuxInstallerProcess) return;
        qInfo() << "[FirstRunWizard] Optional setup exit status:" << exitCode << status;
        m_finishButton->setEnabled(true);
        if (m_skipDepsButton)
            m_skipDepsButton->setEnabled(true);
        if (status == QProcess::NormalExit && exitCode == 0) {
            const bool integration = m_linuxAppImageIntegrationCheck->isChecked()
                && shouldOfferAppImageIntegration(qEnvironmentVariable("APPIMAGE"));
            if (!selectedLinuxCapabilitiesAvailable(m_linuxFfmpegCheck->isChecked(), m_linuxOcrCheck->isChecked(), integration)) {
                m_linuxInstallStatus->setText(TranslationManager::tr("optionalSetupIncomplete"));
                m_linuxInstallerProcess->deleteLater(); m_linuxInstallerProcess = nullptr;
                return;
            }
            markLinuxSetupCompleted();
            m_linuxInstallStatus->setText(TranslationManager::tr("optionalSetupSucceeded"));
            showLinuxReloginHintIfNeeded(m_linuxInstallerProcess, this);
            const bool restartFromDesktop = integration && scheduleIntegratedAppRestart();
            accept();
            if (restartFromDesktop)
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            return;
        }
        const QString detail = QString::fromUtf8(m_linuxInstallerProcess->readAllStandardError()).trimmed();
        m_linuxInstallStatus->setText(TranslationManager::tr("optionalSetupFailed").arg(detail));
        m_linuxInstallerProcess->deleteLater(); m_linuxInstallerProcess = nullptr;
    });
    m_linuxInstallerProcess->start(program, processArgs);
}

namespace {
// Refuses Esc, the close button and reject() from elsewhere while the
// installer process runs, and says why in the status label.
class LinuxDependencyDialog : public QDialog {
public:
    using QDialog::QDialog;
    QProcess *installer = nullptr;
    QLabel *status = nullptr;
    void reject() override
    {
        const bool running = installer && installer->state() != QProcess::NotRunning;
        if (!linuxDependencyDialogMayClose(running)) {
            if (status)
                status->setText(TranslationManager::installRunningWaitToClose());
            return;
        }
        QDialog::reject();
    }
};
}

void FirstRunWizard::showLinuxDependencySetup(QWidget *parent)
{
    LinuxDependencyDialog dialog(parent);
    // The capture hotkey looks for this name and leaves the dialog alone.
    dialog.setObjectName(QStringLiteral("linuxDependencyDialog"));
    dialog.setWindowTitle(TranslationManager::tr("linuxDependencySetupTitle"));
    QVBoxLayout layout(&dialog);
    auto *ffmpeg = new QCheckBox(TranslationManager::tr("linuxFeatureFfmpeg")); ffmpeg->setChecked(true);
    auto *ocr = new QCheckBox(QStringLiteral("Tesseract OCR")); ocr->setChecked(true);
    auto *desktop = new QCheckBox(TranslationManager::tr("linuxFeaturePortal"));
    desktop->setChecked(defaultLinuxPortalSelection(qEnvironmentVariable("XDG_SESSION_TYPE")));
    ffmpeg->setToolTip(TranslationManager::tr("linuxFeatureFfmpegShortTip"));
    ocr->setToolTip(TranslationManager::tr("linuxFeatureOcrShortTip"));
    desktop->setToolTip(TranslationManager::tr("linuxFeaturePortalShortTip"));
    const auto defaults = defaultOcrLanguageCodes(QLocale::system().name());
    layout.addWidget(ffmpeg); layout.addWidget(ocr);
    QList<QCheckBox *> languageChecks;
    const auto names = ocrLanguageDisplayNames();
    QGridLayout languageLayout;
    int languageIndex = 0;
    for (const QString &code : supportedOcrLanguageCodes()) { auto *check = new QCheckBox(names.value(code)); check->setProperty("ocrCode", code); check->setChecked(defaults.contains(code)); check->setToolTip(TranslationManager::tr("ocrLanguageDataShortTip").arg(names.value(code))); languageLayout.addWidget(check, languageIndex / 4, languageIndex % 4); languageChecks << check; ++languageIndex; }
    layout.addLayout(&languageLayout); layout.addWidget(desktop);
    QObject::connect(ocr, &QCheckBox::toggled, &dialog, [&languageChecks](bool enabled) { for (auto *check : languageChecks) check->setEnabled(enabled); });
    QHBoxLayout buttons; auto *cancel = new QPushButton(TranslationManager::cancel()); auto *install = new QPushButton(TranslationManager::tr("installSelected")); buttons.addStretch(); buttons.addWidget(cancel); buttons.addWidget(install); layout.addLayout(&buttons);
    QLabel statusLabel; statusLabel.setWordWrap(true); layout.addWidget(&statusLabel);
    QProcess installer(&dialog);
    dialog.installer = &installer;
    dialog.status = &statusLabel;
    QProcessEnvironment installerEnvironment = QProcessEnvironment::systemEnvironment();
    installerEnvironment.insert(QStringLiteral("ESHOT_LANGUAGE"), TranslationManager::langCode());
    installer.setProcessEnvironment(installerEnvironment);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(install, &QPushButton::clicked, &dialog, [&] {
        QStringList languages; for (auto *check : languageChecks) if (check->isChecked()) languages << check->property("ocrCode").toString();
        const auto args = linuxDependencyArguments(ffmpeg->isChecked(), ocr->isChecked(), languages, desktop->isChecked());
        if (args.isEmpty()) { dialog.accept(); return; }
        if (!QFileInfo::exists(linuxInstallerPath())) { statusLabel.setText(TranslationManager::tr("installerScriptMissingDialog")); return; }
        install->setEnabled(false); cancel->setEnabled(false); statusLabel.setText(TranslationManager::tr("installingSelectedDependencies"));
        installer.start("bash", QStringList{linuxInstallerPath()} + args);
    });
    QObject::connect(&installer, &QProcess::finished, &dialog, [&](int exitCode, QProcess::ExitStatus exitStatus) {
        install->setEnabled(true); cancel->setEnabled(true);
        if (exitStatus == QProcess::NormalExit && exitCode == 0) { statusLabel.setText(TranslationManager::tr("dependenciesInstalled")); showLinuxReloginHintIfNeeded(&installer, &dialog); dialog.accept(); return; }
        const QString detail = QString::fromUtf8(installer.readAllStandardError()).trimmed();
        statusLabel.setText(TranslationManager::tr("dependencyInstallFailed").arg(detail));
    });
    QObject::connect(&installer, &QProcess::errorOccurred, &dialog, [&](QProcess::ProcessError) { install->setEnabled(true); cancel->setEnabled(true); statusLabel.setText(TranslationManager::tr("installerStartFailed")); });
    dialog.exec();
}
#endif
