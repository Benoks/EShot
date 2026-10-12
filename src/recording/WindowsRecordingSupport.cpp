#include "WindowsRecordingSupport.h"

#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>

namespace {
QString volumeFactor(int percent)
{
    return QString::number(qBound(0, percent, 100) / 100.0, 'f', 2);
}

QString seconds(qint64 ms)
{
    return QString::number(ms / 1000.0, 'f', 3);
}

void appendLe32(QByteArray &out, quint32 value)
{
    for (int shift = 0; shift < 32; shift += 8)
        out.append(static_cast<char>((value >> shift) & 0xff));
}

void appendLe64(QByteArray &out, quint64 value)
{
    for (int shift = 0; shift < 64; shift += 8)
        out.append(static_cast<char>((value >> shift) & 0xff));
}

// ds64 payload: RIFF size, data size, sample count (64-bit each) and an empty
// chunk size table.
constexpr int Ds64PayloadSize = 28;
}

QStringList ffmpegAudioMixArguments(const QList<QPair<int, int>> &inputs)
{
    QStringList args;
    if (inputs.size() == 1) {
        args << QStringLiteral("-filter_complex")
             << QStringLiteral("[%1:a]volume=%2[aout]")
                    .arg(inputs.first().first)
                    .arg(volumeFactor(inputs.first().second));
    } else if (inputs.size() == 2) {
        // amix divides every input by the input count unless normalize=0,
        // which halved the level of both sources.
        args << QStringLiteral("-filter_complex")
             << QStringLiteral("[%1:a]volume=%2[a1];[%3:a]volume=%4[a2];"
                               "[a1][a2]amix=inputs=2:duration=longest:normalize=0[aout]")
                    .arg(inputs.at(0).first)
                    .arg(volumeFactor(inputs.at(0).second))
                    .arg(inputs.at(1).first)
                    .arg(volumeFactor(inputs.at(1).second));
    } else {
        return args;
    }
    args << QStringLiteral("-map") << QStringLiteral("0:v")
         << QStringLiteral("-map") << QStringLiteral("[aout]")
         << QStringLiteral("-c:a") << QStringLiteral("aac")
         << QStringLiteral("-b:a") << QStringLiteral("160k");
    return args;
}

QStringList windowsFfmpegCaptureArguments(const WindowsCaptureOptions &options,
                                          const QString &outputPath)
{
    const QRect &rect = options.captureRect;
    const bool ddaGrab = options.plan.source == WindowsCaptureSource::DdaGrab;
    QStringList args;
    // Info level prints the input dump (with gdigrab's wall-clock start time)
    // and the real error on failure; -nostats keeps the periodic status line
    // out of stderr, progress goes to stdout instead.
    args << QStringLiteral("-y")
         << QStringLiteral("-hide_banner")
         << QStringLiteral("-loglevel") << QStringLiteral("info")
         << QStringLiteral("-nostats")
         << QStringLiteral("-progress") << QStringLiteral("pipe:1")
         << QStringLiteral("-thread_queue_size") << QStringLiteral("1024");
    if (ddaGrab) {
        // Desktop Duplication source filter: frames stay in D3D11 textures.
        // 8bit keeps BGRA on HDR desktops, where "auto" would switch formats.
        const QSize size = options.plan.ddaSize;
        args << QStringLiteral("-f") << QStringLiteral("lavfi")
             << QStringLiteral("-i")
             << QStringLiteral("ddagrab=output_idx=%1:framerate=%2:offset_x=%3:offset_y=%4"
                               ":video_size=%5x%6:draw_mouse=1:output_fmt=8bit")
                    .arg(options.plan.outputIndex)
                    .arg(options.fps)
                    .arg(options.plan.outputOffset.x())
                    .arg(options.plan.outputOffset.y())
                    .arg(size.width())
                    .arg(size.height());
    } else {
        args << QStringLiteral("-f") << QStringLiteral("gdigrab")
             << QStringLiteral("-draw_mouse") << QStringLiteral("1")
             << QStringLiteral("-framerate") << QString::number(options.fps)
             << QStringLiteral("-offset_x") << QString::number(rect.x())
             << QStringLiteral("-offset_y") << QString::number(rect.y())
             << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(rect.width()).arg(rect.height())
             << QStringLiteral("-i") << QStringLiteral("desktop");
    }

    QList<QPair<int, int>> audioInputs;
    int nextInputIndex = 1;
    auto addDshowInput = [&](const QString &name, int volume) {
        if (options.losslessIntermediate || name.isEmpty() || volume <= 0)
            return;
        args << QStringLiteral("-thread_queue_size") << QStringLiteral("1024")
             << QStringLiteral("-f") << QStringLiteral("dshow")
             << QStringLiteral("-audio_buffer_size") << QStringLiteral("50")
             << QStringLiteral("-i") << QStringLiteral("audio=%1").arg(name);
        audioInputs.append(qMakePair(nextInputIndex++, volume));
    };
    addDshowInput(options.desktopDshowInput, options.desktopVolume);
    addDshowInput(options.microphoneDshowInput, options.microphoneVolume);

    args << windowsVideoEncodeArguments(options.plan.source, options.plan.encoder,
                                        options.crf, options.losslessIntermediate);
    args << ffmpegAudioMixArguments(audioInputs);
    // Fragmented MP4 (a fragment at least every second): a killed encoder
    // loses the last fragment instead of the whole file. The join/mux step
    // rewrites it as a regular MP4 with +faststart.
    args << QStringLiteral("-movflags") << QStringLiteral("+frag_keyframe+empty_moov+default_base_moof")
         << QStringLiteral("-frag_duration") << QStringLiteral("1000000")
         << outputPath;
    return args;
}

