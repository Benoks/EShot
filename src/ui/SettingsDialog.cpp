#include "SettingsDialog.h"
#include "../core/HotkeyManager.h"
#include "../core/TranslationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QListWidgetItem>
#include <QKeySequenceEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(TranslationManager::settingsTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setMinimumSize(560, 580);
    setMaximumSize(750, 720);

    m_settings = new QSettings("EShot", "EShot", this);
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog() {}

QString SettingsDialog::resolvePatternPreview(const QString &pattern) const
{
    QDateTime now = QDateTime::currentDateTime();
    QString r = pattern;
    r.replace("%Y", now.toString("yyyy"));
    r.replace("%y", now.toString("yy"));
    r.replace("%M", now.toString("MM"));
    r.replace("%D", now.toString("dd"));
    r.replace("%h", now.toString("HH"));
    r.replace("%m", now.toString("mm"));
    r.replace("%s", now.toString("ss"));
    r.replace("%T", "WindowTitle");
    return r;
}

bool SettingsDialog::keySequenceToWin32(const QKeySequence &seq, UINT &modifiers, UINT &vkey)
{
#ifdef Q_OS_WIN
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

    struct { Qt::Key qt; UINT win; } mapping[] = {
        { Qt::Key_Print,      VK_SNAPSHOT },
        { Qt::Key_F1,         VK_F1 }, { Qt::Key_F2,  VK_F2  }, { Qt::Key_F3,  VK_F3  },
        { Qt::Key_F4,         VK_F4 }, { Qt::Key_F5,  VK_F5  }, { Qt::Key_F6,  VK_F6  },
        { Qt::Key_F7,         VK_F7 }, { Qt::Key_F8,  VK_F8  }, { Qt::Key_F9,  VK_F9  },
        { Qt::Key_F10,        VK_F10}, { Qt::Key_F11, VK_F11 }, { Qt::Key_F12, VK_F12 },
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
#else
    Q_UNUSED(seq); Q_UNUSED(modifiers); Q_UNUSED(vkey);
    return false;
#endif
}

static QKeySequence win32ToKeySequence(UINT modifiers, UINT vkey)
{
    Qt::KeyboardModifiers qtMod;
#ifdef Q_OS_WIN
    if (modifiers & MOD_SHIFT)   qtMod |= Qt::ShiftModifier;
    if (modifiers & MOD_CONTROL) qtMod |= Qt::ControlModifier;
    if (modifiers & MOD_ALT)     qtMod |= Qt::AltModifier;
    if (modifiers & MOD_WIN)     qtMod |= Qt::MetaModifier;

    struct { UINT win; Qt::Key qt; } mapping[] = {
        { VK_SNAPSHOT, Qt::Key_Print },
        { VK_F1,  Qt::Key_F1  }, { VK_F2,  Qt::Key_F2  }, { VK_F3,  Qt::Key_F3  },
        { VK_F4,  Qt::Key_F4  }, { VK_F5,  Qt::Key_F5  }, { VK_F6,  Qt::Key_F6  },
        { VK_F7,  Qt::Key_F7  }, { VK_F8,  Qt::Key_F8  }, { VK_F9,  Qt::Key_F9  },
        { VK_F10, Qt::Key_F10 }, { VK_F11, Qt::Key_F11 }, { VK_F12, Qt::Key_F12 },
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
    if (vkey >= 'A' && vkey <= 'Z') {
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_A + (vkey - 'A'))));
    }
    if (vkey >= '0' && vkey <= '9') {
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_0 + (vkey - '0'))));
    }
#else
    Q_UNUSED(modifiers); Q_UNUSED(vkey);
#endif
    return QKeySequence(Qt::Key_Print);
}

bool SettingsDialog::isAutoStartEnabled()
{
#ifdef Q_OS_WIN
    return QProcess::execute(QStringLiteral("schtasks"),
                             {QStringLiteral("/Query"), QStringLiteral("/TN"), QStringLiteral("EShot")}) == 0;
#else
    return false;
#endif
}

static bool setAutoStartTask(bool enabled)
{
#ifdef Q_OS_WIN
    QProcess::execute(QStringLiteral("schtasks"),
                      {QStringLiteral("/Delete"), QStringLiteral("/TN"), QStringLiteral("EShot"), QStringLiteral("/F")});

    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    reg.remove(QStringLiteral("EShot"));

    if (!enabled)
        return true;

    QString appPath = QCoreApplication::applicationFilePath().replace('/', '\\');
    QString taskCommand = QStringLiteral("\"%1\" --silent").arg(appPath);
    return QProcess::execute(QStringLiteral("schtasks"),
                             {QStringLiteral("/Create"),
                              QStringLiteral("/TN"), QStringLiteral("EShot"),
                              QStringLiteral("/SC"), QStringLiteral("ONLOGON"),
                              QStringLiteral("/RL"), QStringLiteral("HIGHEST"),
                              QStringLiteral("/TR"), taskCommand,
                              QStringLiteral("/F")}) == 0;
#else
    Q_UNUSED(enabled);
    return true;
#endif
}

static QString printScreenConflictTitle()
{
    return TranslationManager::currentLanguage() == TranslationManager::Turkish
        ? QString::fromUtf8("Print Screen çakışması")
        : QStringLiteral("Print Screen conflict");
}

