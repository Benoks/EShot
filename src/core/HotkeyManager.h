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
    // Pause/stop/cancel recording keys are registered only while a recording
    // or its start countdown runs (see recordingHotkeysRegisteredOnDemand).
    // Returns false when a configured key could not be registered; the keys
    // are then listed by failedHotkeys() until deactivateRecordingHotkeys().
    bool activateRecordingHotkeys();
    void deactivateRecordingHotkeys();
    bool recordingHotkeysActive() const { return m_recordingHotkeysActive; }
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
    UINT m_recordingPauseModifiers = 0;
    UINT m_recordingPauseVirtualKey = 0;
    UINT m_recordingStopModifiers = 0;
    UINT m_recordingStopVirtualKey = 0;
    UINT m_recordingCancelModifiers = 0;
    UINT m_recordingCancelVirtualKey = 0;
    bool m_recordingHotkeysActive = false;
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
    QList<HotkeyBinding> recordingHotkeyBindings() const;
    bool recordingHotkeysOnDemand() const;
    void releaseIdleRecordingHotkeys();
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
