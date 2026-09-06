#include "FirstRunWizard.h"
#include "../core/HotkeyManager.h"
#include "../core/TranslationManager.h"
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
    setWindowTitle(TranslationManager::wizardTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setMinimumSize(560, 400);
    resize(640, 720);

    setupUi();
    loadDefaults();
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

static QKeySequence win32ToKeySequence(UINT modifiers, UINT vkey)
{
    Qt::KeyboardModifiers qtMod = Qt::NoModifier;
    if (modifiers & MOD_SHIFT)   qtMod |= Qt::ShiftModifier;
    if (modifiers & MOD_CONTROL) qtMod |= Qt::ControlModifier;
    if (modifiers & MOD_ALT)     qtMod |= Qt::AltModifier;
    if (modifiers & MOD_WIN)     qtMod |= Qt::MetaModifier;

    struct { UINT win; Qt::Key qt; } mapping[] = {
        { VK_SNAPSHOT, Qt::Key_Print },
        { VK_SCROLL, Qt::Key_ScrollLock }, { VK_PAUSE, Qt::Key_Pause },
        { VK_CAPITAL, Qt::Key_CapsLock },  { VK_NUMLOCK, Qt::Key_NumLock },
        { VK_APPS, Qt::Key_Menu },
        { VK_F1,  Qt::Key_F1  }, { VK_F2,  Qt::Key_F2  }, { VK_F3,  Qt::Key_F3  },
        { VK_F4,  Qt::Key_F4  }, { VK_F5,  Qt::Key_F5  }, { VK_F6,  Qt::Key_F6  },
        { VK_F7,  Qt::Key_F7  }, { VK_F8,  Qt::Key_F8  }, { VK_F9,  Qt::Key_F9  },
        { VK_F10, Qt::Key_F10 }, { VK_F11, Qt::Key_F11 }, { VK_F12, Qt::Key_F12 },
        { VK_F13, Qt::Key_F13 }, { VK_F14, Qt::Key_F14 }, { VK_F15, Qt::Key_F15 },
        { VK_F16, Qt::Key_F16 }, { VK_F17, Qt::Key_F17 }, { VK_F18, Qt::Key_F18 },
        { VK_F19, Qt::Key_F19 }, { VK_F20, Qt::Key_F20 }, { VK_F21, Qt::Key_F21 },
        { VK_F22, Qt::Key_F22 }, { VK_F23, Qt::Key_F23 }, { VK_F24, Qt::Key_F24 },
        { VK_HOME,   Qt::Key_Home   }, { VK_END,    Qt::Key_End      },
        { VK_PRIOR,  Qt::Key_PageUp }, { VK_NEXT,   Qt::Key_PageDown  },
        { VK_INSERT, Qt::Key_Insert }, { VK_DELETE, Qt::Key_Delete    },
        { VK_LEFT,   Qt::Key_Left   }, { VK_RIGHT,  Qt::Key_Right     },
        { VK_UP,     Qt::Key_Up     }, { VK_DOWN,   Qt::Key_Down      },
        { VK_SPACE,  Qt::Key_Space  }, { VK_RETURN, Qt::Key_Return    },
        { VK_ESCAPE, Qt::Key_Escape }, { VK_TAB,    Qt::Key_Tab       },
        { VK_BACK,   Qt::Key_Backspace },
    };
    for (auto &m : mapping) {
        if (vkey == m.win) return QKeySequence(QKeyCombination(qtMod, m.qt));
    }
    if (vkey >= 'A' && vkey <= 'Z')
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_A + (vkey - 'A'))));
    if (vkey >= '0' && vkey <= '9')
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_0 + (vkey - '0'))));
    if (vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9)
        return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, static_cast<Qt::Key>(Qt::Key_0 + (vkey - VK_NUMPAD0))));
    if (vkey == VK_ADD)      return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Plus));
    if (vkey == VK_SUBTRACT) return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Minus));
    if (vkey == VK_MULTIPLY) return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Asterisk));
    if (vkey == VK_DIVIDE)   return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Slash));
    if (vkey == VK_DECIMAL)  return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Period));
    return QKeySequence(Qt::Key_Print);
}

