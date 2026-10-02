#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

enum class VideoMuxCompletionAction {
    UseMuxedOutput,
    UseVideoOnlyFallback
};

bool portalGifConversionSucceeded(bool normalExit, int exitCode, qint64 outputSize);
VideoMuxCompletionAction videoMuxCompletionAction(bool normalExit, int exitCode,
                                                  qint64 outputSize);

// Post-processing limits grow with the recorded (active) duration so long
// recordings are not killed halfway through a healthy finalization step.
int videoMuxTimeoutMs(qint64 recordedMs);
int segmentConcatTimeoutMs(qint64 recordedMs);
int portalGifPassTimeoutMs(qint64 recordedMs);

// Two-pass portal GIF conversion: the palette pass writes a PNG so the encode
// pass streams frames instead of buffering the whole recording.
QStringList portalGifPaletteArguments(const QString &inputPath, const QString &palettePath,
                                      int fps);
QStringList portalGifEncodeArguments(const QString &inputPath, const QString &palettePath,
                                     int fps, int loopCount, const QString &outputPath);
// Where the portal video is kept when the GIF could not be created.
QString keptPortalVideoPath(const QString &gifPath);
