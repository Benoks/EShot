#include <QtTest>
#include "ui/SettingsHotkeyPolicy.h"

class SettingsHotkeyPolicyTests : public QObject {
    Q_OBJECT
private slots:
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
