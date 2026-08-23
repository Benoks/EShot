#include "SettingsDialog.h"
#include "SettingsHotkeyPolicy.h"
#include "SettingsLayoutPolicy.h"
#include "ApplicationTheme.h"
#include "../core/HotkeyManager.h"
#include "../core/LinuxAutoStartPolicy.h"
#include "../core/LinuxDesktopIntegration.h"
#include "../core/LinuxGnomeShortcutInstaller.h"
#include "../core/OcrEngine.h"
#include "../core/TranslationManager.h"
#include "../recording/LinuxRecordingSupport.h"
#include "../recording/RecordingSettingsPolicy.h"
#ifdef Q_OS_LINUX
#include "FirstRunWizard.h"
#endif
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
#include <QSignalBlocker>
#include <QDir>
#include <QFileInfo>
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QTabBar>
#include <QStyle>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#endif

namespace {
constexpr UINT defaultWindowCaptureModifiers()
{
#ifdef Q_OS_WIN
    return MOD_SHIFT;
#else
    return 0;
#endif
}

constexpr UINT defaultWindowCaptureVirtualKey()
{
#ifdef Q_OS_WIN
    return VK_SNAPSHOT;
#else
    return 0;
#endif
}

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
    return {"Color","Eyedropper","Lock","BlurIntensity","Undo","Redo","Ocr","Upload","GoogleLens","Gif","Video"};
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
        {"actionGoogleLens", TranslationManager::visualSearchAction(), "Ctrl+L"},
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

QString defaultDesktopAudioDevice()
{
#ifdef Q_OS_WIN
    return QStringLiteral("__wasapi__");
#else
    return QStringLiteral("@DEFAULT_SINK@.monitor");
#endif
}

QList<QPair<QString, QString>> microphoneAudioDevices()
{
#ifdef Q_OS_WIN
    QList<QPair<QString, QString>> devices;
    for (const QString &name : windowsAudioInputDevices()) devices.append(qMakePair(name, name));
    return devices;
#else
    return discoverLinuxMicrophoneDevices();
#endif
}
}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    // Strip maximize button to prevent Windows 11 ARM64 Snap Layouts crash when dragging near top edge
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowTitle(TranslationManager::settingsTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setMinimumSize(560, 400); // Small enough to fit on 150% scaled 768p screens
    setMaximumSize(750, 800);

    m_settings = new QSettings("EShot", "EShot", this);
    m_packageNetwork = new QNetworkAccessManager(this);
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    if (m_packageReply)
        m_packageReply->abort();
    if (m_packageDownloadFile) {
        m_packageDownloadFile->close();
        delete m_packageDownloadFile;
    }
}

void SettingsDialog::setUpdateInfo(bool available, const QString &version, bool busy, const QString &status)
{
    if (m_updateStatusLabel) {
        QString text = status;
        if (text.isEmpty())
            text = available
                ? TranslationManager::updateStatusAvailable(version)
                : TranslationManager::updateStatusIdle();
        m_updateStatusLabel->setText(text);
    }
    if (m_updateGroup)
        m_updateGroup->setVisible(available || busy);
    if (m_updateButton) {
        m_updateButton->setEnabled(!busy);
        m_updateButton->setText(available ? TranslationManager::updateNow() : TranslationManager::checkForUpdates());
        m_updateButton->setStyleSheet(available
            ? QStringLiteral("color: #f5c542; font-weight: 600;")
            : QString());
    }
}

struct OcrPackageDef {
    const char *code;
    const char *name;
    bool recommended;
    bool essential;
};

QVector<OcrPackageDef> ocrPackageDefs()
{
    return {
        {"eng", "English", true, true},
        {"tur", "Turkish", true, true},
        {"rus", "Russian", true, false},
        {"deu", "German", false, false},
        {"fra", "French", false, false},
        {"spa", "Spanish", false, false},
        {"ita", "Italian", false, false},
        {"por", "Portuguese", false, false},
        {"pol", "Polish", false, false},
        {"nld", "Dutch", false, false},
        {"jpn", "Japanese", false, false},
        {"kor", "Korean", false, false},
        {"chi_sim", "Chinese Simplified", false, false},
    };
}

QString packageStatusText(bool installed)
{
    return installed
        ? TranslationManager::tr("packageInstalled")
        : TranslationManager::tr("packageMissingDownloadable");
}

QString packageSourceUrl(const QString &code)
{
    return QStringLiteral("https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/main/%1.traineddata").arg(code);
}

QString bundledTesseractDir()
{
    const QString appTesseractDir = QCoreApplication::applicationDirPath() + QStringLiteral("/tesseract");
    const QStringList names = {
        QStringLiteral("tesseract.exe"),
        QStringLiteral("tesseract")
    };
    for (const QString &name : names) {
        if (QFileInfo::exists(QDir(appTesseractDir).filePath(name)))
            return appTesseractDir;
    }
    return QString();
}

QString bundledFfmpegDir()
{
    const QString appFfmpegDir = QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg");
    const QStringList names = {
        QStringLiteral("ffmpeg.exe"),
        QStringLiteral("ffmpeg")
    };
    for (const QString &name : names) {
        if (QFileInfo::exists(QDir(appFfmpegDir).filePath(name)))
            return appFfmpegDir;
    }
    return QString();
}

QString componentExeName(const QString &base)
{
#ifdef Q_OS_WIN
    return base + QStringLiteral(".exe");
#else
    return base;
#endif
}

