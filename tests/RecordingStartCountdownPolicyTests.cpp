#include <QtTest>

#include "recording/RecordingStartCountdownPolicy.h"

class RecordingStartCountdownPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void boundsConfiguredDelay()
    {
        QCOMPARE(recordingStartDelayMs(0), 0);
        QCOMPARE(recordingStartDelayMs(-3), 0);
        QCOMPARE(recordingStartDelayMs(3), 3000);
        QCOMPARE(recordingStartDelayMs(10), 10000);
        QCOMPARE(recordingStartDelayMs(42), 10000);
    }

    void roundsRemainingSecondsUp()
    {
        QCOMPARE(recordingStartCountdownSeconds(3000, 0), 3);
        QCOMPARE(recordingStartCountdownSeconds(3000, 1), 3);
        QCOMPARE(recordingStartCountdownSeconds(3000, 999), 3);
        QCOMPARE(recordingStartCountdownSeconds(3000, 1000), 2);
        QCOMPARE(recordingStartCountdownSeconds(3000, 2999), 1);
        QCOMPARE(recordingStartCountdownSeconds(3000, 3000), 0);
        QCOMPARE(recordingStartCountdownSeconds(3000, 5000), 0);
        QCOMPARE(recordingStartCountdownSeconds(0, 0), 0);
        QCOMPARE(recordingStartCountdownSeconds(2000, -50), 2);
    }

    void centersCountdownInCaptureRegion()
    {
        const QRect rect = recordingStartCountdownRect(
            QRect(100, 100, 800, 600), QRect(0, 0, 1920, 1080), QSize(200, 100));
        QCOMPARE(rect.size(), QSize(200, 100));
        QCOMPARE(rect.center(), QRect(100, 100, 800, 600).center());
    }

    void keepsCountdownOnScreenForSmallOrEdgeRegions()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QRect rect = recordingStartCountdownRect(
            QRect(1900, 1060, 20, 20), screen, QSize(200, 100));
        QVERIFY(screen.contains(rect));
        QCOMPARE(rect.right(), screen.right());
        QCOMPARE(rect.bottom(), screen.bottom());
    }

    void usesVisiblePartOfRegionSpanningScreens()
    {
        const QRect screen(1920, 0, 1920, 1080);
        const QRect rect = recordingStartCountdownRect(
            QRect(1000, 0, 2000, 1080), screen, QSize(200, 100));
        QVERIFY(screen.contains(rect));
        QCOMPARE(rect.center().x(), QRect(1920, 0, 1080, 1080).center().x());
    }

    void fallsBackToScreenCenterWhenRegionIsOffScreen()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QRect rect = recordingStartCountdownRect(
            QRect(5000, 5000, 100, 100), screen, QSize(200, 100));
        QCOMPARE(rect.center(), screen.center());
    }
};

QTEST_APPLESS_MAIN(RecordingStartCountdownPolicyTests)
#include "RecordingStartCountdownPolicyTests.moc"
