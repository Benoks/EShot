#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

// Pure helpers for the Windows (gdigrab/dshow/WASAPI) recording backend. They
// build no processes themselves, so they are unit tested on every platform.

struct WindowsCaptureOptions {
    QRect captureRect;
    int fps = 30;
    int crf = 24;
    // `audio=` names for dshow inputs; empty leaves the source out.
    QString desktopDshowInput;
    int desktopVolume = 0;
    QString microphoneDshowInput;
    int microphoneVolume = 0;
};

// FFmpeg arguments for one gdigrab segment. The segment is a fragmented MP4 so
// it stays playable when FFmpeg has to be killed, and progress is reported on
// stdout so the caller knows when capture really started.
QStringList windowsFfmpegCaptureArguments(const WindowsCaptureOptions &options,
                                          const QString &outputPath);

// -filter_complex/-map/-c:a arguments for the audio inputs, given as
// (input index, volume percent) pairs. Mixing keeps each input's level.
QStringList ffmpegAudioMixArguments(const QList<QPair<int, int>> &inputs);

// Adds the WASAPI loopback WAV to the finished video. `audioLeadMs` is how
// much earlier the WAV started than the first video frame (negative: later).
QStringList systemAudioMuxArguments(const QString &videoPath, const QString &audioPath,
                                    int desktopVolume, bool videoHasAudio,
                                    qint64 audioLeadMs, const QString &outputPath);

// `-progress pipe:1` key=value line (frame=, out_time_us=, progress=, ...).
bool isFfmpegProgressLine(const QString &line);
// Line that closes a progress block; the first one means capture is running.
bool isFfmpegProgressBlockEnd(const QString &line);
// Start time of an input from its "Duration: ..., start: <s>, ..." dump line
// in milliseconds since the epoch (gdigrab stamps frames with wall-clock
// time), or -1 when the line has none or it is not a wall-clock time.
qint64 ffmpegInputStartEpochMs(const QString &line);
// Lead of the system audio over the video; 0 when either start is unknown or
// the difference is implausible.
qint64 systemAudioLeadMs(qint64 audioStartEpochMs, qint64 videoStartEpochMs);
// Last lines of an FFmpeg log that explain a failure.
QString ffmpegFailureReason(const QStringList &logTail);

// How long FFmpeg gets to finish writing after "q" before it is killed;
// grows with the pixel rate the encoder has to drain.
int windowsEncoderStopGraceMs(const QSize &captureSize, int fps);

// Size of the header written by wavFileHeader for a format chunk.
int wavHeaderSize(const QByteArray &waveFormat);
// WAV header for `dataBytes` of audio in `waveFormat` (a WAVEFORMATEX(TENSIBLE)).
// Space for an RF64 ds64 chunk is reserved as JUNK, so the header has the same
// size either way: a plain RIFF WAV up to 4 GiB, RF64 beyond.
QByteArray wavFileHeader(const QByteArray &waveFormat, quint64 dataBytes);