namespace {
// `desktop` is the filter chain of the loopback audio, without an output label.
QStringList systemAudioMuxArgumentsWithChain(const QString &videoPath, const QString &audioPath,
                                             const QString &desktop, bool videoHasAudio,
                                             const QString &outputPath)
{
    QStringList args = {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), videoPath,
        QStringLiteral("-i"), audioPath
    };

    // apad makes the audio endless so -shortest always ends on the video:
    // a short system-audio track must never truncate the recording.
    if (videoHasAudio) {
        args << QStringLiteral("-filter_complex")
             << QStringLiteral("%1[desk];[0:a][desk]amix=inputs=2:duration=longest:normalize=0,apad[aout]")
                    .arg(desktop);
    } else {
        args << QStringLiteral("-filter_complex")
             << QStringLiteral("%1,apad[aout]").arg(desktop);
    }
    args << QStringLiteral("-map") << QStringLiteral("0:v")
         << QStringLiteral("-map") << QStringLiteral("[aout]")
         << QStringLiteral("-c:v") << QStringLiteral("copy")
         << QStringLiteral("-c:a") << QStringLiteral("aac")
         << QStringLiteral("-b:a") << QStringLiteral("160k")
         << QStringLiteral("-shortest")
         << QStringLiteral("-movflags") << QStringLiteral("+faststart")
         << outputPath;
    return args;
}
}

QStringList systemAudioMuxArguments(const QString &videoPath, const QString &audioPath,
                                    int desktopVolume, bool videoHasAudio,
                                    qint64 audioLeadMs, const QString &outputPath)
{
    // Line the loopback audio up with the first video frame: drop what was
    // captured before it, or pad with silence when audio started later.
    QString align;
    if (audioLeadMs > 0)
        align = QStringLiteral("atrim=start=%1,asetpts=PTS-STARTPTS,").arg(seconds(audioLeadMs));
    else if (audioLeadMs < 0)
        align = QStringLiteral("adelay=delays=%1:all=1,").arg(-audioLeadMs);
    const QString desktop = QStringLiteral("[1:a]%1volume=%2").arg(align, volumeFactor(desktopVolume));
    return systemAudioMuxArgumentsWithChain(videoPath, audioPath, desktop, videoHasAudio, outputPath);
}

QStringList systemAudioSpanMuxArguments(const QString &videoPath, const QString &audioPath,
                                        int desktopVolume, bool videoHasAudio,
                                        const QList<SystemAudioSpan> &spans, int sampleRate,
                                        qint64 firstSpanLeadMs, const QString &outputPath)
{
    const QString aligned = systemAudioSpanFilter(spans, sampleRate, firstSpanLeadMs,
                                                  QStringLiteral("dsync"));
    if (aligned.isEmpty()) {
        return systemAudioMuxArguments(videoPath, audioPath, desktopVolume, videoHasAudio,
                                       firstSpanLeadMs, outputPath);
    }
    const QString desktop = QStringLiteral("%1;[dsync]volume=%2").arg(aligned, volumeFactor(desktopVolume));
    return systemAudioMuxArgumentsWithChain(videoPath, audioPath, desktop, videoHasAudio, outputPath);
}

