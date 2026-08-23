#include "HotkeyManager.h"
#include "LinuxDesktopIntegration.h"
#include "LinuxGnomeShortcutInstaller.h"
#include "LinuxPortalGlobalShortcuts.h"
#include "LinuxKGlobalAccelShortcuts.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QTimer>

#if defined(ESHOT_HAVE_X11)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

namespace {
#if defined(ESHOT_HAVE_X11)
KeySym keySymForVirtualKey(UINT virtualKey)
{
    if (virtualKey >= 'A' && virtualKey <= 'Z')
        return XK_A + (virtualKey - 'A');
    if (virtualKey >= '0' && virtualKey <= '9')
        return XK_0 + (virtualKey - '0');
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24)
        return XK_F1 + (virtualKey - VK_F1);
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9)
        return XK_KP_0 + (virtualKey - VK_NUMPAD0);

    switch (virtualKey) {
    case VK_SNAPSHOT: return XK_Print;
    case VK_SCROLL: return XK_Scroll_Lock;
    case VK_PAUSE: return XK_Pause;
    case VK_CAPITAL: return XK_Caps_Lock;
    case VK_NUMLOCK: return XK_Num_Lock;
    case VK_APPS: return XK_Menu;
    case VK_HOME: return XK_Home;
    case VK_END: return XK_End;
    case VK_PRIOR: return XK_Page_Up;
    case VK_NEXT: return XK_Page_Down;
    case VK_INSERT: return XK_Insert;
    case VK_DELETE: return XK_Delete;
    case VK_LEFT: return XK_Left;
    case VK_RIGHT: return XK_Right;
    case VK_UP: return XK_Up;
    case VK_DOWN: return XK_Down;
    case VK_SPACE: return XK_space;
    case VK_RETURN: return XK_Return;
    case VK_ESCAPE: return XK_Escape;
    case VK_TAB: return XK_Tab;
    case VK_BACK: return XK_BackSpace;
    case VK_ADD: return XK_KP_Add;
    case VK_SUBTRACT: return XK_KP_Subtract;
    case VK_MULTIPLY: return XK_KP_Multiply;
    case VK_DIVIDE: return XK_KP_Divide;
    case VK_DECIMAL: return XK_KP_Decimal;
    default: return NoSymbol;
    }
}

unsigned int x11ModifiersForHotkey(UINT modifiers)
{
    unsigned int mask = 0;
    if (modifiers & MOD_SHIFT) mask |= ShiftMask;
    if (modifiers & MOD_CONTROL) mask |= ControlMask;
    if (modifiers & MOD_ALT) mask |= Mod1Mask;
    if (modifiers & MOD_WIN) mask |= Mod4Mask;
    return mask;
}

QList<unsigned int> lockModifierVariants(unsigned int base)
{
    return {
        base,
        base | LockMask,
        base | Mod2Mask,
        base | LockMask | Mod2Mask,
        base | Mod5Mask,
        base | LockMask | Mod5Mask,
        base | Mod2Mask | Mod5Mask,
        base | LockMask | Mod2Mask | Mod5Mask
    };
}

// Set while the scoped error handler below observes BadAccess, i.e. another
// client already owns the requested passive grab.
bool x11GrabFailed = false;

int x11HotkeyErrorHandler(Display *, XErrorEvent *error)
{
    if (error && error->error_code == BadAccess)
        x11GrabFailed = true;
    return 0;
}
#endif
}

HotkeyManager& HotkeyManager::instance()
{
    static HotkeyManager inst;
    return inst;
}

QString HotkeyManager::shortcutText(UINT modifiers, UINT virtualKey)
{
    QString portable = LinuxPortalGlobalShortcuts::preferredTrigger(modifiers, virtualKey);
    // QKeySequence knows "Meta" but not the portal-style "SUPER".
    portable.replace(QStringLiteral("SUPER"), QStringLiteral("Meta"));
    return QKeySequence::fromString(portable, QKeySequence::PortableText)
        .toString(QKeySequence::NativeText);
}

