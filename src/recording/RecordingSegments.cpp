#include "RecordingSegments.h"

#include <QDir>
#include <QFileInfo>

QString recordingSegmentPath(const QString &directory, const QString &stamp, int index)
{
    // Zero padding keeps the lexical order used by splitmuxsrc chronological.
    return QDir(directory).filePath(QStringLiteral(".eshot_segment_%1_%2.mp4")
                                        .arg(stamp)
                                        .arg(index, 3, 10, QLatin1Char('0')));
}

QString recordingSegmentPattern(const QString &directory, const QString &stamp)
{
    return QDir(directory).filePath(QStringLiteral(".eshot_segment_%1_*.mp4").arg(stamp));
}

QString recordingPartPath(const QString &outputPath, int partNumber)
{
    const QFileInfo info(outputPath);
    return info.dir().filePath(QStringLiteral("%1_part%2.mp4")
                                   .arg(info.completeBaseName())
                                   .arg(partNumber));
}

QByteArray ffmpegConcatList(const QStringList &segmentPaths)
{
    QByteArray list("ffconcat version 1.0\n");
    for (const QString &path : segmentPaths) {
        // Inside single quotes only the quote itself needs escaping; it has
        // to close the quote, add an escaped quote and reopen it.
        QString quoted = QDir::fromNativeSeparators(path);
        quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
        list += "file '" + quoted.toUtf8() + "'\n";
    }
    return list;
}

QStringList ffmpegConcatArguments(const QString &listPath, const QString &outputPath)
{
    return {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("concat"),
        QStringLiteral("-safe"), QStringLiteral("0"),
        QStringLiteral("-i"), listPath,
        QStringLiteral("-map"), QStringLiteral("0"),
        QStringLiteral("-c"), QStringLiteral("copy"),
        outputPath
    };
}

QStringList gstConcatArguments(const QString &segmentPattern, const QString &outputPath,
                               bool hasAudio)
{
    QStringList args = {
        QStringLiteral("-e"),
        QStringLiteral("splitmuxsrc"),
        QStringLiteral("name=src"),
        QStringLiteral("location=%1").arg(segmentPattern),
        QStringLiteral("mp4mux"),
        QStringLiteral("name=mux"),
        QStringLiteral("!"),
        QStringLiteral("filesink"),
        QStringLiteral("location=%1").arg(outputPath),
        QStringLiteral("src.video_0"),
        QStringLiteral("!"),
        QStringLiteral("h264parse"),
        QStringLiteral("!"),
        QStringLiteral("queue"),
        QStringLiteral("!"),
        QStringLiteral("mux.")
    };
    if (hasAudio) {
        args << QStringLiteral("src.audio_0")
             << QStringLiteral("!")
             << QStringLiteral("aacparse")
             << QStringLiteral("!")
             << QStringLiteral("queue")
             << QStringLiteral("!")
             << QStringLiteral("mux.");
    }
    return args;
}
