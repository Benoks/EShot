#include <QtTest>

#include "recording/VideoRecordingCompletionPolicy.h"

class VideoRecordingCompletionPolicyTests : public QObject {
    Q_OBJECT

private slots:
    void keepsFinalizedGstreamerOutputAfterUserStop()
    {
        QVERIFY(videoRecordingProcessSucceeded(
            true, true, QProcess::CrashExit, 2, 2048));
    }

    void dropsKilledRegularMp4()
    {
        // A regular MP4 has no index until FFmpeg finishes it.
        QVERIFY(!videoRecordingProcessSucceeded(
            false, true, QProcess::CrashExit, 1, 10 * 1024 * 1024));
    }

    void keepsKilledFragmentedMp4WithMedia()
    {
        QVERIFY(videoRecordingProcessSucceeded(
            false, true, QProcess::CrashExit, 1, 10 * 1024 * 1024, true));
        // Also when the encoder died on its own: the fragments are complete.
        QVERIFY(videoRecordingProcessSucceeded(
            false, false, QProcess::NormalExit, 1, 10 * 1024 * 1024, true));
    }

    void dropsFragmentedMp4WithOnlyAHeader()
    {
        QVERIFY(!videoRecordingProcessSucceeded(
            false, true, QProcess::CrashExit, 1, 1200, true));
        QVERIFY(!videoRecordingProcessSucceeded(
            false, true, QProcess::CrashExit, 1, 0, true));
    }

    void cleanExitStillNeedsOutput()
    {
        QVERIFY(videoRecordingProcessSucceeded(false, false, QProcess::NormalExit, 0, 1));
        QVERIFY(!videoRecordingProcessSucceeded(false, false, QProcess::NormalExit, 0, 0));
        QVERIFY(!videoRecordingProcessSucceeded(false, false, QProcess::NormalExit, 1, 4096));
    }
};

QTEST_APPLESS_MAIN(VideoRecordingCompletionPolicyTests)
#include "VideoRecordingCompletionPolicyTests.moc"
