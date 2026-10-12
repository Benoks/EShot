#include <QtTest>

#include "recording/WindowsRecordingSupport.h"

namespace {
QString valueAfter(const QStringList &args, const QString &flag, int occurrence = 0)
{
    int index = -1;
    for (int i = 0; i <= occurrence; ++i) {
        index = args.indexOf(flag, index + 1);
        if (index < 0)
            return QString();
    }
    return index + 1 < args.size() ? args.at(index + 1) : QString();
}

quint32 le32(const QByteArray &data, int offset)
{
    return static_cast<quint32>(static_cast<uchar>(data.at(offset)))
        | (static_cast<quint32>(static_cast<uchar>(data.at(offset + 1))) << 8)
        | (static_cast<quint32>(static_cast<uchar>(data.at(offset + 2))) << 16)
        | (static_cast<quint32>(static_cast<uchar>(data.at(offset + 3))) << 24);
}

quint64 le64(const QByteArray &data, int offset)
{
    return static_cast<quint64>(le32(data, offset))
        | (static_cast<quint64>(le32(data, offset + 4)) << 32);
}

// WAVEFORMATEXTENSIBLE-sized format: 48 kHz, 2 ch, 32-bit float (block align 8).
QByteArray floatStereoFormat()
{
    QByteArray format(40, '\0');
    format[0] = static_cast<char>(0xFE);
    format[1] = static_cast<char>(0xFF);
    format[2] = 2;
    const quint32 rate = 48000;
    for (int i = 0; i < 4; ++i)
        format[4 + i] = static_cast<char>((rate >> (8 * i)) & 0xff);
    format[12] = 8;
    format[14] = 32;
    format[16] = 22;
    return format;
}
}