QString psQuote(QString value)
{
    value.replace(QStringLiteral("'"), QStringLiteral("''"));
    return QStringLiteral("'") + value + QStringLiteral("'");
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

static QKeySequence win32ToKeySequence(UINT modifiers, UINT vkey)
{
    if (vkey == 0) {
        Q_UNUSED(modifiers);
        return QKeySequence();
    }
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
    return QKeySequence(Qt::Key_Print);
}

bool SettingsDialog::isAutoStartEnabled()
{
#ifdef Q_OS_WIN
    // Fast path: the HKCU Run key check needs no external process. Only fall
    // back to schtasks when the Run entry is absent.
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    const QString runEntry = runKey.value(QStringLiteral("EShot")).toString();
    QString appPath = QCoreApplication::applicationFilePath().replace('/', '\\');
    if (!runEntry.isEmpty())
        return runEntry.contains(appPath, Qt::CaseInsensitive);

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

    return xml.contains(appPath, Qt::CaseInsensitive);
#else
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("autostart/io.github.benoks.EShot.desktop"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString text = QString::fromUtf8(file.readAll());
    const QString appPath = LinuxAutoStartPolicy::executablePath(
        qEnvironmentVariable("APPIMAGE"), QCoreApplication::applicationFilePath());
    return text.contains(appPath);
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
    const QString autostartDir = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("autostart"));
    const QString desktopPath = QDir(autostartDir).filePath(QStringLiteral("io.github.benoks.EShot.desktop"));
    if (!enabled)
        return !QFileInfo::exists(desktopPath) || QFile::remove(desktopPath);

    QDir dir(autostartDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;

    const QString appPath = LinuxAutoStartPolicy::executablePath(
        qEnvironmentVariable("APPIMAGE"), QCoreApplication::applicationFilePath());
    const QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
    const QString desktopName = qEnvironmentVariable("XDG_CURRENT_DESKTOP",
                                                       qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    const QString execLine = LinuxAutoStartPolicy::commandLine(
        appPath, desktopName, QString(), sessionType);
    const QString desktop = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=EShot\n"
        "Comment=Start EShot in the background\n"
        "Exec=%1\n"
        "Icon=io.github.benoks.EShot-v4\n"
        "Terminal=false\n"
        "StartupNotify=false\n"
        "X-KDE-StartupNotify=false\n"
        "X-GNOME-Autostart-enabled=true\n")
        .arg(execLine);

    QFile file(desktopPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    file.write(desktop.toUtf8());
    file.close();
    return file.error() == QFile::NoError;
#endif
}

static QString printScreenConflictTitle()
{
    return TranslationManager::printScreenConflictTitle();
}

static QString printScreenConflictMessage()
{
    return TranslationManager::printScreenConflictMessage();
}

static QString printScreenConflictFixText()
{
    return TranslationManager::printScreenConflictFix();
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
    tabs->addTab(createPackagesTab(),    TranslationManager::tr("tabPackages"));
    tabs->addTab(createCaptureTab(),     TranslationManager::tabCapture());
    tabs->addTab(createRecordingTab(),   TranslationManager::tabRecording());
    tabs->addTab(createAppearanceTab(),  TranslationManager::tabAppearance());
    tabs->addTab(createInterfaceTab(),   TranslationManager::tabInterface());
    tabs->addTab(createHotkeyTab(),      TranslationManager::tabHotkey());
    mainLayout->addWidget(tabs);

    const QMargins margins = mainLayout->contentsMargins();
    const int frameWidth = tabs->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, tabs) * 2;
    const int horizontalChrome = margins.left() + margins.right() + frameWidth + 4;
    const int fittedWidth = settingsDialogWidthForTabs(
        tabs->tabBar()->sizeHint().width(), horizontalChrome, 560);
    setMinimumWidth(fittedWidth);
    setMaximumWidth(qMax(750, fittedWidth));

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

    const QSize actionButtonSize = settingsDialogActionButtonSize(
        {resetBtn->sizeHint(), cancelBtn->sizeHint(), saveBtn->sizeHint()});
    resetBtn->setFixedSize(actionButtonSize);
    cancelBtn->setFixedSize(actionButtonSize);
    saveBtn->setFixedSize(actionButtonSize);
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

QWidget* SettingsDialog::createGeneralTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *outerLayout = new QVBoxLayout(tab);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    QScrollArea *scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget(scroll);
    QVBoxLayout *layout = new QVBoxLayout(content);

    m_updateGroup = new QGroupBox(TranslationManager::updateTitle());
    QVBoxLayout *updateLayout = new QVBoxLayout(m_updateGroup);
    m_updateStatusLabel = new QLabel(TranslationManager::updateStatusIdle(), m_updateGroup);
    m_updateStatusLabel->setWordWrap(true);
    m_updateStatusLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    m_updateButton = new QPushButton(TranslationManager::updateNow(), m_updateGroup);
    connect(m_updateButton, &QPushButton::clicked, this, &SettingsDialog::updateRequested);
    updateLayout->addWidget(m_updateStatusLabel);
    updateLayout->addWidget(m_updateButton);
    m_updateGroup->setVisible(false);
    layout->addWidget(m_updateGroup);

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

    QGroupBox *mediaPathGroup = new QGroupBox(TranslationManager::tr("mediaFolders"));
    QFormLayout *mediaPathLayout = new QFormLayout(mediaPathGroup);
    auto makePathRow = [this, mediaPathGroup](QLineEdit **editPtr, const QString &placeholder) {
        QWidget *row = new QWidget(mediaPathGroup);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        *editPtr = new QLineEdit(row);
        (*editPtr)->setPlaceholderText(placeholder);
        QPushButton *browse = new QPushButton(TranslationManager::browse(), row);
        browse->setFixedWidth(82);
        connect(browse, &QPushButton::clicked, this, [this, editPtr]() {
            QString start = (*editPtr)->text().trimmed();
            if (start.isEmpty() && m_savePathEdit)
                start = m_savePathEdit->text().trimmed();
            QString dir = QFileDialog::getExistingDirectory(this, TranslationManager::saveDir(), start);
            if (!dir.isEmpty())
                (*editPtr)->setText(dir);
        });
        rowLayout->addWidget(*editPtr);
        rowLayout->addWidget(browse);
        return row;
    };
    mediaPathLayout->addRow(TranslationManager::tr("screenshotsLabel"),
                            makePathRow(&m_screenshotPathEdit, TranslationManager::tr("defaultFolderHint")));
    mediaPathLayout->addRow(QStringLiteral("GIF:"),
                            makePathRow(&m_gifPathEdit, TranslationManager::tr("defaultFolderHint")));
    mediaPathLayout->addRow(TranslationManager::tr("videosLabel"),
                            makePathRow(&m_videoPathEdit, TranslationManager::tr("defaultFolderHint")));
    layout->addWidget(mediaPathGroup);

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
    m_notificationOpenFolderCheck = new QCheckBox(
        TranslationManager::notificationOpenFolder(), m_notificationOptionsWidget);
    m_notifyCopyCheck->setToolTip(uiLabel("Gorsel panoya kopyalaninca bildirim gosterir.", "Show a notification when an image is copied."));
    m_notifySaveCheck->setToolTip(uiLabel("Gorsel dosyaya kaydedilince klasoru acabilen bildirim gosterir.", "Show a folder-opening notification when an image is saved."));
    m_notifyGifCheck->setToolTip(uiLabel("GIF kaydi bitince klasoru acabilen bildirim gosterir.", "Show a folder-opening notification when a GIF recording finishes."));
    m_notifyVideoCheck->setToolTip(uiLabel("Video kaydi bitince klasoru acabilen bildirim gosterir.", "Show a folder-opening notification when a video recording finishes."));
    m_notificationOpenFolderCheck->setToolTip(uiLabel(
        "Kaydedilen dosyalarin bildirimlerinde Klasoru Ac eylemini gosterir.",
        "Show the Open Folder action in notifications for saved files."));
    notifLayout->addWidget(m_notifyCopyCheck);
    notifLayout->addWidget(m_notifySaveCheck);
    notifLayout->addWidget(m_notifyGifCheck);
    notifLayout->addWidget(m_notifyVideoCheck);
    notifLayout->addWidget(m_notificationOpenFolderCheck);
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
    m_blackTrayIconCheck = new QCheckBox(TranslationManager::trayIconDark());
    m_highContrastCheck->setToolTip(TranslationManager::tipHighContrast());
    m_blackTrayIconCheck->setToolTip(TranslationManager::tipTrayIcon());
    connect(m_highContrastCheck, &QCheckBox::toggled, this, &SettingsDialog::onThemeChanged);
    accessLayout->addWidget(m_highContrastCheck);
    accessLayout->addWidget(m_blackTrayIconCheck);
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
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
    return tab;
}

QWidget* SettingsDialog::createPackagesTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *outerLayout = new QVBoxLayout(tab);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget(scroll);
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setSpacing(10);

    QGroupBox *componentsGroup = new QGroupBox(TranslationManager::tr("componentStatus"));
    QGridLayout *componentsLayout = new QGridLayout(componentsGroup);
    const bool tesseractInstalled = QFileInfo::exists(OcrEngine::tesseractPath());
    const bool tesseractBundled = !bundledTesseractDir().isEmpty();
    const bool ffmpegBundled = !bundledFfmpegDir().isEmpty();
    const bool ffmpegInstalled = ffmpegBundled || !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
#ifdef Q_OS_WIN
    const bool canDownloadComponents = true;
#else
    const bool canDownloadComponents = false;
#endif
    auto addComponentRow = [&](int row, const QString &name, const QString &status, bool canDelete,
                               QLabel **statusPtr, QPushButton **buttonPtr, const QObject *receiver, const char *slot) {
        QLabel *nameLabel = new QLabel(name, componentsGroup);
        QLabel *statusLabel = new QLabel(status, componentsGroup);
        statusLabel->setStyleSheet(status == TranslationManager::tr("packageInstalled") ? "color: #8bd17c;" : "color: #f0c36d;");
        QPushButton *deleteButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), TranslationManager::tr("packageDelete"), componentsGroup);
        deleteButton->setEnabled(canDelete);
        deleteButton->setFixedWidth(82);
        deleteButton->setStyleSheet("color: #ff6b6b;");
        connect(deleteButton, SIGNAL(clicked()), receiver, slot);
        if (statusPtr) *statusPtr = statusLabel;
        if (buttonPtr) *buttonPtr = deleteButton;
        componentsLayout->addWidget(nameLabel, row, 0);
        componentsLayout->addWidget(statusLabel, row, 1);
        componentsLayout->addWidget(deleteButton, row, 2);
    };
    addComponentRow(0, QStringLiteral("Tesseract OCR:"), tesseractInstalled ? TranslationManager::tr("packageInstalled") : TranslationManager::tr("packageMissing"),
                    tesseractBundled, &m_tesseractStatusLabel, &m_tesseractDeleteButton, this, SLOT(onTesseractComponentAction()));
    addComponentRow(1, QStringLiteral("FFmpeg:"), ffmpegInstalled ? TranslationManager::tr("packageInstalled") : TranslationManager::tr("packageMissing"),
                    ffmpegBundled, &m_ffmpegStatusLabel, &m_ffmpegDeleteButton, this, SLOT(onFfmpegComponentAction()));
    componentsLayout->setColumnStretch(1, 1);
    layout->addWidget(componentsGroup);
#ifdef Q_OS_LINUX
    QPushButton *linuxSetupButton = new QPushButton(uiLabel("Linux bağımlılık kurulumunu aç", "Open Linux dependency setup"), tab);
    connect(linuxSetupButton, &QPushButton::clicked, this, &SettingsDialog::onOpenLinuxDependencySetup);
    layout->addWidget(linuxSetupButton);
#endif

    QGroupBox *ocrGroup = new QGroupBox(TranslationManager::tr("ocrLanguagePacks"));
    QVBoxLayout *ocrLayout = new QVBoxLayout(ocrGroup);
    QLabel *hint = new QLabel(TranslationManager::tr("ocrPackagesHint"), ocrGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #aaa; font-size: 12px;");
    ocrLayout->addWidget(hint);

    m_packageList = new QListWidget(ocrGroup);
    m_packageList->setSelectionMode(QAbstractItemView::NoSelection);
    m_packageList->setMinimumHeight(300);
    m_packageList->setStyleSheet(R"(
        QListWidget {
            background: #252525;
            border: 1px solid #3d3d3d;
            border-radius: 6px;
            padding: 6px;
        }
        QListWidget::item {
            min-height: 38px;
            border-radius: 4px;
        }
    )");
    ocrLayout->addWidget(m_packageList);

    QHBoxLayout *bulkLayout = new QHBoxLayout();
    m_essentialOcrButton = new QPushButton(TranslationManager::tr("downloadEssentials"), ocrGroup);
    m_deleteSelectedOcrButton = new QPushButton(TranslationManager::tr("deleteSelected"), ocrGroup);
    m_deleteSelectedOcrButton->setStyleSheet("color: #ff6b6b;");
    connect(m_essentialOcrButton, &QPushButton::clicked, this, &SettingsDialog::onDownloadEssentialOcr);
    connect(m_deleteSelectedOcrButton, &QPushButton::clicked, this, &SettingsDialog::onDeleteSelectedOcr);
    bulkLayout->addWidget(m_essentialOcrButton);
    bulkLayout->addWidget(m_deleteSelectedOcrButton);
    bulkLayout->addStretch();
    ocrLayout->addLayout(bulkLayout);

    m_packageStatusLabel = new QLabel(ocrGroup);
    m_packageStatusLabel->setWordWrap(true);
    m_packageStatusLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    ocrLayout->addWidget(m_packageStatusLabel);

    layout->addWidget(ocrGroup);
    layout->addStretch();
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);

    refreshPackageStatus();
    return tab;
}

