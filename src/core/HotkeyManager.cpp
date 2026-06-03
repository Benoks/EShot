#include "HotkeyManager.h"
#include <QApplication>
#include <QSettings>
#include <QDebug>

HotkeyManager& HotkeyManager::instance()
{
    static HotkeyManager inst;
    return inst;
}

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent)
{
    qApp->installNativeEventFilter(this);

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
}

HotkeyManager::~HotkeyManager()
{
    unregisterAllHotkeys();
    qApp->removeNativeEventFilter(this);
}

bool HotkeyManager::registerHotkey(int id, UINT modifiers, UINT virtualKey)
{
    if (RegisterHotKey(nullptr, id, modifiers, virtualKey)) {
        m_registeredHotkeys.append(id);
        return true;
    }
    return false;
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
    return reg.value(QStringLiteral("PrintScreenKeyForSnippingEnabled"), 0).toInt() != 0;
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
    return reg.status() == QSettings::NoError;
#else
    Q_UNUSED(enabled);
    return false;
#endif
}

void HotkeyManager::unregisterHotkey(int id)
{
    UnregisterHotKey(nullptr, id);
    m_registeredHotkeys.removeAll(id);
}

void HotkeyManager::unregisterAllHotkeys()
{
    for (int id : m_registeredHotkeys) UnregisterHotKey(nullptr, id);
    m_registeredHotkeys.clear();
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

bool HotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            int id = static_cast<int>(msg->wParam);
            emit hotkeyTriggered(id);
            if (id == HOTKEY_CAPTURE) emit captureRequested();
            return true;
        }
    }
    return false;
}
