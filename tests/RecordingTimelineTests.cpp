#include <QtTest>

#include "recording/RecordingTimeline.h"

class RecordingTimelineTests : public QObject
{
    Q_OBJECT

private slots:
    void excludesPausedTimeFromElapsedDuration()
    {
        RecordingTimeline timeline;
        timeline.start(1000);
        QCOMPARE(timeline.activeElapsedMs(4000), qint64(3000));

        QVERIFY(timeline.pause(4000));
        QCOMPARE(timeline.activeElapsedMs(9000), qint64(3000));

        QVERIFY(timeline.resume(9000));
        QCOMPARE(timeline.activeElapsedMs(11000), qint64(5000));
    }

    void rejectsDuplicatePauseAndResumeRequests()
    {
        RecordingTimeline timeline;
        timeline.start(1000);

        QVERIFY(timeline.pause(1500));
        QVERIFY(!timeline.pause(1600));
        QVERIFY(timeline.resume(2000));
        QVERIFY(!timeline.resume(2100));
        QVERIFY(!timeline.isPaused());
    }

    void gifFrameDelaysDoNotDriftAtThirtyFps()
    {
        GifFrameClock clock;
        clock.start(0);
        qint64 totalCs = 0;
        for (int frame = 1; frame <= 300; ++frame) {
            const int delay = clock.takeDelayCs(frame * 1000 / 30);
            QVERIFY(delay == 3 || delay == 4);
            totalCs += delay;
        }
        // 300 frames at 30 fps last exactly 10 s; rounding each 33 ms frame
        // to 3 cs on its own would have produced 9 s.
        QCOMPARE(totalCs, qint64(1000));
    }

    void gifFrameDelaysCarryRemainderAtFifteenFps()
    {
        GifFrameClock clock;
        clock.start(40);
        qint64 totalCs = 0;
        for (int frame = 1; frame <= 150; ++frame)
            totalCs += clock.takeDelayCs(40 + frame * 1000 / 15);
        QCOMPARE(totalCs, qint64(1000));
    }

    void gifFrameDelayIsNeverZero()
    {
        GifFrameClock clock;
        clock.start(0);
        QCOMPARE(clock.pendingCs(3), 0);
        QCOMPARE(clock.takeDelayCs(3), 1);
        // The borrowed centisecond is absorbed by the next frame.
        QCOMPARE(clock.takeDelayCs(100), 9);
    }
};

QTEST_APPLESS_MAIN(RecordingTimelineTests)
#include "RecordingTimelineTests.moc"