QString systemAudioSpanFilter(const QList<SystemAudioSpan> &spans, int sampleRate,
                              qint64 firstSpanLeadMs, const QString &outLabel)
{
    if (sampleRate <= 0 || spans.isEmpty())
        return {};
    auto framesForMs = [sampleRate](qint64 ms) {
        return ms * sampleRate / 1000;
    };

    QStringList chains;
    for (int i = 0; i < spans.size(); ++i) {
        const SystemAudioSpan &span = spans.at(i);
        qint64 start = qMax<qint64>(0, span.startFrame);
        const qint64 end = span.endFrame;
        if (end <= start)
            continue;
        qint64 padFrames = 0;
        if (span.videoDurationMs >= 0) {
            const qint64 videoFrames = framesForMs(span.videoDurationMs);
            const qint64 audioFrames = end - start;
            if (audioFrames > videoFrames)
                start = end - videoFrames;
            else
                padFrames = videoFrames - audioFrames;
        } else if (i == 0) {
            if (firstSpanLeadMs > 0)
                start = qMin(end, start + framesForMs(firstSpanLeadMs));
            else if (firstSpanLeadMs < 0)
                padFrames = framesForMs(-firstSpanLeadMs);
        }
        if (end <= start)
            continue;
        QString chain = QStringLiteral("atrim=start_sample=%1:end_sample=%2,asetpts=PTS-STARTPTS")
                            .arg(start)
                            .arg(end);
        if (padFrames > 0)
            chain += QStringLiteral(",adelay=delays=%1S:all=1").arg(padFrames);
        chains.append(chain);
    }
    if (chains.isEmpty())
        return {};
    if (chains.size() == 1)
        return QStringLiteral("[1:a]%1[%2]").arg(chains.first(), outLabel);

    QString split = QStringLiteral("[1:a]asplit=%1").arg(chains.size());
    QStringList parts;
    QString concatInputs;
    for (int i = 0; i < chains.size(); ++i) {
        split += QStringLiteral("[r%1]").arg(i);
        parts.append(QStringLiteral("[r%1]%2[s%1]").arg(i).arg(chains.at(i)));
        concatInputs += QStringLiteral("[s%1]").arg(i);
    }
    parts.prepend(split);
    parts.append(QStringLiteral("%1concat=n=%2:v=0:a=1[%3]").arg(concatInputs).arg(chains.size()).arg(outLabel));
    return parts.join(QLatin1Char(';'));
}

qint64 ffmpegProgressOutTimeUs(const QString &line)
{
    const QString trimmed = line.trimmed();
    static const QString key = QStringLiteral("out_time_us=");
    if (!trimmed.startsWith(key))
        return -1;
    bool ok = false;
    const qint64 value = trimmed.mid(key.size()).toLongLong(&ok);
    return ok && value >= 0 ? value : -1;
}

QString ffmpegVideoEncoderName(WindowsVideoEncoder encoder)
{
    switch (encoder) {
    case WindowsVideoEncoder::Nvenc:
        return QStringLiteral("h264_nvenc");
    case WindowsVideoEncoder::Amf:
        return QStringLiteral("h264_amf");
    case WindowsVideoEncoder::Qsv:
        return QStringLiteral("h264_qsv");
    case WindowsVideoEncoder::Libx264:
        break;
    }
    return QStringLiteral("libx264");
}