QString HotkeyManager::recordingPauseShortcutText() const
{
    return shortcutText(m_recordingPauseModifiers, m_recordingPauseVirtualKey);
}

QString HotkeyManager::captureShortcutText() const
{
    return shortcutText(m_captureModifiers, m_captureVirtualKey);
}

QString HotkeyManager::windowCaptureShortcutText() const
{
    return shortcutText(m_windowCaptureModifiers, m_windowCaptureVirtualKey);
}

QString HotkeyManager::recordingStopShortcutText() const
{
    return shortcutText(m_recordingStopModifiers, m_recordingStopVirtualKey);
}

QString HotkeyManager::recordingCancelShortcutText() const
{
    return shortcutText(m_recordingCancelModifiers, m_recordingCancelVirtualKey);
}

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent)
{
    qApp->installNativeEventFilter(this);

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP",
                                                  qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    const LinuxDesktopEnvironment desktopEnvironment = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    if (LinuxKGlobalAccelShortcuts::isKdeDesktop(desktop)) {
        m_kdeShortcuts = new LinuxKGlobalAccelShortcuts(this);
        if (m_kdeShortcuts->isAvailable()) {
            connect(m_kdeShortcuts, &LinuxKGlobalAccelShortcuts::shortcutActivated,
                    this, &HotkeyManager::emitHotkey);
        }
    }
    m_portalShortcuts = new LinuxPortalGlobalShortcuts(this);
    const bool kdeAvailable = m_kdeShortcuts && m_kdeShortcuts->isAvailable();
    const bool portalAvailable = m_portalShortcuts->isAvailable();
    connect(m_portalShortcuts, &LinuxPortalGlobalShortcuts::shortcutActivated,
            this, &HotkeyManager::emitHotkey, Qt::UniqueConnection);
    connect(m_portalShortcuts, &LinuxPortalGlobalShortcuts::portalFailed,
            this, [this](const QString &, const QString &) {
                activateGnomeShortcutFallback();
            });
    m_usePortalShortcuts = !kdeAvailable && portalAvailable;
    m_useGnomeShortcutFallback = LinuxDesktopIntegration::useGnomeShortcutFallback(
        desktopEnvironment, portalAvailable);
    bool x11Available = false;
#if defined(ESHOT_HAVE_X11)
    const bool useX11Hotkeys = QGuiApplication::platformName().contains(
        QStringLiteral("xcb"), Qt::CaseInsensitive);
    if (!kdeAvailable && !portalAvailable && useX11Hotkeys
        && !m_useGnomeShortcutFallback)
        m_x11Display = XOpenDisplay(nullptr);
    x11Available = m_x11Display != nullptr;
#endif

    const LinuxHotkeyBackend backend = LinuxPortalGlobalShortcuts::preferredBackend(
        portalAvailable, x11Available);
    if (!kdeAvailable && backend == LinuxHotkeyBackend::Portal)
        m_usePortalShortcuts = true;
#if defined(ESHOT_HAVE_X11)
    else if (backend == LinuxHotkeyBackend::X11) {
        m_x11RootWindow = DefaultRootWindow(static_cast<Display *>(m_x11Display));
        QTimer *pollTimer = new QTimer(this);
        pollTimer->setInterval(40);
        connect(pollTimer, &QTimer::timeout, this, [this]() {
            Display *display = static_cast<Display *>(m_x11Display);
            if (!display)
                return;
            while (XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.type != KeyPress)
                    continue;
                const unsigned int keycode = event.xkey.keycode;
                const unsigned int state = event.xkey.state & ~(LockMask | Mod2Mask | Mod5Mask);
                for (auto it = m_registeredHotkeyDefs.constBegin(); it != m_registeredHotkeyDefs.constEnd(); ++it) {
                    const UINT modifiers = it.value().first;
                    const UINT virtualKey = it.value().second;
                    const KeySym sym = keySymForVirtualKey(virtualKey);
                    if (sym == NoSymbol)
                        continue;
                    const KeyCode registeredCode = XKeysymToKeycode(display, sym);
                    if (registeredCode == keycode && x11ModifiersForHotkey(modifiers) == state) {
                        emitHotkey(it.key());
                        break;
                    }
                }
            }
        });
        pollTimer->start();
    }
