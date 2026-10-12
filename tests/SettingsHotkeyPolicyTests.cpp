#include <QtTest>
#include "ui/SettingsHotkeyPolicy.h"
#include "core/PlatformHotkey.h"

class SettingsHotkeyPolicyTests : public QObject {
    Q_OBJECT
private slots:
    void windowsRecordingDefaultsAvoidAltGr()
    {
        for (RecordingHotkeyAction action : {RecordingHotkeyAction::Pause, RecordingHotkeyAction::Stop,
                                             RecordingHotkeyAction::Cancel}) {
            const SettingsHotkeyDefinition hotkey = defaultRecordingHotkey(action, true);
            // Ctrl+Alt is AltGr on Windows.
            QVERIFY(!((hotkey.modifiers & MOD_CONTROL) && (hotkey.modifiers & MOD_ALT)));
            QCOMPARE(hotkey.modifiers, quint32(MOD_CONTROL | MOD_SHIFT));
        }
        QCOMPARE(defaultRecordingHotkey(RecordingHotkeyAction::Pause, true).virtualKey, quint32(VK_F9));
        QCOMPARE(defaultRecordingHotkey(RecordingHotkeyAction::Stop, true).virtualKey, quint32(VK_F10));
        QCOMPARE(defaultRecordingHotkey(RecordingHotkeyAction::Cancel, true).virtualKey, quint32(VK_F11));
    }

    void linuxRecordingDefaultsAreUnchanged()
    {
        const quint32 ctrlAlt = MOD_CONTROL | MOD_ALT;
        QCOMPARE(defaultRecordingHotkey(RecordingHotkeyAction::Pause, false).modifiers, ctrlAlt);
        QCOMPARE(defaultRecordingHotkey(RecordingHotkeyAction::Pause, false).virtualKey, quint32('P'));
        QCOMPARE(defaultRecordingHotkey(RecordingHotkeyAction::Stop, false).virtualKey, quint32('S'));
        QCOMPARE(defaultRecordingHotkey(RecordingHotkeyAction::Cancel, false).virtualKey, quint32('X'));
    }

    void recordingDefaultsDoNotConflict()
    {
        for (bool windows : {true, false}) {
            const QList<SettingsHotkeyEntry> entries {
                {1, {0, VK_SNAPSHOT}},
                {2, defaultRecordingHotkey(RecordingHotkeyAction::Pause, windows)},
                {3, defaultRecordingHotkey(RecordingHotkeyAction::Stop, windows)},
                {4, defaultRecordingHotkey(RecordingHotkeyAction::Cancel, windows)},
                {8, {MOD_SHIFT, VK_SNAPSHOT}},
            };
            QVERIFY(settingsHotkeyConflicts(entries).isEmpty());
        }
    }

    void recordingHotkeySettingKeysAreStable()
    {
        QCOMPARE(QString::fromLatin1(recordingHotkeyModifiersKey(RecordingHotkeyAction::Pause)),
                 QStringLiteral("recordingPauseHotkeyModifiers"));
        QCOMPARE(QString::fromLatin1(recordingHotkeyVirtualKeyKey(RecordingHotkeyAction::Stop)),
                 QStringLiteral("recordingStopHotkeyVKey"));
        QCOMPARE(QString::fromLatin1(recordingHotkeyVirtualKeyKey(RecordingHotkeyAction::Cancel)),
                 QStringLiteral("recordingCancelHotkeyVKey"));
    }

    void recordingHotkeysAreOnDemandExceptWithThePortal()
    {
        QVERIFY(recordingHotkeysRegisteredOnDemand(false));
        QVERIFY(!recordingHotkeysRegisteredOnDemand(true));
    }

    void unchangedHotkeyDoesNotNeedReregistration()
    {
        const SettingsHotkeyDefinition saved { 3, 44 };
        QVERIFY(!settingsHotkeyChanged(saved, saved));
    }

    void changedHotkeyNeedsReregistration()
    {
        const SettingsHotkeyDefinition saved { 3, 44 };
        QVERIFY(settingsHotkeyChanged({ 3, 45 }, saved));
        QVERIFY(settingsHotkeyChanged({ 2, 44 }, saved));
    }

    void distinctHotkeysDoNotConflict()
    {
        const QList<SettingsHotkeyEntry> entries {
            { 1, { 0, 44 } },
            { 2, { 3, 'P' } },
            { 3, { 3, 'S' } },
            { 4, { 4, 44 } },
        };
        QVERIFY(settingsHotkeyConflicts(entries).isEmpty());
    }

    void sameHotkeyMarksBothEntries()
    {
        const QList<SettingsHotkeyEntry> entries {
            { 1, { 0, 44 } },
            { 2, { 3, 'P' } },
            { 6, { 3, 'P' } },
        };
        const QHash<int, int> conflicts = settingsHotkeyConflicts(entries);
        QCOMPARE(conflicts.size(), 2);
        QCOMPARE(conflicts.value(2), 6);
        QCOMPARE(conflicts.value(6), 2);
        QVERIFY(!conflicts.contains(1));
    }

    void threeWayConflictPointsAtFirstOtherEntry()
    {
        const QList<SettingsHotkeyEntry> entries {
            { 1, { 2, 'A' } },
            { 5, { 2, 'A' } },
            { 7, { 2, 'A' } },
        };
        const QHash<int, int> conflicts = settingsHotkeyConflicts(entries);
        QCOMPARE(conflicts.size(), 3);
        QCOMPARE(conflicts.value(1), 5);
        QCOMPARE(conflicts.value(5), 1);
        QCOMPARE(conflicts.value(7), 1);
    }

    void disabledHotkeysNeverConflict()
    {
        const QList<SettingsHotkeyEntry> entries {
            { 5, { 0, 0 } },
            { 6, { 0, 0 } },
            { 7, { 2, 0 } },
        };
        QVERIFY(settingsHotkeyConflicts(entries).isEmpty());
    }

    void sameKeyWithDifferentModifiersDoesNotConflict()
    {
        const QList<SettingsHotkeyEntry> entries {
            { 1, { 0, 44 } },
            { 8, { 4, 44 } },
        };
        QVERIFY(settingsHotkeyConflicts(entries).isEmpty());
    }
};

QTEST_APPLESS_MAIN(SettingsHotkeyPolicyTests)
#include "SettingsHotkeyPolicyTests.moc"
