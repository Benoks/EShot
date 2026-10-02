#include <QtTest>

#include "recording/RecordingSegments.h"

class RecordingSegmentsTests : public QObject
{
    Q_OBJECT

private slots:
    void segmentPathsAreHiddenAndSortChronologically()
    {
        const QString first = recordingSegmentPath(QStringLiteral("/videos"), QStringLiteral("20260102_030405_006"), 0);
        const QString tenth = recordingSegmentPath(QStringLiteral("/videos"), QStringLiteral("20260102_030405_006"), 9);
        const QString eleventh = recordingSegmentPath(QStringLiteral("/videos"), QStringLiteral("20260102_030405_006"), 10);
        QCOMPARE(first, QStringLiteral("/videos/.eshot_segment_20260102_030405_006_000.mp4"));
        QVERIFY(tenth < eleventh);
        QCOMPARE(recordingSegmentPattern(QStringLiteral("/videos"), QStringLiteral("20260102_030405_006")),
                 QStringLiteral("/videos/.eshot_segment_20260102_030405_006_*.mp4"));
    }

    void partPathsAreVisibleNextToTheOutput()
    {
        QCOMPARE(recordingPartPath(QStringLiteral("/videos/EShot_Video_1.mp4"), 2),
                 QStringLiteral("/videos/EShot_Video_1_part2.mp4"));
        QCOMPARE(recordingPartPath(QStringLiteral("/gifs/EShot_GIF_1.gif"), 1),
                 QStringLiteral("/gifs/EShot_GIF_1_part1.mp4"));
    }

    void concatListQuotesEveryPath()
    {
        const QByteArray list = ffmpegConcatList({
            QStringLiteral("/videos/a.mp4"),
            QStringLiteral("/videos/it's here.mp4")
        });
        QCOMPARE(list, QByteArray("ffconcat version 1.0\n"
                                  "file '/videos/a.mp4'\n"
                                  "file '/videos/it'\\''s here.mp4'\n"));
    }

    void concatListKeepsNonAsciiPathsAsUtf8()
    {
        const QByteArray list = ffmpegConcatList({QStringLiteral("/home/u/Masaüstü/a.mp4")});
        QVERIFY(list.contains(QStringLiteral("Masaüstü").toUtf8()));
    }

    void ffmpegConcatCopiesStreams()
    {
        const QStringList args = ffmpegConcatArguments(QStringLiteral("/v/list.txt"), QStringLiteral("/v/out.mp4"));
        QVERIFY(args.contains(QStringLiteral("concat")));
        QCOMPARE(args.at(args.indexOf(QStringLiteral("-safe")) + 1), QStringLiteral("0"));
        QCOMPARE(args.at(args.indexOf(QStringLiteral("-i")) + 1), QStringLiteral("/v/list.txt"));
        QCOMPARE(args.at(args.indexOf(QStringLiteral("-c")) + 1), QStringLiteral("copy"));
        QCOMPARE(args.last(), QStringLiteral("/v/out.mp4"));
    }

    void gstConcatAddsAudioBranchOnlyWhenRecorded()
    {
        const QStringList videoOnly = gstConcatArguments(QStringLiteral("/v/.eshot_segment_x_*.mp4"),
                                                         QStringLiteral("/v/out.mp4"), false);
        QVERIFY(videoOnly.contains(QStringLiteral("splitmuxsrc")));
        QVERIFY(videoOnly.contains(QStringLiteral("location=/v/.eshot_segment_x_*.mp4")));
        QVERIFY(videoOnly.contains(QStringLiteral("location=/v/out.mp4")));
        QVERIFY(!videoOnly.contains(QStringLiteral("src.audio_0")));

        const QStringList withAudio = gstConcatArguments(QStringLiteral("/v/.eshot_segment_x_*.mp4"),
                                                         QStringLiteral("/v/out.mp4"), true);
        QVERIFY(withAudio.contains(QStringLiteral("src.audio_0")));
        QVERIFY(withAudio.contains(QStringLiteral("aacparse")));
    }
};

QTEST_APPLESS_MAIN(RecordingSegmentsTests)
#include "RecordingSegmentsTests.moc"