#endif
    if (!kdeAvailable && backend == LinuxHotkeyBackend::Unavailable
        && !m_useGnomeShortcutFallback)
        qWarning() << "[HotkeyManager] GlobalShortcuts portal and X11 fallback are unavailable; global hotkeys are disabled.";
#endif

    QSettings s("EShot", "EShot");
    UINT modifiers = static_cast<UINT>(s.value("hotkeyModifiers", 0).toUInt());
    UINT vkey      = static_cast<UINT>(s.value("hotkeyVKey", VK_SNAPSHOT).toUInt());

    if (registerHotkey(HOTKEY_CAPTURE, modifiers, vkey)) {
        m_captureModifiers = modifiers;
        m_captureVirtualKey = vkey;
        qDebug() << "[HotkeyManager] Capture hotkey registered (mod=" << modifiers << " vk=" << vkey << ")";
    } else {
        qWarning() << "[HotkeyManager] Failed to register hotkey (mod=" << modifiers << " vk=" << vkey << "). Trying default...";
        // Try default PrtSc as well
        if (!registerHotkey(HOTKEY_CAPTURE, 0, VK_SNAPSHOT)) {
            qWarning() << "[HotkeyManager] Default hotkey also failed. User must change hotkey in settings.";
        } else {
            m_captureModifiers = 0;
            m_captureVirtualKey = VK_SNAPSHOT;
        }
    }

    m_recordingPauseModifiers = static_cast<UINT>(s.value("recordingPauseHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt());
    m_recordingPauseVirtualKey = static_cast<UINT>(s.value("recordingPauseHotkeyVKey", 'P').toUInt());
    m_recordingStopModifiers = static_cast<UINT>(s.value("recordingStopHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt());
    m_recordingStopVirtualKey = static_cast<UINT>(s.value("recordingStopHotkeyVKey", 'S').toUInt());
    m_recordingCancelModifiers = static_cast<UINT>(s.value("recordingCancelHotkeyModifiers", MOD_CONTROL | MOD_ALT).toUInt());
    m_recordingCancelVirtualKey = static_cast<UINT>(s.value("recordingCancelHotkeyVKey", 'X').toUInt());
    registerHotkey(HOTKEY_RECORDING_PAUSE, m_recordingPauseModifiers, m_recordingPauseVirtualKey);
    registerHotkey(HOTKEY_RECORDING_STOP, m_recordingStopModifiers, m_recordingStopVirtualKey);
    registerHotkey(HOTKEY_RECORDING_CANCEL, m_recordingCancelModifiers, m_recordingCancelVirtualKey);

    m_instantCaptureModifiers = static_cast<UINT>(s.value("instantCaptureHotkeyModifiers", 0).toUInt());
    m_instantCaptureVirtualKey = static_cast<UINT>(s.value("instantCaptureHotkeyVKey", 0).toUInt());
    m_gifCaptureModifiers = static_cast<UINT>(s.value("gifCaptureHotkeyModifiers", 0).toUInt());
    m_gifCaptureVirtualKey = static_cast<UINT>(s.value("gifCaptureHotkeyVKey", 0).toUInt());
    m_videoCaptureModifiers = static_cast<UINT>(s.value("videoCaptureHotkeyModifiers", 0).toUInt());
    m_videoCaptureVirtualKey = static_cast<UINT>(s.value("videoCaptureHotkeyVKey", 0).toUInt());
#ifdef Q_OS_WIN
    m_windowCaptureModifiers = static_cast<UINT>(s.value("windowCaptureHotkeyModifiers", MOD_SHIFT).toUInt());
    m_windowCaptureVirtualKey = static_cast<UINT>(s.value("windowCaptureHotkeyVKey", VK_SNAPSHOT).toUInt());
#endif
    registerHotkey(HOTKEY_INSTANT_CAPTURE, m_instantCaptureModifiers, m_instantCaptureVirtualKey);
    registerHotkey(HOTKEY_GIF_CAPTURE, m_gifCaptureModifiers, m_gifCaptureVirtualKey);
    registerHotkey(HOTKEY_VIDEO_CAPTURE, m_videoCaptureModifiers, m_videoCaptureVirtualKey);
    if (!registerHotkey(HOTKEY_WINDOW_CAPTURE, m_windowCaptureModifiers, m_windowCaptureVirtualKey))
        qWarning() << "[HotkeyManager] Window capture hotkey could not be registered";
}

bool HotkeyManager::requestLinuxPortalShortcutRebind()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return m_portalShortcuts && m_portalShortcuts->requestRebind();
#else
    return false;
#endif
}

bool HotkeyManager::linuxPortalShortcutsAvailable() const
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return m_portalShortcuts && m_portalShortcuts->isAvailable();
#else
    return false;
#endif
}

HotkeyManager::~HotkeyManager()
{
    unregisterAllHotkeys();
#if defined(ESHOT_HAVE_X11)
    if (m_x11Display) {
        XCloseDisplay(static_cast<Display *>(m_x11Display));
        m_x11Display = nullptr;
        m_x11RootWindow = 0;
    }
#endif
    qApp->removeNativeEventFilter(this);
}

bool HotkeyManager::registerHotkey(int id, UINT modifiers, UINT virtualKey)
{
    if (virtualKey == 0)
        return true;
#ifdef Q_OS_WIN
    if (RegisterHotKey(nullptr, id, modifiers, virtualKey)) {
        if (!m_registeredHotkeys.contains(id))
            m_registeredHotkeys.append(id);
        m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
        return true;
    }
    return false;
#elif defined(ESHOT_HAVE_X11)
    if (m_useGnomeShortcutFallback) {
        if (id != HOTKEY_CAPTURE)
            return false;
        const auto previous = m_registeredHotkeyDefs;
        if (!m_registeredHotkeys.contains(id))
            m_registeredHotkeys.append(id);
        m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
        if (activateGnomeShortcutFallback())
            return true;
        m_registeredHotkeyDefs = previous;
        m_registeredHotkeys.removeAll(id);
        return false;
    }
    if (m_kdeShortcuts && m_kdeShortcuts->isAvailable() && !m_usePortalShortcuts) {
        const auto previous = m_registeredHotkeyDefs;
        if (!m_registeredHotkeys.contains(id)) m_registeredHotkeys.append(id);
        m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
        if (m_kdeShortcuts->setShortcuts(m_registeredHotkeyDefs)) return true;
        if (id == HOTKEY_CAPTURE && previous.isEmpty()
            && LinuxKGlobalAccelShortcuts::shouldUsePortalFallback(
                false, m_portalShortcuts && m_portalShortcuts->isAvailable())) {
            qWarning() << "[HotkeyManager] KGlobalAccel could not claim PrintScreen;"
                          " falling back to the GlobalShortcuts portal.";
            m_usePortalShortcuts = true;
            refreshPortalShortcuts();
            return true;
        }
        m_registeredHotkeyDefs = previous;
        m_registeredHotkeys.removeAll(id);
        // Push the previous definitions back so partially applied
        // registrations do not leave ghost entries in kglobalaccel.
        if (!previous.isEmpty())
            m_kdeShortcuts->setShortcuts(previous);
        else
            m_kdeShortcuts->deRegisterShortcut(id);
        return false;
    }
    Display *display = static_cast<Display *>(m_x11Display);
    if (!display || m_x11RootWindow == 0) {
        if (m_portalShortcuts && m_portalShortcuts->isAvailable()) {
            if (!m_registeredHotkeys.contains(id))
                m_registeredHotkeys.append(id);
            m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
            refreshPortalShortcuts();
            return true;
        }
        return false;
    }

    const KeySym sym = keySymForVirtualKey(virtualKey);
    if (sym == NoSymbol)
        return false;
    const KeyCode keycode = XKeysymToKeycode(display, sym);
    if (keycode == 0)
        return false;

    const unsigned int baseModifiers = x11ModifiersForHotkey(modifiers);
    XSync(display, False);
    x11GrabFailed = false;
    XErrorHandler previousHandler = XSetErrorHandler(x11HotkeyErrorHandler);
    for (unsigned int variant : lockModifierVariants(baseModifiers)) {
        XGrabKey(display, keycode, variant, m_x11RootWindow, True, GrabModeAsync, GrabModeAsync);
    }
    XSync(display, False);
    XSetErrorHandler(previousHandler);
    if (x11GrabFailed) {
        qWarning() << "[HotkeyManager] XGrabKey failed (BadAccess):"
                      " the key combination is already grabbed by another client";
        for (unsigned int variant : lockModifierVariants(baseModifiers))
            XUngrabKey(display, keycode, variant, m_x11RootWindow);
        XSync(display, False);
        return false;
    }
    XSelectInput(display, m_x11RootWindow, KeyPressMask);
    XSync(display, False);
    if (!m_registeredHotkeys.contains(id))
        m_registeredHotkeys.append(id);
    m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
    return true;
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_useGnomeShortcutFallback) {
        if (id != HOTKEY_CAPTURE)
            return false;
        const auto previous = m_registeredHotkeyDefs;
        if (!m_registeredHotkeys.contains(id))
            m_registeredHotkeys.append(id);
        m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
        if (activateGnomeShortcutFallback())
            return true;
        m_registeredHotkeyDefs = previous;
        m_registeredHotkeys.removeAll(id);
        return false;
    }
    if (m_kdeShortcuts && m_kdeShortcuts->isAvailable() && !m_usePortalShortcuts) {
        const auto previous = m_registeredHotkeyDefs;
        if (!m_registeredHotkeys.contains(id)) m_registeredHotkeys.append(id);
        m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
        if (m_kdeShortcuts->setShortcuts(m_registeredHotkeyDefs)) return true;
        if (id == HOTKEY_CAPTURE && previous.isEmpty()
            && LinuxKGlobalAccelShortcuts::shouldUsePortalFallback(
                false, m_portalShortcuts && m_portalShortcuts->isAvailable())) {
            qWarning() << "[HotkeyManager] KGlobalAccel could not claim PrintScreen;"
                          " falling back to the GlobalShortcuts portal.";
            m_usePortalShortcuts = true;
            refreshPortalShortcuts();
            return true;
        }
        m_registeredHotkeyDefs = previous;
        m_registeredHotkeys.removeAll(id);
        // Push the previous definitions back so partially applied
        // registrations do not leave ghost entries in kglobalaccel.
        if (!previous.isEmpty())
            m_kdeShortcuts->setShortcuts(previous);
        else
            m_kdeShortcuts->deRegisterShortcut(id);
        return false;
    }
    if (!m_portalShortcuts || !m_portalShortcuts->isAvailable())
        return false;
    if (!m_registeredHotkeys.contains(id))
        m_registeredHotkeys.append(id);
    m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
    refreshPortalShortcuts();
    return true;
#else
    Q_UNUSED(modifiers);
    if (!m_registeredHotkeys.contains(id))
        m_registeredHotkeys.append(id);
    m_registeredHotkeyDefs.insert(id, qMakePair(modifiers, virtualKey));
    return true;
#endif
}

