#include "SettingsHotkeyPolicy.h"

#include "core/PlatformHotkey.h"

bool settingsHotkeyChanged(const SettingsHotkeyDefinition &proposed,
                           const SettingsHotkeyDefinition &saved)
{
    return proposed.modifiers != saved.modifiers || proposed.virtualKey != saved.virtualKey;
}

QHash<int, int> settingsHotkeyConflicts(const QList<SettingsHotkeyEntry> &entries)
{
    QHash<int, int> conflicts;
    for (const SettingsHotkeyEntry &entry : entries) {
        if (entry.hotkey.virtualKey == 0)
            continue;
        for (const SettingsHotkeyEntry &other : entries) {
            if (other.id != entry.id
                && !settingsHotkeyChanged(entry.hotkey, other.hotkey)) {
                conflicts.insert(entry.id, other.id);
                break;
            }
        }
    }
    return conflicts;
}

SettingsHotkeyDefinition defaultRecordingHotkey(RecordingHotkeyAction action, bool windows)
{
    if (windows) {
        const quint32 modifiers = MOD_CONTROL | MOD_SHIFT;
        switch (action) {
        case RecordingHotkeyAction::Pause:
            return {modifiers, VK_F9};
        case RecordingHotkeyAction::Stop:
            return {modifiers, VK_F10};
        case RecordingHotkeyAction::Cancel:
            return {modifiers, VK_F11};
        }
    }
    const quint32 modifiers = MOD_CONTROL | MOD_ALT;
    switch (action) {
    case RecordingHotkeyAction::Pause:
        return {modifiers, 'P'};
    case RecordingHotkeyAction::Stop:
        return {modifiers, 'S'};
    case RecordingHotkeyAction::Cancel:
        break;
    }
    return {modifiers, 'X'};
}

SettingsHotkeyDefinition platformDefaultRecordingHotkey(RecordingHotkeyAction action)
{
#ifdef Q_OS_WIN
    return defaultRecordingHotkey(action, true);
#else
    return defaultRecordingHotkey(action, false);
#endif
}

const char *recordingHotkeyModifiersKey(RecordingHotkeyAction action)
{
    switch (action) {
    case RecordingHotkeyAction::Pause:
        return "recordingPauseHotkeyModifiers";
    case RecordingHotkeyAction::Stop:
        return "recordingStopHotkeyModifiers";
    case RecordingHotkeyAction::Cancel:
        break;
    }
    return "recordingCancelHotkeyModifiers";
}

const char *recordingHotkeyVirtualKeyKey(RecordingHotkeyAction action)
{
    switch (action) {
    case RecordingHotkeyAction::Pause:
        return "recordingPauseHotkeyVKey";
    case RecordingHotkeyAction::Stop:
        return "recordingStopHotkeyVKey";
    case RecordingHotkeyAction::Cancel:
        break;
    }
    return "recordingCancelHotkeyVKey";
}

bool recordingHotkeysRegisteredOnDemand(bool portalShortcutBackend)
{
    return !portalShortcutBackend;
}
