#include "RecordingFinalizationPolicy.h"

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