QStringList windowsVideoEncodeArguments(WindowsCaptureSource source, WindowsVideoEncoder encoder,
                                        int crf, bool losslessIntermediate)
{
    const bool ddaGrab = source == WindowsCaptureSource::DdaGrab;
    const QString quality = QString::number(crf);
    if (losslessIntermediate || !ddaGrab)
        encoder = WindowsVideoEncoder::Libx264;

    QStringList args;
    switch (encoder) {
    case WindowsVideoEncoder::Nvenc:
        // Constant-quality VBR: -cq follows the CRF scale closely.
        args << QStringLiteral("-c:v") << QStringLiteral("h264_nvenc")
             << QStringLiteral("-preset") << QStringLiteral("p4")
             << QStringLiteral("-rc") << QStringLiteral("vbr")
             << QStringLiteral("-cq") << quality
             << QStringLiteral("-b:v") << QStringLiteral("0");
        return args;
    case WindowsVideoEncoder::Amf:
        args << QStringLiteral("-c:v") << QStringLiteral("h264_amf")
             << QStringLiteral("-quality") << QStringLiteral("speed")
             << QStringLiteral("-rc") << QStringLiteral("cqp")
             << QStringLiteral("-qp_i") << quality
             << QStringLiteral("-qp_p") << quality
             << QStringLiteral("-qp_b") << quality;
        return args;
    case WindowsVideoEncoder::Qsv:
        // The encoder wants NV12 surfaces; VPP converts on the GPU.
        args << QStringLiteral("-vf") << QStringLiteral("hwmap=derive_device=qsv,format=qsv,vpp_qsv=format=nv12")
             << QStringLiteral("-c:v") << QStringLiteral("h264_qsv")
             << QStringLiteral("-preset") << QStringLiteral("veryfast")
             << QStringLiteral("-global_quality") << quality;
        return args;
    case WindowsVideoEncoder::Libx264:
        break;
    }

    if (losslessIntermediate) {
        // Only read back by the palette passes: lossless and full chroma, so
        // the GIF palette sees the real colours.
        if (ddaGrab)
            args << QStringLiteral("-vf") << QStringLiteral("hwdownload,format=bgra,format=yuv444p");
        args << QStringLiteral("-c:v") << QStringLiteral("libx264")
             << QStringLiteral("-preset") << QStringLiteral("ultrafast")
             << QStringLiteral("-qp") << QStringLiteral("0")
             << QStringLiteral("-pix_fmt") << QStringLiteral("yuv444p");
        return args;
    }
    // ddagrab sizes are even already; gdigrab regions may not be.
    args << QStringLiteral("-vf")
         << (ddaGrab ? QStringLiteral("hwdownload,format=bgra,format=yuv420p")
                     : QStringLiteral("pad=ceil(iw/2)*2:ceil(ih/2)*2"))
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("veryfast")
         << QStringLiteral("-crf") << quality
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    return args;
}

QStringList windowsDdaEncoderProbeArguments(WindowsVideoEncoder encoder)
{
    QStringList args = {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"),
        QStringLiteral("ddagrab=output_idx=0:framerate=10:video_size=320x240:draw_mouse=0:output_fmt=8bit"),
        QStringLiteral("-frames:v"), QStringLiteral("1")
    };
    args << windowsVideoEncodeArguments(WindowsCaptureSource::DdaGrab, encoder, 23, false);
    args << QStringLiteral("-f") << QStringLiteral("null") << QStringLiteral("-");
    return args;
}

bool ffmpegListHasFilter(const QString &filtersOutput, const QString &name)
{
    const QStringList lines = filtersOutput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList tokens = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        // " T.C name  A->A  description"; very old builds had no flag column.
        if (tokens.size() >= 3 && tokens.at(1) == name && tokens.at(2).contains(QStringLiteral("->")))
            return true;
        if (tokens.size() >= 2 && tokens.at(0) == name && tokens.at(1).contains(QStringLiteral("->")))
            return true;
    }
    return false;
}

QStringList ffmpegListedEncoders(const QString &encodersOutput)
{
    static const QRegularExpression flags(QStringLiteral("^[VAS][A-Z.]{5}$"));
    QStringList encoders;
    const QStringList lines = encodersOutput.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList tokens = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() < 2 || tokens.at(1) == QStringLiteral("="))
            continue;
        if (flags.match(tokens.at(0)).hasMatch())
            encoders.append(tokens.at(1));
    }
    return encoders;
}

QList<WindowsVideoEncoder> windowsDdaEncoderCandidates(const QStringList &listedEncoders)
{
    QList<WindowsVideoEncoder> candidates;
    for (WindowsVideoEncoder encoder : {WindowsVideoEncoder::Nvenc, WindowsVideoEncoder::Amf,
                                        WindowsVideoEncoder::Qsv, WindowsVideoEncoder::Libx264}) {
        if (listedEncoders.contains(ffmpegVideoEncoderName(encoder)))
            candidates.append(encoder);
    }
    return candidates;
}

namespace {
const QString CapabilitiesPrefix = QStringLiteral("v1:");
}