static QString printScreenConflictMessage()
{
    return TranslationManager::currentLanguage() == TranslationManager::Turkish
        ? QString::fromUtf8("Windows, Print Screen tuşunu Snipping Tool için kullanıyor. EShot'un Print Screen ile çalışması için bu Windows ayarını kapatmanız gerekir.")
        : QStringLiteral("Windows is using the Print Screen key for Snipping Tool. Turn off this Windows setting to let EShot use Print Screen.");
}

static QString printScreenConflictFixText()
{
    return TranslationManager::currentLanguage() == TranslationManager::Turkish
        ? QString::fromUtf8("Windows Print Screen ayarını kapat")
        : QStringLiteral("Disable Windows Print Screen shortcut");
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel(TranslationManager::settingsTitle());
    QFont tf = titleLabel->font(); tf.setPointSize(16); tf.setBold(true);
    titleLabel->setFont(tf);
    mainLayout->addWidget(titleLabel);

    QTabWidget *tabs = new QTabWidget(this);
    tabs->addTab(createGeneralTab(),     TranslationManager::tabGeneral());
    tabs->addTab(createCaptureTab(),     TranslationManager::tabCapture());
    tabs->addTab(createRecordingTab(),   TranslationManager::tabRecording());
    tabs->addTab(createAppearanceTab(),  TranslationManager::tabAppearance());
    tabs->addTab(createInterfaceTab(),   TranslationManager::tabInterface());
    tabs->addTab(createHotkeyTab(),      TranslationManager::tabHotkey());
    mainLayout->addWidget(tabs);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *resetBtn = new QPushButton(TranslationManager::reset());
    resetBtn->setStyleSheet("color: #ff6b6b;");
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::onReset);
    btnLayout->addWidget(resetBtn);

    QPushButton *cancelBtn = new QPushButton(TranslationManager::cancel());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton(TranslationManager::save());
    saveBtn->setDefault(true);
    saveBtn->setStyleSheet(R"(
        QPushButton { background-color: #0078D4; color: white; border: none;
                      padding: 8px 24px; border-radius: 4px; font-weight: bold; }
        QPushButton:hover { background-color: #1a8cff; }
    )");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

QWidget* SettingsDialog::createGeneralTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Language selection
    QGroupBox *langGroup = new QGroupBox(TranslationManager::language());
    QHBoxLayout *langLayout = new QHBoxLayout(langGroup);
    m_langCombo = new QComboBox();
    m_langCombo->addItem(TranslationManager::langTurkish(), "tr");
    m_langCombo->addItem(TranslationManager::langEnglish(), "en");
    m_langCombo->addItem(TranslationManager::langGerman(), "de");
    m_langCombo->addItem(TranslationManager::langFrench(), "fr");
    m_langCombo->addItem(TranslationManager::langSpanish(), "es");
    m_langCombo->addItem(TranslationManager::langJapanese(), "ja");
    m_langCombo->addItem(TranslationManager::langChinese(), "zh");
    m_langCombo->addItem(TranslationManager::langRussian(), "ru");
    m_langCombo->setToolTip(TranslationManager::tipLanguage());
    langLayout->addWidget(m_langCombo);
    langLayout->addStretch();
    layout->addWidget(langGroup);

    // Save directory
    QGroupBox *pathGroup = new QGroupBox(TranslationManager::saveDir());
    QHBoxLayout *pathLayout = new QHBoxLayout(pathGroup);
    m_savePathEdit = new QLineEdit();
    m_savePathEdit->setPlaceholderText(TranslationManager::saveDirDesc());
    m_savePathEdit->setToolTip(TranslationManager::tipSaveDir());
    QPushButton *browseBtn = new QPushButton(TranslationManager::browse());
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowse);
    pathLayout->addWidget(m_savePathEdit);
    pathLayout->addWidget(browseBtn);
    layout->addWidget(pathGroup);

    // Filename template
    QGroupBox *fnGroup = new QGroupBox(TranslationManager::filenamePattern());
    QVBoxLayout *fnLayout = new QVBoxLayout(fnGroup);
    m_filenamePatternEdit = new QLineEdit();
    m_filenamePatternEdit->setPlaceholderText("Screenshot_%Y-%M-%D_%h-%m-%s");
    connect(m_filenamePatternEdit, &QLineEdit::textChanged, this, &SettingsDialog::onFilenamePatternChanged);
    fnLayout->addWidget(m_filenamePatternEdit);
    m_patternPreviewLabel = new QLabel();
    m_patternPreviewLabel->setStyleSheet("color: #888; font-style: italic;");
    fnLayout->addWidget(m_patternPreviewLabel);
    QLabel *helpLabel = new QLabel(TranslationManager::patternVars());
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("color: #999; font-size: 11px;");
    fnLayout->addWidget(helpLabel);
    layout->addWidget(fnGroup);

    // General options
    QGroupBox *genGroup = new QGroupBox(TranslationManager::generalOptions());
    QVBoxLayout *genLayout = new QVBoxLayout(genGroup);
    m_autoStartCheck         = new QCheckBox(TranslationManager::autoStart());
    m_showNotificationsCheck = new QCheckBox(TranslationManager::showNotifications());
    m_playSoundCheck         = new QCheckBox(TranslationManager::playSound());
    m_copyPathAfterSaveCheck = new QCheckBox(TranslationManager::copyPathAfterSave());
    m_autoStartCheck->setToolTip(TranslationManager::tipAutoStart());
    m_showNotificationsCheck->setToolTip(TranslationManager::tipNotifications());
    m_playSoundCheck->setToolTip(TranslationManager::tipPlaySound());
    m_copyPathAfterSaveCheck->setToolTip(TranslationManager::tipCopyPath());
    genLayout->addWidget(m_autoStartCheck);
    genLayout->addWidget(m_showNotificationsCheck);
    genLayout->addWidget(m_playSoundCheck);
    genLayout->addWidget(m_copyPathAfterSaveCheck);
    layout->addWidget(genGroup);

    // Accessibility
    QGroupBox *accessGroup = new QGroupBox(TranslationManager::accessibility());
    QVBoxLayout *accessLayout = new QVBoxLayout(accessGroup);
    m_highContrastCheck = new QCheckBox(TranslationManager::highContrast());
    m_highContrastCheck->setToolTip(TranslationManager::tipHighContrast());
    connect(m_highContrastCheck, &QCheckBox::toggled, this, &SettingsDialog::onThemeChanged);
    accessLayout->addWidget(m_highContrastCheck);
    layout->addWidget(accessGroup);

    // Import/export settings
    QGroupBox *impExpGroup = new QGroupBox(TranslationManager::settingsExportImport());
    QHBoxLayout *impExpLayout = new QHBoxLayout(impExpGroup);
    QPushButton *exportBtn = new QPushButton(TranslationManager::exportSettings());
    connect(exportBtn, &QPushButton::clicked, this, &SettingsDialog::onExportSettings);
    QPushButton *importBtn = new QPushButton(TranslationManager::importSettings());
    connect(importBtn, &QPushButton::clicked, this, &SettingsDialog::onImportSettings);
    impExpLayout->addWidget(exportBtn);
    impExpLayout->addWidget(importBtn);
    impExpLayout->addStretch();
    layout->addWidget(impExpGroup);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createCaptureTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QGroupBox *fmtGroup = new QGroupBox(TranslationManager::fileFormat());
    QFormLayout *fmtLayout = new QFormLayout(fmtGroup);
    m_formatCombo = new QComboBox();
    m_formatCombo->addItem(TranslationManager::formatPng(), "PNG");
    m_formatCombo->addItem(TranslationManager::formatJpeg(), "JPEG");
    m_formatCombo->addItem(TranslationManager::formatBmp(), "BMP");
    fmtLayout->addRow("Format:", m_formatCombo);

    QHBoxLayout *qLayout = new QHBoxLayout();
    m_qualitySlider = new QSlider(Qt::Horizontal);
    m_qualitySlider->setRange(10, 100);
    m_qualitySpin = new QSpinBox();
    m_qualitySpin->setRange(10, 100);
    m_qualitySpin->setSuffix("%");
    connect(m_qualitySlider, &QSlider::valueChanged, m_qualitySpin, &QSpinBox::setValue);
    connect(m_qualitySpin, QOverload<int>::of(&QSpinBox::valueChanged), m_qualitySlider, &QSlider::setValue);
    qLayout->addWidget(m_qualitySlider);
    qLayout->addWidget(m_qualitySpin);
    fmtLayout->addRow(TranslationManager::jpegQuality(), qLayout);

    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        bool jpeg = (m_formatCombo->itemData(idx).toString() == "JPEG");
        m_qualitySlider->setEnabled(jpeg);
        m_qualitySpin->setEnabled(jpeg);
        onFilenamePatternChanged(m_filenamePatternEdit ? m_filenamePatternEdit->text() : QString());
    });
    layout->addWidget(fmtGroup);

    QGroupBox *capGroup = new QGroupBox(TranslationManager::captureSettings());
    QFormLayout *capLayout = new QFormLayout(capGroup);
    m_delaySpin = new QSpinBox();
    m_delaySpin->setRange(0, 10000);
    m_delaySpin->setSingleStep(500);
    m_delaySpin->setSuffix(" ms");
    m_delaySpin->setSpecialValueText(TranslationManager::noDelay());
    capLayout->addRow(TranslationManager::delay(), m_delaySpin);
    m_copyAfterCaptureCheck = new QCheckBox(TranslationManager::copyAfterCapture());
    m_copyAfterCaptureCheck->hide();
    m_closeAfterCopyCheck = new QCheckBox(TranslationManager::closeAfterCopy());
    capLayout->addRow(m_closeAfterCopyCheck);
    layout->addWidget(capGroup);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createAppearanceTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QGroupBox *themeGroup = new QGroupBox(TranslationManager::theme());
    QVBoxLayout *themeLayout = new QVBoxLayout(themeGroup);
    m_darkModeCheck = new QCheckBox(TranslationManager::darkMode());
    connect(m_darkModeCheck, &QCheckBox::toggled, this, &SettingsDialog::onThemeChanged);
    themeLayout->addWidget(m_darkModeCheck);
    layout->addWidget(themeGroup);

    QGroupBox *overlayGroup = new QGroupBox(TranslationManager::overlaySettings());
    QFormLayout *overlayLayout = new QFormLayout(overlayGroup);
    QHBoxLayout *opLayout = new QHBoxLayout();
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 255);
    m_opacitySlider->setTickInterval(25);
    m_opacityValueLabel = new QLabel("0%");
    m_opacityValueLabel->setFixedWidth(40);
    connect(m_opacitySlider, &QSlider::valueChanged, [this](int val) {
        int pct = qRound(val * 100.0 / 255.0);
        m_opacityValueLabel->setText(QString("%1%").arg(pct));
    });
    opLayout->addWidget(m_opacitySlider);
    opLayout->addWidget(m_opacityValueLabel);
    overlayLayout->addRow(TranslationManager::bgOpacity(), opLayout);
    m_crosshairStyleCombo = new QComboBox();
    m_crosshairStyleCombo->addItem(TranslationManager::crossDash(), "dash");
    m_crosshairStyleCombo->addItem(TranslationManager::crossSolid(), "solid");
    m_crosshairStyleCombo->addItem(TranslationManager::crossNone(), "none");
    overlayLayout->addRow(TranslationManager::crosshair(), m_crosshairStyleCombo);
    layout->addWidget(overlayGroup);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createRecordingTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QGroupBox *recGroup = new QGroupBox(TranslationManager::recordingSettings());
    QFormLayout *recForm = new QFormLayout(recGroup);

    m_recordingFpsSpin = new QSpinBox();
    m_recordingFpsSpin->setRange(1, 30);
    m_recordingFpsSpin->setSuffix(QStringLiteral(" fps"));
    m_recordingFpsSpin->setValue(10);
    recForm->addRow(TranslationManager::recordingFps(), m_recordingFpsSpin);

    m_recordingMaxSecSpin = new QSpinBox();
    m_recordingMaxSecSpin->setRange(0, 600);
    m_recordingMaxSecSpin->setSuffix(QStringLiteral(" s"));
    m_recordingMaxSecSpin->setSpecialValueText(TranslationManager::recordingUnlimited());
    m_recordingMaxSecSpin->setValue(30);
    recForm->addRow(TranslationManager::recordingMaxTime(), m_recordingMaxSecSpin);

    m_recordingLoopCombo = new QComboBox();
    m_recordingLoopCombo->addItem(TranslationManager::recordingLoopInfinite(), 0);
    m_recordingLoopCombo->addItem(QStringLiteral("1"), 1);
    m_recordingLoopCombo->addItem(QStringLiteral("2"), 2);
    m_recordingLoopCombo->addItem(QStringLiteral("3"), 3);
    m_recordingLoopCombo->addItem(QStringLiteral("5"), 5);
    m_recordingLoopCombo->addItem(QStringLiteral("10"), 10);
    recForm->addRow(TranslationManager::recordingLoop(), m_recordingLoopCombo);

    layout->addWidget(recGroup);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createInterfaceTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QGroupBox *toolsGroup = new QGroupBox(TranslationManager::toolbarVisibility());
    QVBoxLayout *toolsLayout = new QVBoxLayout(toolsGroup);

    QLabel *infoLabel = new QLabel(TranslationManager::toolbarDesc());
    infoLabel->setStyleSheet("color: #999;");
    toolsLayout->addWidget(infoLabel);

    m_toolVisibilityList = new QListWidget();
    m_toolVisibilityList->setAlternatingRowColors(true);

    struct ToolInfo { QString key; QString label; };
    QVector<ToolInfo> tools = {
        {"Pen",         TranslationManager::toolListPen()},
        {"Arrow",       TranslationManager::toolListArrow()},
        {"Line",        TranslationManager::toolListLine()},
        {"Rectangle",   TranslationManager::toolListRect()},
        {"SemiRect",    TranslationManager::toolListSemiRect()},
        {"Circle",      TranslationManager::toolListCircle()},
        {"Text",        TranslationManager::toolListText()},
        {"Highlighter", TranslationManager::toolListHighlight()},
        {"Blur",        TranslationManager::toolListBlur()},
        {"Counter",     TranslationManager::toolListCounter()},
        {"Eraser",      TranslationManager::toolListEraser()},
    };

    for (const auto &t : tools) {
        QListWidgetItem *item = new QListWidgetItem(t.label);
        item->setData(Qt::UserRole, t.key);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        m_toolVisibilityList->addItem(item);
    }
    toolsLayout->addWidget(m_toolVisibilityList);

    QHBoxLayout *selBtnLayout = new QHBoxLayout();
    QPushButton *selectAllBtn   = new QPushButton(TranslationManager::selectAll());
    QPushButton *deselectAllBtn = new QPushButton(TranslationManager::deselectAll());
    connect(selectAllBtn,   &QPushButton::clicked, this, &SettingsDialog::onSelectAllTools);
    connect(deselectAllBtn, &QPushButton::clicked, this, &SettingsDialog::onDeselectAllTools);
    selBtnLayout->addWidget(selectAllBtn);
    selBtnLayout->addWidget(deselectAllBtn);
    selBtnLayout->addStretch();
    toolsLayout->addLayout(selBtnLayout);

    layout->addWidget(toolsGroup);
    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createHotkeyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(12);

    QGroupBox *group = new QGroupBox(TranslationManager::hotkeyTitle());
    QVBoxLayout *gl = new QVBoxLayout(group);

    QLabel *desc = new QLabel(TranslationManager::hotkeyDesc());
    desc->setWordWrap(true);
    desc->setStyleSheet("color: #ccc; font-size: 12px;");
    gl->addWidget(desc);

    m_hotkeyEdit = new QKeySequenceEdit();
    m_hotkeyEdit->setMinimumHeight(40);
    m_hotkeyEdit->setStyleSheet(R"(
        QKeySequenceEdit {
            font-size: 14px;
            font-weight: bold;
            padding: 6px 12px;
            border: 2px solid #0078D4;
            border-radius: 6px;
            background: #2a2a2a;
            color: #ffffff;
        }
        QKeySequenceEdit:focus { border-color: #1a8cff; }
    )");
    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged,
            this, &SettingsDialog::onHotkeyChanged);
    gl->addWidget(m_hotkeyEdit);

    m_hotkeyStatusLabel = new QLabel(TranslationManager::hotkeyValid());
    m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
    gl->addWidget(m_hotkeyStatusLabel);

    m_printScreenConflictLabel = new QLabel(printScreenConflictMessage());
    m_printScreenConflictLabel->setWordWrap(true);
    m_printScreenConflictLabel->setStyleSheet(
        "background: rgba(255, 193, 7, 0.14); color: #ffd166; "
        "border: 1px solid rgba(255, 193, 7, 0.45); border-radius: 6px; "
        "padding: 8px; font-size: 12px;");
    gl->addWidget(m_printScreenConflictLabel);

    m_printScreenFixButton = new QPushButton(printScreenConflictFixText());
    connect(m_printScreenFixButton, &QPushButton::clicked,
            this, &SettingsDialog::onDisableWindowsPrintScreenSnipping);
    gl->addWidget(m_printScreenFixButton);

    QPushButton *resetHkBtn = new QPushButton(TranslationManager::hotkeyReset());
    resetHkBtn->setStyleSheet("color: #aaa;");
    connect(resetHkBtn, &QPushButton::clicked, [this]() {
        m_hotkeyEdit->setKeySequence(QKeySequence(Qt::Key_Print));
    });
    gl->addWidget(resetHkBtn);

    QLabel *noteLabel = new QLabel(TranslationManager::hotkeyNote());
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet("color: #888; font-size: 11px;");
    gl->addWidget(noteLabel);

    layout->addWidget(group);
    layout->addStretch();
    return tab;
}