#ifdef Q_OS_LINUX
void SettingsDialog::onOpenLinuxDependencySetup()
{
    FirstRunWizard::showLinuxDependencySetup(this);
    refreshPackageStatus();
}
#endif

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
    m_formatCombo->setToolTip(uiLabel("Kaydedilen ekran görüntüsünün dosya biçimi.", "File format for saved screenshots."));
    fmtLayout->addRow(TranslationManager::tr("formatLabel"), m_formatCombo);

    QHBoxLayout *qLayout = new QHBoxLayout();
    m_qualitySlider = new QSlider(Qt::Horizontal);
    m_qualitySlider->setRange(10, 100);
    m_qualitySpin = new QSpinBox();
    m_qualitySpin->setRange(10, 100);
    m_qualitySpin->setSuffix("%");
    m_qualitySlider->setToolTip(uiLabel("Yalnızca JPEG için kalite ayarı.", "Quality setting for JPEG only."));
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
    m_delaySpin->setToolTip(uiLabel("Kısayola bastıktan sonra yakalama ekranının açılması için bekleme süresi.", "Delay before opening the capture overlay after the shortcut is pressed."));
    capLayout->addRow(TranslationManager::delay(), m_delaySpin);
    m_copyAfterCaptureCheck = new QCheckBox(TranslationManager::copyAfterCapture());
    m_copyAfterCaptureCheck->hide();
    m_closeAfterCopyCheck = new QCheckBox(TranslationManager::closeAfterCopy());
    m_closeAfterCopyCheck->setToolTip(uiLabel("Kopyala komutundan sonra seçim ekranını otomatik kapatır.", "Automatically closes the capture overlay after copying."));
    m_instantCopyAfterSelectionCheck = new QCheckBox(TranslationManager::tr("instantCopyAfterSelection"));
    m_instantCopyAfterSelectionCheck->setToolTip(uiLabel("Varsayilan kapali. Aciksa alan secimini bitirdigin anda goruntu panoya kopyalanir.", "Off by default. When enabled, the image is copied as soon as you finish selecting a region."));
    capLayout->addRow(m_closeAfterCopyCheck);
    capLayout->addRow(m_instantCopyAfterSelectionCheck);
    m_rememberLastAnnotationToolCheck = new QCheckBox(TranslationManager::rememberLastAnnotationTool());
    m_rememberLastAnnotationToolCheck->setToolTip(TranslationManager::rememberLastAnnotationToolHint());
    capLayout->addRow(m_rememberLastAnnotationToolCheck);
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
    m_captureHintsCheck = new QCheckBox(TranslationManager::showCaptureHints());
    m_captureHintsCheck->setToolTip(TranslationManager::showCaptureHintsTip());
    overlayLayout->addRow(m_captureHintsCheck);
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
    m_recordingFpsSpin->setRange(1, gifRecordingFpsLimit());
    m_recordingFpsSpin->setSuffix(QStringLiteral(" fps"));
    m_recordingFpsSpin->setValue(10);
    m_recordingFpsSpin->setToolTip(uiLabel("GIF icin saniyedeki kare sayisi. Daha yuksek deger daha akici ama daha buyuk dosya uretir.", "Frames per second for GIF. Higher values are smoother but create larger files."));
    recForm->addRow(TranslationManager::gifFpsLabel(), m_recordingFpsSpin);

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

    m_gifSizePresetCombo = new QComboBox();
    m_gifSizePresetCombo->addItem(TranslationManager::tr("gifSizeSmallest"), 720);
    m_gifSizePresetCombo->addItem(TranslationManager::tr("gifSizeBalanced"), 1280);
    m_gifSizePresetCombo->addItem(TranslationManager::tr("gifSizeBest"), 1920);
    m_gifSizePresetCombo->setToolTip(uiLabel("GIF'in uzun kenar sinirini belirler. Dusuk deger dosya boyutunu ciddi azaltir.", "Controls the GIF max side length. Lower values can greatly reduce file size."));
    recForm->addRow(TranslationManager::tr("gifSizePresetLabel"), m_gifSizePresetCombo);

    m_recordingStartDelaySpin = new QSpinBox();
    m_recordingStartDelaySpin->setRange(0, 10);
    m_recordingStartDelaySpin->setSuffix(QStringLiteral(" s"));
    m_recordingStartDelaySpin->setSpecialValueText(TranslationManager::noDelay());
    m_recordingStartDelaySpin->setToolTip(uiLabel("Alan secildikten sonra GIF/video kaydinin baslamadan once bekleyecegi sure.", "Delay after selecting the area before GIF/video recording starts."));
    recForm->addRow(TranslationManager::tr("recordingStartDelayLabel"), m_recordingStartDelaySpin);

    layout->addWidget(recGroup);

    QGroupBox *videoGroup = new QGroupBox(TranslationManager::videoRecordingTitle());
    QFormLayout *videoForm = new QFormLayout(videoGroup);

    m_videoFpsSpin = new QSpinBox();
    m_videoFpsSpin->setRange(1, videoRecordingFpsLimit());
    m_videoFpsSpin->setSuffix(QStringLiteral(" fps"));
    m_videoFpsSpin->setValue(30);
    m_videoFpsSpin->setToolTip(uiLabel("Video icin saniyedeki kare sayisi.", "Frames per second for video recording."));
    videoForm->addRow(TranslationManager::videoFpsLabel(), m_videoFpsSpin);

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
    auto *desktopVolumeLabel = new QLabel(TranslationManager::tr("desktopVolumeLabel"), videoGroup);
    videoForm->addRow(desktopVolumeLabel, desktopVolumeRow);

    m_videoMicrophoneCheck = new QCheckBox(TranslationManager::audioMicrophone());
    m_videoMicrophoneCheck->setToolTip(uiLabel("Video kaydina mikrofon sesini ekler.", "Include microphone audio in video recordings."));
    videoForm->addRow(QString(), m_videoMicrophoneCheck);

    m_videoMicrophoneDeviceCombo = new QComboBox(videoGroup);
    m_videoMicrophoneDeviceCombo->addItem(TranslationManager::tr("defaultAudioDevice"), QStringLiteral("default"));
    for (const auto &device : microphoneAudioDevices()) {
        m_videoMicrophoneDeviceCombo->addItem(device.first, device.second);
    }
    m_videoMicrophoneDeviceCombo->setToolTip(uiLabel("Video kaydinda kullanilacak mikrofon kaynagi.", "Microphone source used for video recording."));
    auto *microphoneDeviceLabel = new QLabel(TranslationManager::audioMicrophoneDevice(), videoGroup);
    videoForm->addRow(microphoneDeviceLabel, m_videoMicrophoneDeviceCombo);

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
    auto *microphoneVolumeLabel = new QLabel(TranslationManager::tr("microphoneVolumeLabel"), videoGroup);
    videoForm->addRow(microphoneVolumeLabel, micVolumeRow);

    auto updateDesktopAudioState = [desktopVolumeLabel, desktopVolumeRow](bool enabled) {
        desktopVolumeLabel->setEnabled(enabled);
        desktopVolumeRow->setEnabled(enabled);
    };
    auto updateMicrophoneState = [microphoneDeviceLabel, microphoneVolumeLabel,
                                  micVolumeRow, this](bool enabled) {
        microphoneDeviceLabel->setEnabled(enabled);
        microphoneVolumeLabel->setEnabled(enabled);
        if (m_videoMicrophoneDeviceCombo)
            m_videoMicrophoneDeviceCombo->setEnabled(enabled);
        micVolumeRow->setEnabled(enabled);
    };
    connect(m_videoDesktopAudioCheck, &QCheckBox::toggled, this, updateDesktopAudioState);
    connect(m_videoMicrophoneCheck, &QCheckBox::toggled, this, updateMicrophoneState);
    updateDesktopAudioState(m_videoDesktopAudioCheck->isChecked());
    updateMicrophoneState(m_videoMicrophoneCheck->isChecked());

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

    m_rememberSettingsWindowSizeCheck = new QCheckBox(
        TranslationManager::tr("rememberSettingsWindowSize"), tab);
    layout->addWidget(m_rememberSettingsWindowSizeCheck);

    QGroupBox *visualSearchGroup = new QGroupBox(TranslationManager::visualSearchTitle());
    QFormLayout *visualSearchLayout = new QFormLayout(visualSearchGroup);
    m_visualSearchProviderCombo = new QComboBox(visualSearchGroup);
    m_visualSearchProviderCombo->addItem(TranslationManager::visualSearchGoogleLens(), QStringLiteral("google"));
    m_visualSearchProviderCombo->addItem(TranslationManager::visualSearchYandexImages(), QStringLiteral("yandex"));
    visualSearchLayout->addRow(TranslationManager::visualSearchProvider(), m_visualSearchProviderCombo);
    QLabel *visualSearchPrivacy = new QLabel(TranslationManager::tr("visualSearchPrivacy"), visualSearchGroup);
    visualSearchPrivacy->setWordWrap(true);
    visualSearchPrivacy->setStyleSheet("color: #aaa; font-size: 12px;");
    visualSearchLayout->addRow(visualSearchPrivacy);
    layout->addWidget(visualSearchGroup);

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
        {"GoogleLens",    TranslationManager::visualSearchAction(),           ":/icons/search.svg"},
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

    QWidget *drawingColumn = makeColumn(TranslationManager::drawingTools(), &m_toolVisibilityList);
    QWidget *controlColumn = makeColumn(TranslationManager::bottomToolbarControls(), &m_toolbarControlVisibilityList);
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

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    const QString bindingText = desktop == LinuxDesktopEnvironment::Gnome
        ? uiLabel("PrintScreen kisayolunu GNOME ile ayarla", "Configure PrintScreen shortcut in GNOME")
        : desktop == LinuxDesktopEnvironment::Kde
            ? uiLabel("PrintScreen kisayolunu KDE ile yeniden ayarla", "Reconfigure PrintScreen shortcut in KDE")
            : uiLabel("PrintScreen kisayolunu masaustuyle ayarla", "Configure PrintScreen with the desktop");
    m_linuxPrintScreenBindingButton = new QPushButton(bindingText);
    connect(m_linuxPrintScreenBindingButton, &QPushButton::clicked,
            this, &SettingsDialog::onRequestLinuxPrintScreenBinding);
    gl->addWidget(m_linuxPrintScreenBindingButton);
#endif

    QPushButton *resetHkBtn = new QPushButton(TranslationManager::hotkeyReset());
    resetHkBtn->setStyleSheet("color: #aaa;");
    connect(resetHkBtn, &QPushButton::clicked, [this]() {
        m_hotkeyEdit->setKeySequence(QKeySequence(Qt::Key_Print));
    });
    gl->addWidget(resetHkBtn);

    QGroupBox *actionGroup = new QGroupBox(uiLabel("Direkt yakalama kisayollari", "Direct capture hotkeys"));
    QFormLayout *actionHotkeyLayout = new QFormLayout(actionGroup);
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
    m_instantCaptureHotkeyEdit = makeRecordingHotkeyEdit();
    m_gifCaptureHotkeyEdit = makeRecordingHotkeyEdit();
    m_videoCaptureHotkeyEdit = makeRecordingHotkeyEdit();
#ifdef Q_OS_WIN
    m_windowCaptureHotkeyEdit = makeRecordingHotkeyEdit();
#endif
    m_instantCaptureHotkeyEdit->setToolTip(uiLabel("Bos birakilirsa kapali kalir. Alan secimi bitince otomatik kopyalar.", "Leave empty to disable. Copies automatically when region selection finishes."));
    m_gifCaptureHotkeyEdit->setToolTip(uiLabel("Bos birakilirsa kapali kalir. Dogrudan GIF alan secimini acar.", "Leave empty to disable. Opens GIF area selection directly."));
    m_videoCaptureHotkeyEdit->setToolTip(uiLabel("Bos birakilirsa kapali kalir. Dogrudan video alan secimini acar.", "Leave empty to disable. Opens video area selection directly."));
#ifdef Q_OS_WIN
    m_windowCaptureHotkeyEdit->setToolTip(uiLabel("Bos birakilirsa kapali kalir. Fareyle pencere secme modunu acar.", "Leave empty to disable. Opens window selection mode."));
    actionHotkeyLayout->addRow(uiLabel("Pencere:", "Window:"), m_windowCaptureHotkeyEdit);
