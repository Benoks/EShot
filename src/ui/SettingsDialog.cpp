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
#include <QColor>
#include <QFont>
#include <QAbstractItemView>
#include <QIcon>
#include <QGridLayout>
#include <QScrollArea>
#include <QScreen>
#include <QShowEvent>
#include <QFrame>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#endif

namespace {
QString uiLabel(const char *tr, const char *en)
{
    return TranslationManager::currentLanguage() == TranslationManager::Turkish
        ? QString::fromUtf8(tr)
        : QString::fromLatin1(en);
}

QStringList defaultAnnotationTools()
{
    return {"Pen","Arrow","Line","Rectangle","Circle","Text","Highlighter","SemiRect","Blur","Counter","Eraser"};
}

QStringList defaultToolbarControls()
{
    return {"Color","Eyedropper","Lock","BlurIntensity","Undo","Redo","Ocr","Upload","Gif","Video"};
}

struct OverlayShortcutDef {
    QString key;
    QString label;
    QString defaultSequence;
};

QVector<OverlayShortcutDef> overlayShortcutDefaults()
{
    return {
        {"toolPen", TranslationManager::toolPen(), "P"},
        {"toolArrow", TranslationManager::toolArrow(), "A"},
        {"toolLine", TranslationManager::toolLine(), "L"},
        {"toolRectangle", TranslationManager::toolRect(), "R"},
        {"toolCircle", TranslationManager::toolCircle(), "C"},
        {"toolText", TranslationManager::toolText(), "T"},
        {"toolHighlighter", TranslationManager::toolHighlighter(), "H"},
        {"toolSemiRect", TranslationManager::toolSemiRect(), "D"},
        {"toolBlur", TranslationManager::toolBlur(), "B"},
        {"toolCounter", TranslationManager::toolCounter(), "N"},
        {"toolEraser", TranslationManager::toolEraser(), "X"},
        {"actionEyedropper", TranslationManager::toolEyedropper(), "I"},
        {"actionLock", TranslationManager::actionLock(), "K"},
        {"actionUndo", TranslationManager::toolUndo(), "Ctrl+Z"},
        {"actionRedo", TranslationManager::toolRedo(), "Ctrl+Shift+Z"},
        {"actionCopy", TranslationManager::actionCopy(), "Ctrl+C"},
        {"actionSave", TranslationManager::actionSave(), "Ctrl+S"},
        {"actionPin", TranslationManager::actionPin(), "Ctrl+P"},
        {"actionOcr", TranslationManager::actionOcr(), "Ctrl+O"},
        {"actionUpload", TranslationManager::uploadToService(), "Ctrl+U"},
        {"actionGif", TranslationManager::recordingStartTitle(), "Ctrl+G"},
        {"actionVideo", TranslationManager::videoRecordingTitle(), "Ctrl+Shift+V"}
    };
}

QString defaultSaveDirectory()
{
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.trimmed().isEmpty())
        picturesPath = QDir::homePath();
    return QDir(picturesPath).filePath(QStringLiteral("EShot"));
}

QString cleanToolLabel(const QString &label)
{
    int space = label.indexOf(' ');
    if (space > 0 && !label.at(0).isLetterOrNumber())
        return label.mid(space + 1);
    return label;
}

#ifdef Q_OS_WIN
void appendDeviceProperty(IPropertyStore *store, const PROPERTYKEY &key, QStringList &devices)
{
    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(store->GetValue(key, &value)) && value.vt == VT_LPWSTR && value.pwszVal) {
        const QString name = QString::fromWCharArray(value.pwszVal).trimmed();
        if (!name.isEmpty() && !devices.contains(name))
            devices.append(name);
    }
    PropVariantClear(&value);
}
#endif

QStringList windowsAudioInputDevices()
{
    QStringList devices;
#ifdef Q_OS_WIN
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninit = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE)
        return devices;

    IMMDeviceEnumerator *enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void **>(&enumerator));
    if (SUCCEEDED(hr) && enumerator) {
        IMMDeviceCollection *collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
        if (SUCCEEDED(hr) && collection) {
            UINT count = 0;
            collection->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice *device = nullptr;
                if (FAILED(collection->Item(i, &device)) || !device)
                    continue;
                IPropertyStore *store = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && store) {
                    appendDeviceProperty(store, PKEY_DeviceInterface_FriendlyName, devices);
                    appendDeviceProperty(store, PKEY_Device_FriendlyName, devices);
                    appendDeviceProperty(store, PKEY_Device_DeviceDesc, devices);
                    store->Release();
                }
                device->Release();
            }
            collection->Release();
        }
        enumerator->Release();
    }
    if (shouldUninit)
        CoUninitialize();
#endif
    return devices;
}
}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags((windowFlags() & ~Qt::Dialog) | Qt::Window);
    setWindowTitle(TranslationManager::settingsTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setMinimumSize(560, 580);
    setMaximumSize(750, 720);

    m_settings = new QSettings("EShot", "EShot", this);
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog() {}

void SettingsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    
#ifdef Q_OS_WIN
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect avail = screen->availableGeometry();
        QRect geom = frameGeometry();
        // Center the window if its titlebar is off-screen (y < avail.y()) or if it's completely out of bounds
        if (geom.y() < avail.y() || !avail.intersects(geom)) {
            int x = avail.center().x() - width() / 2;
            int y = avail.center().y() - height() / 2;
            move(x, y);
        }
    }
#endif
}

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
#else
    Q_UNUSED(seq); Q_UNUSED(modifiers); Q_UNUSED(vkey);
    return false;
#endif
}

static QKeySequence win32ToKeySequence(UINT modifiers, UINT vkey)
{
    Qt::KeyboardModifiers qtMod = Qt::NoModifier;
#ifdef Q_OS_WIN
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
    if (vkey >= 'A' && vkey <= 'Z') {
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_A + (vkey - 'A'))));
    }
    if (vkey >= '0' && vkey <= '9') {
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_0 + (vkey - '0'))));
    }
    if (vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9) {
        return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, static_cast<Qt::Key>(Qt::Key_0 + (vkey - VK_NUMPAD0))));
    }
    if (vkey == VK_ADD)      return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Plus));
    if (vkey == VK_SUBTRACT) return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Minus));
    if (vkey == VK_MULTIPLY) return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Asterisk));
    if (vkey == VK_DIVIDE)   return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Slash));
    if (vkey == VK_DECIMAL)  return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Period));
#else
    Q_UNUSED(modifiers); Q_UNUSED(vkey);
#endif
    return QKeySequence(Qt::Key_Print);
}