bool FirstRunWizard::keySequenceToWin32(const QKeySequence &seq, UINT &modifiers, UINT &vkey)
{
    if (seq.isEmpty()) return false;

    QKeyCombination combo = seq[0];
    Qt::KeyboardModifiers qtMod = combo.keyboardModifiers();
    Qt::Key qtKey = combo.key();

    if (qtKey == Qt::Key_unknown ||
        qtKey == Qt::Key_Shift ||
        qtKey == Qt::Key_Control ||
        qtKey == Qt::Key_Alt ||
        qtKey == Qt::Key_Meta)
        return false;

    modifiers = 0;
    if (qtMod & Qt::ShiftModifier)   modifiers |= MOD_SHIFT;
    if (qtMod & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (qtMod & Qt::AltModifier)     modifiers |= MOD_ALT;
    if (qtMod & Qt::MetaModifier)    modifiers |= MOD_WIN;
    const bool keypad = qtMod.testFlag(Qt::KeypadModifier);
    qtMod &= ~Qt::KeypadModifier;

    struct { Qt::Key qt; UINT win; } mapping[] = {
        { Qt::Key_Print,      VK_SNAPSHOT },
        { Qt::Key_ScrollLock, VK_SCROLL }, { Qt::Key_Pause, VK_PAUSE },
        { Qt::Key_CapsLock,   VK_CAPITAL },{ Qt::Key_NumLock, VK_NUMLOCK },
        { Qt::Key_Menu,       VK_APPS },
        { Qt::Key_F1,         VK_F1 }, { Qt::Key_F2,  VK_F2  }, { Qt::Key_F3,  VK_F3  },
        { Qt::Key_F4,         VK_F4 }, { Qt::Key_F5,  VK_F5  }, { Qt::Key_F6,  VK_F6  },
        { Qt::Key_F7,         VK_F7 }, { Qt::Key_F8,  VK_F8  }, { Qt::Key_F9,  VK_F9  },
        { Qt::Key_F10,        VK_F10}, { Qt::Key_F11, VK_F11 }, { Qt::Key_F12, VK_F12 },
        { Qt::Key_F13,        VK_F13}, { Qt::Key_F14, VK_F14 }, { Qt::Key_F15, VK_F15 },
        { Qt::Key_F16,        VK_F16}, { Qt::Key_F17, VK_F17 }, { Qt::Key_F18, VK_F18 },
        { Qt::Key_F19,        VK_F19}, { Qt::Key_F20, VK_F20 }, { Qt::Key_F21, VK_F21 },
        { Qt::Key_F22,        VK_F22}, { Qt::Key_F23, VK_F23 }, { Qt::Key_F24, VK_F24 },
        { Qt::Key_Home,       VK_HOME },  { Qt::Key_End,    VK_END    },
        { Qt::Key_PageUp,     VK_PRIOR }, { Qt::Key_PageDown,VK_NEXT  },
        { Qt::Key_Insert,     VK_INSERT },{ Qt::Key_Delete,  VK_DELETE },
        { Qt::Key_Left,       VK_LEFT },  { Qt::Key_Right,   VK_RIGHT },
        { Qt::Key_Up,         VK_UP   },  { Qt::Key_Down,    VK_DOWN  },
        { Qt::Key_Space,      VK_SPACE }, { Qt::Key_Return,  VK_RETURN},
        { Qt::Key_Escape,     VK_ESCAPE },{ Qt::Key_Tab,     VK_TAB   },
        { Qt::Key_Backspace,  VK_BACK  },
    };
    for (auto &m : mapping) {
        if (qtKey == m.qt) { vkey = m.win; return true; }
    }

    if (keypad) {
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
            vkey = VK_NUMPAD0 + (qtKey - Qt::Key_0);
            return true;
        }
        if (qtKey == Qt::Key_Plus)     { vkey = VK_ADD; return true; }
        if (qtKey == Qt::Key_Minus)    { vkey = VK_SUBTRACT; return true; }
        if (qtKey == Qt::Key_Asterisk) { vkey = VK_MULTIPLY; return true; }
        if (qtKey == Qt::Key_Slash)    { vkey = VK_DIVIDE; return true; }
        if (qtKey == Qt::Key_Period)   { vkey = VK_DECIMAL; return true; }
        if (qtKey == Qt::Key_Enter || qtKey == Qt::Key_Return) { vkey = VK_RETURN; return true; }
    }

    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        if (qtMod == Qt::NoModifier) return false;
        vkey = 'A' + (qtKey - Qt::Key_A);
        return true;
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        if (qtMod == Qt::NoModifier) return false;
        vkey = '0' + (qtKey - Qt::Key_0);
        return true;
    }
    return false;
}

