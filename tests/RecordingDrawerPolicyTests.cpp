#include <QtTest>

#include "recording/RecordingDrawerPolicy.h"

class RecordingDrawerPolicyTests : public QObject {
    Q_OBJECT

private slots:
    void gifShowsOnlyGifEssentials();
    void videoShowsOnlyVideoEssentials();
    void noneShowsNoFields();
    void drawerIsCenteredInAvailableOverlayArea();
    void quickSettingsOutsidePressClosesAndContinuesCapture();
    void recordingOutsidePressRemainsModal();
    void insidePressDoesNotCloseAnOverlayMenu();
    void pressWithoutMenuKeepsNormalCaptureBehavior();
    void escapeClosesAnOpenOverlayMenuFirst();
    void durationChoicesCoverCommonLimits();
    void startDelayChoicesMatchTheDrawer();
    void customValuesAreNotAChoice();
    void enterStartsOnlyWhileTheDrawerIsOpen();
};

void RecordingDrawerPolicyTests::gifShowsOnlyGifEssentials()
{
    const QStringList fields = recordingDrawerFields(RecordingDrawerMode::Gif);
    QCOMPARE(fields, QStringList({"mode", "quality", "duration", "startDelay",
                                  "advancedSettings", "hotkeys", "start"}));
    // Raw knobs and audio stay out of the drawer.
    QVERIFY(!fields.contains("systemAudio"));
    QVERIFY(!fields.contains("fps"));
}

void RecordingDrawerPolicyTests::videoShowsOnlyVideoEssentials()
{
    QCOMPARE(recordingDrawerFields(RecordingDrawerMode::Video),
             QStringList({"mode", "quality", "duration", "startDelay",
                          "systemAudio", "microphone", "microphoneDevice",
                          "advancedSettings", "hotkeys", "start"}));
}

void RecordingDrawerPolicyTests::noneShowsNoFields()
{
    QVERIFY(recordingDrawerFields(RecordingDrawerMode::None).isEmpty());
}

void RecordingDrawerPolicyTests::drawerIsCenteredInAvailableOverlayArea()
{
    QCOMPARE(recordingDrawerPosition(QRect(10, 20, 1000, 700), QSize(260, 200)),
             QPoint(380, 270));
}

void RecordingDrawerPolicyTests::quickSettingsOutsidePressClosesAndContinuesCapture()
{
    QCOMPARE(overlayMenuPressAction(false, true, false),
             OverlayMenuPressAction::CloseAndContinueCapture);
}

void RecordingDrawerPolicyTests::recordingOutsidePressRemainsModal()
{
    QCOMPARE(overlayMenuPressAction(true, false, false),
             OverlayMenuPressAction::CloseAndConsume);
}

void RecordingDrawerPolicyTests::insidePressDoesNotCloseAnOverlayMenu()
{
    QCOMPARE(overlayMenuPressAction(true, false, true),
             OverlayMenuPressAction::ConsumeInsideMenu);
    QCOMPARE(overlayMenuPressAction(false, true, true),
             OverlayMenuPressAction::ConsumeInsideMenu);
}

void RecordingDrawerPolicyTests::pressWithoutMenuKeepsNormalCaptureBehavior()
{
    QCOMPARE(overlayMenuPressAction(false, false, false),
             OverlayMenuPressAction::ContinueCapture);
}

void RecordingDrawerPolicyTests::escapeClosesAnOpenOverlayMenuFirst()
{
    QVERIFY(escapeClosesOverlayMenu(true, false));
    QVERIFY(escapeClosesOverlayMenu(false, true));
    QVERIFY(!escapeClosesOverlayMenu(false, false));
}

void RecordingDrawerPolicyTests::durationChoicesCoverCommonLimits()
{
    QCOMPARE(recordingDurationChoices(), QList<int>({0, 15, 30, 60, 300}));
}

void RecordingDrawerPolicyTests::startDelayChoicesMatchTheDrawer()
{
    QCOMPARE(recordingStartDelayChoices(), QList<int>({0, 3, 5, 10}));
}

void RecordingDrawerPolicyTests::customValuesAreNotAChoice()
{
    QCOMPARE(recordingChoiceIndex(recordingDurationChoices(), 0), 0);
    QCOMPARE(recordingChoiceIndex(recordingDurationChoices(), 60), 3);
    QCOMPARE(recordingChoiceIndex(recordingDurationChoices(), 45), -1);
    QCOMPARE(recordingChoiceIndex(recordingStartDelayChoices(), 7), -1);
}

void RecordingDrawerPolicyTests::enterStartsOnlyWhileTheDrawerIsOpen()
{
    QVERIFY(recordingDrawerStartsOnKey(Qt::Key_Return, true));
    QVERIFY(recordingDrawerStartsOnKey(Qt::Key_Enter, true));
    QVERIFY(!recordingDrawerStartsOnKey(Qt::Key_Return, false));
    QVERIFY(!recordingDrawerStartsOnKey(Qt::Key_Space, true));
}

QTEST_MAIN(RecordingDrawerPolicyTests)

#include "RecordingDrawerPolicyTests.moc"