#endif
    actionHotkeyLayout->addRow(uiLabel("Instant bolge:", "Instant region:"), m_instantCaptureHotkeyEdit);
    actionHotkeyLayout->addRow(QStringLiteral("GIF:"), m_gifCaptureHotkeyEdit);
    actionHotkeyLayout->addRow(uiLabel("Video:", "Video:"), m_videoCaptureHotkeyEdit);
    gl->addWidget(actionGroup);

    QGroupBox *recordingGroup = new QGroupBox(TranslationManager::videoRecordingTitle());
    QFormLayout *recordingHotkeyLayout = new QFormLayout(recordingGroup);
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

QString SettingsDialog::tessdataTargetDir() const
{
    QString dir = OcrEngine::tessdataDir();
    if (dir.trimmed().isEmpty())
        dir = QCoreApplication::applicationDirPath() + QStringLiteral("/tesseract/tessdata");
    return dir;
}

void SettingsDialog::refreshPackageStatus()
{
    if (!m_packageList)
        return;

    const bool busy = m_packageReply != nullptr || !m_pendingOcrDownloads.isEmpty();
    const bool tesseractInstalled = QFileInfo::exists(OcrEngine::tesseractPath());
    const bool tesseractBundled = !bundledTesseractDir().isEmpty();
    const bool ffmpegBundled = !bundledFfmpegDir().isEmpty();
    const bool ffmpegInstalled = ffmpegBundled || !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
#ifdef Q_OS_WIN
    const bool canDownloadComponents = true;
#else
    const bool canDownloadComponents = false;
#endif
    auto setComponentStatus = [](QLabel *label, QPushButton *button, bool installed, bool canUse, bool downloadWhenMissing = false) {
        if (label) {
            label->setText(installed ? TranslationManager::tr("packageInstalled") : TranslationManager::tr("packageMissing"));
            label->setStyleSheet(installed ? "color: #8bd17c;" : "color: #f0c36d;");
        }
        if (button) {
            button->setText(installed ? TranslationManager::tr("packageDelete") : TranslationManager::tr("packageDownload"));
            button->setIcon(installed ? QIcon::fromTheme(QStringLiteral("edit-delete")) : QIcon());
            button->setStyleSheet(installed ? "color: #ff6b6b;" : QString());
            button->setEnabled(canUse || (!installed && downloadWhenMissing));
        }
    };
    setComponentStatus(m_tesseractStatusLabel, m_tesseractDeleteButton, tesseractInstalled, !busy && tesseractBundled, !busy && canDownloadComponents);
    setComponentStatus(m_ffmpegStatusLabel, m_ffmpegDeleteButton, ffmpegInstalled, ffmpegBundled && !busy, !busy && canDownloadComponents);

    m_packageList->clear();
    m_packageList->setEnabled(tesseractInstalled);

    const QString tessDir = tessdataTargetDir();
    for (const OcrPackageDef &def : ocrPackageDefs()) {
        const QString code = QString::fromLatin1(def.code);
        const QString name = QString::fromLatin1(def.name);
        const bool installed = QFileInfo::exists(QDir(tessDir).filePath(code + QStringLiteral(".traineddata")));
        const bool active = code == m_activeOcrDownload;

        QListWidgetItem *item = new QListWidgetItem(m_packageList);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setData(Qt::UserRole, code);
        item->setData(Qt::UserRole + 1, installed);
        item->setSizeHint(QSize(0, 44));

        QWidget *row = new QWidget(m_packageList);
        row->setEnabled(tesseractInstalled);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(6, 4, 6, 4);
        rowLayout->setSpacing(6);

        QCheckBox *selectBox = new QCheckBox(row);
        selectBox->setFixedWidth(22);
        selectBox->setEnabled(tesseractInstalled && installed && !busy);
        connect(selectBox, &QCheckBox::toggled, this, [item](bool checked) {
            item->setData(Qt::UserRole + 2, checked);
        });
        QLabel *nameLabel = new QLabel(QStringLiteral("%1 (%2)").arg(name, code), row);
        nameLabel->setFixedWidth(150);
        nameLabel->setToolTip(QStringLiteral("%1 (%2)").arg(name, code));
        QLabel *statusLabel = new QLabel(active ? TranslationManager::tr("packageDownloading") : packageStatusText(installed), row);
        statusLabel->setStyleSheet(installed ? "color: #8bd17c;" : "color: #f0c36d;");
        statusLabel->setFixedWidth(128);

        QPushButton *downloadButton = new QPushButton(TranslationManager::tr("packageDownload"), row);
        QPushButton *deleteButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), TranslationManager::tr("packageDelete"), row);
        downloadButton->setFixedWidth(78);
        deleteButton->setFixedWidth(78);
        deleteButton->setStyleSheet("color: #ff6b6b;");
        downloadButton->setEnabled(tesseractInstalled && !busy && !installed);
        deleteButton->setEnabled(tesseractInstalled && !busy && installed);
        downloadButton->setVisible(!installed);
        connect(downloadButton, &QPushButton::clicked, this, [this, code]() { downloadOcrLanguage(code); });
        connect(deleteButton, &QPushButton::clicked, this, [this, code]() { deleteOcrLanguage(code); });

        rowLayout->addWidget(selectBox);
        rowLayout->addWidget(nameLabel);
        rowLayout->addWidget(statusLabel);
        rowLayout->addWidget(downloadButton);
        rowLayout->addWidget(deleteButton);
        m_packageList->addItem(item);
        m_packageList->setItemWidget(item, row);
    }

    if (m_packageStatusLabel) {
        m_packageStatusLabel->setText(!m_packageOperationStatus.isEmpty()
            ? m_packageOperationStatus
            : tesseractInstalled
            ? TranslationManager::tr("ocrPackageInstallHint")
            : TranslationManager::tr("ocrEngineMissingHint"));
    }
    if (m_essentialOcrButton) m_essentialOcrButton->setEnabled(tesseractInstalled && !busy);
    if (m_deleteSelectedOcrButton) m_deleteSelectedOcrButton->setEnabled(tesseractInstalled && !busy);
}

void SettingsDialog::downloadOcrLanguage(const QString &code)
{
    if (code.isEmpty())
        return;
    if (m_packageReply || m_packageExtractProcess) {
        if (!m_pendingOcrDownloads.contains(code))
            m_pendingOcrDownloads.append(code);
        refreshPackageStatus();
        return;
    }

    const QString targetDir = tessdataTargetDir();
    if (!QDir().mkpath(targetDir)) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             uiLabel("Paket klasörü oluşturulamadı.", "Could not create the package folder."));
        return;
    }

    const QString targetPath = QDir(targetDir).filePath(code + QStringLiteral(".traineddata"));
    if (QFileInfo::exists(targetPath)) {
        refreshPackageStatus();
        return;
    }

    m_activeOcrDownload = code;
    refreshPackageStatus();

    QNetworkRequest request(QUrl(packageSourceUrl(code)));
    request.setRawHeader("User-Agent", "EShot-Package-Manager");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_packageReply = m_packageNetwork->get(request);
    connect(m_packageReply, &QNetworkReply::finished, this, [this, code, targetPath]() {
        QNetworkReply *reply = m_packageReply;
        m_packageReply = nullptr;
        m_activeOcrDownload.clear();

        const bool networkOk = reply && reply->error() == QNetworkReply::NoError;
        const QByteArray data = networkOk ? reply->readAll() : QByteArray();
        const QString errorText = reply ? reply->errorString() : uiLabel("Bilinmeyen ağ hatası", "Unknown network error");
        if (reply)
            reply->deleteLater();

        if (!networkOk || data.size() < 1024) {
            QMessageBox::warning(this, TranslationManager::errTitle(),
                                 uiLabel("OCR paketi indirilemedi: ", "Could not download OCR package: ") + errorText);
        } else {
            const QString tempPath = targetPath + QStringLiteral(".download");
            QFile file(tempPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size()) {
                QMessageBox::warning(this, TranslationManager::errTitle(),
                                     uiLabel("OCR paketi yazılamadı.", "Could not write the OCR package."));
                file.close();
                QFile::remove(tempPath);
            } else {
                file.close();
                QFile::remove(targetPath);
                if (!QFile::rename(tempPath, targetPath)) {
                    QFile::remove(tempPath);
                    QMessageBox::warning(this, TranslationManager::errTitle(),
                                         uiLabel("OCR paketi yerine taşınamadı.", "Could not move the OCR package into place."));
                }
            }
        }

        if (!m_pendingOcrDownloads.isEmpty()) {
            const QString next = m_pendingOcrDownloads.takeFirst();
            QTimer::singleShot(0, this, [this, next]() { downloadOcrLanguage(next); });
        }
        refreshPackageStatus();
    });
}

void SettingsDialog::deleteOcrLanguage(const QString &code)
{
    if (code.isEmpty() || m_packageReply)
        return;
    const QString path = QDir(tessdataTargetDir()).filePath(code + QStringLiteral(".traineddata"));
    if (!QFileInfo::exists(path)) {
        refreshPackageStatus();
        return;
    }
    if (!QFile::remove(path)) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             uiLabel("OCR paketi silinemedi.", "Could not delete the OCR package."));
    }
    refreshPackageStatus();
}