class WindowsRecordingSupportTests : public QObject
{
    Q_OBJECT

private slots:
    void captureWritesFragmentedMp4AndReportsProgress()
    {
        WindowsCaptureOptions options;
        options.captureRect = QRect(10, 20, 640, 480);
        options.fps = 30;
        options.crf = 23;
        const QStringList args = windowsFfmpegCaptureArguments(options, QStringLiteral("C:/v/out.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-progress")), QStringLiteral("pipe:1"));
        QVERIFY(args.contains(QStringLiteral("-nostats")));
        QCOMPARE(valueAfter(args, QStringLiteral("-thread_queue_size")), QStringLiteral("1024"));
        QCOMPARE(valueAfter(args, QStringLiteral("-f")), QStringLiteral("gdigrab"));
        QCOMPARE(valueAfter(args, QStringLiteral("-offset_x")), QStringLiteral("10"));
        QCOMPARE(valueAfter(args, QStringLiteral("-video_size")), QStringLiteral("640x480"));
        QCOMPARE(valueAfter(args, QStringLiteral("-movflags")),
                 QStringLiteral("+frag_keyframe+empty_moov+default_base_moof"));
        QVERIFY(!args.contains(QStringLiteral("dshow")));
        QVERIFY(!args.contains(QStringLiteral("-filter_complex")));
        QCOMPARE(args.last(), QStringLiteral("C:/v/out.mp4"));
    }

    void captureAddsBufferedDshowInputs()
    {
        WindowsCaptureOptions options;
        options.captureRect = QRect(0, 0, 100, 100);
        options.desktopDshowInput = QStringLiteral("@device_cm_mix");
        options.desktopVolume = 50;
        options.microphoneDshowInput = QStringLiteral("@device_cm_mic");
        options.microphoneVolume = 100;
        const QStringList args = windowsFfmpegCaptureArguments(options, QStringLiteral("out.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-f"), 1), QStringLiteral("dshow"));
        QCOMPARE(valueAfter(args, QStringLiteral("-audio_buffer_size")), QStringLiteral("50"));
        QCOMPARE(valueAfter(args, QStringLiteral("-thread_queue_size"), 2), QStringLiteral("1024"));
        QCOMPARE(valueAfter(args, QStringLiteral("-i"), 1), QStringLiteral("audio=@device_cm_mix"));
        QCOMPARE(valueAfter(args, QStringLiteral("-i"), 2), QStringLiteral("audio=@device_cm_mic"));
        const QString filter = valueAfter(args, QStringLiteral("-filter_complex"));
        QVERIFY(filter.contains(QStringLiteral("[1:a]volume=0.50[a1]")));
        QVERIFY(filter.contains(QStringLiteral("[2:a]volume=1.00[a2]")));
        QVERIFY(filter.contains(QStringLiteral("normalize=0")));
    }

    void silentDshowInputIsLeftOut()
    {
        WindowsCaptureOptions options;
        options.captureRect = QRect(0, 0, 100, 100);
        options.microphoneDshowInput = QStringLiteral("@device_cm_mic");
        options.microphoneVolume = 0;
        QVERIFY(!windowsFfmpegCaptureArguments(options, QStringLiteral("o.mp4")).contains(QStringLiteral("dshow")));
    }

    void singleAudioInputIsNotMixed()
    {
        const QStringList args = ffmpegAudioMixArguments({qMakePair(1, 80)});
        QCOMPARE(valueAfter(args, QStringLiteral("-filter_complex")), QStringLiteral("[1:a]volume=0.80[aout]"));
        QCOMPARE(valueAfter(args, QStringLiteral("-c:a")), QStringLiteral("aac"));
        QVERIFY(ffmpegAudioMixArguments({}).isEmpty());
    }

    void muxAlignsLoopbackAudioAndKeepsLevels()
    {
        QStringList args = systemAudioMuxArguments(QStringLiteral("v.mp4"), QStringLiteral("a.wav"),
                                                   80, true, 250, QStringLiteral("out.mp4"));
        QString filter = valueAfter(args, QStringLiteral("-filter_complex"));
        QVERIFY(filter.startsWith(QStringLiteral("[1:a]atrim=start=0.250,asetpts=PTS-STARTPTS,volume=0.80[desk]")));
        QVERIFY(filter.contains(QStringLiteral("amix=inputs=2:duration=longest:normalize=0,apad[aout]")));
        QCOMPARE(valueAfter(args, QStringLiteral("-movflags")), QStringLiteral("+faststart"));
        QVERIFY(args.contains(QStringLiteral("-shortest")));
        QCOMPARE(args.last(), QStringLiteral("out.mp4"));

        args = systemAudioMuxArguments(QStringLiteral("v.mp4"), QStringLiteral("a.wav"),
                                       100, false, -120, QStringLiteral("out.mp4"));
        filter = valueAfter(args, QStringLiteral("-filter_complex"));
        QCOMPARE(filter, QStringLiteral("[1:a]adelay=delays=120:all=1,volume=1.00,apad[aout]"));

        args = systemAudioMuxArguments(QStringLiteral("v.mp4"), QStringLiteral("a.wav"),
                                       100, false, 0, QStringLiteral("out.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-filter_complex")), QStringLiteral("[1:a]volume=1.00,apad[aout]"));
    }

    void recognizesProgressOutput()
    {
        QVERIFY(isFfmpegProgressLine(QStringLiteral("frame=12")));
        QVERIFY(isFfmpegProgressLine(QStringLiteral("out_time_us=N/A")));
        QVERIFY(isFfmpegProgressLine(QStringLiteral("progress=continue\r")));
        QVERIFY(isFfmpegProgressBlockEnd(QStringLiteral("progress=continue")));
        QVERIFY(isFfmpegProgressBlockEnd(QStringLiteral("progress=end")));
        QVERIFY(!isFfmpegProgressBlockEnd(QStringLiteral("frame=1")));
        QVERIFY(!isFfmpegProgressLine(QStringLiteral("[dshow @ 01] Could not find audio only device with name [x]")));
        QVERIFY(!isFfmpegProgressLine(QStringLiteral("Press [q] to stop, [?] for help")));
    }

    void readsGdigrabWallClockStart()
    {
        QCOMPARE(ffmpegInputStartEpochMs(
                     QStringLiteral("  Duration: N/A, start: 1791800000.123456, bitrate: 1990656 kb/s")),
                 Q_INT64_C(1791800000123));
        QCOMPARE(ffmpegInputStartEpochMs(QStringLiteral("  Duration: N/A, start: 0.000000, bitrate: N/A")),
                 Q_INT64_C(-1));
        QCOMPARE(ffmpegInputStartEpochMs(QStringLiteral("Input #0, gdigrab, from 'desktop':")), Q_INT64_C(-1));
    }

    void audioLeadIsBoundedAndNeedsBothStarts()
    {
        QCOMPARE(systemAudioLeadMs(1000000, 1000300), Q_INT64_C(300));
        QCOMPARE(systemAudioLeadMs(1000300, 1000000), Q_INT64_C(-300));
        QCOMPARE(systemAudioLeadMs(-1, 1000000), Q_INT64_C(0));
        QCOMPARE(systemAudioLeadMs(1000000, -1), Q_INT64_C(0));
        QCOMPARE(systemAudioLeadMs(1000000, 1000000 + 60000), Q_INT64_C(0));
    }

    void failureReasonUsesLastLogLines()
    {
        const QStringList tail = {
            QStringLiteral("Input #0, gdigrab, from 'desktop':"),
            QStringLiteral("frame=0"),
            QStringLiteral("[dshow @ 01] Could not find audio only device with name [Mic] among source devices of type audio."),
            QStringLiteral("[dshow @ 01] Searching for audio device within video devices for Mic"),
            QStringLiteral("[in#2 @ 02] Error opening input: I/O error"),
            QStringLiteral("progress=end"),
        };
        const QString reason = ffmpegFailureReason(tail);
        QVERIFY(reason.startsWith(QStringLiteral("[dshow @ 01] Could not find")));
        QVERIFY(reason.endsWith(QStringLiteral("I/O error")));
        QVERIFY(ffmpegFailureReason({}).isEmpty());
    }

    void stopGraceGrowsWithPixelRate()
    {
        QCOMPARE(windowsEncoderStopGraceMs(QSize(1280, 720), 30), 15000);
        QCOMPARE(windowsEncoderStopGraceMs(QSize(1920, 1080), 30), 15000);
        QCOMPARE(windowsEncoderStopGraceMs(QSize(1920, 1080), 60), 30000);
        QCOMPARE(windowsEncoderStopGraceMs(QSize(3840, 2160), 60), 60000);
        QCOMPARE(windowsEncoderStopGraceMs(QSize(), 0), 15000);
    }

    void wavHeaderIsPlainRiffBelowFourGigabytes()
    {
        const QByteArray format = floatStereoFormat();
        const QByteArray header = wavFileHeader(format, 384000);
        QCOMPARE(header.size(), wavHeaderSize(format));
        QCOMPARE(header.size(), 12 + 36 + 8 + 40 + 8);
        QCOMPARE(header.left(4), QByteArray("RIFF"));
        QCOMPARE(le32(header, 4), quint32(header.size() - 8 + 384000));
        QCOMPARE(header.mid(8, 4), QByteArray("WAVE"));
        QCOMPARE(header.mid(12, 4), QByteArray("JUNK"));
        QCOMPARE(header.mid(48, 4), QByteArray("fmt "));
        QCOMPARE(le32(header, 52), quint32(40));
        QCOMPARE(header.mid(56, 40), format);
        QCOMPARE(header.mid(96, 4), QByteArray("data"));
        QCOMPARE(le32(header, 100), quint32(384000));
    }

    void wavHeaderSwitchesToRf64BeyondFourGigabytes()
    {
        const QByteArray format = floatStereoFormat();
        const quint64 dataBytes = Q_UINT64_C(5) * 1024 * 1024 * 1024;
        const QByteArray header = wavFileHeader(format, dataBytes);
        QCOMPARE(header.size(), wavHeaderSize(format));
        QCOMPARE(header.left(4), QByteArray("RF64"));
        QCOMPARE(le32(header, 4), quint32(0xFFFFFFFFu));
        QCOMPARE(header.mid(12, 4), QByteArray("ds64"));
        QCOMPARE(le32(header, 16), quint32(28));
        QCOMPARE(le64(header, 20), quint64(header.size() - 8) + dataBytes);
        QCOMPARE(le64(header, 28), dataBytes);
        QCOMPARE(le64(header, 36), dataBytes / 8);
        QCOMPARE(le32(header, 100), quint32(0xFFFFFFFFu));

        // The boundary: exactly what still fits in a 32-bit RIFF size.
        const quint64 maxRiffData = 0xFFFFFFFFull - quint64(header.size() - 8);
        QCOMPARE(wavFileHeader(format, maxRiffData).left(4), QByteArray("RIFF"));
        QCOMPARE(wavFileHeader(format, maxRiffData + 1).left(4), QByteArray("RF64"));
    }

    void wavHeaderPadsOddFormatChunks()
    {
        const QByteArray format(19, '\x01');
        const QByteArray header = wavFileHeader(format, 0);
        QCOMPARE(header.size(), wavHeaderSize(format));
        QCOMPARE(header.size() % 2, 0);
        QCOMPARE(le32(header, 52), quint32(19));
        QCOMPARE(header.mid(56 + 20, 4), QByteArray("data"));
    }
};

QTEST_APPLESS_MAIN(WindowsRecordingSupportTests)
#include "WindowsRecordingSupportTests.moc"
