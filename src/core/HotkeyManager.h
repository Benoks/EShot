#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QList>
#include <QPair>
#include "PlatformHotkey.h"

class LinuxPortalGlobalShortcuts;
class LinuxKGlobalAccelShortcuts;

struct HotkeyBinding {
    int id = 0;
    UINT modifiers = 0;
    UINT virtualKey = 0;
};

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    static HotkeyManager& instance();
    bool registerHotkey(int id, UINT modifiers, UINT virtualKey);
    void unregisterHotkey(int id);
    void unregisterAllHotkeys();
    bool reRegisterCaptureHotkey(UINT modifiers, UINT virtualKey);
    bool reRegisterRecordingHotkeys(UINT pauseModifiers, UINT pauseVirtualKey,
                                    UINT stopModifiers, UINT stopVirtualKey,
                                    UINT cancelModifiers, UINT cancelVirtualKey);
    bool reRegisterActionHotkeys(UINT instantModifiers, UINT instantVirtualKey,
                                 UINT gifModifiers, UINT gifVirtualKey,
                                 UINT videoModifiers, UINT videoVirtualKey,
                                 UINT windowModifiers, UINT windowVirtualKey);
    bool requestLinuxPortalShortcutRebind();
    bool linuxPortalShortcutsAvailable() const;
    UINT captureModifiers() const { return m_captureModifiers; }
    UINT captureVirtualKey() const { return m_captureVirtualKey; }
    QString recordingPauseShortcutText() const;
    QString recordingStopShortcutText() const;
    QString recordingCancelShortcutText() const;
    // Configured hotkeys that are not active, e.g. because another app owns
    // the key. Keys rejected by reRegister*() are reported by its return value.
    QList<HotkeyBinding> failedHotkeys() const;
    bool isHotkeyActive(int id) const;
    // Text of the key that is actually registered for id; empty when none.
    QString activeShortcutText(int id) const;
    static QString shortcutText(UINT modifiers, UINT virtualKey);
    static bool isPlainPrintScreen(UINT modifiers, UINT virtualKey);
    static bool isWindowsPrintScreenSnippingEnabled();
    static bool setWindowsPrintScreenSnippingEnabled(bool enabled);
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void captureRequested();
    void recordingPauseRequested();
    void recordingStopRequested();
    void recordingCancelRequested();
    void instantCaptureRequested();
    void gifCaptureRequested();
    void videoCaptureRequested();
    void windowCaptureRequested();
    // Emitted when configured hotkeys stop being or could not become active
    // outside a reRegister*() request; see failedHotkeys().
    void hotkeyRegistrationFailed(const QList<int> &ids);

private:
    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager();
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    QList<int> m_registeredHotkeys;
    QHash<int, QPair<UINT, UINT>> m_registeredHotkeyDefs;
    QHash<int, QPair<UINT, UINT>> m_failedHotkeys;
    UINT m_captureModifiers = 0;
    UINT m_captureVirtualKey = VK_SNAPSHOT;
    UINT m_recordingPauseModifiers = MOD_CONTROL | MOD_ALT;
    UINT m_recordingPauseVirtualKey = 'P';
    UINT m_recordingStopModifiers = MOD_CONTROL | MOD_ALT;
    UINT m_recordingStopVirtualKey = 'S';
    UINT m_recordingCancelModifiers = MOD_CONTROL | MOD_ALT;
    UINT m_recordingCancelVirtualKey = 'X';
    UINT m_instantCaptureModifiers = 0;
    UINT m_instantCaptureVirtualKey = 0;
    UINT m_gifCaptureModifiers = 0;
    UINT m_gifCaptureVirtualKey = 0;
    UINT m_videoCaptureModifiers = 0;
    UINT m_videoCaptureVirtualKey = 0;
    UINT m_windowCaptureModifiers = 0;
    UINT m_windowCaptureVirtualKey = 0;
    void *m_x11Display = nullptr;
    unsigned long m_x11RootWindow = 0;
    LinuxPortalGlobalShortcuts *m_portalShortcuts = nullptr;
    LinuxKGlobalAccelShortcuts *m_kdeShortcuts = nullptr;
    bool m_usePortalShortcuts = false;
    bool m_useGnomeShortcutFallback = false;

    bool registerPlatformHotkey(int id, UINT modifiers, UINT virtualKey);
    void recordFailure(int id, UINT modifiers, UINT virtualKey);
    void restoreHotkeys(const QList<HotkeyBinding> &previous);
    void onPortalShortcutsFailed();
    void emitHotkey(int id);
    void refreshPortalShortcuts();
    bool activateGnomeShortcutFallback();

public:
    static constexpr int HOTKEY_CAPTURE = 1;
    static constexpr int HOTKEY_RECORDING_PAUSE = 2;
    static constexpr int HOTKEY_RECORDING_STOP = 3;
    static constexpr int HOTKEY_RECORDING_CANCEL = 4;
    static constexpr int HOTKEY_INSTANT_CAPTURE = 5;
    static constexpr int HOTKEY_GIF_CAPTURE = 6;
    static constexpr int HOTKEY_VIDEO_CAPTURE = 7;
    static constexpr int HOTKEY_WINDOW_CAPTURE = 8;
};

#endif