QString serializeWindowsCaptureCapabilities(const WindowsCaptureCapabilities &capabilities)
{
    QStringList names;
    for (WindowsVideoEncoder encoder : capabilities.ddaEncoders)
        names.append(ffmpegVideoEncoderName(encoder));
    return CapabilitiesPrefix + names.join(QLatin1Char(','));
}

WindowsCaptureCapabilities parseWindowsCaptureCapabilities(const QString &value)
{
    WindowsCaptureCapabilities capabilities;
    if (!value.startsWith(CapabilitiesPrefix))
        return capabilities;
    capabilities.ready = true;
    const QStringList names = value.mid(CapabilitiesPrefix.size()).split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &name : names) {
        for (WindowsVideoEncoder encoder : {WindowsVideoEncoder::Nvenc, WindowsVideoEncoder::Amf,
                                            WindowsVideoEncoder::Qsv, WindowsVideoEncoder::Libx264}) {
            if (name.trimmed() == ffmpegVideoEncoderName(encoder) && !capabilities.ddaEncoders.contains(encoder))
                capabilities.ddaEncoders.append(encoder);
        }
    }
    return capabilities;
}

QString windowsCaptureProbeCacheKey(const QString &ffmpegPath, qint64 modifiedMsecs, qint64 size)
{
    const QByteArray identity = QStringLiteral("%1|%2|%3")
                                    .arg(QDir::cleanPath(ffmpegPath).toLower())
                                    .arg(modifiedMsecs)
                                    .arg(size)
                                    .toUtf8();
    const QByteArray hash = QCryptographicHash::hash(identity, QCryptographicHash::Sha1).toHex();
    return QStringLiteral("recordingCaptureProbe/%1").arg(QString::fromLatin1(hash.left(16)));
}

DdaOutputMapping ddaOutputForCaptureRect(const QRect &captureRect,
                                         const QList<DxgiOutputInfo> &outputs)
{
    DdaOutputMapping mapping;
    if (!captureRect.isValid())
        return mapping;
    for (const DxgiOutputInfo &output : outputs) {
        if (!output.desktopRect.contains(captureRect))
            continue;
        // Monitors do not overlap: this is the only one that holds the rect.
        if (output.adapterIndex != 0 || output.rotated || output.outputIndex < 0)
            return mapping;
        const QSize size(captureRect.width() & ~1, captureRect.height() & ~1);
        if (size.width() < 2 || size.height() < 2)
            return mapping;
        mapping.valid = true;
        mapping.outputIndex = output.outputIndex;
        mapping.offset = captureRect.topLeft() - output.desktopRect.topLeft();
        mapping.size = size;
        return mapping;
    }
    return mapping;
}

WindowsVideoPlan chooseWindowsVideoPlan(const QRect &captureRect,
                                        const WindowsCaptureCapabilities &capabilities,
                                        const QList<DxgiOutputInfo> &outputs,
                                        const WindowsVideoPlanRequest &request)
{
    WindowsVideoPlan plan;
    if (!request.allowDdaGrab || !capabilities.ready || !capabilities.ddaGrab())
        return plan;
    const DdaOutputMapping mapping = ddaOutputForCaptureRect(captureRect, outputs);
    if (!mapping.valid)
        return plan;

    const bool hardwareFits = mapping.size.width() <= WindowsHardwareEncoderMaxSide
        && mapping.size.height() <= WindowsHardwareEncoderMaxSide;
    for (WindowsVideoEncoder encoder : capabilities.ddaEncoders) {
        const bool hardware = encoder != WindowsVideoEncoder::Libx264;
        if (hardware && (request.losslessIntermediate || !request.allowHardwareEncoder || !hardwareFits))
            continue;
        plan.source = WindowsCaptureSource::DdaGrab;
        plan.encoder = encoder;
        plan.outputIndex = mapping.outputIndex;
        plan.outputOffset = mapping.offset;
        plan.ddaSize = mapping.size;
        return plan;
    }
    return plan;
}

QString describeWindowsVideoPlan(const WindowsVideoPlan &plan)
{
    if (plan.source != WindowsCaptureSource::DdaGrab)
        return QStringLiteral("gdigrab, libx264");
    return QStringLiteral("ddagrab output %1 +%2+%3 %4x%5, %6")
        .arg(plan.outputIndex)
        .arg(plan.outputOffset.x())
        .arg(plan.outputOffset.y())
        .arg(plan.ddaSize.width())
        .arg(plan.ddaSize.height())
        .arg(ffmpegVideoEncoderName(plan.encoder));
}

