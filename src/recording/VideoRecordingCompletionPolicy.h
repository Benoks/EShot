#pragma once

#include <QProcess>

// Smallest fragmented MP4 that holds at least one media fragment (the empty
// ftyp/moov header alone is around a kilobyte).
constexpr qint64 MinimumFragmentedSegmentBytes = 4096;

// `fragmentedOutput`: the encoder writes a fragmented MP4 (Windows), which
// stays playable up to its last fragment even when the encoder was killed.
bool videoRecordingProcessSucceeded(bool usesGStreamer, bool expectedStop,
                                    QProcess::ExitStatus status, int exitCode,
                                    qint64 outputSize, bool fragmentedOutput = false);

class QIODevice;


// A recorder process that ends this soon after it was launched, without
// having been asked to stop, failed to start (bad device, caps, encoder...).
constexpr qint64 recordingStartupWindowMs = 1500;
bool recordingProcessFailedAtStartup(qint64 runMs, bool expectedStop);

// Walks the top-level MP4 boxes: true when the file has a movie header and
// media data, i.e. at least one written fragment of a fragmented MP4 or a
// finished regular MP4. A file cut off before its first fragment is not.
bool mp4LooksPlayable(QIODevice &device);
bool mp4FileLooksPlayable(const QString &path);

enum class RecordingSegmentDisposition {
    Keep,     // finished normally
    Salvage,  // the recorder failed or was killed, but the file plays
    Drop
};
RecordingSegmentDisposition recordingSegmentDisposition(bool processSucceeded,
                                                        bool startupFailure,
                                                        bool playable);
