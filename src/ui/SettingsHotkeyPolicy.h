#pragma once

#include <QHash>
#include <QList>
#include <QtGlobal>

struct SettingsHotkeyDefinition {
    quint32 modifiers = 0;
    quint32 virtualKey = 0;
};

struct SettingsHotkeyEntry {
    int id = 0;
    SettingsHotkeyDefinition hotkey;
};

bool settingsHotkeyChanged(const SettingsHotkeyDefinition &proposed,
                           const SettingsHotkeyDefinition &saved);

// Maps the id of every entry whose key is also used by another entry to the
// id of the first other entry with that key. Disabled keys (virtualKey 0)
// never conflict.
QHash<int, int> settingsHotkeyConflicts(const QList<SettingsHotkeyEntry> &entries);
