#include "RecordingFinalizationPolicy.h"

#include <QDir>
#include <QFileInfo>

namespace {
int scaledTimeoutMs(qint64 recordedMs, qint64 baseMs, qint64 msPerRecordedSecond)
{
    // Cap at a day so the result always fits a QTimer interval.
    const qint64 seconds = qMax<qint64>(0, recordedMs) / 1000 + 1;
    const qint64 timeout = baseMs + seconds * msPerRecordedSecond;
    return static_cast<int>(qMin<qint64>(timeout, 24LL * 60 * 60 * 1000));
}
}

bool portalGifConversionSucceeded(bool normalExit, int exitCode, qint64 outputSize)
{
    return normalExit && exitCode == 0 && outputSize > 0;
}

VideoMuxCompletionAction videoMuxCompletionAction(bool normalExit, int exitCode,
                                                  qint64 outputSize)
{
    return normalExit && exitCode == 0 && outputSize > 0
        ? VideoMuxCompletionAction::UseMuxedOutput
        : VideoMuxCompletionAction::UseVideoOnlyFallback;
}

int videoMuxTimeoutMs(qint64 recordedMs)
{
    // Video is stream-copied and only the audio is encoded: allow real time.
    return scaledTimeoutMs(recordedMs, 30000, 1000);
}

int segmentConcatTimeoutMs(qint64 recordedMs)
{
    return scaledTimeoutMs(recordedMs, 30000, 1000);
}

int portalGifPassTimeoutMs(qint64 recordedMs)
{
    // Decoding, scaling and dithering every frame can be slower than real
    // time on large regions; each pass gets four times the recording length.
    return scaledTimeoutMs(recordedMs, 60000, 4000);
}

QStringList portalGifPaletteArguments(const QString &inputPath, const QString &palettePath,
                                      int fps)
{
    return {
        QStringLiteral("-y"), QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), inputPath,
        QStringLiteral("-vf"),
        QStringLiteral("fps=%1,palettegen=max_colors=256").arg(qMax(1, fps)),
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-update"), QStringLiteral("1"),
        palettePath
    };
}

QStringList portalGifEncodeArguments(const QString &inputPath, const QString &palettePath,
                                     int fps, int loopCount, const QString &outputPath)
{
    return {
        QStringLiteral("-y"), QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), inputPath,
        QStringLiteral("-i"), palettePath,
        QStringLiteral("-lavfi"),
        QStringLiteral("fps=%1[x];[x][1:v]paletteuse=dither=sierra2_4a").arg(qMax(1, fps)),
        QStringLiteral("-loop"), QString::number(loopCount),
        outputPath
    };
}

QString keptPortalVideoPath(const QString &gifPath)
{
    const QFileInfo info(gifPath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".mp4"));
}