void SettingsDialog::onTesseractComponentAction()
{
    if (m_packageReply || m_packageExtractProcess)
        return;
    const QString dirPath = bundledTesseractDir();
    if (dirPath.isEmpty()) {
        downloadTesseractComponent();
        return;
    }
    if (QMessageBox::question(this,
            uiLabel("OCR bileşenini sil", "Delete OCR component"),
            uiLabel("Tesseract OCR ve tüm OCR dil paketleri silinsin mi?",
                    "Delete Tesseract OCR and all OCR language packs?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    if (!QDir(dirPath).removeRecursively()) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             uiLabel("Tesseract OCR bileşeni silinemedi.", "Could not delete the Tesseract OCR component."));
    }
    refreshPackageStatus();
}

void SettingsDialog::downloadTesseractComponent()
{
    downloadReleaseComponent(QStringLiteral("tesseract"), componentExeName(QStringLiteral("tesseract")), uiLabel("OCR bileşeni", "OCR component"));
}

void SettingsDialog::downloadFfmpegComponent()
{
    downloadReleaseComponent(QStringLiteral("ffmpeg"), componentExeName(QStringLiteral("ffmpeg")), QStringLiteral("FFmpeg"));
}

void SettingsDialog::downloadReleaseComponent(const QString &componentDir, const QString &exeName, const QString &statusPrefix)
{
#ifndef Q_OS_WIN
    Q_UNUSED(componentDir);
    Q_UNUSED(exeName);
    Q_UNUSED(statusPrefix);
    QMessageBox::information(this, TranslationManager::errTitle(),
                             uiLabel("Linux'ta bileşenler sistem paket yöneticisinden veya ileride eklenecek Linux paketinden kurulacak.",
                                     "On Linux, components should be installed through the system package manager or a future Linux package."));
    return;
#endif
    if (m_packageReply || m_packageExtractProcess)
        return;
    m_packageOperationStatus = QStringLiteral("%1: %2").arg(
        statusPrefix,
        uiLabel("release paketi aranıyor...", "looking for release package..."));
    refreshPackageStatus();

    QUrl url(QStringLiteral("https://api.github.com/repos/Benoks/EShot/releases/latest"));
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "EShot-Package-Manager");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_packageReply = m_packageNetwork->get(request);
    connect(m_packageReply, &QNetworkReply::finished, this, [this, componentDir, exeName, statusPrefix]() {
        QNetworkReply *reply = m_packageReply;
        m_packageReply = nullptr;
        const bool ok = reply && reply->error() == QNetworkReply::NoError;
        const QByteArray data = ok ? reply->readAll() : QByteArray();
        const QString errorText = reply ? reply->errorString() : uiLabel("Bilinmeyen ağ hatası", "Unknown network error");
        if (reply)
            reply->deleteLater();

        if (!ok) {
            QMessageBox::warning(this, TranslationManager::errTitle(),
                                 uiLabel("Release bilgisi alınamadı: ", "Could not read release info: ") + errorText);
            m_packageOperationStatus.clear();
            refreshPackageStatus();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const QJsonArray assets = doc.object().value(QStringLiteral("assets")).toArray();
        const QString arch = QSysInfo::currentCpuArchitecture().toLower().contains(QStringLiteral("arm"))
            ? QStringLiteral("arm64")
            : QStringLiteral("x64");
        QString assetUrl;
        QString assetName;
        qint64 assetSize = 0;
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QString lower = name.toLower();
            if (lower.endsWith(QStringLiteral(".zip"))
                && lower.contains(QStringLiteral("portable"))
                && lower.contains(arch)) {
                assetName = name;
                assetUrl = asset.value(QStringLiteral("browser_download_url")).toString();
                assetSize = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble());
                break;
            }
        }

        if (assetUrl.isEmpty()) {
            QMessageBox::warning(this, TranslationManager::errTitle(),
                                 uiLabel("Bu cihaz için portable release paketi bulunamadı.", "No portable release package was found for this device."));
            m_packageOperationStatus.clear();
            refreshPackageStatus();
            return;
        }
        downloadComponentArchive(assetUrl, assetName, assetSize, componentDir, exeName, statusPrefix);
    });
}

void SettingsDialog::downloadComponentArchive(const QString &url, const QString &assetName, qint64 expectedSize,
                                              const QString &componentDir, const QString &exeName, const QString &statusPrefix)
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.trimmed().isEmpty())
        cacheDir = QDir::tempPath() + QStringLiteral("/EShot");
    cacheDir = QDir(cacheDir).filePath(QStringLiteral("packages"));
    QDir().mkpath(cacheDir);

    m_packageDownloadPath = QDir(cacheDir).filePath(assetName.isEmpty() ? QStringLiteral("EShot_portable.zip") : assetName);
    m_packageExpectedSize = expectedSize;
    delete m_packageDownloadFile;
    m_packageDownloadFile = new QFile(m_packageDownloadPath);
    if (!m_packageDownloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             uiLabel("OCR bileşeni indirilecek dosya açılamadı.", "Could not open the OCR component download file."));
        delete m_packageDownloadFile;
        m_packageDownloadFile = nullptr;
        m_packageOperationStatus.clear();
        refreshPackageStatus();
        return;
    }

    m_packageOperationStatus = QStringLiteral("%1: %2").arg(statusPrefix, uiLabel("indiriliyor...", "downloading..."));
    refreshPackageStatus();

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "EShot-Package-Manager");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_packageReply = m_packageNetwork->get(request);
    connect(m_packageReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_packageDownloadFile && m_packageReply)
            m_packageDownloadFile->write(m_packageReply->readAll());
    });
    connect(m_packageReply, &QNetworkReply::finished, this, [this, componentDir, exeName, statusPrefix]() {
        QNetworkReply *reply = m_packageReply;
        m_packageReply = nullptr;
        if (m_packageDownloadFile && reply)
            m_packageDownloadFile->write(reply->readAll());
        if (m_packageDownloadFile) {
            m_packageDownloadFile->flush();
            m_packageDownloadFile->close();
            delete m_packageDownloadFile;
            m_packageDownloadFile = nullptr;
        }

        const bool ok = reply && reply->error() == QNetworkReply::NoError;
        const QString errorText = reply ? reply->errorString() : uiLabel("Bilinmeyen ağ hatası", "Unknown network error");
        if (reply)
            reply->deleteLater();

        const qint64 size = QFileInfo(m_packageDownloadPath).size();
        if (!ok || size <= 0 || (m_packageExpectedSize > 0 && size != m_packageExpectedSize)) {
            QFile::remove(m_packageDownloadPath);
            QMessageBox::warning(this, TranslationManager::errTitle(),
                                 uiLabel("OCR bileşeni indirilemedi: ", "Could not download OCR component: ") + errorText);
            m_packageOperationStatus.clear();
            refreshPackageStatus();
            return;
        }
        extractComponentArchive(m_packageDownloadPath, componentDir, exeName, statusPrefix);
    });
}

void SettingsDialog::extractComponentArchive(const QString &archivePath, const QString &componentDir,
                                             const QString &exeName, const QString &statusPrefix)
{
#ifndef Q_OS_WIN
    Q_UNUSED(archivePath);
    Q_UNUSED(componentDir);
    Q_UNUSED(exeName);
    Q_UNUSED(statusPrefix);
    m_packageOperationStatus.clear();
    refreshPackageStatus();
    return;
#endif
    if (m_packageExtractProcess)
        return;
    m_packageOperationStatus = QStringLiteral("%1: %2").arg(statusPrefix, uiLabel("kuruluyor...", "installing..."));
    refreshPackageStatus();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString extractDir = QFileInfo(archivePath).absoluteDir().filePath(QStringLiteral("extract_tesseract"));
    QString script = QStringLiteral(
        "$ErrorActionPreference='Stop';"
        "$zip=%1;$extract=%2;$app=%3;$exeName=%4;$component=%5;"
        "Remove-Item -LiteralPath $extract -Recurse -Force -ErrorAction SilentlyContinue;"
        "Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force;"
        "$exe=Get-ChildItem -LiteralPath $extract -Recurse -Filter $exeName | Select-Object -First 1;"
        "if (-not $exe) { exit 2 };"
        "$src=Split-Path -Parent $exe.FullName;"
        "$dst=Join-Path $app $component;"
        "Remove-Item -LiteralPath $dst -Recurse -Force -ErrorAction SilentlyContinue;"
        "Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force;"
        "Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue;"
        "Remove-Item -LiteralPath $extract -Recurse -Force -ErrorAction SilentlyContinue;")
        .arg(psQuote(QDir::toNativeSeparators(archivePath)),
             psQuote(QDir::toNativeSeparators(extractDir)),
             psQuote(QDir::toNativeSeparators(appDir)),
             psQuote(exeName),
             psQuote(componentDir));

    // Run the extraction asynchronously: QProcess::execute() would freeze the
    // dialog and the whole GUI thread for the duration of the unpack.
    m_packageExtractProcess = new QProcess(this);
    connect(m_packageExtractProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_packageExtractProcess)
            return;
        m_packageExtractProcess->deleteLater();
        m_packageExtractProcess = nullptr;
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             uiLabel("OCR bileşeni kurulamadı. Kurulum klasörü için yönetici izni gerekebilir.",
                                     "Could not install the OCR component. Administrator permission may be required for the install folder."));
        m_packageOperationStatus.clear();
        refreshPackageStatus();
    });
    connect(m_packageExtractProcess, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
        if (!m_packageExtractProcess)
            return;
        m_packageExtractProcess->deleteLater();
        m_packageExtractProcess = nullptr;
        if (status == QProcess::NormalExit && exitCode != 0) {
            QMessageBox::warning(this, TranslationManager::errTitle(),
                                 uiLabel("OCR bileşeni kurulamadı. Kurulum klasörü için yönetici izni gerekebilir.",
                                         "Could not install the OCR component. Administrator permission may be required for the install folder."));
        }
        m_packageOperationStatus.clear();
        refreshPackageStatus();
    });
    m_packageExtractProcess->start(QStringLiteral("powershell.exe"),
        {QStringLiteral("-NoProfile"),
         QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
         QStringLiteral("-Command"), script});
}

