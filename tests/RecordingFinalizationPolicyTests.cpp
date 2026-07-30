#include <QtTest>

#include "recording/RecordingFinalizationPolicy.h"

class RecordingFinalizationPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void gifRequiresSuccessfulProcessAndNonEmptyOutput()
    {
        QVERIFY(portalGifConversionSucceeded(true, 0, 1024));
        QVERIFY(!portalGifConversionSucceeded(false, 0, 1024));
        QVERIFY(!portalGifConversionSucceeded(true, 1, 1024));
        QVERIFY(!portalGifConversionSucceeded(true, 0, 0));
    }

    void muxFallsBackToVideoOnlyWhenFinalOutputFails()
    {
        QCOMPARE(videoMuxCompletionAction(true, 0, 4096),
                 VideoMuxCompletionAction::UseMuxedOutput);
        QCOMPARE(videoMuxCompletionAction(false, 1, 0),
                 VideoMuxCompletionAction::UseVideoOnlyFallback);
    }
};

QTEST_APPLESS_MAIN(RecordingFinalizationPolicyTests)
#include "RecordingFinalizationPolicyTests.moc"