bool HotkeyManager::isPlainPrintScreen(UINT modifiers, UINT virtualKey)
{
    return modifiers == 0 && virtualKey == VK_SNAPSHOT;
}

bool HotkeyManager::isWindowsPrintScreenSnippingEnabled()
{
#ifdef Q_OS_WIN
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Keyboard"),
                  QSettings::NativeFormat);
    // Current Windows 11 installations may omit this value while still using
    // Print Screen for Snipping Tool. Treat the missing value as the Windows
    // default (enabled), otherwise a clean installation hides EShot's fix UI.
    return reg.value(QStringLiteral("PrintScreenKeyForSnippingEnabled"), 1).toInt() != 0;
#else
    return false;
#endif
}

bool HotkeyManager::setWindowsPrintScreenSnippingEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Keyboard"),
                  QSettings::NativeFormat);
    reg.setValue(QStringLiteral("PrintScreenKeyForSnippingEnabled"), enabled ? 1 : 0);
    reg.sync();
    if (reg.status() != QSettings::NoError
        || reg.value(QStringLiteral("PrintScreenKeyForSnippingEnabled"), -1).toInt() != (enabled ? 1 : 0)) {
        return false;
    }

    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST,
                        WM_SETTINGCHANGE,
                        0,
                        reinterpret_cast<LPARAM>(L"Control Panel\\Keyboard"),
                        SMTO_ABORTIFHUNG,
                        1000,
                        &result);
    return true;
