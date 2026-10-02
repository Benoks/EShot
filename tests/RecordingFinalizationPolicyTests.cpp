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

    void timeoutsGrowWithTheRecordedDuration()
    {
        // Short recordings keep the previous fixed limits.
        QVERIFY(videoMuxTimeoutMs(0) >= 30000);
        QVERIFY(portalGifPassTimeoutMs(0) >= 60000);
        QVERIFY(segmentConcatTimeoutMs(0) >= 30000);

        const qint64 tenMinutes = 10 * 60 * 1000;
        QVERIFY(videoMuxTimeoutMs(tenMinutes) >= 30000 + tenMinutes);
        QVERIFY(segmentConcatTimeoutMs(tenMinutes) >= 30000 + tenMinutes);
        QVERIFY(portalGifPassTimeoutMs(tenMinutes) >= 60000 + 4 * tenMinutes);
        QVERIFY(videoMuxTimeoutMs(2 * tenMinutes) > videoMuxTimeoutMs(tenMinutes));
    }

    void timeoutsStayWithinTimerRange()
    {
        const qint64 hundredHours = 100LL * 60 * 60 * 1000;
        QVERIFY(portalGifPassTimeoutMs(hundredHours) > 0);
        QVERIFY(portalGifPassTimeoutMs(hundredHours) <= 24 * 60 * 60 * 1000);
        QVERIFY(videoMuxTimeoutMs(-5) >= 30000);
    }

    void gifConversionUsesSeparatePaletteAndEncodePasses()
    {
        const QStringList palette = portalGifPaletteArguments(
            QStringLiteral("/g/in.mp4"), QStringLiteral("/g/.p.png"), 12);
        QCOMPARE(palette.at(palette.indexOf(QStringLiteral("-i")) + 1), QStringLiteral("/g/in.mp4"));
        QCOMPARE(palette.at(palette.indexOf(QStringLiteral("-vf")) + 1),
                 QStringLiteral("fps=12,palettegen=max_colors=256"));
        QCOMPARE(palette.last(), QStringLiteral("/g/.p.png"));

        const QStringList encode = portalGifEncodeArguments(
            QStringLiteral("/g/in.mp4"), QStringLiteral("/g/.p.png"), 12, 0, QStringLiteral("/g/out.gif"));
        QCOMPARE(encode.count(QStringLiteral("-i")), 2);
        QVERIFY(encode.contains(QStringLiteral("/g/.p.png")));
        QCOMPARE(encode.at(encode.indexOf(QStringLiteral("-lavfi")) + 1),
                 QStringLiteral("fps=12[x];[x][1:v]paletteuse=dither=sierra2_4a"));
        QCOMPARE(encode.at(encode.indexOf(QStringLiteral("-loop")) + 1), QStringLiteral("0"));
        QCOMPARE(encode.last(), QStringLiteral("/g/out.gif"));
        QVERIFY(!encode.join(QLatin1Char(' ')).contains(QStringLiteral("split")));
    }

    void failedGifKeepsVideoNextToIt()
    {
        QCOMPARE(keptPortalVideoPath(QStringLiteral("/g/EShot_GIF_1.gif")),
                 QStringLiteral("/g/EShot_GIF_1.mp4"));
    }
};

QTEST_APPLESS_MAIN(RecordingFinalizationPolicyTests)
#include "RecordingFinalizationPolicyTests.moc"
