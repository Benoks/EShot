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
};

QTEST_APPLESS_MAIN(VideoRecordingCompletionPolicyTests)
#include "VideoRecordingCompletionPolicyTests.moc"