void SettingsDialog::onFfmpegComponentAction()
{
    if (m_packageReply || m_packageExtractProcess)
        return;
    const QString dirPath = QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg");
    if (bundledFfmpegDir().isEmpty()) {
        downloadFfmpegComponent();
        return;
    }
    if (QMessageBox::question(this,
            uiLabel("FFmpeg bileşenini sil", "Delete FFmpeg component"),
            uiLabel("Bundled FFmpeg video bileşeni silinsin mi?",
                    "Delete the bundled FFmpeg video component?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    if (!QDir(dirPath).removeRecursively()) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             uiLabel("FFmpeg bileşeni silinemedi.", "Could not delete the FFmpeg component."));
    }
    refreshPackageStatus();
}

void SettingsDialog::onDownloadEssentialOcr()
{
    for (const OcrPackageDef &def : ocrPackageDefs()) {
        if (!def.essential)
            continue;
        const QString code = QString::fromLatin1(def.code);
        if (!QFileInfo::exists(QDir(tessdataTargetDir()).filePath(code + QStringLiteral(".traineddata")))
            && !m_pendingOcrDownloads.contains(code)) {
            m_pendingOcrDownloads.append(code);
        }
    }
    if (!m_pendingOcrDownloads.isEmpty()) {
        const QString next = m_pendingOcrDownloads.takeFirst();
        downloadOcrLanguage(next);
    }
}

void SettingsDialog::onDeleteSelectedOcr()
{
    if (!m_packageList || m_packageReply)
        return;
    for (int i = 0; i < m_packageList->count(); ++i) {
        QListWidgetItem *item = m_packageList->item(i);
        if (!item || !item->data(Qt::UserRole + 2).toBool())
            continue;
        const QString code = item->data(Qt::UserRole).toString();
        const QString path = QDir(tessdataTargetDir()).filePath(code + QStringLiteral(".traineddata"));
        if (QFileInfo::exists(path))
            QFile::remove(path);
    }
    refreshPackageStatus();
}

void SettingsDialog::loadSettings()
{
    QString defPath = defaultSaveDirectory();

    m_savePathEdit->setText(m_settings->value("savePath", defPath).toString());
    if (m_screenshotPathEdit)
        m_screenshotPathEdit->setText(m_settings->contains("screenshotSavePath")
            ? m_settings->value("screenshotSavePath").toString()
            : QDir(defPath).filePath(QStringLiteral("Screenshots")));
    if (m_gifPathEdit)
        m_gifPathEdit->setText(m_settings->contains("gifSavePath")
            ? m_settings->value("gifSavePath").toString()
            : QDir(defPath).filePath(QStringLiteral("GIFs")));
    if (m_videoPathEdit)
        m_videoPathEdit->setText(m_settings->contains("videoSavePath")
            ? m_settings->value("videoSavePath").toString()
            : QDir(defPath).filePath(QStringLiteral("Videos")));
    m_filenamePatternEdit->setText(m_settings->value("filenamePattern", "Screenshot_%Y-%M-%D_%h-%m-%s").toString());
    onFilenamePatternChanged(m_filenamePatternEdit->text());

#ifdef Q_OS_WIN
    m_autoStartCheck->setChecked(isAutoStartEnabled());
#else
    // Reflect the actual autostart desktop entry instead of a stale
    // QSettings flag.
    const QString autostartDesktopPath =
        QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
            .filePath(QStringLiteral("autostart/io.github.benoks.EShot.desktop"));
    m_autoStartCheck->setChecked(QFileInfo::exists(autostartDesktopPath));
#endif
    m_loadedAutoStart = m_autoStartCheck->isChecked();
    m_showNotificationsCheck->setChecked(m_settings->value("showNotifications", true).toBool());
    if (m_notifyCopyCheck) m_notifyCopyCheck->setChecked(m_settings->value("notifyCopy", false).toBool());
    if (m_notifySaveCheck) m_notifySaveCheck->setChecked(m_settings->value("notifySave", true).toBool());
    if (m_notifyGifCheck) m_notifyGifCheck->setChecked(m_settings->value("notifyGif", true).toBool());
    if (m_notifyVideoCheck) m_notifyVideoCheck->setChecked(m_settings->value("notifyVideo", true).toBool());
    if (m_notificationOpenFolderCheck) m_notificationOpenFolderCheck->setChecked(
        m_settings->value("notificationOpenFolder", true).toBool());
    if (m_notificationOptionsWidget) m_notificationOptionsWidget->setEnabled(m_showNotificationsCheck->isChecked());
    m_playSoundCheck->setChecked(m_settings->value("playSound", false).toBool());
    m_copyPathAfterSaveCheck->setChecked(m_settings->value("copyPathAfterSave", false).toBool());
    m_rememberSettingsWindowSizeEnabled = m_settings->value(
        "rememberSettingsWindowSize", false).toBool();
    if (m_rememberSettingsWindowSizeCheck)
        m_rememberSettingsWindowSizeCheck->setChecked(m_rememberSettingsWindowSizeEnabled);

    // Language (saved as int, convert to string)
    int langInt = m_settings->value("language",
                                    static_cast<int>(TranslationManager::currentLanguage())).toInt();
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
    if (m_instantCopyAfterSelectionCheck)
        m_instantCopyAfterSelectionCheck->setChecked(m_settings->value("instantCopyAfterSelection", false).toBool());
    if (m_rememberLastAnnotationToolCheck)
        m_rememberLastAnnotationToolCheck->setChecked(m_settings->value("rememberLastAnnotationTool", false).toBool());

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
    if (m_gifSizePresetCombo) {
        int idx = m_gifSizePresetCombo->findData(m_settings->value("recordingMaxSide", 1280).toInt());
        if (idx < 0) idx = m_gifSizePresetCombo->findData(1280);
        if (idx < 0) idx = 0;
        m_gifSizePresetCombo->setCurrentIndex(idx);
    }
    if (m_recordingStartDelaySpin)
        m_recordingStartDelaySpin->setValue(m_settings->value("recordingStartDelaySeconds", 0).toInt());
    if (m_videoFpsSpin)
        m_videoFpsSpin->setValue(m_settings->value("videoRecordingFps", 30).toInt());
    if (m_videoMaxSecSpin)
        m_videoMaxSecSpin->setValue(m_settings->value("videoRecordingMaxSeconds", 0).toInt());
    if (m_videoCrfSpin)
        m_videoCrfSpin->setValue(m_settings->value("videoRecordingCrf", 24).toInt());
    const bool videoDesktopAudio = loadRecordingAudioEnabled(
        *m_settings, RecordingAudioSource::Desktop);
    const bool videoMicrophoneAudio = loadRecordingAudioEnabled(
        *m_settings, RecordingAudioSource::Microphone);
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
    }

    bool jpeg = (fmt == "JPEG");
    m_qualitySlider->setEnabled(jpeg);
    m_qualitySpin->setEnabled(jpeg);

    const QSignalBlocker darkModeBlocker(m_darkModeCheck);
    const QSignalBlocker highContrastBlocker(m_highContrastCheck);
    m_darkModeCheck->setChecked(m_settings->value("darkMode", true).toBool());
    int opacity = m_settings->value("overlayOpacity", 100).toInt();
    m_opacitySlider->setValue(opacity);
    QString cross = m_settings->value("crosshairStyle", "dash").toString();
    int ci = m_crosshairStyleCombo->findData(cross);
    if (ci >= 0) m_crosshairStyleCombo->setCurrentIndex(ci);
    if (m_captureHintsCheck)
        m_captureHintsCheck->setChecked(m_settings->value("showCaptureHints", true).toBool());

    m_highContrastCheck->setChecked(m_settings->value("highContrast", false).toBool());
    if (m_blackTrayIconCheck)
        m_blackTrayIconCheck->setChecked(m_settings->value("blackTrayIcon", false).toBool());
    if (m_visualSearchProviderCombo) {
        const int index = m_visualSearchProviderCombo->findData(
            m_settings->value("visualSearchProvider", QStringLiteral("google")).toString());
        m_visualSearchProviderCombo->setCurrentIndex(index >= 0 ? index : 0);
    }

    QStringList visibleTools = m_settings->value("visibleTools", defaultAnnotationTools()).toStringList();
    QStringList visibleToolbarControls = m_settings->value("visibleToolbarControls", defaultToolbarControls()).toStringList();
    if (m_settings->contains("visibleToolbarControls") &&
        !m_settings->value("toolbarControlsMigratedVideo", false).toBool()) {
        if (!visibleToolbarControls.contains(QStringLiteral("Video")))
            visibleToolbarControls.append(QStringLiteral("Video"));
        // Migration flags are persisted in onSave so that merely opening the
        // dialog leaves no settings side effects.
    }
    if (m_settings->contains("visibleToolbarControls") &&
        !m_settings->value("toolbarControlsMigratedGoogleLens", false).toBool()) {
        if (!visibleToolbarControls.contains(QStringLiteral("GoogleLens")))
            visibleToolbarControls.append(QStringLiteral("GoogleLens"));
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
    if (m_instantCaptureHotkeyEdit)
        m_instantCaptureHotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(m_settings->value("instantCaptureHotkeyModifiers", 0).toUInt()),
            static_cast<UINT>(m_settings->value("instantCaptureHotkeyVKey", 0).toUInt())));
    if (m_gifCaptureHotkeyEdit)
        m_gifCaptureHotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(m_settings->value("gifCaptureHotkeyModifiers", 0).toUInt()),
            static_cast<UINT>(m_settings->value("gifCaptureHotkeyVKey", 0).toUInt())));
    if (m_videoCaptureHotkeyEdit)
        m_videoCaptureHotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(m_settings->value("videoCaptureHotkeyModifiers", 0).toUInt()),
            static_cast<UINT>(m_settings->value("videoCaptureHotkeyVKey", 0).toUInt())));
    if (m_windowCaptureHotkeyEdit)
        m_windowCaptureHotkeyEdit->setKeySequence(win32ToKeySequence(
            static_cast<UINT>(m_settings->value("windowCaptureHotkeyModifiers", defaultWindowCaptureModifiers()).toUInt()),
            static_cast<UINT>(m_settings->value("windowCaptureHotkeyVKey", defaultWindowCaptureVirtualKey()).toUInt())));
    for (const auto &def : overlayShortcutDefaults()) {
        if (QKeySequenceEdit *edit = m_overlayHotkeyEdits.value(def.key, nullptr)) {
            edit->setKeySequence(QKeySequence(m_settings->value(QStringLiteral("overlayShortcut/%1").arg(def.key),
                                                                def.defaultSequence).toString()));
        }
    }
    m_hotkeyStatusLabel->setText(TranslationManager::hotkeyValid());
    m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
    updatePrintScreenConflictUi();

    const QSize restoredSize = settingsDialogRestoredSize(
        m_rememberSettingsWindowSizeEnabled,
        m_settings->value("settingsWindowSize").toSize(), minimumSize(), maximumSize());
    if (restoredSize.isValid())
        resize(restoredSize);
}

void SettingsDialog::done(int result)
{
    if (result != QDialog::Accepted) {
        // Cancel/reject: revert any live theme preview back to the persisted
        // settings (theme changes are only persisted by onSave).
        applyEShotApplicationTheme(*qApp,
                                   m_settings->value("darkMode", true).toBool(),
                                   m_settings->value("highContrast", false).toBool());
    }
    // Only remember the window size when the dialog was actually accepted.
    if (result == QDialog::Accepted && m_rememberSettingsWindowSizeEnabled)
        m_settings->setValue("settingsWindowSize", size());
    QDialog::done(result);
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
        TranslationManager::printScreenConflictDisabled());
}

void SettingsDialog::onRequestLinuxPrintScreenBinding()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    if (desktop == LinuxDesktopEnvironment::Gnome) {
        if (HotkeyManager::instance().requestLinuxPortalShortcutRebind())
            return;

        const QString integrated = QDir::home().filePath(
            QStringLiteral(".local/opt/EShot/EShot.AppImage"));
        const QString executable = LinuxGnomeShortcutInstaller::preferredExecutable(
            qEnvironmentVariable("APPIMAGE"), QCoreApplication::applicationFilePath(),
            QFileInfo::exists(integrated) ? integrated : QString());
        const auto installed = LinuxGnomeShortcutInstaller::installPrintScreen(
            LinuxGnomeShortcutInstaller::captureCommand(executable));
        if (installed.success) {
            QMessageBox::information(this, QStringLiteral("EShot"),
                                     uiLabel("PrintScreen GNOME'da EShot'a atandi.",
                                             "PrintScreen was assigned to EShot in GNOME."));
        } else {
            QMessageBox::warning(this, QStringLiteral("EShot"),
                                 uiLabel("GNOME kisayolu ayarlanamadi: ",
                                         "Could not configure the GNOME shortcut: ")
                                     + installed.error);
        }
        return;
    }

    HotkeyManager::instance().requestLinuxPortalShortcutRebind();
    if (desktop == LinuxDesktopEnvironment::Kde
        && !QProcess::startDetached(QStringLiteral("kcmshell6"), {QStringLiteral("kcm_keys")})) {
        QMessageBox::warning(this, QStringLiteral("EShot"),
                             uiLabel("KDE kisayol ayarlari acilamadi.",
                                     "KDE shortcut settings could not be opened."));
    }
