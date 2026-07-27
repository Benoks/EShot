#include <QtTest>

#include "recording/RecordingDrawerPolicy.h"

class RecordingDrawerPolicyTests : public QObject {
    Q_OBJECT

private slots:
    void gifShowsOnlyGifEssentials();
    void videoShowsOnlyVideoEssentials();
    void noneShowsNoFields();
    void drawerIsCenteredInAvailableOverlayArea();
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

QTEST_MAIN(RecordingDrawerPolicyTests)

#include "RecordingDrawerPolicyTests.moc"