bool SettingsDialog::isAutoStartEnabled()
{
#ifdef Q_OS_WIN
    auto queryTaskXml = [](const QString &taskName) {
        QProcess query;
        query.start(QStringLiteral("schtasks"),
                    {QStringLiteral("/Query"), QStringLiteral("/TN"), taskName, QStringLiteral("/XML")});
        if (!query.waitForFinished(3000) || query.exitCode() != 0)
            return QString();
        return QString::fromLocal8Bit(query.readAllStandardOutput());
    };

    QString xml = queryTaskXml(QStringLiteral("EShot"));
    if (xml.isEmpty())
        xml = queryTaskXml(QStringLiteral("\\EShot"));
    if (xml.isEmpty())
        return false;

    QString appPath = QCoreApplication::applicationFilePath().replace('/', '\\');
    return xml.contains(appPath, Qt::CaseInsensitive);
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
    QString psPath = appPath;
    psPath.replace(QStringLiteral("'"), QStringLiteral("''"));
    QString script = QStringLiteral(
        "Unregister-ScheduledTask -TaskName 'EShot' -Confirm:$false -ErrorAction SilentlyContinue; "
        "$User=[System.Security.Principal.WindowsIdentity]::GetCurrent().Name; "
        "$A=New-ScheduledTaskAction -Execute '%1' -Argument '--silent'; "
        "$T=New-ScheduledTaskTrigger -AtLogOn -User $User; "
        "$T.Delay='PT30S'; "
        "$P=New-ScheduledTaskPrincipal -UserId $User -LogonType Interactive -RunLevel Highest; "
        "$S=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries; "
        "Register-ScheduledTask -TaskName 'EShot' -Action $A -Trigger $T -Principal $P -Settings $S -Force | Out-Null")
        .arg(psPath);

    return QProcess::execute(QStringLiteral("powershell.exe"),
                             {QStringLiteral("-NoProfile"),
                              QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                              QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
                              QStringLiteral("-Command"), script}) == 0;
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
    m_filenamePatternEdit->setToolTip(uiLabel("Dosya adinda tarih/saat ve pencere basligi degiskenlerini kullanir.",
                                              "Use date/time and window title variables in saved filenames."));
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
    m_notificationOptionsWidget = new QWidget(genGroup);
    m_notificationOptionsWidget->setStyleSheet(R"(
        QCheckBox { color: #cfcfcf; }
        QCheckBox:disabled { color: #777777; }
        QCheckBox::indicator:disabled { border: 1px solid #555555; background: #333333; }
    )");
    QVBoxLayout *notifLayout = new QVBoxLayout(m_notificationOptionsWidget);
    notifLayout->setContentsMargins(22, 0, 0, 0);
    notifLayout->setSpacing(2);
    m_notifyCopyCheck = new QCheckBox(TranslationManager::notifyCopy(), m_notificationOptionsWidget);
    m_notifySaveCheck = new QCheckBox(TranslationManager::notifySave(), m_notificationOptionsWidget);
    m_notifyGifCheck = new QCheckBox(TranslationManager::notifyGif(), m_notificationOptionsWidget);
    m_notifyVideoCheck = new QCheckBox(TranslationManager::notifyVideo(), m_notificationOptionsWidget);
    m_notifyCopyCheck->setToolTip(uiLabel("Gorsel panoya kopyalaninca bildirim gosterir.", "Show a notification when an image is copied."));
    m_notifySaveCheck->setToolTip(uiLabel("Gorsel dosyaya kaydedilince klasoru acabilen bildirim gosterir.", "Show a folder-opening notification when an image is saved."));
    m_notifyGifCheck->setToolTip(uiLabel("GIF kaydi bitince klasoru acabilen bildirim gosterir.", "Show a folder-opening notification when a GIF recording finishes."));
    m_notifyVideoCheck->setToolTip(uiLabel("Video kaydi bitince klasoru acabilen bildirim gosterir.", "Show a folder-opening notification when a video recording finishes."));
    notifLayout->addWidget(m_notifyCopyCheck);
    notifLayout->addWidget(m_notifySaveCheck);
    notifLayout->addWidget(m_notifyGifCheck);
    notifLayout->addWidget(m_notifyVideoCheck);
    auto updateNotifChildren = [this](bool enabled) {
        if (m_notificationOptionsWidget) m_notificationOptionsWidget->setEnabled(enabled);
    };
    connect(m_showNotificationsCheck, &QCheckBox::toggled, this, updateNotifChildren);
    genLayout->addWidget(m_notificationOptionsWidget);
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
    m_formatCombo->setToolTip(uiLabel("Kaydedilen ekran goruntusunun dosya bicimi.", "File format for saved screenshots."));
    fmtLayout->addRow("Format:", m_formatCombo);

    QHBoxLayout *qLayout = new QHBoxLayout();
    m_qualitySlider = new QSlider(Qt::Horizontal);
    m_qualitySlider->setRange(10, 100);
    m_qualitySpin = new QSpinBox();
    m_qualitySpin->setRange(10, 100);
    m_qualitySpin->setSuffix("%");
    m_qualitySlider->setToolTip(uiLabel("Sadece JPEG icin kalite ayari.", "Quality setting for JPEG only."));
    m_qualitySpin->setToolTip(m_qualitySlider->toolTip());
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
    m_delaySpin->setToolTip(uiLabel("Kisayola bastiktan sonra yakalama ekraninin acilmasi icin bekleme suresi.", "Delay before opening the capture overlay after the shortcut is pressed."));
    capLayout->addRow(TranslationManager::delay(), m_delaySpin);
    m_copyAfterCaptureCheck = new QCheckBox(TranslationManager::copyAfterCapture());
    m_copyAfterCaptureCheck->hide();
    m_closeAfterCopyCheck = new QCheckBox(TranslationManager::closeAfterCopy());
    m_closeAfterCopyCheck->setToolTip(uiLabel("Kopyala komutundan sonra secim ekranini otomatik kapatir.", "Automatically closes the capture overlay after copying."));
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
    m_darkModeCheck->setToolTip(uiLabel("Ayarlar penceresi ve yardimci pencereler icin koyu tema.", "Dark theme for settings and helper windows."));
    connect(m_darkModeCheck, &QCheckBox::toggled, this, &SettingsDialog::onThemeChanged);
    themeLayout->addWidget(m_darkModeCheck);
    layout->addWidget(themeGroup);

    QGroupBox *overlayGroup = new QGroupBox(TranslationManager::overlaySettings());
    QFormLayout *overlayLayout = new QFormLayout(overlayGroup);
    QHBoxLayout *opLayout = new QHBoxLayout();
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 255);
    m_opacitySlider->setTickInterval(25);
    m_opacitySlider->setToolTip(uiLabel("Secilmeyen ekran alaninin karartma miktari.", "Dim amount for the non-selected screen area."));
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
    m_crosshairStyleCombo->setToolTip(uiLabel("Alan secmeden once imlec kilavuz cizgisi stili.", "Cursor guide line style before selecting an area."));
    overlayLayout->addRow(TranslationManager::crosshair(), m_crosshairStyleCombo);
    layout->addWidget(overlayGroup);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createRecordingTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QGroupBox *recGroup = new QGroupBox(TranslationManager::gifSettings());
    QFormLayout *recForm = new QFormLayout(recGroup);

    m_recordingFpsSpin = new QSpinBox();
    m_recordingFpsSpin->setRange(1, 30);
    m_recordingFpsSpin->setSuffix(QStringLiteral(" fps"));
    m_recordingFpsSpin->setValue(10);
    m_recordingFpsSpin->setToolTip(uiLabel("GIF icin saniyedeki kare sayisi. Daha yuksek deger daha akici ama daha buyuk dosya uretir.", "Frames per second for GIF. Higher values are smoother but create larger files."));
    recForm->addRow(TranslationManager::recordingFps(), m_recordingFpsSpin);

    m_recordingMaxSecSpin = new QSpinBox();
    m_recordingMaxSecSpin->setRange(0, 600);
    m_recordingMaxSecSpin->setSuffix(QStringLiteral(" s"));
    m_recordingMaxSecSpin->setSpecialValueText(TranslationManager::recordingUnlimited());
    m_recordingMaxSecSpin->setValue(30);
    m_recordingMaxSecSpin->setToolTip(uiLabel("GIF kaydinin otomatik duracagi sure. 0 sinirsizdir.", "Time limit for GIF recording. 0 means unlimited."));
    recForm->addRow(TranslationManager::recordingMaxTime(), m_recordingMaxSecSpin);

    m_recordingLoopCombo = new QComboBox();
    m_recordingLoopCombo->addItem(TranslationManager::recordingLoopInfinite(), 0);
    m_recordingLoopCombo->addItem(QStringLiteral("1"), 1);
    m_recordingLoopCombo->addItem(QStringLiteral("2"), 2);
    m_recordingLoopCombo->addItem(QStringLiteral("3"), 3);
    m_recordingLoopCombo->addItem(QStringLiteral("5"), 5);
    m_recordingLoopCombo->addItem(QStringLiteral("10"), 10);
    m_recordingLoopCombo->setToolTip(uiLabel("GIF dosyasinin kac kez donguye girecegi.", "How many times the GIF should loop."));
    recForm->addRow(TranslationManager::recordingLoop(), m_recordingLoopCombo);

    layout->addWidget(recGroup);

    QGroupBox *videoGroup = new QGroupBox(TranslationManager::videoRecordingTitle());
    QFormLayout *videoForm = new QFormLayout(videoGroup);

    m_videoFpsSpin = new QSpinBox();
    m_videoFpsSpin->setRange(1, 60);
    m_videoFpsSpin->setSuffix(QStringLiteral(" fps"));
    m_videoFpsSpin->setValue(30);
    m_videoFpsSpin->setToolTip(uiLabel("Video icin saniyedeki kare sayisi.", "Frames per second for video recording."));
    videoForm->addRow(TranslationManager::recordingFps(), m_videoFpsSpin);

    m_videoMaxSecSpin = new QSpinBox();
    m_videoMaxSecSpin->setRange(0, 3600);
    m_videoMaxSecSpin->setSuffix(QStringLiteral(" s"));
    m_videoMaxSecSpin->setSpecialValueText(TranslationManager::recordingUnlimited());
    m_videoMaxSecSpin->setValue(0);
    m_videoMaxSecSpin->setToolTip(uiLabel("Video kaydinin otomatik duracagi sure. 0 sinirsizdir.", "Time limit for video recording. 0 means unlimited."));
    videoForm->addRow(TranslationManager::recordingMaxTime(), m_videoMaxSecSpin);

    m_videoCrfSpin = new QSpinBox();
    m_videoCrfSpin->setRange(18, 32);
    m_videoCrfSpin->setValue(24);
    m_videoCrfSpin->setToolTip(TranslationManager::videoCrfHint());
    videoForm->addRow(TranslationManager::videoQualityCrf(), m_videoCrfSpin);

    m_videoDesktopAudioCheck = new QCheckBox(TranslationManager::audioDesktop());
    m_videoDesktopAudioCheck->setToolTip(uiLabel("Video kaydina sistem/masaustu sesini ekler.", "Include system/desktop audio in video recordings."));
    videoForm->addRow(TranslationManager::audioMode(), m_videoDesktopAudioCheck);

    QWidget *desktopVolumeRow = new QWidget(videoGroup);
    QHBoxLayout *desktopVolumeLayout = new QHBoxLayout(desktopVolumeRow);
    desktopVolumeLayout->setContentsMargins(0, 0, 0, 0);
    desktopVolumeLayout->setSpacing(8);
    m_videoDesktopVolumeSlider = new QSlider(Qt::Horizontal, desktopVolumeRow);
    m_videoDesktopVolumeSlider->setRange(0, 100);
    m_videoDesktopVolumeSlider->setValue(80);
    m_videoDesktopVolumeSpin = new QSpinBox(desktopVolumeRow);
    m_videoDesktopVolumeSpin->setRange(0, 100);
    m_videoDesktopVolumeSpin->setSuffix(QStringLiteral("%"));
    m_videoDesktopVolumeSpin->setFixedWidth(72);
    m_videoDesktopVolumeSlider->setToolTip(uiLabel("Kaydedilen masaustu sesi seviyesi.", "Recorded desktop audio volume."));
    m_videoDesktopVolumeSpin->setToolTip(m_videoDesktopVolumeSlider->toolTip());
    connect(m_videoDesktopVolumeSlider, &QSlider::valueChanged, m_videoDesktopVolumeSpin, &QSpinBox::setValue);
    connect(m_videoDesktopVolumeSpin, qOverload<int>(&QSpinBox::valueChanged), m_videoDesktopVolumeSlider, &QSlider::setValue);
    desktopVolumeLayout->addWidget(m_videoDesktopVolumeSlider);
    desktopVolumeLayout->addWidget(m_videoDesktopVolumeSpin);
    videoForm->addRow(uiLabel("Masaustu ses duzeyi", "Desktop volume"), desktopVolumeRow);

    m_videoMicrophoneCheck = new QCheckBox(TranslationManager::audioMicrophone());
    m_videoMicrophoneCheck->setToolTip(uiLabel("Video kaydina mikrofon sesini ekler.", "Include microphone audio in video recordings."));
    videoForm->addRow(QString(), m_videoMicrophoneCheck);

    m_videoMicrophoneDeviceCombo = new QComboBox(videoGroup);
    m_videoMicrophoneDeviceCombo->addItem(QStringLiteral("Default"), QStringLiteral("default"));
    for (const QString &device : windowsAudioInputDevices())
        m_videoMicrophoneDeviceCombo->addItem(device, device);
    m_videoMicrophoneDeviceCombo->setToolTip(uiLabel("Video kaydinda kullanilacak mikrofon kaynagi.", "Microphone source used for video recording."));
    videoForm->addRow(TranslationManager::audioMicrophoneDevice(), m_videoMicrophoneDeviceCombo);

    QWidget *micVolumeRow = new QWidget(videoGroup);
    QHBoxLayout *micVolumeLayout = new QHBoxLayout(micVolumeRow);
    micVolumeLayout->setContentsMargins(0, 0, 0, 0);
    micVolumeLayout->setSpacing(8);
    m_videoMicrophoneVolumeSlider = new QSlider(Qt::Horizontal, micVolumeRow);
    m_videoMicrophoneVolumeSlider->setRange(0, 100);
    m_videoMicrophoneVolumeSlider->setValue(80);
    m_videoMicrophoneVolumeSpin = new QSpinBox(micVolumeRow);
    m_videoMicrophoneVolumeSpin->setRange(0, 100);
    m_videoMicrophoneVolumeSpin->setSuffix(QStringLiteral("%"));
    m_videoMicrophoneVolumeSpin->setFixedWidth(72);
    m_videoMicrophoneVolumeSlider->setToolTip(uiLabel("Kaydedilen mikrofon sesi seviyesi.", "Recorded microphone volume."));
    m_videoMicrophoneVolumeSpin->setToolTip(m_videoMicrophoneVolumeSlider->toolTip());
    connect(m_videoMicrophoneVolumeSlider, &QSlider::valueChanged, m_videoMicrophoneVolumeSpin, &QSpinBox::setValue);
    connect(m_videoMicrophoneVolumeSpin, qOverload<int>(&QSpinBox::valueChanged), m_videoMicrophoneVolumeSlider, &QSlider::setValue);
    micVolumeLayout->addWidget(m_videoMicrophoneVolumeSlider);
    micVolumeLayout->addWidget(m_videoMicrophoneVolumeSpin);
    videoForm->addRow(uiLabel("Mikrofon ses duzeyi", "Microphone volume"), micVolumeRow);

    auto updateDesktopAudioState = [this](bool enabled) {
        if (m_videoDesktopVolumeSlider) m_videoDesktopVolumeSlider->setEnabled(enabled);
        if (m_videoDesktopVolumeSpin) m_videoDesktopVolumeSpin->setEnabled(enabled);
    };
    auto updateMicrophoneState = [this](bool enabled) {
        if (m_videoMicrophoneDeviceCombo) m_videoMicrophoneDeviceCombo->setEnabled(enabled);
        if (m_videoMicrophoneVolumeSlider) m_videoMicrophoneVolumeSlider->setEnabled(enabled);
        if (m_videoMicrophoneVolumeSpin) m_videoMicrophoneVolumeSpin->setEnabled(enabled);
    };
    connect(m_videoDesktopAudioCheck, &QCheckBox::toggled, this, updateDesktopAudioState);
    connect(m_videoMicrophoneCheck, &QCheckBox::toggled, this, updateMicrophoneState);

    layout->addWidget(videoGroup);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createInterfaceTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    QGroupBox *toolsGroup = new QGroupBox(TranslationManager::toolbarVisibility());
    QVBoxLayout *toolsLayout = new QVBoxLayout(toolsGroup);
    toolsLayout->setContentsMargins(10, 10, 10, 10);
    toolsLayout->setSpacing(8);

    QLabel *infoLabel = new QLabel(TranslationManager::toolbarDesc());
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    toolsLayout->addWidget(infoLabel);

    auto configureList = [](QListWidget *list) {
        list->setAlternatingRowColors(false);
        list->setSelectionMode(QAbstractItemView::NoSelection);
        list->setFocusPolicy(Qt::NoFocus);
        list->setIconSize(QSize(18, 18));
        list->setSpacing(1);
        list->setMinimumHeight(300);
        list->setMaximumHeight(330);
        list->setStyleSheet(R"(
        QListWidget {
            background: #252525;
            border: 1px solid #3d3d3d;
            border-radius: 6px;
            padding: 6px;
        }
        QListWidget::item {
            min-height: 24px;
            border-radius: 4px;
            padding: 2px 6px;
        }
        QListWidget::item:hover {
            background: #303030;
        }
        QListWidget::indicator {
            width: 16px;
            height: 16px;
        }
    )");
    };

    auto addOption = [](QListWidget *list, const QString &category, const QString &key, const QString &label, const QString &iconPath) {
        QListWidgetItem *item = new QListWidgetItem(cleanToolLabel(label));
        if (!iconPath.isEmpty())
            item->setIcon(QIcon(iconPath));
        item->setData(Qt::UserRole, key);
        item->setData(Qt::UserRole + 1, category);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        list->addItem(item);
    };

    struct ToolInfo { QString key; QString label; QString icon; };
    QVector<ToolInfo> tools = {
        {"Pen",         TranslationManager::toolListPen(),       ":/icons/pen.svg"},
        {"Arrow",       TranslationManager::toolListArrow(),     ":/icons/arrow.svg"},
        {"Line",        TranslationManager::toolListLine(),      ":/icons/line.svg"},
        {"Rectangle",   TranslationManager::toolListRect(),      ":/icons/rectangle.svg"},
        {"SemiRect",    TranslationManager::toolListSemiRect(),  ":/icons/semirect.svg"},
        {"Circle",      TranslationManager::toolListCircle(),    ":/icons/circle.svg"},
        {"Text",        TranslationManager::toolListText(),      ":/icons/text.svg"},
        {"Highlighter", TranslationManager::toolListHighlight(), ":/icons/highlighter.svg"},
        {"Blur",        TranslationManager::toolListBlur(),      ":/icons/blur.svg"},
        {"Counter",     TranslationManager::toolListCounter(),   ":/icons/counter.svg"},
        {"Eraser",      TranslationManager::toolListEraser(),    ":/icons/eraser.svg"},
    };

    QVector<ToolInfo> controls = {
        {"Color",         TranslationManager::toolColor(),                    ":/icons/color.svg"},
        {"Eyedropper",    TranslationManager::toolEyedropper(),               ":/icons/eyedropper.svg"},
        {"Lock",          TranslationManager::actionLock(),                   ":/icons/lock_open.svg"},
        {"BlurIntensity", TranslationManager::toolBlurIntensity(),            ":/icons/blur.svg"},
        {"Undo",          TranslationManager::toolUndo(),                     ":/icons/undo.svg"},
        {"Redo",          TranslationManager::toolRedo(),                     ":/icons/redo.svg"},
        {"Ocr",           TranslationManager::actionOcr(),                    ":/icons/ocr.svg"},
        {"Upload",        TranslationManager::uploadToService(),              ":/icons/upload.svg"},
        {"Gif",           TranslationManager::recordingStartTitle(),          ":/icons/gif.svg"},
        {"Video",         TranslationManager::videoRecordingTitle(),          ":/icons/video.svg"},
    };

    QWidget *columns = new QWidget(toolsGroup);
    QHBoxLayout *columnsLayout = new QHBoxLayout(columns);
    columnsLayout->setContentsMargins(0, 0, 0, 0);
    columnsLayout->setSpacing(10);

    auto makeColumn = [&](const QString &title, QListWidget **listPtr) {
        QWidget *column = new QWidget(columns);
        QVBoxLayout *columnLayout = new QVBoxLayout(column);
        columnLayout->setContentsMargins(0, 0, 0, 0);
        columnLayout->setSpacing(5);

        QLabel *titleLabel = new QLabel(title, column);
        titleLabel->setStyleSheet("color: #8ab4f8; font-weight: 600; font-size: 12px;");
        columnLayout->addWidget(titleLabel);

        *listPtr = new QListWidget(column);
        configureList(*listPtr);
        columnLayout->addWidget(*listPtr);
        return column;
    };

    QWidget *drawingColumn = makeColumn(uiLabel("Çizim araçları", "Drawing tools"), &m_toolVisibilityList);
    QWidget *controlColumn = makeColumn(uiLabel("Alt toolbar kontrolleri", "Bottom toolbar controls"), &m_toolbarControlVisibilityList);
    columnsLayout->addWidget(drawingColumn, 1);
    columnsLayout->addWidget(controlColumn, 1);

    for (const auto &t : tools) {
        addOption(m_toolVisibilityList, QStringLiteral("tool"), t.key, t.label, t.icon);
    }
    for (const auto &c : controls) {
        addOption(m_toolbarControlVisibilityList, QStringLiteral("control"), c.key, c.label, c.icon);
    }

    toolsLayout->addWidget(columns);

    QHBoxLayout *selBtnLayout = new QHBoxLayout();
    selBtnLayout->setSpacing(8);
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
    QVBoxLayout *outerLayout = new QVBoxLayout(tab);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    QScrollArea *scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget(scroll);
    QVBoxLayout *layout = new QVBoxLayout(content);
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

    QGroupBox *recordingGroup = new QGroupBox(TranslationManager::videoRecordingTitle());
    QFormLayout *recordingHotkeyLayout = new QFormLayout(recordingGroup);
    auto makeRecordingHotkeyEdit = []() {
        auto *edit = new QKeySequenceEdit();
        edit->setMinimumHeight(32);
        edit->setStyleSheet(R"(
            QKeySequenceEdit {
                padding: 5px 10px;
                border: 1px solid #555;
                border-radius: 5px;
                background: #2a2a2a;
                color: #ffffff;
            }
            QKeySequenceEdit:focus { border-color: #1a8cff; }
        )");
        return edit;
    };
    m_recordingPauseHotkeyEdit = makeRecordingHotkeyEdit();
    m_recordingStopHotkeyEdit = makeRecordingHotkeyEdit();
    m_recordingCancelHotkeyEdit = makeRecordingHotkeyEdit();
    recordingHotkeyLayout->addRow(TranslationManager::recordingPauseResume(), m_recordingPauseHotkeyEdit);
    recordingHotkeyLayout->addRow(TranslationManager::recordingStop(), m_recordingStopHotkeyEdit);
    recordingHotkeyLayout->addRow(TranslationManager::recordingCancel(), m_recordingCancelHotkeyEdit);
    gl->addWidget(recordingGroup);

    QGroupBox *overlayGroup = new QGroupBox(uiLabel("SS ekrani kisayollari", "Screenshot screen shortcuts"));
    QGridLayout *overlayLayout = new QGridLayout(overlayGroup);
    overlayLayout->setContentsMargins(10, 10, 10, 10);
    overlayLayout->setHorizontalSpacing(10);
    overlayLayout->setVerticalSpacing(6);
    m_overlayHotkeyEdits.clear();
    const auto overlayDefs = overlayShortcutDefaults();
    for (int i = 0; i < overlayDefs.size(); ++i) {
        const int row = i / 2;
        const int col = (i % 2) * 2;
        QLabel *label = new QLabel(overlayDefs[i].label, overlayGroup);
        label->setStyleSheet(QStringLiteral("color: #d0d0d0; font-size: 12px;"));
        auto *edit = makeRecordingHotkeyEdit();
        edit->setMinimumHeight(28);
        edit->setToolTip(uiLabel("Bu kisayol sadece ekran goruntusu secim/duzenleme ekraninda calisir.",
                                 "This shortcut works only on the screenshot selection/annotation screen."));
        m_overlayHotkeyEdits.insert(overlayDefs[i].key, edit);
        overlayLayout->addWidget(label, row, col);
        overlayLayout->addWidget(edit, row, col + 1);
    }
    gl->addWidget(overlayGroup);

    QLabel *noteLabel = new QLabel(TranslationManager::hotkeyNote());
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet("color: #888; font-size: 11px;");
    gl->addWidget(noteLabel);

    layout->addWidget(group);
    layout->addStretch();
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
    return tab;
}

void SettingsDialog::loadSettings()
{
    QString defPath = defaultSaveDirectory();

    m_savePathEdit->setText(m_settings->value("savePath", defPath).toString());
    m_filenamePatternEdit->setText(m_settings->value("filenamePattern", "Screenshot_%Y-%M-%D_%h-%m-%s").toString());
    onFilenamePatternChanged(m_filenamePatternEdit->text());

#ifdef Q_OS_WIN
    m_autoStartCheck->setChecked(isAutoStartEnabled());
#else
    m_autoStartCheck->setChecked(m_settings->value("autoStart", false).toBool());
#endif
    m_loadedAutoStart = m_autoStartCheck->isChecked();
    m_showNotificationsCheck->setChecked(m_settings->value("showNotifications", true).toBool());
    if (m_notifyCopyCheck) m_notifyCopyCheck->setChecked(m_settings->value("notifyCopy", false).toBool());
    if (m_notifySaveCheck) m_notifySaveCheck->setChecked(m_settings->value("notifySave", true).toBool());
    if (m_notifyGifCheck) m_notifyGifCheck->setChecked(m_settings->value("notifyGif", true).toBool());
    if (m_notifyVideoCheck) m_notifyVideoCheck->setChecked(m_settings->value("notifyVideo", true).toBool());
    if (m_notificationOptionsWidget) m_notificationOptionsWidget->setEnabled(m_showNotificationsCheck->isChecked());
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
    if (m_videoFpsSpin)
        m_videoFpsSpin->setValue(m_settings->value("videoRecordingFps", 30).toInt());
    if (m_videoMaxSecSpin)
        m_videoMaxSecSpin->setValue(m_settings->value("videoRecordingMaxSeconds", 0).toInt());
    if (m_videoCrfSpin)
        m_videoCrfSpin->setValue(m_settings->value("videoRecordingCrf", 24).toInt());
    const bool videoDesktopAudio = m_settings->contains("videoDesktopAudioEnabled")
        ? m_settings->value("videoDesktopAudioEnabled", false).toBool()
        : (m_settings->value("videoAudioMode", "none").toString() == QStringLiteral("desktop") ||
           m_settings->value("videoAudioMode", "none").toString() == QStringLiteral("both"));
    const bool videoMicrophoneAudio = m_settings->contains("videoMicrophoneEnabled")
        ? m_settings->value("videoMicrophoneEnabled", false).toBool()
        : (m_settings->value("videoAudioMode", "none").toString() == QStringLiteral("microphone") ||
           m_settings->value("videoAudioMode", "none").toString() == QStringLiteral("both"));
    if (m_videoDesktopAudioCheck)
        m_videoDesktopAudioCheck->setChecked(videoDesktopAudio);
    if (m_videoDesktopVolumeSlider)
        m_videoDesktopVolumeSlider->setValue(m_settings->value("videoDesktopAudioVolume", 80).toInt());
    if (m_videoDesktopVolumeSpin)
        m_videoDesktopVolumeSpin->setValue(m_settings->value("videoDesktopAudioVolume", 80).toInt());
    if (m_videoMicrophoneCheck)
        m_videoMicrophoneCheck->setChecked(videoMicrophoneAudio);
    if (m_videoMicrophoneVolumeSlider)
        m_videoMicrophoneVolumeSlider->setValue(m_settings->value("videoMicrophoneVolume", 80).toInt());
    if (m_videoMicrophoneVolumeSpin)
        m_videoMicrophoneVolumeSpin->setValue(m_settings->value("videoMicrophoneVolume", 80).toInt());
    if (m_videoMicrophoneDeviceCombo) {
        int idx = m_videoMicrophoneDeviceCombo->findData(m_settings->value("videoMicrophoneDevice", "default").toString());
        if (idx < 0) idx = 0;
        m_videoMicrophoneDeviceCombo->setCurrentIndex(idx);
        m_videoMicrophoneDeviceCombo->setEnabled(videoMicrophoneAudio);
    }
    if (m_videoDesktopVolumeSlider) m_videoDesktopVolumeSlider->setEnabled(videoDesktopAudio);
    if (m_videoDesktopVolumeSpin) m_videoDesktopVolumeSpin->setEnabled(videoDesktopAudio);
    if (m_videoMicrophoneVolumeSlider) m_videoMicrophoneVolumeSlider->setEnabled(videoMicrophoneAudio);
    if (m_videoMicrophoneVolumeSpin) m_videoMicrophoneVolumeSpin->setEnabled(videoMicrophoneAudio);

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

    QStringList visibleTools = m_settings->value("visibleTools", defaultAnnotationTools()).toStringList();
    QStringList visibleToolbarControls = m_settings->value("visibleToolbarControls", defaultToolbarControls()).toStringList();
    if (m_settings->contains("visibleToolbarControls") &&
        !m_settings->value("toolbarControlsMigratedVideo", false).toBool()) {
        if (!visibleToolbarControls.contains(QStringLiteral("Video")))
            visibleToolbarControls.append(QStringLiteral("Video"));
        m_settings->setValue("toolbarControlsMigratedVideo", true);
        m_settings->setValue("visibleToolbarControls", visibleToolbarControls);
    }
    auto applyVisibility = [](QListWidget *list, const QStringList &visible) {
        if (!list) return;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *item = list->item(i);
            QString key = item->data(Qt::UserRole).toString();
            if (!key.isEmpty())
                item->setCheckState(visible.contains(key) ? Qt::Checked : Qt::Unchecked);
        }
    };
    applyVisibility(m_toolVisibilityList, visibleTools);
    applyVisibility(m_toolbarControlVisibilityList, visibleToolbarControls);

    UINT savedMod  = static_cast<UINT>(m_settings->value("hotkeyModifiers", 0).toUInt());
    UINT savedVKey = static_cast<UINT>(m_settings->value("hotkeyVKey", VK_SNAPSHOT).toUInt());
    m_hotkeyEdit->setKeySequence(win32ToKeySequence(savedMod, savedVKey));
    if (m_recordingPauseHotkeyEdit)
        m_recordingPauseHotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(m_settings->value("recordingPauseHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt()),
            static_cast<UINT>(m_settings->value("recordingPauseHotkeyVKey", 'P').toUInt())));
    if (m_recordingStopHotkeyEdit)
        m_recordingStopHotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(m_settings->value("recordingStopHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt()),
            static_cast<UINT>(m_settings->value("recordingStopHotkeyVKey", 'S').toUInt())));
    if (m_recordingCancelHotkeyEdit)
        m_recordingCancelHotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(m_settings->value("recordingCancelHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt()),
            static_cast<UINT>(m_settings->value("recordingCancelHotkeyVKey", 'X').toUInt())));
    for (const auto &def : overlayShortcutDefaults()) {
        if (QKeySequenceEdit *edit = m_overlayHotkeyEdits.value(def.key, nullptr)) {
            edit->setKeySequence(QKeySequence(m_settings->value(QStringLiteral("overlayShortcut/%1").arg(def.key),
                                                                def.defaultSequence).toString()));
        }
    }
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
    for (QListWidget *list : {m_toolVisibilityList, m_toolbarControlVisibilityList}) {
        if (!list) continue;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *item = list->item(i);
            if (!item->data(Qt::UserRole).toString().isEmpty())
                item->setCheckState(Qt::Checked);
        }
    }
}

void SettingsDialog::onDeselectAllTools()
{
    for (QListWidget *list : {m_toolVisibilityList, m_toolbarControlVisibilityList}) {
        if (!list) continue;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *item = list->item(i);
            if (!item->data(Qt::UserRole).toString().isEmpty())
                item->setCheckState(Qt::Unchecked);
        }
    }
}