#else
    Q_UNUSED(enabled);
    return false;
#endif
}

void HotkeyManager::unregisterHotkey(int id)
{
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, id);
#elif defined(ESHOT_HAVE_X11)
    Display *display = static_cast<Display *>(m_x11Display);
    if (display && m_x11RootWindow != 0 && m_registeredHotkeyDefs.contains(id)) {
        const auto def = m_registeredHotkeyDefs.value(id);
        const KeySym sym = keySymForVirtualKey(def.second);
        const KeyCode keycode = sym == NoSymbol ? 0 : XKeysymToKeycode(display, sym);
        if (keycode != 0) {
            for (unsigned int variant : lockModifierVariants(x11ModifiersForHotkey(def.first)))
                XUngrabKey(display, keycode, variant, m_x11RootWindow);
            XSync(display, False);
        }
    }
#endif
    m_registeredHotkeys.removeAll(id);
    m_registeredHotkeyDefs.remove(id);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_kdeShortcuts && m_kdeShortcuts->isAvailable() && !m_usePortalShortcuts) {
        m_kdeShortcuts->deRegisterShortcut(id);
        m_kdeShortcuts->setShortcuts(m_registeredHotkeyDefs);
    }
#endif
    refreshPortalShortcuts();
}

void HotkeyManager::unregisterAllHotkeys()
{
#ifdef Q_OS_WIN
    for (int id : m_registeredHotkeys) UnregisterHotKey(nullptr, id);
#elif defined(ESHOT_HAVE_X11)
    const QList<int> ids = m_registeredHotkeys;
    for (int id : ids)
        unregisterHotkey(id);
#endif
    m_registeredHotkeys.clear();
    m_registeredHotkeyDefs.clear();
    refreshPortalShortcuts();
}