#endif
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
    auto validatedOptionalPath = [this](QLineEdit *edit) -> QString {
        if (!edit)
            return QString();
        const QString path = edit->text().trimmed();
        if (path.isEmpty())
            return QString();
        QDir dir(path);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::errSaveDir() + path);
            return QStringLiteral("__invalid__");
        }
        return path;
    };
    const QString screenshotPath = validatedOptionalPath(m_screenshotPathEdit);
    if (screenshotPath == QStringLiteral("__invalid__"))
        return;
    const QString gifPath = validatedOptionalPath(m_gifPathEdit);
    if (gifPath == QStringLiteral("__invalid__"))
        return;
    const QString videoPath = validatedOptionalPath(m_videoPathEdit);
    if (videoPath == QStringLiteral("__invalid__"))
        return;

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

    const bool captureHotkeyChanged = settingsHotkeyChanged(
        {newMod, newVKey},
        {static_cast<quint32>(m_settings->value("hotkeyModifiers", 0).toUInt()),
         static_cast<quint32>(m_settings->value("hotkeyVKey", VK_SNAPSHOT).toUInt())});
    if (captureHotkeyChanged) {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
            qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
            qEnvironmentVariable("XDG_SESSION_DESKTOP"));
        if (desktop == LinuxDesktopEnvironment::Gnome
            && !HotkeyManager::instance().linuxPortalShortcutsAvailable()) {
            const QString integrated = QDir::home().filePath(
                QStringLiteral(".local/opt/EShot/EShot.AppImage"));
            const QString executable = LinuxGnomeShortcutInstaller::preferredExecutable(
                qEnvironmentVariable("APPIMAGE"), QCoreApplication::applicationFilePath(),
                QFileInfo::exists(integrated) ? integrated : QString());
            const QString binding = LinuxGnomeShortcutInstaller::acceleratorFromPortableSequence(
                seq.toString(QKeySequence::PortableText));
            const auto installed = LinuxGnomeShortcutInstaller::installCaptureShortcut(
                LinuxGnomeShortcutInstaller::captureCommand(executable), binding);
            if (!installed.success) {
                QMessageBox::warning(
                    this, TranslationManager::errInvalidHotkeyTitle(),
                    uiLabel("GNOME yakalama kisayolu ayarlanamadi: ",
                            "Could not configure the GNOME capture shortcut: ")
                        + installed.error);
                return;
            }
        }