void SettingsDialog::loadSettings()
{
    QString defPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    m_savePathEdit->setText(m_settings->value("savePath", defPath).toString());
    m_filenamePatternEdit->setText(m_settings->value("filenamePattern", "Screenshot_%Y-%M-%D_%h-%m-%s").toString());
    onFilenamePatternChanged(m_filenamePatternEdit->text());

#ifdef Q_OS_WIN
    m_autoStartCheck->setChecked(isAutoStartEnabled());
#else
    m_autoStartCheck->setChecked(m_settings->value("autoStart", false).toBool());
#endif
    m_showNotificationsCheck->setChecked(m_settings->value("showNotifications", true).toBool());
    m_playSoundCheck->setChecked(m_settings->value("playSound", false).toBool());
    m_copyPathAfterSaveCheck->setChecked(m_settings->value("copyPathAfterSave", false).toBool());

    // Language (saved as int, convert to string)
    int langInt = m_settings->value("language", 1).toInt(); // 1=English default
    static const char* langCodes[] = {"tr","en","de","fr","es","ja","zh","ru"};
    QString lang = (langInt >= 0 && langInt <= 7) ? langCodes[langInt] : "en";
    int li = m_langCombo->findData(lang);
    if (li >= 0) m_langCombo->setCurrentIndex(li);

    QString fmt = m_settings->value("imageFormat", "PNG").toString();
    int fi = m_formatCombo->findData(fmt);
    if (fi >= 0) m_formatCombo->setCurrentIndex(fi);

    int q = m_settings->value("imageQuality", 95).toInt();
    m_qualitySlider->setValue(q);
    m_qualitySpin->setValue(q);
    m_delaySpin->setValue(m_settings->value("captureDelay", 0).toInt());
    m_copyAfterCaptureCheck->setChecked(false);
    m_closeAfterCopyCheck->setChecked(m_settings->value("closeAfterCopy", true).toBool());

    if (m_recordingFpsSpin)
        m_recordingFpsSpin->setValue(m_settings->value("recordingFps", 10).toInt());
    if (m_recordingMaxSecSpin)
        m_recordingMaxSecSpin->setValue(m_settings->value("recordingMaxSeconds", 30).toInt());
    if (m_recordingLoopCombo) {
        const int loop = m_settings->value("recordingLoop", 0).toInt();
        int idx = m_recordingLoopCombo->findData(loop);
        if (idx < 0) idx = 0;
        m_recordingLoopCombo->setCurrentIndex(idx);
    }

    bool jpeg = (fmt == "JPEG");
    m_qualitySlider->setEnabled(jpeg);
    m_qualitySpin->setEnabled(jpeg);

    m_darkModeCheck->setChecked(m_settings->value("darkMode", true).toBool());
    int opacity = m_settings->value("overlayOpacity", 100).toInt();
    m_opacitySlider->setValue(opacity);
    QString cross = m_settings->value("crosshairStyle", "dash").toString();
    int ci = m_crosshairStyleCombo->findData(cross);
    if (ci >= 0) m_crosshairStyleCombo->setCurrentIndex(ci);

    m_highContrastCheck->setChecked(m_settings->value("highContrast", false).toBool());

    QStringList visibleTools = m_settings->value("visibleTools",
        QStringList{"Pen","Arrow","Line","Rectangle","Circle","Text","Highlighter","SemiRect","Blur","Counter","Eraser"})
        .toStringList();
    for (int i = 0; i < m_toolVisibilityList->count(); ++i) {
        QListWidgetItem *item = m_toolVisibilityList->item(i);
        QString key = item->data(Qt::UserRole).toString();
        item->setCheckState(visibleTools.contains(key) ? Qt::Checked : Qt::Unchecked);
    }

    UINT savedMod  = static_cast<UINT>(m_settings->value("hotkeyModifiers", 0).toUInt());
    UINT savedVKey = static_cast<UINT>(m_settings->value("hotkeyVKey", VK_SNAPSHOT).toUInt());
    m_hotkeyEdit->setKeySequence(win32ToKeySequence(savedMod, savedVKey));
    m_hotkeyStatusLabel->setText(TranslationManager::hotkeyValid());
    m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
    updatePrintScreenConflictUi();
    HotkeyManager::instance().reRegisterCaptureHotkey(
        HotkeyManager::instance().captureModifiers(),
        HotkeyManager::instance().captureVirtualKey());
}