void FirstRunWizard::setupUi()
{
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

    QLabel *titleLabel = new QLabel(TranslationManager::wizardTitle());
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel(TranslationManager::wizardDesc());
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #aaa;");
    mainLayout->addWidget(descLabel);

    QGroupBox *langGroup = new QGroupBox(TranslationManager::language());
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
    m_langCombo->setToolTip(tr("Language used by EShot after setup. The setup screen starts in English."));
    langLayout->addWidget(m_langCombo);
    mainLayout->addWidget(langGroup);

    QGroupBox *hkGroup = new QGroupBox(TranslationManager::hotkeyTitle());
    QVBoxLayout *hkLayout = new QVBoxLayout(hkGroup);
    QLabel *hkDesc = new QLabel(TranslationManager::wizardHotkeyDesc());
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

    m_printScreenConflictLabel = new QLabel(TranslationManager::printScreenConflictMessage());
    m_printScreenConflictLabel->setWordWrap(true);
    m_printScreenConflictLabel->setStyleSheet(
        "background: rgba(255, 193, 7, 0.14); color: #ffd166; "
        "border: 1px solid rgba(255, 193, 7, 0.45); border-radius: 6px; "
        "padding: 7px; font-size: 12px;");
    hkLayout->addWidget(m_printScreenConflictLabel);

    m_printScreenFixButton = new QPushButton(TranslationManager::printScreenConflictFix());
    connect(m_printScreenFixButton, &QPushButton::clicked,
            this, &FirstRunWizard::onDisableWindowsPrintScreenSnipping);
    hkLayout->addWidget(m_printScreenFixButton);
#ifdef Q_OS_LINUX
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    auto *activatePrintButton = new QPushButton(tr("Use Print Screen for EShot"));
    activatePrintButton->setToolTip(desktop == LinuxDesktopEnvironment::Kde
        ? tr("Removes only the plain Print Screen shortcut from Spectacle and assigns it to EShot. Spectacle keeps its other shortcuts.")
        : tr("Asks the desktop to assign Print Screen to EShot. Other shortcuts are left unchanged."));
    connect(activatePrintButton, &QPushButton::clicked,
            this, &FirstRunWizard::onActivateLinuxPrintScreen);
    hkLayout->addWidget(activatePrintButton);
#endif
    mainLayout->addWidget(hkGroup);

    QGroupBox *pathGroup = new QGroupBox(TranslationManager::saveDir());
    QHBoxLayout *pathLayout = new QHBoxLayout(pathGroup);
    m_savePathEdit = new QLineEdit();
    QPushButton *browseBtn = new QPushButton(TranslationManager::browse());
    connect(browseBtn, &QPushButton::clicked, this, &FirstRunWizard::onBrowse);
    pathLayout->addWidget(m_savePathEdit);
    pathLayout->addWidget(browseBtn);
    mainLayout->addWidget(pathGroup);

#ifdef Q_OS_LINUX
    QGroupBox *depsGroup = new QGroupBox(tr("Optional Linux features"));
    QVBoxLayout *depsLayout = new QVBoxLayout(depsGroup);
    QLabel *depsHint = new QLabel(tr("Select optional features to install with the system package manager. You can skip and retry from Settings."));
    depsHint->setWordWrap(true); depsLayout->addWidget(depsHint);
    m_linuxFfmpegCheck = new QCheckBox(tr("FFmpeg (video and GIF recording)"));
    m_linuxOcrCheck = new QCheckBox(tr("Tesseract OCR"));
    m_linuxDesktopCheck = new QCheckBox(tr("Wayland recording and desktop portal packages"));
    m_linuxAppImageIntegrationCheck = new QCheckBox(tr("Add EShot to the application menu and install shortcuts"));
    m_linuxFfmpegCheck->setToolTip(tr("Installs the media encoder used to save MP4 videos and GIF recordings. Screenshots work without it."));
    m_linuxOcrCheck->setToolTip(tr("Installs text recognition so EShot can read and copy text from screenshots. Select OCR languages below."));
    m_linuxDesktopCheck->setToolTip(tr("Installs PipeWire and desktop portal components used for secure screen sharing and recording on Wayland desktops such as KDE Plasma and GNOME."));
    m_linuxAppImageIntegrationCheck->setToolTip(tr("Copies this AppImage to your user applications folder and adds EShot to the application menu. No system-wide installation is performed. After setup, use the application-menu entry; the downloaded AppImage remains a separate portable copy."));
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
    for (const QString &code : supportedOcrLanguageCodes()) { auto *check = new QCheckBox(names.value(code)); check->setProperty("ocrCode", code); check->setChecked(defaults.contains(code)); check->setToolTip(tr("OCR language data for recognizing text written in %1. This does not change the EShot interface language.").arg(names.value(code))); languages->addWidget(check, languageIndex / 2, languageIndex % 2); m_linuxLanguageChecks << check; ++languageIndex; }
    depsLayout->addLayout(languages);
    connect(m_linuxOcrCheck, &QCheckBox::toggled, depsGroup, [this](bool enabled) { for (auto *check : m_linuxLanguageChecks) check->setEnabled(enabled); });
    QPushButton *skipDeps = new QPushButton(tr("Skip optional dependency setup"));
    skipDeps->setToolTip(tr("Starts EShot without installing optional recording or OCR components. You can reopen this setup from Settings later."));
    connect(skipDeps, &QPushButton::clicked, depsGroup, [this] { m_linuxExplicitSkip = true; m_linuxFfmpegCheck->setChecked(false); m_linuxOcrCheck->setChecked(false); m_linuxDesktopCheck->setChecked(false); m_linuxAppImageIntegrationCheck->setChecked(false); m_linuxInstallStatus->setText(tr("Optional setup will be skipped. Click Finish to continue.")); });
    depsLayout->addWidget(skipDeps);
    m_skipDepsButton = skipDeps;
    m_linuxInstallStatus = new QLabel(); m_linuxInstallStatus->setWordWrap(true); depsLayout->addWidget(m_linuxInstallStatus);
    mainLayout->addWidget(depsGroup);
#endif

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *finishBtn = new QPushButton(TranslationManager::wizardFinish());
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

    int langInt = s.value("language", static_cast<int>(TranslationManager::currentLanguage())).toInt();
#ifdef Q_OS_LINUX
    // A configuration carried over from another platform must not silently
    // choose the language for Linux's first-run setup. main.cpp deliberately
    // starts that fresh onboarding flow in English; preserve that choice until
    // the user explicitly selects a language in this wizard.
    if (!s.value(QStringLiteral("linuxSetupCompleted"), false).toBool())
        langInt = static_cast<int>(TranslationManager::currentLanguage());
#endif
    static const char* langCodes[] = {"tr","en","de","fr","es","ja","zh","ru"};
    QString lang = (langInt >= 0 && langInt < TranslationManager::LangCount) ? langCodes[langInt] : "en";
    int li = m_langCombo->findData(lang);
    if (li >= 0) m_langCombo->setCurrentIndex(li);

    UINT savedMod = static_cast<UINT>(s.value("hotkeyModifiers", 0).toUInt());
    UINT savedVKey = static_cast<UINT>(s.value("hotkeyVKey", VK_SNAPSHOT).toUInt());
    m_hotkeyEdit->setKeySequence(win32ToKeySequence(savedMod, savedVKey));

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
        m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
    } else {
        m_hotkeyStatusLabel->setText(TranslationManager::hotkeyInvalid());
        m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
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
        m_hotkeyStatusLabel->setText(tr("Automatic Print Screen activation is currently available on KDE Plasma and GNOME."));
        m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
        return;
    }

    m_hotkeyEdit->setKeySequence(QKeySequence(Qt::Key_Print));
    if (desktop == LinuxDesktopEnvironment::Gnome) {
        if (HotkeyManager::instance().linuxPortalShortcutsAvailable()) {
            if (!HotkeyManager::instance().reRegisterCaptureHotkey(0, VK_SNAPSHOT)) {
                m_hotkeyStatusLabel->setText(tr("GNOME did not allow the Print Screen shortcut. Try again from Settings."));
                m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
                return;
            }
            if (shouldOfferAppImageIntegration(qEnvironmentVariable("APPIMAGE")))
                m_linuxAppImageIntegrationCheck->setChecked(true);
            m_hotkeyStatusLabel->setText(tr("Choose Print Screen in the GNOME shortcut permission window."));
            m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
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
            m_hotkeyStatusLabel->setText(tr("Could not configure GNOME Print Screen: %1").arg(installed.error));
            m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
            return;
        }
        m_hotkeyStatusLabel->setText(tr("Print Screen activated for EShot in GNOME."));
        m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
        return;
    }

    if (!HotkeyManager::instance().reRegisterCaptureHotkey(0, VK_SNAPSHOT)) {
        m_hotkeyStatusLabel->setText(tr("EShot could not register Print Screen with KDE. Spectacle was not changed."));
        m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
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
        m_hotkeyStatusLabel->setText(tr("Could not read KDE shortcuts. Open System Settings > Shortcuts and remove Print from Spectacle."));
        m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
        return;
    }

    const QList<int> remaining = kdeShortcutsWithoutPlainPrint(currentReply.value());
    const QDBusReply<void> setReply = globalAccel.call(
        QStringLiteral("setForeignShortcut"),
        spectacleLaunchId,
        QVariant::fromValue(remaining));
    if (!setReply.isValid()) {
        m_hotkeyStatusLabel->setText(tr("KDE did not allow the shortcut change. Use System Settings > Shortcuts instead."));
        m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
        return;
    }

    if (shouldOfferAppImageIntegration(qEnvironmentVariable("APPIMAGE")))
        m_linuxAppImageIntegrationCheck->setChecked(true);
    m_hotkeyStatusLabel->setText(tr("Print Screen activated for EShot. Spectacle's other shortcuts were kept."));
    m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
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
                tr("Could not configure the GNOME capture shortcut: %1")
                    .arg(installed.error));
            return;
        }
    }
