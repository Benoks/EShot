#include "WindowsRecordingSupport.h"

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
    QStringList args;
    // Info level prints the input dump (with gdigrab's wall-clock start time)
    // and the real error on failure; -nostats keeps the periodic status line
    // out of stderr, progress goes to stdout instead.
    args << QStringLiteral("-y")
         << QStringLiteral("-hide_banner")
         << QStringLiteral("-loglevel") << QStringLiteral("info")
         << QStringLiteral("-nostats")
         << QStringLiteral("-progress") << QStringLiteral("pipe:1")
         << QStringLiteral("-thread_queue_size") << QStringLiteral("1024")
         << QStringLiteral("-f") << QStringLiteral("gdigrab")
         << QStringLiteral("-draw_mouse") << QStringLiteral("1")
         << QStringLiteral("-framerate") << QString::number(options.fps)
         << QStringLiteral("-offset_x") << QString::number(rect.x())
         << QStringLiteral("-offset_y") << QString::number(rect.y())
         << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(rect.width()).arg(rect.height())
         << QStringLiteral("-i") << QStringLiteral("desktop");

    QList<QPair<int, int>> audioInputs;
    int nextInputIndex = 1;
    auto addDshowInput = [&](const QString &name, int volume) {
        if (name.isEmpty() || volume <= 0)
            return;
        args << QStringLiteral("-thread_queue_size") << QStringLiteral("1024")
             << QStringLiteral("-f") << QStringLiteral("dshow")
             << QStringLiteral("-audio_buffer_size") << QStringLiteral("50")
             << QStringLiteral("-i") << QStringLiteral("audio=%1").arg(name);
        audioInputs.append(qMakePair(nextInputIndex++, volume));
    };
    addDshowInput(options.desktopDshowInput, options.desktopVolume);
    addDshowInput(options.microphoneDshowInput, options.microphoneVolume);

    args << QStringLiteral("-vf") << QStringLiteral("pad=ceil(iw/2)*2:ceil(ih/2)*2")
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("veryfast")
         << QStringLiteral("-crf") << QString::number(options.crf)
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    args << ffmpegAudioMixArguments(audioInputs);
    // Fragmented MP4 (a fragment at least every second): a killed encoder
    // loses the last fragment instead of the whole file. The join/mux step
    // rewrites it as a regular MP4 with +faststart.
    args << QStringLiteral("-movflags") << QStringLiteral("+frag_keyframe+empty_moov+default_base_moof")
         << QStringLiteral("-frag_duration") << QStringLiteral("1000000")
         << outputPath;
    return args;
}

QStringList systemAudioMuxArguments(const QString &videoPath, const QString &audioPath,
                                    int desktopVolume, bool videoHasAudio,
                                    qint64 audioLeadMs, const QString &outputPath)
{
    QStringList args = {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), videoPath,
        QStringLiteral("-i"), audioPath
    };

    // Line the loopback audio up with the first video frame: drop what was
    // captured before it, or pad with silence when audio started later.
    QString align;
    if (audioLeadMs > 0)
        align = QStringLiteral("atrim=start=%1,asetpts=PTS-STARTPTS,").arg(seconds(audioLeadMs));
    else if (audioLeadMs < 0)
        align = QStringLiteral("adelay=delays=%1:all=1,").arg(-audioLeadMs);
    const QString desktop = QStringLiteral("[1:a]%1volume=%2").arg(align, volumeFactor(desktopVolume));

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