bool isFfmpegProgressLine(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("^[a-z][a-z0-9_]*=\\S*$"));
    return re.match(line.trimmed()).hasMatch();
}

bool isFfmpegProgressBlockEnd(const QString &line)
{
    return line.trimmed().startsWith(QStringLiteral("progress="));
}

qint64 ffmpegInputStartEpochMs(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("Duration:.*\\bstart:\\s*(-?\\d+(?:\\.\\d+)?)"));
    const QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch())
        return -1;
    bool ok = false;
    const double startSeconds = match.captured(1).toDouble(&ok);
    // Anything before 2001-09-09 is a stream-relative time, not wall clock.
    if (!ok || startSeconds < 1.0e9)
        return -1;
    return static_cast<qint64>(startSeconds * 1000.0 + 0.5);
}

qint64 systemAudioLeadMs(qint64 audioStartEpochMs, qint64 videoStartEpochMs)
{
    if (audioStartEpochMs <= 0 || videoStartEpochMs <= 0)
        return 0;
    const qint64 lead = videoStartEpochMs - audioStartEpochMs;
    return qAbs(lead) <= 5000 ? lead : 0;
}

QString ffmpegFailureReason(const QStringList &logTail)
{
    QStringList lines;
    for (const QString &line : logTail) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !isFfmpegProgressLine(trimmed))
            lines.append(trimmed);
    }
    if (lines.size() > 3)
        lines = lines.mid(lines.size() - 3);
    return lines.join(QLatin1Char('\n'));
}

int windowsEncoderStopGraceMs(const QSize &captureSize, int fps)
{
    constexpr int BaseMs = 15000;
    constexpr int MaxMs = 60000;
    constexpr double ReferenceRate = 1920.0 * 1080.0 * 30.0;
    const double rate = static_cast<double>(qMax(0, captureSize.width()))
        * qMax(0, captureSize.height()) * qMax(1, fps);
    const double scaled = BaseMs * qMax(1.0, rate / ReferenceRate);
    return static_cast<int>(qBound<double>(BaseMs, scaled, MaxMs));
}

int wavHeaderSize(const QByteArray &waveFormat)
{
    const int fmtSize = static_cast<int>(waveFormat.size());
    const int fmtPadded = fmtSize + (fmtSize % 2);
    // RIFF/RF64 + size + WAVE, JUNK/ds64 chunk, fmt chunk, data chunk header.
    return 12 + (8 + Ds64PayloadSize) + (8 + fmtPadded) + 8;
}

QByteArray wavFileHeader(const QByteArray &waveFormat, quint64 dataBytes)
{
    const int fmtSize = static_cast<int>(waveFormat.size());
    const quint64 headerSize = static_cast<quint64>(wavHeaderSize(waveFormat));
    const quint64 riffSize = headerSize - 8 + dataBytes;
    const bool rf64 = riffSize > 0xFFFFFFFFull;

    quint16 blockAlign = 0;
    if (fmtSize >= 14) {
        blockAlign = static_cast<quint16>(static_cast<uchar>(waveFormat.at(12))
                                          | (static_cast<uchar>(waveFormat.at(13)) << 8));
    }

    QByteArray header;
    header.reserve(static_cast<int>(headerSize));
    header.append(rf64 ? "RF64" : "RIFF", 4);
    appendLe32(header, rf64 ? 0xFFFFFFFFu : static_cast<quint32>(riffSize));
    header.append("WAVE", 4);
    if (rf64) {
        header.append("ds64", 4);
        appendLe32(header, Ds64PayloadSize);
        appendLe64(header, riffSize);
        appendLe64(header, dataBytes);
        appendLe64(header, blockAlign ? dataBytes / blockAlign : 0);
        appendLe32(header, 0);
    } else {
        header.append("JUNK", 4);
        appendLe32(header, Ds64PayloadSize);
        header.append(QByteArray(Ds64PayloadSize, '\0'));
    }
    header.append("fmt ", 4);
    appendLe32(header, static_cast<quint32>(fmtSize));
    header.append(waveFormat);
    if (fmtSize % 2)
        header.append('\0');
    header.append("data", 4);
    appendLe32(header, rf64 ? 0xFFFFFFFFu : static_cast<quint32>(dataBytes));
    return header;
}