#endif

    if (!deferHotkeyRegistration
        && !HotkeyManager::instance().reRegisterCaptureHotkey(modifiers, vkey)) {
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            TranslationManager::hotkeyMayBeInUse());
        return;
    }

    QString lang = m_langCombo->currentData().toString();
    TranslationManager::Language langEnum = TranslationManager::English;
    if (lang == "tr") langEnum = TranslationManager::Turkish;
    else if (lang == "de") langEnum = TranslationManager::German;
    else if (lang == "fr") langEnum = TranslationManager::French;
    else if (lang == "es") langEnum = TranslationManager::Spanish;
    else if (lang == "ja") langEnum = TranslationManager::Japanese;
    else if (lang == "zh") langEnum = TranslationManager::Chinese;
    else if (lang == "ru") langEnum = TranslationManager::Russian;

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

#ifdef Q_OS_LINUX
static QString linuxInstallerPath()
{
    const QString appDir = qEnvironmentVariable("ESHOT_APPDIR");
    if (!appDir.isEmpty()) return QDir(appDir).filePath("usr/lib/eshot/install-runtime-deps.sh");
    return QDir(QCoreApplication::applicationDirPath()).filePath("../lib/eshot/install-runtime-deps.sh");
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
    if (!QFileInfo::exists(script)) { m_linuxInstallStatus->setText(tr("Installer script was not found. Retry after reinstalling EShot, or skip optional setup.")); return; }
    m_finishButton->setEnabled(false);
    if (m_skipDepsButton)
        m_skipDepsButton->setEnabled(false);
    m_linuxInstallStatus->setText(tr("Installing selected dependencies…"));
    m_linuxInstallerProcess = new QProcess(this);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("ESHOT_LANGUAGE"), TranslationManager::langCode());
    m_linuxInstallerProcess->setProcessEnvironment(environment);
    const QString program = QStringLiteral("bash");
    const QStringList processArgs = QStringList{script} + args;
    qInfo() << "[FirstRunWizard] Starting optional setup:" << program << processArgs << "packages/options; no credentials are logged";
    connect(m_linuxInstallerProcess, &QProcess::started, this, [this] { m_linuxInstallStatus->setText(tr("Installing selected optional components…")); });
    connect(m_linuxInstallerProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_linuxInstallerProcess) return;
        m_finishButton->setEnabled(true);
        if (m_skipDepsButton)
            m_skipDepsButton->setEnabled(true);
        m_linuxInstallStatus->setText(tr("Could not start optional setup. Click Finish to retry, or choose Skip."));
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
                m_linuxInstallStatus->setText(tr("Setup finished, but one or more selected capabilities are still unavailable. Click Finish to retry, or choose Skip."));
                m_linuxInstallerProcess->deleteLater(); m_linuxInstallerProcess = nullptr;
                return;
            }
            markLinuxSetupCompleted();
            m_linuxInstallStatus->setText(tr("Selected optional components installed successfully."));
            const bool restartFromDesktop = integration && scheduleIntegratedAppRestart();
            accept();
            if (restartFromDesktop)
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            return;
        }
        const QString detail = QString::fromUtf8(m_linuxInstallerProcess->readAllStandardError()).trimmed();
        m_linuxInstallStatus->setText(tr("Installation failed or authorization was cancelled. Check your package manager, then click Finish to retry, or choose Skip. %1").arg(detail));
        m_linuxInstallerProcess->deleteLater(); m_linuxInstallerProcess = nullptr;
    });
    m_linuxInstallerProcess->start(program, processArgs);
}