void SettingsDialog::onSave()
{
    QString savePath = m_savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = defaultSaveDirectory();
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

    UINT pauseMod = 0, pauseVKey = 0;
    UINT stopMod = 0, stopVKey = 0;
    UINT cancelMod = 0, cancelVKey = 0;
    if (!m_recordingPauseHotkeyEdit || !keySequenceToWin32(m_recordingPauseHotkeyEdit->keySequence(), pauseMod, pauseVKey) ||
        !m_recordingStopHotkeyEdit || !keySequenceToWin32(m_recordingStopHotkeyEdit->keySequence(), stopMod, stopVKey) ||
        !m_recordingCancelHotkeyEdit || !keySequenceToWin32(m_recordingCancelHotkeyEdit->keySequence(), cancelMod, cancelVKey)) {
        QMessageBox::warning(this, TranslationManager::errInvalidHotkeyTitle(), TranslationManager::errInvalidHotkey());
        return;
    }
    if (!HotkeyManager::instance().reRegisterRecordingHotkeys(pauseMod, pauseVKey, stopMod, stopVKey, cancelMod, cancelVKey)) {
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            TranslationManager::errInvalidHotkey() + QStringLiteral("\n\nBu kayÄ±t kÄ±sayollarÄ±ndan biri baÅŸka bir uygulama tarafÄ±ndan kullanÄ±lÄ±yor olabilir."));
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
    if (m_notifyCopyCheck) m_settings->setValue("notifyCopy", m_notifyCopyCheck->isChecked());
    if (m_notifySaveCheck) m_settings->setValue("notifySave", m_notifySaveCheck->isChecked());
    if (m_notifyGifCheck) m_settings->setValue("notifyGif", m_notifyGifCheck->isChecked());
    if (m_notifyVideoCheck) m_settings->setValue("notifyVideo", m_notifyVideoCheck->isChecked());
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
    QStringList visibleToolbarControls;
    auto collectVisible = [](QListWidget *list) {
        QStringList result;
        if (!list) return result;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *item = list->item(i);
            QString key = item->data(Qt::UserRole).toString();
            if (!key.isEmpty() && item->checkState() == Qt::Checked)
                result.append(key);
        }
        return result;
    };
    visibleTools = collectVisible(m_toolVisibilityList);
    visibleToolbarControls = collectVisible(m_toolbarControlVisibilityList);
    m_settings->setValue("visibleTools", visibleTools);
    m_settings->setValue("visibleToolbarControls", visibleToolbarControls);

    m_settings->setValue("hotkeyModifiers", newMod);
    m_settings->setValue("hotkeyVKey",      newVKey);
    m_settings->setValue("recordingPauseHotkeyModifiers", pauseMod);
    m_settings->setValue("recordingPauseHotkeyVKey",      pauseVKey);
    m_settings->setValue("recordingStopHotkeyModifiers",  stopMod);
    m_settings->setValue("recordingStopHotkeyVKey",       stopVKey);
    m_settings->setValue("recordingCancelHotkeyModifiers", cancelMod);
    m_settings->setValue("recordingCancelHotkeyVKey",      cancelVKey);
    for (auto it = m_overlayHotkeyEdits.constBegin(); it != m_overlayHotkeyEdits.constEnd(); ++it) {
        if (it.value())
            m_settings->setValue(QStringLiteral("overlayShortcut/%1").arg(it.key()),
                                 it.value()->keySequence().toString(QKeySequence::PortableText));
    }

    if (m_recordingFpsSpin)
        m_settings->setValue("recordingFps", m_recordingFpsSpin->value());
    if (m_recordingMaxSecSpin)
        m_settings->setValue("recordingMaxSeconds", m_recordingMaxSecSpin->value());
    if (m_recordingLoopCombo)
        m_settings->setValue("recordingLoop", m_recordingLoopCombo->currentData().toInt());
    if (m_videoFpsSpin)
        m_settings->setValue("videoRecordingFps", m_videoFpsSpin->value());
    if (m_videoMaxSecSpin)
        m_settings->setValue("videoRecordingMaxSeconds", m_videoMaxSecSpin->value());
    if (m_videoCrfSpin)
        m_settings->setValue("videoRecordingCrf", m_videoCrfSpin->value());
    const bool desktopAudio = m_videoDesktopAudioCheck && m_videoDesktopAudioCheck->isChecked();
    const bool microphoneAudio = m_videoMicrophoneCheck && m_videoMicrophoneCheck->isChecked();
    m_settings->setValue("videoDesktopAudioEnabled", desktopAudio);
    m_settings->setValue("videoMicrophoneEnabled", microphoneAudio);
    m_settings->setValue("videoDesktopAudioVolume", m_videoDesktopVolumeSpin ? m_videoDesktopVolumeSpin->value() : 80);
    m_settings->setValue("videoMicrophoneVolume", m_videoMicrophoneVolumeSpin ? m_videoMicrophoneVolumeSpin->value() : 80);
    m_settings->setValue("videoMicrophoneDevice",
                         m_videoMicrophoneDeviceCombo ? m_videoMicrophoneDeviceCombo->currentData().toString() : QStringLiteral("default"));
    m_settings->setValue("videoAudioMode", desktopAudio && microphoneAudio ? QStringLiteral("both")
        : desktopAudio ? QStringLiteral("desktop")
        : microphoneAudio ? QStringLiteral("microphone")
        : QStringLiteral("none"));

    if (m_autoStartCheck->isChecked() != m_loadedAutoStart && !setAutoStartTask(m_autoStartCheck->isChecked())) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             QStringLiteral("Windows ile başlat ayarı kaydedilemedi."));
        return;
    }

    m_loadedAutoStart = m_autoStartCheck->isChecked();
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
    obj["notifyCopy"] = m_notifyCopyCheck ? m_notifyCopyCheck->isChecked() : false;
    obj["notifySave"] = m_notifySaveCheck ? m_notifySaveCheck->isChecked() : true;
    obj["notifyGif"] = m_notifyGifCheck ? m_notifyGifCheck->isChecked() : true;
    obj["notifyVideo"] = m_notifyVideoCheck ? m_notifyVideoCheck->isChecked() : true;
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
    QJsonArray toolbarControls;
    auto appendChecked = [](QListWidget *list, QJsonArray &array) {
        if (!list) return;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *item = list->item(i);
            QString key = item->data(Qt::UserRole).toString();
            if (!key.isEmpty() && item->checkState() == Qt::Checked)
                array.append(key);
        }
    };
    appendChecked(m_toolVisibilityList, tools);
    appendChecked(m_toolbarControlVisibilityList, toolbarControls);
    obj["visibleTools"] = tools;
    obj["visibleToolbarControls"] = toolbarControls;

    QKeySequence seq = m_hotkeyEdit->keySequence();
    UINT mod = 0, vk = 0;
    if (!seq.isEmpty() && keySequenceToWin32(seq, mod, vk)) {
        obj["hotkeyModifiers"] = static_cast<int>(mod);
        obj["hotkeyVKey"] = static_cast<int>(vk);
    }
    auto appendHotkey = [this, &obj](const char *modKey, const char *vkeyKey, QKeySequenceEdit *edit) {
        UINT mod = 0, vk = 0;
        if (edit && keySequenceToWin32(edit->keySequence(), mod, vk)) {
            obj[modKey] = static_cast<int>(mod);
            obj[vkeyKey] = static_cast<int>(vk);
        }
    };
    appendHotkey("recordingPauseHotkeyModifiers", "recordingPauseHotkeyVKey", m_recordingPauseHotkeyEdit);
    appendHotkey("recordingStopHotkeyModifiers", "recordingStopHotkeyVKey", m_recordingStopHotkeyEdit);
    appendHotkey("recordingCancelHotkeyModifiers", "recordingCancelHotkeyVKey", m_recordingCancelHotkeyEdit);
    QJsonObject overlayShortcuts;
    for (auto it = m_overlayHotkeyEdits.constBegin(); it != m_overlayHotkeyEdits.constEnd(); ++it) {
        if (it.value())
            overlayShortcuts[it.key()] = it.value()->keySequence().toString(QKeySequence::PortableText);
    }
    obj["overlayShortcuts"] = overlayShortcuts;

    if (m_recordingFpsSpin)
        obj["recordingFps"] = m_recordingFpsSpin->value();
    if (m_recordingMaxSecSpin)
        obj["recordingMaxSeconds"] = m_recordingMaxSecSpin->value();
    if (m_recordingLoopCombo)
        obj["recordingLoop"] = m_recordingLoopCombo->currentData().toInt();
    if (m_videoFpsSpin)
        obj["videoRecordingFps"] = m_videoFpsSpin->value();
    if (m_videoMaxSecSpin)
        obj["videoRecordingMaxSeconds"] = m_videoMaxSecSpin->value();
    if (m_videoCrfSpin)
        obj["videoRecordingCrf"] = m_videoCrfSpin->value();
    const bool desktopAudio = m_videoDesktopAudioCheck && m_videoDesktopAudioCheck->isChecked();
    const bool microphoneAudio = m_videoMicrophoneCheck && m_videoMicrophoneCheck->isChecked();
    obj["videoDesktopAudioEnabled"] = desktopAudio;
    obj["videoMicrophoneEnabled"] = microphoneAudio;
    obj["videoDesktopAudioVolume"] = m_videoDesktopVolumeSpin ? m_videoDesktopVolumeSpin->value() : 80;
    obj["videoMicrophoneVolume"] = m_videoMicrophoneVolumeSpin ? m_videoMicrophoneVolumeSpin->value() : 80;
    obj["videoMicrophoneDevice"] = m_videoMicrophoneDeviceCombo ? m_videoMicrophoneDeviceCombo->currentData().toString() : QStringLiteral("default");
    obj["videoAudioMode"] = desktopAudio && microphoneAudio ? QStringLiteral("both")
        : desktopAudio ? QStringLiteral("desktop")
        : microphoneAudio ? QStringLiteral("microphone")
        : QStringLiteral("none");

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
    if (m_notifyCopyCheck && obj.contains("notifyCopy")) m_notifyCopyCheck->setChecked(obj["notifyCopy"].toBool());
    if (m_notifySaveCheck && obj.contains("notifySave")) m_notifySaveCheck->setChecked(obj["notifySave"].toBool());
    if (m_notifyGifCheck && obj.contains("notifyGif")) m_notifyGifCheck->setChecked(obj["notifyGif"].toBool());
    if (m_notifyVideoCheck && obj.contains("notifyVideo")) m_notifyVideoCheck->setChecked(obj["notifyVideo"].toBool());
    if (m_notificationOptionsWidget) m_notificationOptionsWidget->setEnabled(m_showNotificationsCheck->isChecked());
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
        if (m_toolVisibilityList) {
            for (int i = 0; i < m_toolVisibilityList->count(); ++i) {
                QListWidgetItem *item = m_toolVisibilityList->item(i);
                item->setCheckState(visibleTools.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
            }
        }
    }
    if (obj.contains("visibleToolbarControls")) {
        QJsonArray controls = obj["visibleToolbarControls"].toArray();
        QStringList visibleControls;
        for (const auto &c : controls) visibleControls.append(c.toString());
        if (m_toolbarControlVisibilityList) {
            for (int i = 0; i < m_toolbarControlVisibilityList->count(); ++i) {
                QListWidgetItem *item = m_toolbarControlVisibilityList->item(i);
                item->setCheckState(visibleControls.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
            }
        }
    }
    if (obj.contains("hotkeyModifiers") && obj.contains("hotkeyVKey")) {
        m_hotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(obj["hotkeyModifiers"].toInt()),
            static_cast<UINT>(obj["hotkeyVKey"].toInt())));
    }
    auto importHotkey = [&obj](const char *modKey, const char *vkeyKey, QKeySequenceEdit *edit) {
        if (edit && obj.contains(modKey) && obj.contains(vkeyKey)) {
            edit->setKeySequence(win32ToKeySequence(
                static_cast<UINT>(obj[modKey].toInt()),
                static_cast<UINT>(obj[vkeyKey].toInt())));
        }
    };
    importHotkey("recordingPauseHotkeyModifiers", "recordingPauseHotkeyVKey", m_recordingPauseHotkeyEdit);
    importHotkey("recordingStopHotkeyModifiers", "recordingStopHotkeyVKey", m_recordingStopHotkeyEdit);
    importHotkey("recordingCancelHotkeyModifiers", "recordingCancelHotkeyVKey", m_recordingCancelHotkeyEdit);
    if (obj.contains("overlayShortcuts") && obj["overlayShortcuts"].isObject()) {
        const QJsonObject shortcuts = obj["overlayShortcuts"].toObject();
        for (auto it = m_overlayHotkeyEdits.begin(); it != m_overlayHotkeyEdits.end(); ++it) {
            if (it.value() && shortcuts.contains(it.key()))
                it.value()->setKeySequence(QKeySequence(shortcuts[it.key()].toString()));
        }
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
    if (m_videoFpsSpin && obj.contains("videoRecordingFps"))
        m_videoFpsSpin->setValue(obj["videoRecordingFps"].toInt());
    if (m_videoMaxSecSpin && obj.contains("videoRecordingMaxSeconds"))
        m_videoMaxSecSpin->setValue(obj["videoRecordingMaxSeconds"].toInt());
    if (m_videoCrfSpin && obj.contains("videoRecordingCrf"))
        m_videoCrfSpin->setValue(obj["videoRecordingCrf"].toInt());
    if (m_videoDesktopAudioCheck && obj.contains("videoDesktopAudioEnabled"))
        m_videoDesktopAudioCheck->setChecked(obj["videoDesktopAudioEnabled"].toBool());
    if (m_videoMicrophoneCheck && obj.contains("videoMicrophoneEnabled"))
        m_videoMicrophoneCheck->setChecked(obj["videoMicrophoneEnabled"].toBool());
    if (m_videoDesktopVolumeSlider && obj.contains("videoDesktopAudioVolume"))
        m_videoDesktopVolumeSlider->setValue(obj["videoDesktopAudioVolume"].toInt());
    if (m_videoMicrophoneVolumeSlider && obj.contains("videoMicrophoneVolume"))
        m_videoMicrophoneVolumeSlider->setValue(obj["videoMicrophoneVolume"].toInt());
    if (m_videoMicrophoneDeviceCombo && obj.contains("videoMicrophoneDevice")) {
        int idx = m_videoMicrophoneDeviceCombo->findData(obj["videoMicrophoneDevice"].toString());
        if (idx < 0) idx = 0;
        m_videoMicrophoneDeviceCombo->setCurrentIndex(idx);
    } else if (obj.contains("videoAudioMode")) {
        const QString mode = obj["videoAudioMode"].toString();
        if (m_videoDesktopAudioCheck)
            m_videoDesktopAudioCheck->setChecked(mode == QStringLiteral("desktop") || mode == QStringLiteral("both"));
        if (m_videoMicrophoneCheck)
            m_videoMicrophoneCheck->setChecked(mode == QStringLiteral("microphone") || mode == QStringLiteral("both"));
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