bool HotkeyManager::reRegisterCaptureHotkey(UINT modifiers, UINT virtualKey)
{
    if (modifiers == m_captureModifiers && virtualKey == m_captureVirtualKey && m_registeredHotkeys.contains(HOTKEY_CAPTURE))
        return true;

    unregisterHotkey(HOTKEY_CAPTURE);
    bool ok = registerHotkey(HOTKEY_CAPTURE, modifiers, virtualKey);
    if (ok) {
        m_captureModifiers = modifiers;
        m_captureVirtualKey = virtualKey;
        return true;
    }

    bool restored = registerHotkey(HOTKEY_CAPTURE, m_captureModifiers, m_captureVirtualKey);
    if (!restored) {
        qWarning() << "[HotkeyManager] Failed to restore previous hotkey. Trying default...";
        if (registerHotkey(HOTKEY_CAPTURE, 0, VK_SNAPSHOT)) {
            m_captureModifiers = 0;
            m_captureVirtualKey = VK_SNAPSHOT;
        }
    }
    return false;
}

bool HotkeyManager::reRegisterRecordingHotkeys(UINT pauseModifiers, UINT pauseVirtualKey,
                                               UINT stopModifiers, UINT stopVirtualKey,
                                               UINT cancelModifiers, UINT cancelVirtualKey)
{
    const UINT oldPauseModifiers = m_recordingPauseModifiers;
    const UINT oldPauseVirtualKey = m_recordingPauseVirtualKey;
    const UINT oldStopModifiers = m_recordingStopModifiers;
    const UINT oldStopVirtualKey = m_recordingStopVirtualKey;
    const UINT oldCancelModifiers = m_recordingCancelModifiers;
    const UINT oldCancelVirtualKey = m_recordingCancelVirtualKey;

    unregisterHotkey(HOTKEY_RECORDING_PAUSE);
    unregisterHotkey(HOTKEY_RECORDING_STOP);
    unregisterHotkey(HOTKEY_RECORDING_CANCEL);

    bool ok = true;
    ok = registerHotkey(HOTKEY_RECORDING_PAUSE, pauseModifiers, pauseVirtualKey) && ok;
    ok = registerHotkey(HOTKEY_RECORDING_STOP, stopModifiers, stopVirtualKey) && ok;
    ok = registerHotkey(HOTKEY_RECORDING_CANCEL, cancelModifiers, cancelVirtualKey) && ok;

    if (ok) {
        m_recordingPauseModifiers = pauseModifiers;
        m_recordingPauseVirtualKey = pauseVirtualKey;
        m_recordingStopModifiers = stopModifiers;
        m_recordingStopVirtualKey = stopVirtualKey;
        m_recordingCancelModifiers = cancelModifiers;
        m_recordingCancelVirtualKey = cancelVirtualKey;
    } else {
        unregisterHotkey(HOTKEY_RECORDING_PAUSE);
        unregisterHotkey(HOTKEY_RECORDING_STOP);
        unregisterHotkey(HOTKEY_RECORDING_CANCEL);
        registerHotkey(HOTKEY_RECORDING_PAUSE, oldPauseModifiers, oldPauseVirtualKey);
        registerHotkey(HOTKEY_RECORDING_STOP, oldStopModifiers, oldStopVirtualKey);
        registerHotkey(HOTKEY_RECORDING_CANCEL, oldCancelModifiers, oldCancelVirtualKey);
    }
    return ok;
}

