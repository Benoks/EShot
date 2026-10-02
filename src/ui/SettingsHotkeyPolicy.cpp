#include "SettingsHotkeyPolicy.h"

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
