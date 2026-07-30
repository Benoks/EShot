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
};

void RecordingDrawerPolicyTests::gifShowsOnlyGifEssentials()
{
    QCOMPARE(recordingDrawerFields(RecordingDrawerMode::Gif),
             QStringList({"gifFps", "gifDuration", "gifLoop", "startGif"}));
}

void RecordingDrawerPolicyTests::videoShowsOnlyVideoEssentials()
{
    QCOMPARE(recordingDrawerFields(RecordingDrawerMode::Video),
             QStringList({"videoFps", "videoQuality", "videoDuration",
                          "desktopAudio", "microphone", "microphoneDevice", "startVideo"}));
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

QTEST_MAIN(RecordingDrawerPolicyTests)

#include "RecordingDrawerPolicyTests.moc"