bool HotkeyManager::reRegisterActionHotkeys(UINT instantModifiers, UINT instantVirtualKey,
                                            UINT gifModifiers, UINT gifVirtualKey,
                                            UINT videoModifiers, UINT videoVirtualKey,
                                            UINT windowModifiers, UINT windowVirtualKey)
{
    const UINT oldInstantModifiers = m_instantCaptureModifiers;
    const UINT oldInstantVirtualKey = m_instantCaptureVirtualKey;
    const UINT oldGifModifiers = m_gifCaptureModifiers;
    const UINT oldGifVirtualKey = m_gifCaptureVirtualKey;
    const UINT oldVideoModifiers = m_videoCaptureModifiers;
    const UINT oldVideoVirtualKey = m_videoCaptureVirtualKey;
    const UINT oldWindowModifiers = m_windowCaptureModifiers;
    const UINT oldWindowVirtualKey = m_windowCaptureVirtualKey;

    unregisterHotkey(HOTKEY_INSTANT_CAPTURE);
    unregisterHotkey(HOTKEY_GIF_CAPTURE);
    unregisterHotkey(HOTKEY_VIDEO_CAPTURE);
    unregisterHotkey(HOTKEY_WINDOW_CAPTURE);

    bool ok = true;
    ok = registerHotkey(HOTKEY_INSTANT_CAPTURE, instantModifiers, instantVirtualKey) && ok;
    ok = registerHotkey(HOTKEY_GIF_CAPTURE, gifModifiers, gifVirtualKey) && ok;
    ok = registerHotkey(HOTKEY_VIDEO_CAPTURE, videoModifiers, videoVirtualKey) && ok;
    ok = registerHotkey(HOTKEY_WINDOW_CAPTURE, windowModifiers, windowVirtualKey) && ok;

    if (ok) {
        m_instantCaptureModifiers = instantModifiers;
        m_instantCaptureVirtualKey = instantVirtualKey;
        m_gifCaptureModifiers = gifModifiers;
        m_gifCaptureVirtualKey = gifVirtualKey;
        m_videoCaptureModifiers = videoModifiers;
        m_videoCaptureVirtualKey = videoVirtualKey;
        m_windowCaptureModifiers = windowModifiers;
        m_windowCaptureVirtualKey = windowVirtualKey;
    } else {
        unregisterHotkey(HOTKEY_INSTANT_CAPTURE);
        unregisterHotkey(HOTKEY_GIF_CAPTURE);
        unregisterHotkey(HOTKEY_VIDEO_CAPTURE);
        unregisterHotkey(HOTKEY_WINDOW_CAPTURE);
        registerHotkey(HOTKEY_INSTANT_CAPTURE, oldInstantModifiers, oldInstantVirtualKey);
        registerHotkey(HOTKEY_GIF_CAPTURE, oldGifModifiers, oldGifVirtualKey);
        registerHotkey(HOTKEY_VIDEO_CAPTURE, oldVideoModifiers, oldVideoVirtualKey);
        registerHotkey(HOTKEY_WINDOW_CAPTURE, oldWindowModifiers, oldWindowVirtualKey);
    }
    return ok;
}