void SettingsDialog::onHotkeyChanged(const QKeySequence &seq)
{
    UINT mod = 0, vk = 0;
    bool ok = keySequenceToWin32(seq, mod, vk);
    if (!ok || seq.isEmpty()) {
        m_hotkeyStatusLabel->setText(TranslationManager::hotkeyInvalid());
        m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
    } else {
        m_hotkeyStatusLabel->setText(QString("OK: %1").arg(seq.toString(QKeySequence::NativeText)));
        m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
    }
    updatePrintScreenConflictUi();
}

void SettingsDialog::updatePrintScreenConflictUi()
{
    if (!m_hotkeyEdit || !m_printScreenConflictLabel || !m_printScreenFixButton)
        return;

    UINT mod = 0, vk = 0;
    const bool hotkeyOk = keySequenceToWin32(m_hotkeyEdit->keySequence(), mod, vk);
    const bool showWarning = hotkeyOk
        && HotkeyManager::isPlainPrintScreen(mod, vk)
        && HotkeyManager::isWindowsPrintScreenSnippingEnabled();

    m_printScreenConflictLabel->setVisible(showWarning);
    m_printScreenFixButton->setVisible(showWarning);
}

void SettingsDialog::onDisableWindowsPrintScreenSnipping()
{
    if (!HotkeyManager::setWindowsPrintScreenSnippingEnabled(false)) {
        QMessageBox::warning(this, printScreenConflictTitle(), TranslationManager::errTitle());
        return;
    }

    updatePrintScreenConflictUi();
    QMessageBox::information(
        this,
        printScreenConflictTitle(),
        TranslationManager::currentLanguage() == TranslationManager::Turkish
            ? QString::fromUtf8("Windows'un Print Screen Snipping Tool ayarı kapatıldı. Print Screen artık EShot tarafından kullanılabilir.")
            : QStringLiteral("Windows Print Screen Snipping Tool shortcut has been disabled. Print Screen can now be used by EShot."));
}

