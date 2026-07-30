#pragma once

#include <QtGlobal>

enum class VideoMuxCompletionAction {
    UseMuxedOutput,
    UseVideoOnlyFallback
};

bool portalGifConversionSucceeded(bool normalExit, int exitCode, qint64 outputSize);
VideoMuxCompletionAction videoMuxCompletionAction(bool normalExit, int exitCode,
                                                  qint64 outputSize);
