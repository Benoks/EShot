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