void SettingsDialog::onFilenamePatternChanged(const QString &text)
{
    QString preview = resolvePatternPreview(text);
    QString ext = QStringLiteral("png");
    if (m_formatCombo) {
        ext = m_formatCombo->currentData().toString().toLower();
        if (ext == "jpeg") ext = QStringLiteral("jpg");
    }
    m_patternPreviewLabel->setText(TranslationManager::patternPreview() + ": " + preview + "." + ext);
}

void SettingsDialog::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, TranslationManager::saveDir(), m_savePathEdit->text());
    if (!dir.isEmpty()) m_savePathEdit->setText(dir);
}

void SettingsDialog::onSelectAllTools()
{
    for (int i = 0; i < m_toolVisibilityList->count(); ++i)
        m_toolVisibilityList->item(i)->setCheckState(Qt::Checked);
}

void SettingsDialog::onDeselectAllTools()
{
    for (int i = 0; i < m_toolVisibilityList->count(); ++i)
        m_toolVisibilityList->item(i)->setCheckState(Qt::Unchecked);
}

void SettingsDialog::onSave()
{
    QString savePath = m_savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!savePath.isEmpty()) {
        QDir dir(savePath);
        if (!dir.exists() && !dir.mkpath(".")) {
            QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::errSaveDir() + savePath);
            return;
        }
    }

    QKeySequence seq = m_hotkeyEdit->keySequence();
    UINT newMod = 0, newVKey = 0;
    if (!seq.isEmpty()) {
        if (!keySequenceToWin32(seq, newMod, newVKey)) {
            QMessageBox::warning(this, TranslationManager::errInvalidHotkeyTitle(), TranslationManager::errInvalidHotkey());
            return;
        }
    } else {
        newMod  = 0;
        newVKey = VK_SNAPSHOT;
    }

    if (!HotkeyManager::instance().reRegisterCaptureHotkey(newMod, newVKey)) {
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            TranslationManager::errInvalidHotkey() + QStringLiteral("\n\nBu kısayol başka bir uygulama tarafından kullanılıyor olabilir."));
        m_hotkeyEdit->setKeySequence(win32ToKeySequence(
            HotkeyManager::instance().captureModifiers(),
            HotkeyManager::instance().captureVirtualKey()));
        return;
    }

    // Save language
    QString newLang = m_langCombo->currentData().toString();
    TranslationManager::Language lang = TranslationManager::English;
    if (newLang == "tr") lang = TranslationManager::Turkish;
    else if (newLang == "ru") lang = TranslationManager::Russian;
    else if (newLang == "de") lang = TranslationManager::German;
    else if (newLang == "fr") lang = TranslationManager::French;
    else if (newLang == "es") lang = TranslationManager::Spanish;
    else if (newLang == "ja") lang = TranslationManager::Japanese;
    else if (newLang == "zh") lang = TranslationManager::Chinese;
    TranslationManager::setLanguage(lang);

    m_settings->setValue("language",          static_cast<int>(lang));
    m_settings->setValue("savePath",          savePath);
    m_settings->setValue("filenamePattern",    m_filenamePatternEdit->text());
    m_settings->setValue("autoStart",          m_autoStartCheck->isChecked());
    m_settings->setValue("showNotifications",  m_showNotificationsCheck->isChecked());
    m_settings->setValue("playSound",          m_playSoundCheck->isChecked());
    m_settings->setValue("copyPathAfterSave",  m_copyPathAfterSaveCheck->isChecked());

    m_settings->setValue("imageFormat",        m_formatCombo->currentData().toString());
    m_settings->setValue("imageQuality",       m_qualitySpin->value());
    m_settings->setValue("captureDelay",       m_delaySpin->value());
    m_settings->setValue("copyAfterCapture",   false);
    m_settings->setValue("closeAfterCopy",     m_closeAfterCopyCheck->isChecked());

    m_settings->setValue("darkMode",           m_darkModeCheck->isChecked());
    m_settings->setValue("overlayOpacity",     m_opacitySlider->value());
    m_settings->setValue("crosshairStyle",     m_crosshairStyleCombo->currentData().toString());
    m_settings->setValue("highContrast",       m_highContrastCheck->isChecked());

    QStringList visibleTools;
    for (int i = 0; i < m_toolVisibilityList->count(); ++i) {
        QListWidgetItem *item = m_toolVisibilityList->item(i);
        if (item->checkState() == Qt::Checked)
            visibleTools.append(item->data(Qt::UserRole).toString());
    }
    m_settings->setValue("visibleTools", visibleTools);

    m_settings->setValue("hotkeyModifiers", newMod);
    m_settings->setValue("hotkeyVKey",      newVKey);

    if (m_recordingFpsSpin)
        m_settings->setValue("recordingFps", m_recordingFpsSpin->value());
    if (m_recordingMaxSecSpin)
        m_settings->setValue("recordingMaxSeconds", m_recordingMaxSecSpin->value());
    if (m_recordingLoopCombo)
        m_settings->setValue("recordingLoop", m_recordingLoopCombo->currentData().toInt());

    if (!setAutoStartTask(m_autoStartCheck->isChecked())) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             QStringLiteral("Windows ile başlat ayarı kaydedilemedi."));
        return;
    }

    m_settings->sync();
    accept();
}

