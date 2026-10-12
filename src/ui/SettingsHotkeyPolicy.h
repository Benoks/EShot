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

enum class RecordingHotkeyAction {
    Pause,
    Stop,
    Cancel
};

// Default pause/stop/cancel recording hotkeys, used only when no value is
// stored. Windows avoids Ctrl+Alt+<letter>: Windows treats Ctrl+Alt as AltGr,
// so those keys would swallow characters such as Polish AltGr+S (ś).
SettingsHotkeyDefinition defaultRecordingHotkey(RecordingHotkeyAction action, bool windows);
SettingsHotkeyDefinition platformDefaultRecordingHotkey(RecordingHotkeyAction action);
// QSettings keys of a recording hotkey.
const char *recordingHotkeyModifiersKey(RecordingHotkeyAction action);
const char *recordingHotkeyVirtualKeyKey(RecordingHotkeyAction action);

// Recording hotkeys are registered only while a recording (or its start
// countdown) runs, so they do not take common shortcuts away from other apps.
// The GlobalShortcuts portal binds a whole session at once (and may ask the
// user each time), so there they stay registered permanently instead.
bool recordingHotkeysRegisteredOnDemand(bool portalShortcutBackend);