void FirstRunWizard::showLinuxDependencySetup(QWidget *parent)
{
    QDialog dialog(parent); dialog.setWindowTitle(QObject::tr("Linux dependency setup"));
    QVBoxLayout layout(&dialog);
    auto *ffmpeg = new QCheckBox(QObject::tr("FFmpeg (video and GIF recording)")); ffmpeg->setChecked(true);
    auto *ocr = new QCheckBox(QObject::tr("Tesseract OCR")); ocr->setChecked(true);
    auto *desktop = new QCheckBox(QObject::tr("Wayland recording and desktop portal packages"));
    desktop->setChecked(defaultLinuxPortalSelection(qEnvironmentVariable("XDG_SESSION_TYPE")));
    ffmpeg->setToolTip(QObject::tr("Installs the media encoder used to save MP4 videos and GIF recordings."));
    ocr->setToolTip(QObject::tr("Installs text recognition and the selected OCR language data."));
    desktop->setToolTip(QObject::tr("Installs PipeWire and desktop portal components for secure screen recording on Wayland."));
    const auto defaults = defaultOcrLanguageCodes(QLocale::system().name());
    layout.addWidget(ffmpeg); layout.addWidget(ocr);
    QList<QCheckBox *> languageChecks;
    const auto names = ocrLanguageDisplayNames();
    QGridLayout languageLayout;
    int languageIndex = 0;
    for (const QString &code : supportedOcrLanguageCodes()) { auto *check = new QCheckBox(names.value(code)); check->setProperty("ocrCode", code); check->setChecked(defaults.contains(code)); check->setToolTip(QObject::tr("OCR language data for recognizing text written in %1.").arg(names.value(code))); languageLayout.addWidget(check, languageIndex / 4, languageIndex % 4); languageChecks << check; ++languageIndex; }
    layout.addLayout(&languageLayout); layout.addWidget(desktop);
    QObject::connect(ocr, &QCheckBox::toggled, &dialog, [&languageChecks](bool enabled) { for (auto *check : languageChecks) check->setEnabled(enabled); });
    QHBoxLayout buttons; auto *cancel = new QPushButton(QObject::tr("Cancel")); auto *install = new QPushButton(QObject::tr("Install selected")); buttons.addStretch(); buttons.addWidget(cancel); buttons.addWidget(install); layout.addLayout(&buttons);
    QLabel statusLabel; statusLabel.setWordWrap(true); layout.addWidget(&statusLabel);
    QProcess installer(&dialog);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(install, &QPushButton::clicked, &dialog, [&] {
        QStringList languages; for (auto *check : languageChecks) if (check->isChecked()) languages << check->property("ocrCode").toString();
        const auto args = linuxDependencyArguments(ffmpeg->isChecked(), ocr->isChecked(), languages, desktop->isChecked());
        if (args.isEmpty()) { dialog.accept(); return; }
        if (!QFileInfo::exists(linuxInstallerPath())) { statusLabel.setText(QObject::tr("Installer script not found. Reinstall EShot and retry, or cancel.")); return; }
        install->setEnabled(false); cancel->setEnabled(false); statusLabel.setText(QObject::tr("Installing selected dependencies…"));
        installer.start("bash", QStringList{linuxInstallerPath()} + args);
    });
    QObject::connect(&installer, &QProcess::finished, &dialog, [&](int exitCode, QProcess::ExitStatus exitStatus) {
        install->setEnabled(true); cancel->setEnabled(true);
        if (exitStatus == QProcess::NormalExit && exitCode == 0) { statusLabel.setText(QObject::tr("Selected dependencies installed successfully.")); dialog.accept(); return; }
        const QString detail = QString::fromUtf8(installer.readAllStandardError()).trimmed();
        statusLabel.setText(QObject::tr("Installation failed or authorization was cancelled. Check your package manager, then retry or cancel. %1").arg(detail));
    });
    QObject::connect(&installer, &QProcess::errorOccurred, &dialog, [&](QProcess::ProcessError) { install->setEnabled(true); cancel->setEnabled(true); statusLabel.setText(QObject::tr("Could not start the installer. Retry or cancel.")); });
    dialog.exec();
}
#endif