#endif
    }
    if (captureHotkeyChanged && !HotkeyManager::instance().reRegisterCaptureHotkey(newMod, newVKey)) {
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            TranslationManager::errInvalidHotkey() + QStringLiteral("\n\n") + TranslationManager::hotkeyMayBeInUse());
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
    const bool recordingHotkeysChanged =
        settingsHotkeyChanged({pauseMod, pauseVKey},
                              {static_cast<quint32>(m_settings->value("recordingPauseHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt()),
                               static_cast<quint32>(m_settings->value("recordingPauseHotkeyVKey", 'P').toUInt())}) ||
        settingsHotkeyChanged({stopMod, stopVKey},
                              {static_cast<quint32>(m_settings->value("recordingStopHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt()),
                               static_cast<quint32>(m_settings->value("recordingStopHotkeyVKey", 'S').toUInt())}) ||
        settingsHotkeyChanged({cancelMod, cancelVKey},
                              {static_cast<quint32>(m_settings->value("recordingCancelHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt()),
                               static_cast<quint32>(m_settings->value("recordingCancelHotkeyVKey", 'X').toUInt())});
    if (recordingHotkeysChanged && !HotkeyManager::instance().reRegisterRecordingHotkeys(pauseMod, pauseVKey, stopMod, stopVKey, cancelMod, cancelVKey)) {
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            TranslationManager::errInvalidHotkey() + QStringLiteral("\n\n") + TranslationManager::recordingHotkeyMayBeInUse());
        return;
    }

    auto optionalHotkey = [](QKeySequenceEdit *edit, UINT &mod, UINT &vkey) {
        mod = 0;
        vkey = 0;
        if (!edit || edit->keySequence().isEmpty())
            return true;
        return keySequenceToWin32(edit->keySequence(), mod, vkey);
    };
    UINT instantMod = 0, instantVKey = 0;
    UINT gifMod = 0, gifVKey = 0;
    UINT videoMod = 0, videoVKey = 0;
    UINT windowMod = 0, windowVKey = 0;
    if (!optionalHotkey(m_instantCaptureHotkeyEdit, instantMod, instantVKey) ||
        !optionalHotkey(m_gifCaptureHotkeyEdit, gifMod, gifVKey) ||
        !optionalHotkey(m_videoCaptureHotkeyEdit, videoMod, videoVKey) ||
        !optionalHotkey(m_windowCaptureHotkeyEdit, windowMod, windowVKey)) {
        QMessageBox::warning(this, TranslationManager::errInvalidHotkeyTitle(), TranslationManager::errInvalidHotkey());
        return;
    }
    const bool actionHotkeysChanged =
        settingsHotkeyChanged({instantMod, instantVKey},
                              {static_cast<quint32>(m_settings->value("instantCaptureHotkeyModifiers", 0).toUInt()),
                               static_cast<quint32>(m_settings->value("instantCaptureHotkeyVKey", 0).toUInt())}) ||
        settingsHotkeyChanged({gifMod, gifVKey},
                              {static_cast<quint32>(m_settings->value("gifCaptureHotkeyModifiers", 0).toUInt()),
                               static_cast<quint32>(m_settings->value("gifCaptureHotkeyVKey", 0).toUInt())}) ||
        settingsHotkeyChanged({videoMod, videoVKey},
                              {static_cast<quint32>(m_settings->value("videoCaptureHotkeyModifiers", 0).toUInt()),
                               static_cast<quint32>(m_settings->value("videoCaptureHotkeyVKey", 0).toUInt())}) ||
        settingsHotkeyChanged({windowMod, windowVKey},
                              {static_cast<quint32>(m_settings->value("windowCaptureHotkeyModifiers", defaultWindowCaptureModifiers()).toUInt()),
                               static_cast<quint32>(m_settings->value("windowCaptureHotkeyVKey", defaultWindowCaptureVirtualKey()).toUInt())});
    if (actionHotkeysChanged && !HotkeyManager::instance().reRegisterActionHotkeys(
            instantMod, instantVKey, gifMod, gifVKey, videoMod, videoVKey,
            windowMod, windowVKey)) {
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            TranslationManager::errInvalidHotkey() + QStringLiteral("\n\n") + TranslationManager::directCaptureHotkeyMayBeInUse());
        return;
    }

    // Apply the autostart change before persisting anything: if the task
    // registration fails, no partial settings are written (onSave partial
    // success guard).
    if (m_autoStartCheck->isChecked() != m_loadedAutoStart && !setAutoStartTask(m_autoStartCheck->isChecked())) {
        QMessageBox::warning(this, TranslationManager::errTitle(),
                             TranslationManager::autoStartSaveFailed());
        return;
    }
    m_loadedAutoStart = m_autoStartCheck->isChecked();

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

    m_settings->setValue("savePath",          savePath);
    m_settings->setValue("screenshotSavePath", screenshotPath);
    m_settings->setValue("gifSavePath",        gifPath);
    m_settings->setValue("videoSavePath",      videoPath);
    m_settings->setValue("filenamePattern",    m_filenamePatternEdit->text());
    m_settings->setValue("autoStart",          m_autoStartCheck->isChecked());
    m_settings->setValue("showNotifications",  m_showNotificationsCheck->isChecked());
    if (m_notifyCopyCheck) m_settings->setValue("notifyCopy", m_notifyCopyCheck->isChecked());
    if (m_notifySaveCheck) m_settings->setValue("notifySave", m_notifySaveCheck->isChecked());
    if (m_notifyGifCheck) m_settings->setValue("notifyGif", m_notifyGifCheck->isChecked());
    if (m_notifyVideoCheck) m_settings->setValue("notifyVideo", m_notifyVideoCheck->isChecked());
    if (m_notificationOpenFolderCheck) m_settings->setValue(
        "notificationOpenFolder", m_notificationOpenFolderCheck->isChecked());
    m_settings->setValue("playSound",          m_playSoundCheck->isChecked());
    m_settings->setValue("copyPathAfterSave",  m_copyPathAfterSaveCheck->isChecked());
    m_rememberSettingsWindowSizeEnabled = m_rememberSettingsWindowSizeCheck
        && m_rememberSettingsWindowSizeCheck->isChecked();
    m_settings->setValue("rememberSettingsWindowSize", m_rememberSettingsWindowSizeEnabled);
    if (m_rememberSettingsWindowSizeEnabled)
        m_settings->setValue("settingsWindowSize", size());
    else
        m_settings->remove("settingsWindowSize");

    m_settings->setValue("imageFormat",        m_formatCombo->currentData().toString());
    m_settings->setValue("imageQuality",       m_qualitySpin->value());
    m_settings->setValue("captureDelay",       m_delaySpin->value());
    m_settings->setValue("copyAfterCapture",   false);
    m_settings->setValue("closeAfterCopy",     m_closeAfterCopyCheck->isChecked());
    m_settings->setValue("instantCopyAfterSelection",
                         m_instantCopyAfterSelectionCheck ? m_instantCopyAfterSelectionCheck->isChecked() : false);
    m_settings->setValue("rememberLastAnnotationTool",
                         m_rememberLastAnnotationToolCheck ? m_rememberLastAnnotationToolCheck->isChecked() : false);

    m_settings->setValue("darkMode",           m_darkModeCheck->isChecked());
    m_settings->setValue("overlayOpacity",     m_opacitySlider->value());
    m_settings->setValue("crosshairStyle",     m_crosshairStyleCombo->currentData().toString());
    if (m_captureHintsCheck)
        m_settings->setValue("showCaptureHints", m_captureHintsCheck->isChecked());
    m_settings->setValue("highContrast",       m_highContrastCheck->isChecked());
    m_settings->setValue("blackTrayIcon",      m_blackTrayIconCheck ? m_blackTrayIconCheck->isChecked() : false);
    if (m_visualSearchProviderCombo)
        m_settings->setValue("visualSearchProvider", m_visualSearchProviderCombo->currentData().toString());

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
    if (!m_settings->value("toolbarControlsMigratedVideo", false).toBool())
        m_settings->setValue("toolbarControlsMigratedVideo", true);
    if (!m_settings->value("toolbarControlsMigratedGoogleLens", false).toBool())
        m_settings->setValue("toolbarControlsMigratedGoogleLens", true);

    m_settings->setValue("hotkeyModifiers", newMod);
    m_settings->setValue("hotkeyVKey",      newVKey);
    m_settings->setValue("recordingPauseHotkeyModifiers", pauseMod);
    m_settings->setValue("recordingPauseHotkeyVKey",      pauseVKey);
    m_settings->setValue("recordingStopHotkeyModifiers",  stopMod);
    m_settings->setValue("recordingStopHotkeyVKey",       stopVKey);
    m_settings->setValue("recordingCancelHotkeyModifiers", cancelMod);
    m_settings->setValue("recordingCancelHotkeyVKey",      cancelVKey);
    m_settings->setValue("instantCaptureHotkeyModifiers", instantMod);
    m_settings->setValue("instantCaptureHotkeyVKey",      instantVKey);
    m_settings->setValue("gifCaptureHotkeyModifiers",     gifMod);
    m_settings->setValue("gifCaptureHotkeyVKey",          gifVKey);
    m_settings->setValue("videoCaptureHotkeyModifiers",   videoMod);
    m_settings->setValue("videoCaptureHotkeyVKey",        videoVKey);
    m_settings->setValue("windowCaptureHotkeyModifiers", windowMod);
    m_settings->setValue("windowCaptureHotkeyVKey",      windowVKey);
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
    if (m_gifSizePresetCombo)
        m_settings->setValue("recordingMaxSide", m_gifSizePresetCombo->currentData().toInt());
    if (m_recordingStartDelaySpin)
        m_settings->setValue("recordingStartDelaySeconds", m_recordingStartDelaySpin->value());
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
    m_settings->setValue("videoDesktopAudioDevice", defaultDesktopAudioDevice());
    m_settings->setValue("videoMicrophoneDevice",
                         m_videoMicrophoneDeviceCombo ? m_videoMicrophoneDeviceCombo->currentData().toString() : QStringLiteral("default"));
    m_settings->setValue("videoAudioMode", desktopAudio && microphoneAudio ? QStringLiteral("both")
        : desktopAudio ? QStringLiteral("desktop")
        : microphoneAudio ? QStringLiteral("microphone")
        : QStringLiteral("none"));

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
        // clear() wiped the hotkey config; restore the default PrintScreen
        // binding so capture keeps working without reopening Settings.
        HotkeyManager::instance().reRegisterCaptureHotkey(0, VK_SNAPSHOT);
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
    obj["screenshotSavePath"] = m_screenshotPathEdit ? m_screenshotPathEdit->text() : QString();
    obj["gifSavePath"] = m_gifPathEdit ? m_gifPathEdit->text() : QString();
    obj["videoSavePath"] = m_videoPathEdit ? m_videoPathEdit->text() : QString();
    obj["filenamePattern"] = m_filenamePatternEdit->text();
    obj["autoStart"] = m_autoStartCheck->isChecked();
    obj["showNotifications"] = m_showNotificationsCheck->isChecked();
    obj["notifyCopy"] = m_notifyCopyCheck ? m_notifyCopyCheck->isChecked() : false;
    obj["notifySave"] = m_notifySaveCheck ? m_notifySaveCheck->isChecked() : true;
    obj["notifyGif"] = m_notifyGifCheck ? m_notifyGifCheck->isChecked() : true;
    obj["notifyVideo"] = m_notifyVideoCheck ? m_notifyVideoCheck->isChecked() : true;
    obj["notificationOpenFolder"] = m_notificationOpenFolderCheck
        ? m_notificationOpenFolderCheck->isChecked() : true;
    obj["playSound"] = m_playSoundCheck->isChecked();
    obj["copyPathAfterSave"] = m_copyPathAfterSaveCheck->isChecked();
    obj["imageFormat"] = m_formatCombo->currentData().toString();
    obj["imageQuality"] = m_qualitySpin->value();
    obj["captureDelay"] = m_delaySpin->value();
    obj["copyAfterCapture"] = false;
    obj["closeAfterCopy"] = m_closeAfterCopyCheck->isChecked();
    obj["instantCopyAfterSelection"] = m_instantCopyAfterSelectionCheck ? m_instantCopyAfterSelectionCheck->isChecked() : false;
    obj["rememberLastAnnotationTool"] = m_rememberLastAnnotationToolCheck ? m_rememberLastAnnotationToolCheck->isChecked() : false;
    obj["darkMode"] = m_darkModeCheck->isChecked();
    obj["overlayOpacity"] = m_opacitySlider->value();
    obj["crosshairStyle"] = m_crosshairStyleCombo->currentData().toString();
    obj["showCaptureHints"] = m_captureHintsCheck ? m_captureHintsCheck->isChecked() : true;
    obj["highContrast"] = m_highContrastCheck->isChecked();
    obj["blackTrayIcon"] = m_blackTrayIconCheck ? m_blackTrayIconCheck->isChecked() : false;

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
    appendHotkey("instantCaptureHotkeyModifiers", "instantCaptureHotkeyVKey", m_instantCaptureHotkeyEdit);
    appendHotkey("gifCaptureHotkeyModifiers", "gifCaptureHotkeyVKey", m_gifCaptureHotkeyEdit);
    appendHotkey("videoCaptureHotkeyModifiers", "videoCaptureHotkeyVKey", m_videoCaptureHotkeyEdit);
    appendHotkey("windowCaptureHotkeyModifiers", "windowCaptureHotkeyVKey", m_windowCaptureHotkeyEdit);
    obj["uploadProvider"] = m_settings->value("uploadProvider", 0).toInt();
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
    if (m_gifSizePresetCombo)
        obj["recordingMaxSide"] = m_gifSizePresetCombo->currentData().toInt();
    if (m_recordingStartDelaySpin)
        obj["recordingStartDelaySeconds"] = m_recordingStartDelaySpin->value();
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
    if (m_screenshotPathEdit && obj.contains("screenshotSavePath")) m_screenshotPathEdit->setText(obj["screenshotSavePath"].toString());
    if (m_gifPathEdit && obj.contains("gifSavePath")) m_gifPathEdit->setText(obj["gifSavePath"].toString());
    if (m_videoPathEdit && obj.contains("videoSavePath")) m_videoPathEdit->setText(obj["videoSavePath"].toString());
    if (obj.contains("filenamePattern")) m_filenamePatternEdit->setText(obj["filenamePattern"].toString());
    if (obj.contains("autoStart")) m_autoStartCheck->setChecked(obj["autoStart"].toBool());
    if (obj.contains("showNotifications")) m_showNotificationsCheck->setChecked(obj["showNotifications"].toBool());
    if (m_notifyCopyCheck && obj.contains("notifyCopy")) m_notifyCopyCheck->setChecked(obj["notifyCopy"].toBool());
    if (m_notifySaveCheck && obj.contains("notifySave")) m_notifySaveCheck->setChecked(obj["notifySave"].toBool());
    if (m_notifyGifCheck && obj.contains("notifyGif")) m_notifyGifCheck->setChecked(obj["notifyGif"].toBool());
    if (m_notifyVideoCheck && obj.contains("notifyVideo")) m_notifyVideoCheck->setChecked(obj["notifyVideo"].toBool());
    if (m_notificationOpenFolderCheck && obj.contains("notificationOpenFolder")) {
        m_notificationOpenFolderCheck->setChecked(obj["notificationOpenFolder"].toBool());
    }
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
    if (m_instantCopyAfterSelectionCheck && obj.contains("instantCopyAfterSelection"))
        m_instantCopyAfterSelectionCheck->setChecked(obj["instantCopyAfterSelection"].toBool());
    if (m_rememberLastAnnotationToolCheck && obj.contains("rememberLastAnnotationTool"))
        m_rememberLastAnnotationToolCheck->setChecked(obj["rememberLastAnnotationTool"].toBool());
    if (obj.contains("darkMode")) m_darkModeCheck->setChecked(obj["darkMode"].toBool());
    if (obj.contains("overlayOpacity")) m_opacitySlider->setValue(obj["overlayOpacity"].toInt());
    if (obj.contains("crosshairStyle")) {
        int ci = m_crosshairStyleCombo->findData(obj["crosshairStyle"].toString());
        if (ci >= 0) m_crosshairStyleCombo->setCurrentIndex(ci);
    }
    if (obj.contains("showCaptureHints") && m_captureHintsCheck)
        m_captureHintsCheck->setChecked(obj["showCaptureHints"].toBool());
    if (obj.contains("highContrast")) m_highContrastCheck->setChecked(obj["highContrast"].toBool());
    if (obj.contains("blackTrayIcon") && m_blackTrayIconCheck)
        m_blackTrayIconCheck->setChecked(obj["blackTrayIcon"].toBool());
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
    importHotkey("instantCaptureHotkeyModifiers", "instantCaptureHotkeyVKey", m_instantCaptureHotkeyEdit);
    importHotkey("gifCaptureHotkeyModifiers", "gifCaptureHotkeyVKey", m_gifCaptureHotkeyEdit);
    importHotkey("videoCaptureHotkeyModifiers", "videoCaptureHotkeyVKey", m_videoCaptureHotkeyEdit);
    importHotkey("windowCaptureHotkeyModifiers", "windowCaptureHotkeyVKey", m_windowCaptureHotkeyEdit);
    if (obj.contains("uploadProvider"))
        m_settings->setValue("uploadProvider", obj["uploadProvider"].toInt());
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
    if (m_gifSizePresetCombo && obj.contains("recordingMaxSide")) {
        int idx = m_gifSizePresetCombo->findData(obj["recordingMaxSide"].toInt());
        if (idx < 0) idx = m_gifSizePresetCombo->findData(1280);
        if (idx < 0) idx = 0;
        m_gifSizePresetCombo->setCurrentIndex(idx);
    }
    if (m_recordingStartDelaySpin && obj.contains("recordingStartDelaySeconds"))
        m_recordingStartDelaySpin->setValue(obj["recordingStartDelaySeconds"].toInt());
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
    applyEShotApplicationTheme(*qApp, m_darkModeCheck->isChecked(),
                               m_highContrastCheck->isChecked());
}
