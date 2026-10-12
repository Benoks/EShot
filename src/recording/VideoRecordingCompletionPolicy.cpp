#include "VideoRecordingCompletionPolicy.h"

#include <QFile>
#include <QIODevice>
#include <QtEndian>

bool videoRecordingProcessSucceeded(bool usesGStreamer, bool expectedStop,
                                    QProcess::ExitStatus status, int exitCode,
                                    qint64 outputSize, bool fragmentedOutput)
{
    if (outputSize <= 0)
        return false;

    if (status == QProcess::NormalExit && (exitCode == 0 || expectedStop))
        return true;

    // A fragmented MP4 is complete up to its last fragment, so footage from
    // an encoder that was killed (stop timeout) or died is still usable.
    if (fragmentedOutput)
        return outputSize >= MinimumFragmentedSegmentBytes;

    if (status == QProcess::NormalExit)
        return false;

    // gst-launch may finish a finalized pipeline with the SIGINT status sent
    // by VideoRecorder::stop(). The MP4 is valid once it has been written.
    return usesGStreamer && expectedStop && exitCode == 2;
}

bool recordingProcessFailedAtStartup(qint64 runMs, bool expectedStop)
{
    if (expectedStop)
        return false;
    return runMs < 0 || runMs < recordingStartupWindowMs;
}

bool mp4LooksPlayable(QIODevice &device)
{
    if (!device.isOpen() || !device.isReadable())
        return false;
    const qint64 fileSize = device.size();
    qint64 offset = 0;
    bool hasMovie = false;
    bool hasMedia = false;
    // Bounded: a corrupt file must not keep the loop busy.
    for (int boxes = 0; boxes < 1000000 && offset + 8 <= fileSize; ++boxes) {
        if (!device.seek(offset))
            break;
        const QByteArray header = device.read(16);
        if (header.size() < 8)
            break;
        quint64 boxSize = qFromBigEndian<quint32>(header.constData());
        const QByteArray type = header.mid(4, 4);
        qint64 headerSize = 8;
        if (boxSize == 1) {
            if (header.size() < 16)
                break;
            boxSize = qFromBigEndian<quint64>(header.constData() + 8);
            headerSize = 16;
        } else if (boxSize == 0) {
            boxSize = static_cast<quint64>(fileSize - offset);
        }
        if (boxSize < static_cast<quint64>(headerSize))
            break;
        if (type == "moov")
            hasMovie = true;
        else if (type == "mdat" && static_cast<qint64>(boxSize) > headerSize
                 && offset + headerSize < fileSize)
            hasMedia = true;
        if (hasMovie && hasMedia)
            return true;
        if (boxSize > static_cast<quint64>(fileSize - offset))
            break;
        offset += static_cast<qint64>(boxSize);
    }
    return hasMovie && hasMedia;
}

bool mp4FileLooksPlayable(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return mp4LooksPlayable(file);
}

RecordingSegmentDisposition recordingSegmentDisposition(bool processSucceeded,
                                                        bool startupFailure,
                                                        bool playable)
{
    if (!playable)
        return RecordingSegmentDisposition::Drop;
    if (processSucceeded)
        return RecordingSegmentDisposition::Keep;
    return startupFailure ? RecordingSegmentDisposition::Drop
                          : RecordingSegmentDisposition::Salvage;
}
