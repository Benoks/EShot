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

    void gdigrabKeepsTheSoftwarePath()
    {
        WindowsCaptureOptions options;
        options.captureRect = QRect(-1920, 0, 641, 481);
        options.crf = 26;
        const QStringList args = windowsFfmpegCaptureArguments(options, QStringLiteral("o.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-f")), QStringLiteral("gdigrab"));
        QCOMPARE(valueAfter(args, QStringLiteral("-offset_x")), QStringLiteral("-1920"));
        QCOMPARE(valueAfter(args, QStringLiteral("-vf")), QStringLiteral("pad=ceil(iw/2)*2:ceil(ih/2)*2"));
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("libx264"));
        QCOMPARE(valueAfter(args, QStringLiteral("-crf")), QStringLiteral("26"));
        QCOMPARE(valueAfter(args, QStringLiteral("-pix_fmt")), QStringLiteral("yuv420p"));
    }

    void ddagrabCapturesThroughLavfiWithHardwareEncoders()
    {
        WindowsCaptureOptions options;
        options.captureRect = QRect(2000, 100, 801, 600);
        options.fps = 60;
        options.crf = 23;
        options.plan.source = WindowsCaptureSource::DdaGrab;
        options.plan.encoder = WindowsVideoEncoder::Nvenc;
        options.plan.outputIndex = 1;
        options.plan.outputOffset = QPoint(80, 100);
        options.plan.ddaSize = QSize(800, 600);
        options.microphoneDshowInput = QStringLiteral("@device_cm_mic");
        options.microphoneVolume = 70;
        QStringList args = windowsFfmpegCaptureArguments(options, QStringLiteral("o.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-f")), QStringLiteral("lavfi"));
        QCOMPARE(valueAfter(args, QStringLiteral("-i")),
                 QStringLiteral("ddagrab=output_idx=1:framerate=60:offset_x=80:offset_y=100"
                                ":video_size=800x600:draw_mouse=1:output_fmt=8bit"));
        QVERIFY(!args.contains(QStringLiteral("gdigrab")));
        QVERIFY(!args.contains(QStringLiteral("-vf")));
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("h264_nvenc"));
        QCOMPARE(valueAfter(args, QStringLiteral("-cq")), QStringLiteral("23"));
        QCOMPARE(valueAfter(args, QStringLiteral("-rc")), QStringLiteral("vbr"));
        // Audio inputs and the fragmented output stay the same.
        QCOMPARE(valueAfter(args, QStringLiteral("-f"), 1), QStringLiteral("dshow"));
        QCOMPARE(valueAfter(args, QStringLiteral("-filter_complex")), QStringLiteral("[1:a]volume=0.70[aout]"));
        QCOMPARE(valueAfter(args, QStringLiteral("-progress")), QStringLiteral("pipe:1"));
        QCOMPARE(valueAfter(args, QStringLiteral("-movflags")),
                 QStringLiteral("+frag_keyframe+empty_moov+default_base_moof"));

        options.plan.encoder = WindowsVideoEncoder::Libx264;
        args = windowsFfmpegCaptureArguments(options, QStringLiteral("o.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-vf")), QStringLiteral("hwdownload,format=bgra,format=yuv420p"));
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("libx264"));
        QCOMPARE(valueAfter(args, QStringLiteral("-crf")), QStringLiteral("23"));
    }

    void encoderArgumentsPerBackend()
    {
        QStringList args = windowsVideoEncodeArguments(WindowsCaptureSource::DdaGrab,
                                                       WindowsVideoEncoder::Amf, 25, false);
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("h264_amf"));
        QCOMPARE(valueAfter(args, QStringLiteral("-rc")), QStringLiteral("cqp"));
        QCOMPARE(valueAfter(args, QStringLiteral("-qp_i")), QStringLiteral("25"));
        QCOMPARE(valueAfter(args, QStringLiteral("-qp_p")), QStringLiteral("25"));

        args = windowsVideoEncodeArguments(WindowsCaptureSource::DdaGrab, WindowsVideoEncoder::Qsv, 25, false);
        QCOMPARE(valueAfter(args, QStringLiteral("-vf")),
                 QStringLiteral("hwmap=derive_device=qsv,format=qsv,vpp_qsv=format=nv12"));
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("h264_qsv"));
        QCOMPARE(valueAfter(args, QStringLiteral("-global_quality")), QStringLiteral("25"));

        // gdigrab frames are in system memory: always libx264.
        args = windowsVideoEncodeArguments(WindowsCaptureSource::GdiGrab, WindowsVideoEncoder::Nvenc, 25, false);
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("libx264"));

        // GIF source: lossless 4:4:4, no padding, for either source.
        args = windowsVideoEncodeArguments(WindowsCaptureSource::GdiGrab, WindowsVideoEncoder::Libx264, 25, true);
        QCOMPARE(valueAfter(args, QStringLiteral("-qp")), QStringLiteral("0"));
        QCOMPARE(valueAfter(args, QStringLiteral("-preset")), QStringLiteral("ultrafast"));
        QCOMPARE(valueAfter(args, QStringLiteral("-pix_fmt")), QStringLiteral("yuv444p"));
        QVERIFY(!args.contains(QStringLiteral("-vf")));
        args = windowsVideoEncodeArguments(WindowsCaptureSource::DdaGrab, WindowsVideoEncoder::Nvenc, 25, true);
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("libx264"));
        QCOMPARE(valueAfter(args, QStringLiteral("-vf")), QStringLiteral("hwdownload,format=bgra,format=yuv444p"));
    }

    void losslessIntermediateHasNoAudio()
    {
        WindowsCaptureOptions options;
        options.captureRect = QRect(0, 0, 320, 240);
        options.fps = 15;
        options.losslessIntermediate = true;
        options.microphoneDshowInput = QStringLiteral("@device_cm_mic");
        options.microphoneVolume = 100;
        const QStringList args = windowsFfmpegCaptureArguments(options, QStringLiteral("g.mp4"));
        QVERIFY(!args.contains(QStringLiteral("dshow")));
        QVERIFY(!args.contains(QStringLiteral("-filter_complex")));
        QCOMPARE(valueAfter(args, QStringLiteral("-framerate")), QStringLiteral("15"));
        QCOMPARE(valueAfter(args, QStringLiteral("-draw_mouse")), QStringLiteral("1"));
        QCOMPARE(valueAfter(args, QStringLiteral("-pix_fmt")), QStringLiteral("yuv444p"));
    }

    void probeEncodesOneDdagrabFrameIntoNull()
    {
        const QStringList args = windowsDdaEncoderProbeArguments(WindowsVideoEncoder::Nvenc);
        QCOMPARE(valueAfter(args, QStringLiteral("-f")), QStringLiteral("lavfi"));
        QVERIFY(valueAfter(args, QStringLiteral("-i")).startsWith(QStringLiteral("ddagrab=output_idx=0:")));
        QCOMPARE(valueAfter(args, QStringLiteral("-frames:v")), QStringLiteral("1"));
        QCOMPARE(valueAfter(args, QStringLiteral("-c:v")), QStringLiteral("h264_nvenc"));
        QCOMPARE(valueAfter(args, QStringLiteral("-f"), 1), QStringLiteral("null"));
        QCOMPARE(args.last(), QStringLiteral("-"));
    }

    void parsesFilterAndEncoderLists()
    {
        const QString filters = QStringLiteral(
            "Filters:\n"
            "  T.. = Timeline support\n"
            "  .S. = Slice threading\n"
            " ... abench            A->A       Benchmark part of a filtergraph.\n"
            " ... ddagrab           |->V       Grab Windows Desktop images using Desktop Duplication API\n");
        QVERIFY(ffmpegListHasFilter(filters, QStringLiteral("ddagrab")));
        QVERIFY(!ffmpegListHasFilter(filters, QStringLiteral("gfxcapture")));
        QVERIFY(!ffmpegListHasFilter(filters, QStringLiteral("Timeline")));

        const QString encoders = QStringLiteral(
            "Encoders:\n"
            " V..... = Video\n"
            " ......\n"
            " ------\n"
            " V....D libx264              libx264 H.264 / AVC / MPEG-4 AVC (codec h264)\n"
            " V....D h264_amf             AMD AMF H.264 Encoder (codec h264)\n"
            " V....D h264_nvenc           NVIDIA NVENC H.264 encoder (codec h264)\n"
            " A....D aac                  AAC (Advanced Audio Coding)\n");
        const QStringList listed = ffmpegListedEncoders(encoders);
        QVERIFY(listed.contains(QStringLiteral("libx264")));
        QVERIFY(listed.contains(QStringLiteral("h264_nvenc")));
        QVERIFY(listed.contains(QStringLiteral("aac")));
        QVERIFY(!listed.contains(QStringLiteral("=")));
        QVERIFY(!listed.contains(QStringLiteral("Video")));
        QCOMPARE(windowsDdaEncoderCandidates(listed),
                 (QList<WindowsVideoEncoder>{WindowsVideoEncoder::Nvenc, WindowsVideoEncoder::Amf,
                                             WindowsVideoEncoder::Libx264}));
        QVERIFY(windowsDdaEncoderCandidates({}).isEmpty());
    }

    void capabilitiesRoundTripThroughSettings()
    {
        WindowsCaptureCapabilities capabilities;
        capabilities.ready = true;
        capabilities.ddaEncoders = {WindowsVideoEncoder::Qsv, WindowsVideoEncoder::Libx264};
        const QString stored = serializeWindowsCaptureCapabilities(capabilities);
        QCOMPARE(stored, QStringLiteral("v1:h264_qsv,libx264"));
        const WindowsCaptureCapabilities parsed = parseWindowsCaptureCapabilities(stored);
        QVERIFY(parsed.ready);
        QCOMPARE(parsed.ddaEncoders, capabilities.ddaEncoders);

        const WindowsCaptureCapabilities none = parseWindowsCaptureCapabilities(QStringLiteral("v1:"));
        QVERIFY(none.ready);
        QVERIFY(!none.ddaGrab());
        QVERIFY(!parseWindowsCaptureCapabilities(QString()).ready);
        QVERIFY(!parseWindowsCaptureCapabilities(QStringLiteral("v0:libx264")).ready);
    }

    void probeCacheKeyChangesWithTheBinary()
    {
        const QString key = windowsCaptureProbeCacheKey(QStringLiteral("C:/EShot/ffmpeg.exe"), 1000, 500);
        QVERIFY(key.startsWith(QStringLiteral("recordingCaptureProbe/")));
        QCOMPARE(key, windowsCaptureProbeCacheKey(QStringLiteral("C:/EShot/ffmpeg.exe"), 1000, 500));
        QVERIFY(key != windowsCaptureProbeCacheKey(QStringLiteral("C:/EShot/ffmpeg.exe"), 2000, 500));
        QVERIFY(key != windowsCaptureProbeCacheKey(QStringLiteral("C:/EShot/ffmpeg.exe"), 1000, 501));
        QVERIFY(key != windowsCaptureProbeCacheKey(QStringLiteral("D:/ffmpeg.exe"), 1000, 500));
    }

    void mapsCaptureRectToItsDxgiOutput()
    {
        const QList<DxgiOutputInfo> outputs = {
            {0, 0, QRect(0, 0, 2560, 1440), false},
            {0, 1, QRect(-1920, 200, 1920, 1080), false},
            {1, 0, QRect(2560, 0, 1920, 1080), false},
            {0, 2, QRect(0, -1080, 1920, 1080), true},
        };
        DdaOutputMapping mapping = ddaOutputForCaptureRect(QRect(100, 50, 801, 601), outputs);
        QVERIFY(mapping.valid);
        QCOMPARE(mapping.outputIndex, 0);
        QCOMPARE(mapping.offset, QPoint(100, 50));
        QCOMPARE(mapping.size, QSize(800, 600));

        mapping = ddaOutputForCaptureRect(QRect(-1820, 300, 640, 480), outputs);
        QVERIFY(mapping.valid);
        QCOMPARE(mapping.outputIndex, 1);
        QCOMPARE(mapping.offset, QPoint(100, 100));

        // Full monitor.
        mapping = ddaOutputForCaptureRect(QRect(0, 0, 2560, 1440), outputs);
        QVERIFY(mapping.valid);
        QCOMPARE(mapping.size, QSize(2560, 1440));

        // Spanning two monitors, on the second adapter, on a rotated one.
        QVERIFY(!ddaOutputForCaptureRect(QRect(2400, 0, 400, 300), outputs).valid);
        QVERIFY(!ddaOutputForCaptureRect(QRect(2600, 10, 400, 300), outputs).valid);
        QVERIFY(!ddaOutputForCaptureRect(QRect(10, -500, 400, 300), outputs).valid);
        QVERIFY(!ddaOutputForCaptureRect(QRect(10, 10, 400, 300), {}).valid);
    }

    void choosesDdagrabAndTheBestWorkingEncoder()
    {
        const QList<DxgiOutputInfo> outputs = {
            {0, 0, QRect(0, 0, 1920, 1080), false},
            {0, 1, QRect(1920, 0, 5120, 2880), false},
        };
        WindowsCaptureCapabilities capabilities;
        capabilities.ready = true;
        capabilities.ddaEncoders = {WindowsVideoEncoder::Nvenc, WindowsVideoEncoder::Libx264};
        const QRect rect(10, 20, 1280, 720);

        WindowsVideoPlan plan = chooseWindowsVideoPlan(rect, capabilities, outputs, {});
        QCOMPARE(plan.source, WindowsCaptureSource::DdaGrab);
        QCOMPARE(plan.encoder, WindowsVideoEncoder::Nvenc);
        QCOMPARE(plan.outputIndex, 0);
        QCOMPARE(plan.outputOffset, QPoint(10, 20));
        QCOMPARE(plan.ddaSize, QSize(1280, 720));
        QCOMPARE(describeWindowsVideoPlan(plan), QStringLiteral("ddagrab output 0 +10+20 1280x720, h264_nvenc"));

        WindowsVideoPlanRequest request;
        request.allowHardwareEncoder = false;
        plan = chooseWindowsVideoPlan(rect, capabilities, outputs, request);
        QCOMPARE(plan.source, WindowsCaptureSource::DdaGrab);
        QCOMPARE(plan.encoder, WindowsVideoEncoder::Libx264);

        request = WindowsVideoPlanRequest();
        request.losslessIntermediate = true;
        plan = chooseWindowsVideoPlan(rect, capabilities, outputs, request);
        QCOMPARE(plan.source, WindowsCaptureSource::DdaGrab);
        QCOMPARE(plan.encoder, WindowsVideoEncoder::Libx264);

        // Wider than the hardware encoders allow: libx264 on ddagrab.
        plan = chooseWindowsVideoPlan(QRect(1920, 0, 5120, 2880), capabilities, outputs, {});
        QCOMPARE(plan.source, WindowsCaptureSource::DdaGrab);
        QCOMPARE(plan.encoder, WindowsVideoEncoder::Libx264);

        request = WindowsVideoPlanRequest();
        request.allowDdaGrab = false;
        plan = chooseWindowsVideoPlan(rect, capabilities, outputs, request);
        QCOMPARE(plan.source, WindowsCaptureSource::GdiGrab);
        QCOMPARE(describeWindowsVideoPlan(plan), QStringLiteral("gdigrab, libx264"));

        // Only a hardware encoder works, but it cannot take this size.
        WindowsCaptureCapabilities hardwareOnly = capabilities;
        hardwareOnly.ddaEncoders = {WindowsVideoEncoder::Amf};
        QCOMPARE(chooseWindowsVideoPlan(QRect(1920, 0, 5120, 2880), hardwareOnly, outputs, {}).source,
                 WindowsCaptureSource::GdiGrab);
        request = WindowsVideoPlanRequest();
        request.losslessIntermediate = true;
        QCOMPARE(chooseWindowsVideoPlan(rect, hardwareOnly, outputs, request).source,
                 WindowsCaptureSource::GdiGrab);

        // Probe not finished, no ddagrab, region across monitors: gdigrab.
        QCOMPARE(chooseWindowsVideoPlan(rect, WindowsCaptureCapabilities(), outputs, {}).source,
                 WindowsCaptureSource::GdiGrab);
        WindowsCaptureCapabilities noDda;
        noDda.ready = true;
        QCOMPARE(chooseWindowsVideoPlan(rect, noDda, outputs, {}).source, WindowsCaptureSource::GdiGrab);
        QCOMPARE(chooseWindowsVideoPlan(QRect(1800, 0, 400, 300), capabilities, outputs, {}).source,
                 WindowsCaptureSource::GdiGrab);
    }

    void readsProgressOutTime()
    {
        QCOMPARE(ffmpegProgressOutTimeUs(QStringLiteral("out_time_us=2500000")), Q_INT64_C(2500000));
        QCOMPARE(ffmpegProgressOutTimeUs(QStringLiteral("out_time_us=N/A")), Q_INT64_C(-1));
        QCOMPARE(ffmpegProgressOutTimeUs(QStringLiteral("out_time_ms=2500000")), Q_INT64_C(-1));
        QCOMPARE(ffmpegProgressOutTimeUs(QStringLiteral("frame=12")), Q_INT64_C(-1));
    }

    void alignsEachAudioSpanToItsSegment()
    {
        // 48 kHz. Span 0: 2.5 s of audio for a 2.0 s segment (FFmpeg started
        // late): the first 0.5 s is dropped. Span 1: 1.0 s of audio for a
        // 1.2 s segment: padded with 0.2 s of silence at its start.
        const QList<SystemAudioSpan> spans = {
            {0, 120000, 2000},
            {120000, 168000, 1200},
        };
        const QString filter = systemAudioSpanFilter(spans, 48000, 0, QStringLiteral("out"));
        QCOMPARE(filter,
                 QStringLiteral("[1:a]asplit=2[r0][r1];"
                                "[r0]atrim=start_sample=24000:end_sample=120000,asetpts=PTS-STARTPTS[s0];"
                                "[r1]atrim=start_sample=120000:end_sample=168000,asetpts=PTS-STARTPTS,"
                                "adelay=delays=9600S:all=1[s1];"
                                "[s0][s1]concat=n=2:v=0:a=1[out]"));
    }

    void spanWithoutKnownLengthUsesTheLeadOnlyFirst()
    {
        QCOMPARE(systemAudioSpanFilter({{0, 96000, -1}}, 48000, 250, QStringLiteral("d")),
                 QStringLiteral("[1:a]atrim=start_sample=12000:end_sample=96000,asetpts=PTS-STARTPTS[d]"));
        QCOMPARE(systemAudioSpanFilter({{0, 96000, -1}}, 48000, -100, QStringLiteral("d")),
                 QStringLiteral("[1:a]atrim=start_sample=0:end_sample=96000,asetpts=PTS-STARTPTS,"
                                "adelay=delays=4800S:all=1[d]"));
        const QString filter = systemAudioSpanFilter({{0, 48000, 1000}, {48000, 96000, -1}}, 48000, 250,
                                                     QStringLiteral("d"));
        QVERIFY(filter.contains(QStringLiteral("[r1]atrim=start_sample=48000:end_sample=96000,asetpts=PTS-STARTPTS[s1]")));
        // Empty spans are left out; nothing left means no filter.
        QCOMPARE(systemAudioSpanFilter({{500, 500, 1000}}, 48000, 0, QStringLiteral("d")), QString());
        QCOMPARE(systemAudioSpanFilter({}, 48000, 0, QStringLiteral("d")), QString());
        QCOMPARE(systemAudioSpanFilter({{0, 10, 1000}}, 0, 0, QStringLiteral("d")), QString());
    }

    void spanMuxKeepsVolumeMixAndPadding()
    {
        QStringList args = systemAudioSpanMuxArguments(QStringLiteral("v.mp4"), QStringLiteral("a.wav"), 80, true,
                                                       {{0, 48000, 900}, {48000, 96000, 1000}}, 48000, 0,
                                                       QStringLiteral("out.mp4"));
        const QString filter = valueAfter(args, QStringLiteral("-filter_complex"));
        QVERIFY(filter.contains(QStringLiteral("concat=n=2:v=0:a=1[dsync];[dsync]volume=0.80[desk];")));
        QVERIFY(filter.endsWith(QStringLiteral("[0:a][desk]amix=inputs=2:duration=longest:normalize=0,apad[aout]")));
        QVERIFY(args.contains(QStringLiteral("-shortest")));
        QCOMPARE(args.last(), QStringLiteral("out.mp4"));

        args = systemAudioSpanMuxArguments(QStringLiteral("v.mp4"), QStringLiteral("a.wav"), 100, false,
                                           {{0, 48000, 1000}}, 48000, 0, QStringLiteral("out.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-filter_complex")),
                 QStringLiteral("[1:a]atrim=start_sample=0:end_sample=48000,asetpts=PTS-STARTPTS[dsync];"
                                "[dsync]volume=1.00,apad[aout]"));

        // Nothing usable: the start-only alignment.
        args = systemAudioSpanMuxArguments(QStringLiteral("v.mp4"), QStringLiteral("a.wav"), 100, false,
                                           {}, 48000, 250, QStringLiteral("out.mp4"));
        QCOMPARE(valueAfter(args, QStringLiteral("-filter_complex")),
                 QStringLiteral("[1:a]atrim=start=0.250,asetpts=PTS-STARTPTS,volume=1.00,apad[aout]"));
    }
};

QTEST_APPLESS_MAIN(WindowsRecordingSupportTests)
#include "WindowsRecordingSupportTests.moc"
