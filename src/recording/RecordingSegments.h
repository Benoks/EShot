#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Pausing closes the encoder, so every active span of a recording is its own
// file. They are joined when the recording stops, which keeps the paused
// interval out of the output entirely.

// Hidden file for active span `index` of the recording identified by `stamp`.
QString recordingSegmentPath(const QString &directory, const QString &stamp, int index);
// Glob matching every segment of `stamp` (GStreamer splitmuxsrc location).
QString recordingSegmentPattern(const QString &directory, const QString &stamp);
// Visible name a segment is kept under when joining the segments failed.
QString recordingPartPath(const QString &outputPath, int partNumber);

// ffconcat list for FFmpeg's concat demuxer.
QByteArray ffmpegConcatList(const QStringList &segmentPaths);
// Joins identically encoded segments without re-encoding.
QStringList ffmpegConcatArguments(const QString &listPath, const QString &outputPath);
// Joins the segments matched by `segmentPattern` with GStreamer only, for
// Wayland sessions without FFmpeg.
QStringList gstConcatArguments(const QString &segmentPattern, const QString &outputPath,
                               bool hasAudio);
