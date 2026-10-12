#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

class QDateTime;

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

// `path`, or `name_2.ext`, `name_3.ext`, ... when it is taken: file names only
// have one-second resolution and FFmpeg overwrites existing files (-y).
QString uniqueRecordingOutputPath(const QString &path,
                                  const std::function<bool(const QString &)> &exists);
// Hidden `.eshot_*` intermediate older than a day: left behind by a crash and
// safe to sweep when a new recording starts in the same folder.
bool isStaleRecordingIntermediate(const QString &fileName, const QDateTime &lastModified,
                                  const QDateTime &now);
