#include "VideoRecordingCompletionPolicy.h"

bool videoRecordingProcessSucceeded(bool usesGStreamer, bool expectedStop,
                                    QProcess::ExitStatus status, int exitCode,
                                    qint64 outputSize)
{
    if (outputSize <= 0)
        return false;

    if (status == QProcess::NormalExit)
        return exitCode == 0 || expectedStop;

    // gst-launch may finish a finalized pipeline with the SIGINT status sent
    // by VideoRecorder::stop(). The MP4 is valid once it has been written.
    return usesGStreamer && expectedStop && exitCode == 2;
}
