#include <QtTest>
#include "ui/SettingsLayoutPolicy.h"

class SettingsLayoutPolicyTests : public QObject {
    Q_OBJECT
private slots:
    void restoresRememberedWindowSizeWithinDialogBounds()
    {
        QCOMPARE(settingsDialogRestoredSize(false, QSize(720, 640), QSize(560, 400), QSize(750, 800)),
                 QSize());
        QCOMPARE(settingsDialogRestoredSize(true, QSize(720, 640), QSize(560, 400), QSize(750, 800)),
                 QSize(720, 640));
        QCOMPARE(settingsDialogRestoredSize(true, QSize(1200, 200), QSize(560, 400), QSize(750, 800)),
                 QSize(750, 400));
    }

    void keepsAdaptiveSizingWhenWindowSizeIsNotRemembered()
    {
        QVERIFY(settingsDialogUsesAdaptiveSize(false));
        QVERIFY(!settingsDialogUsesAdaptiveSize(true));
    }

    void expandsDialogToFitTabs()
    {
        QCOMPARE(settingsDialogWidthForTabs(680, 24, 560), 704);
        QCOMPARE(settingsDialogWidthForTabs(420, 24, 560), 560);
    }

    void givesAllDialogActionsOneSharedSize()
    {
        QCOMPARE(settingsDialogActionButtonSize({QSize(74, 28), QSize(86, 30), QSize(112, 38)}),
                 QSize(112, 38));
        QCOMPARE(settingsDialogActionButtonSize({QSize(60, 24), QSize(70, 25)}, 104, 34),
                 QSize(104, 34));
    }
};

QTEST_APPLESS_MAIN(SettingsLayoutPolicyTests)
#include "SettingsLayoutPolicyTests.moc"