void SettingsDialog::onReset()
{
    if (QMessageBox::question(this, TranslationManager::resetTitle(),
            TranslationManager::resetConfirm(),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        m_settings->clear();
        m_settings->sync();
        TranslationManager::init();
        loadSettings();
    }
}

void SettingsDialog::onExportSettings()
{
    QString path = QFileDialog::getSaveFileName(this, TranslationManager::exportSettings(),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/EShot_Settings.json",
        "JSON (*.json)");
    if (path.isEmpty()) return;

    QJsonObject obj;
    obj["language"] = m_langCombo->currentData().toString();
    obj["savePath"] = m_savePathEdit->text();
    obj["filenamePattern"] = m_filenamePatternEdit->text();
    obj["autoStart"] = m_autoStartCheck->isChecked();
    obj["showNotifications"] = m_showNotificationsCheck->isChecked();
    obj["playSound"] = m_playSoundCheck->isChecked();
    obj["copyPathAfterSave"] = m_copyPathAfterSaveCheck->isChecked();
    obj["imageFormat"] = m_formatCombo->currentData().toString();
    obj["imageQuality"] = m_qualitySpin->value();
    obj["captureDelay"] = m_delaySpin->value();
    obj["copyAfterCapture"] = false;
    obj["closeAfterCopy"] = m_closeAfterCopyCheck->isChecked();
    obj["darkMode"] = m_darkModeCheck->isChecked();
    obj["overlayOpacity"] = m_opacitySlider->value();
    obj["crosshairStyle"] = m_crosshairStyleCombo->currentData().toString();
    obj["highContrast"] = m_highContrastCheck->isChecked();

    QJsonArray tools;
    for (int i = 0; i < m_toolVisibilityList->count(); ++i) {
        QListWidgetItem *item = m_toolVisibilityList->item(i);
        if (item->checkState() == Qt::Checked)
            tools.append(item->data(Qt::UserRole).toString());
    }
    obj["visibleTools"] = tools;

    QKeySequence seq = m_hotkeyEdit->keySequence();
    UINT mod = 0, vk = 0;
    if (!seq.isEmpty() && keySequenceToWin32(seq, mod, vk)) {
        obj["hotkeyModifiers"] = static_cast<int>(mod);
        obj["hotkeyVKey"] = static_cast<int>(vk);
    }

    if (m_recordingFpsSpin)
        obj["recordingFps"] = m_recordingFpsSpin->value();
    if (m_recordingMaxSecSpin)
        obj["recordingMaxSeconds"] = m_recordingMaxSecSpin->value();
    if (m_recordingLoopCombo)
        obj["recordingLoop"] = m_recordingLoopCombo->currentData().toInt();

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.close();
        QMessageBox::information(this, TranslationManager::exportSettings(), TranslationManager::exportSuccess());
    }
}