bool HotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            emitHotkey(static_cast<int>(msg->wParam));
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

void HotkeyManager::emitHotkey(int id)
{
    emit hotkeyTriggered(id);
    if (id == HOTKEY_CAPTURE) emit captureRequested();
    else if (id == HOTKEY_RECORDING_PAUSE) emit recordingPauseRequested();
    else if (id == HOTKEY_RECORDING_STOP) emit recordingStopRequested();
    else if (id == HOTKEY_RECORDING_CANCEL) emit recordingCancelRequested();
    else if (id == HOTKEY_INSTANT_CAPTURE) emit instantCaptureRequested();
    else if (id == HOTKEY_GIF_CAPTURE) emit gifCaptureRequested();
    else if (id == HOTKEY_VIDEO_CAPTURE) emit videoCaptureRequested();
    else if (id == HOTKEY_WINDOW_CAPTURE) emit windowCaptureRequested();
}

void HotkeyManager::refreshPortalShortcuts()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_usePortalShortcuts && m_portalShortcuts && m_portalShortcuts->isAvailable())
        m_portalShortcuts->setShortcuts(m_registeredHotkeyDefs);
#endif
}

bool HotkeyManager::activateGnomeShortcutFallback()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    if (desktop != LinuxDesktopEnvironment::Gnome
        || !m_registeredHotkeyDefs.contains(HOTKEY_CAPTURE)) {
        return false;
    }

    m_usePortalShortcuts = false;
    m_useGnomeShortcutFallback = true;
    const auto capture = m_registeredHotkeyDefs.value(HOTKEY_CAPTURE);
    const QString binding = LinuxPortalGlobalShortcuts::preferredTrigger(
        capture.first, capture.second);
    const QString integratedAppImage = QDir::home().filePath(
        QStringLiteral(".local/opt/EShot/EShot.AppImage"));
    const QString executable = LinuxGnomeShortcutInstaller::preferredExecutable(
        qEnvironmentVariable("APPIMAGE"),
        QCoreApplication::applicationFilePath(),
        QFileInfo::exists(integratedAppImage) ? integratedAppImage : QString());
    const auto installed = LinuxGnomeShortcutInstaller::installCaptureShortcut(
        LinuxGnomeShortcutInstaller::captureCommand(executable), binding);
    if (installed.success) {
        qInfo() << "[HotkeyManager] GNOME shortcut fallback installed:" << binding;
        return true;
    } else {
        qWarning() << "[HotkeyManager] GNOME shortcut fallback failed:"
                   << installed.error;
        return false;
    }
#else
    return false;
#endif
}
