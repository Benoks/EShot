#include "VideoRecordingCompletionPolicy.h"

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