void SettingsDialog::onImportSettings()
{
    QString path = QFileDialog::getOpenFileName(this, TranslationManager::importSettings(),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        "JSON (*.json)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::importError());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::importError());
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("language")) {
        int li = m_langCombo->findData(obj["language"].toString());
        if (li >= 0) m_langCombo->setCurrentIndex(li);
    }
    if (obj.contains("savePath")) m_savePathEdit->setText(obj["savePath"].toString());
    if (obj.contains("filenamePattern")) m_filenamePatternEdit->setText(obj["filenamePattern"].toString());
    if (obj.contains("autoStart")) m_autoStartCheck->setChecked(obj["autoStart"].toBool());
    if (obj.contains("showNotifications")) m_showNotificationsCheck->setChecked(obj["showNotifications"].toBool());
    if (obj.contains("playSound")) m_playSoundCheck->setChecked(obj["playSound"].toBool());
    if (obj.contains("copyPathAfterSave")) m_copyPathAfterSaveCheck->setChecked(obj["copyPathAfterSave"].toBool());
    if (obj.contains("imageFormat")) {
        int fi = m_formatCombo->findData(obj["imageFormat"].toString());
        if (fi >= 0) m_formatCombo->setCurrentIndex(fi);
    }
    if (obj.contains("imageQuality")) {
        m_qualitySlider->setValue(obj["imageQuality"].toInt());
        m_qualitySpin->setValue(obj["imageQuality"].toInt());
    }
    if (obj.contains("captureDelay")) m_delaySpin->setValue(obj["captureDelay"].toInt());
    if (obj.contains("copyAfterCapture")) m_copyAfterCaptureCheck->setChecked(false);
    if (obj.contains("closeAfterCopy")) m_closeAfterCopyCheck->setChecked(obj["closeAfterCopy"].toBool());
    if (obj.contains("darkMode")) m_darkModeCheck->setChecked(obj["darkMode"].toBool());
    if (obj.contains("overlayOpacity")) m_opacitySlider->setValue(obj["overlayOpacity"].toInt());
    if (obj.contains("crosshairStyle")) {
        int ci = m_crosshairStyleCombo->findData(obj["crosshairStyle"].toString());
        if (ci >= 0) m_crosshairStyleCombo->setCurrentIndex(ci);
    }
    if (obj.contains("highContrast")) m_highContrastCheck->setChecked(obj["highContrast"].toBool());
    if (obj.contains("visibleTools")) {
        QJsonArray tools = obj["visibleTools"].toArray();
        QStringList visibleTools;
        for (const auto &t : tools) visibleTools.append(t.toString());
        for (int i = 0; i < m_toolVisibilityList->count(); ++i) {
            QListWidgetItem *item = m_toolVisibilityList->item(i);
            item->setCheckState(visibleTools.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
        }
    }
    if (obj.contains("hotkeyModifiers") && obj.contains("hotkeyVKey")) {
        m_hotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(obj["hotkeyModifiers"].toInt()),
            static_cast<UINT>(obj["hotkeyVKey"].toInt())));
    }
    if (m_recordingFpsSpin && obj.contains("recordingFps"))
        m_recordingFpsSpin->setValue(obj["recordingFps"].toInt());
    if (m_recordingMaxSecSpin && obj.contains("recordingMaxSeconds"))
        m_recordingMaxSecSpin->setValue(obj["recordingMaxSeconds"].toInt());
    if (m_recordingLoopCombo && obj.contains("recordingLoop")) {
        int idx = m_recordingLoopCombo->findData(obj["recordingLoop"].toInt());
        if (idx < 0) idx = 0;
        m_recordingLoopCombo->setCurrentIndex(idx);
    }

    QMessageBox::information(this, TranslationManager::importSettings(), TranslationManager::importSuccess());
}

