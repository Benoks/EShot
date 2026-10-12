#include <QtTest>

#include "recording/VideoRecordingCompletionPolicy.h"

#include <QBuffer>
#include <QtEndian>

namespace {
QByteArray box(const char *type, const QByteArray &payload = QByteArray())
{
    QByteArray data(4, '\0');
    qToBigEndian<quint32>(static_cast<quint32>(payload.size() + 8), data.data());
    return data + QByteArray(type, 4) + payload;
}

bool playable(const QByteArray &bytes)
{
    QByteArray copy = bytes;
    QBuffer buffer(&copy);
    buffer.open(QIODevice::ReadOnly);
    return mp4LooksPlayable(buffer);
}
}

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

    void recognizesPlayableFragmentedAndRegularMp4()
    {
        const QByteArray ftyp = box("ftyp", QByteArray("isom", 4));
        const QByteArray moov = box("moov", QByteArray(16, 'm'));
        const QByteArray moof = box("moof", QByteArray(16, 'f'));
        const QByteArray mdat = box("mdat", QByteArray(64, 'd'));
        // Fragmented: header first, then fragments.
        QVERIFY(playable(ftyp + moov + moof + mdat));
        // Killed in the middle of a fragment: earlier fragments still play.
        QVERIFY(playable(ftyp + moov + moof + mdat + moof + mdat.left(20)));
        // Regular MP4 finalized with the header at the end.
        QVERIFY(playable(ftyp + mdat + moov));
        // Killed before the first fragment / before the header was written.
        QVERIFY(!playable(ftyp + moov));
        QVERIFY(!playable(ftyp + mdat));
        QVERIFY(!playable(QByteArray()));
        QVERIFY(!playable(QByteArray("garbage")));
    }

    void salvagesInterruptedButPlayableSegments()
    {
        QCOMPARE(recordingSegmentDisposition(true, false, true), RecordingSegmentDisposition::Keep);
        QCOMPARE(recordingSegmentDisposition(false, false, true), RecordingSegmentDisposition::Salvage);
        QCOMPARE(recordingSegmentDisposition(false, true, true), RecordingSegmentDisposition::Drop);
        QCOMPARE(recordingSegmentDisposition(true, false, false), RecordingSegmentDisposition::Drop);
        QCOMPARE(recordingSegmentDisposition(false, false, false), RecordingSegmentDisposition::Drop);
    }

    void treatsAnEarlyUnrequestedExitAsAStartupFailure()
    {
        QVERIFY(recordingProcessFailedAtStartup(200, false));
        QVERIFY(recordingProcessFailedAtStartup(-1, false));
        QVERIFY(!recordingProcessFailedAtStartup(200, true));
        QVERIFY(!recordingProcessFailedAtStartup(recordingStartupWindowMs + 1, false));
    }
};

QTEST_APPLESS_MAIN(VideoRecordingCompletionPolicyTests)
#include "VideoRecordingCompletionPolicyTests.moc"