void SettingsDialog::onThemeChanged()
{
    bool dark = m_darkModeCheck->isChecked();
    bool highContrast = m_highContrastCheck->isChecked();
    QPalette p;
    if (highContrast) {
        // High contrast mode
        p.setColor(QPalette::Window, Qt::black);
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, Qt::black);
        p.setColor(QPalette::AlternateBase, QColor(30,30,30));
        p.setColor(QPalette::ToolTipBase, Qt::black);
        p.setColor(QPalette::ToolTipText, Qt::yellow);
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, Qt::black);
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, Qt::cyan);
        p.setColor(QPalette::Highlight, Qt::yellow);
        p.setColor(QPalette::HighlightedText, Qt::black);
    } else if (dark) {
        p.setColor(QPalette::Window, QColor(53,53,53));
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, QColor(42,42,42));
        p.setColor(QPalette::AlternateBase, QColor(66,66,66));
        p.setColor(QPalette::ToolTipBase, QColor(53,53,53));
        p.setColor(QPalette::ToolTipText, Qt::white);
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, QColor(53,53,53));
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(42,130,218));
        p.setColor(QPalette::Highlight, QColor(42,130,218));
        p.setColor(QPalette::HighlightedText, Qt::black);
    } else {
        p.setColor(QPalette::Window, QColor(240,240,240));
        p.setColor(QPalette::WindowText, Qt::black);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(233,233,233));
        p.setColor(QPalette::ToolTipBase, QColor(255,255,220));
        p.setColor(QPalette::ToolTipText, Qt::black);
        p.setColor(QPalette::Text, Qt::black);
        p.setColor(QPalette::Button, QColor(240,240,240));
        p.setColor(QPalette::ButtonText, Qt::black);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(0,0,255));
        p.setColor(QPalette::Highlight, QColor(0,120,215));
        p.setColor(QPalette::HighlightedText, Qt::white);
    }
    qApp->setPalette(p);
}
