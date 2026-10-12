#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

// Pure helpers for the Windows (gdigrab/dshow/WASAPI) recording backend. They
// build no processes themselves, so they are unit tested on every platform.

// Where the frames come from: GDI BitBlt (gdigrab) or the Desktop
// Duplication API (ddagrab, the frames stay on the GPU).
enum class WindowsCaptureSource {
    GdiGrab,
    DdaGrab
};

// H.264 encoder of a ddagrab capture; gdigrab always uses libx264.
enum class WindowsVideoEncoder {
    Libx264,
    Nvenc,
    Amf,
    Qsv
};

// How one recording is captured. A recording keeps its plan for every
// segment, so the paused parts can be joined without re-encoding.
struct WindowsVideoPlan {
    WindowsCaptureSource source = WindowsCaptureSource::GdiGrab;
    WindowsVideoEncoder encoder = WindowsVideoEncoder::Libx264;
    // ddagrab only: DXGI output index (of adapter 0), the region's offset
    // inside that output and its (even) size.
    int outputIndex = -1;
    QPoint outputOffset;
    QSize ddaSize;
};

struct WindowsCaptureOptions {
    // Physical desktop coordinates (gdigrab offsets).
    QRect captureRect;
    int fps = 30;
    int crf = 24;
    WindowsVideoPlan plan;
    // GIF source video: lossless 4:4:4 H.264 for the palette passes; no
    // audio inputs are added.
    bool losslessIntermediate = false;
    // `audio=` names for dshow inputs; empty leaves the source out.
    QString desktopDshowInput;
    int desktopVolume = 0;
    QString microphoneDshowInput;
    int microphoneVolume = 0;
};

// FFmpeg arguments for one segment (gdigrab or ddagrab, see options.plan). The
// segment is a fragmented MP4 so it stays playable when FFmpeg has to be
// killed, and progress is reported on stdout so the caller knows when capture
// really started.
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

// --- Capture backend selection (ddagrab, hardware encoders) ----------------

// FFmpeg encoder name ("libx264", "h264_nvenc", ...).
QString ffmpegVideoEncoderName(WindowsVideoEncoder encoder);
// Arguments that filter and encode the captured video. ddagrab delivers
// D3D11 frames: NVENC and AMF take them directly, QSV maps them and libx264
// downloads them first. `crf` (libx264 scale) sets every encoder's quality.
QStringList windowsVideoEncodeArguments(WindowsCaptureSource source, WindowsVideoEncoder encoder,
                                        int crf, bool losslessIntermediate);
// One ddagrab frame encoded with `encoder` into the null muxer: an encoder
// that is listed but cannot open (no such GPU, driver, session limit) fails.
QStringList windowsDdaEncoderProbeArguments(WindowsVideoEncoder encoder);

// `ffmpeg -filters` lists the filter (" ... ddagrab  |->V  ...").
bool ffmpegListHasFilter(const QString &filtersOutput, const QString &name);
// Encoder names in `ffmpeg -encoders` output.
QStringList ffmpegListedEncoders(const QString &encodersOutput);
// Encoders worth probing with ddagrab, best first: NVENC, AMF, QSV, libx264.
QList<WindowsVideoEncoder> windowsDdaEncoderCandidates(const QStringList &listedEncoders);

// Result of the (cached) probe of one ffmpeg.exe.
struct WindowsCaptureCapabilities {
    // False until the probe finished: record with gdigrab + libx264.
    bool ready = false;
    // Encoders that encoded a ddagrab frame, best first; empty: no ddagrab.
    QList<WindowsVideoEncoder> ddaEncoders;
    bool ddaGrab() const { return !ddaEncoders.isEmpty(); }
};
QString serializeWindowsCaptureCapabilities(const WindowsCaptureCapabilities &capabilities);
// Not ready for an empty, unknown or outdated value.
WindowsCaptureCapabilities parseWindowsCaptureCapabilities(const QString &value);
// QSettings key of the probe result for one ffmpeg.exe build; a replaced or
// updated FFmpeg gets a new key and is probed again.
QString windowsCaptureProbeCacheKey(const QString &ffmpegPath, qint64 modifiedMsecs, qint64 size);

// A DXGI output (monitor) in desktop coordinates (physical pixels).
struct DxgiOutputInfo {
    int adapterIndex = 0;
    int outputIndex = 0;
    QRect desktopRect;
    bool rotated = false;
};
struct DdaOutputMapping {
    bool valid = false;
    int outputIndex = -1;
    QPoint offset;
    // Even width and height (H.264 4:2:0): an odd edge loses one pixel.
    QSize size;
};
// The output of adapter 0 that contains the whole capture rect. ddagrab
// opens the default adapter and reads one output, so a region that spans
// monitors, lies on another adapter's monitor or on a rotated one is not
// mapped (and recorded with gdigrab).
DdaOutputMapping ddaOutputForCaptureRect(const QRect &captureRect,
                                         const QList<DxgiOutputInfo> &outputs);

struct WindowsVideoPlanRequest {
    bool allowDdaGrab = true;
    bool allowHardwareEncoder = true;
    // GIF source: always libx264 (lossless); ddagrab may still capture.
    bool losslessIntermediate = false;
};
// Hardware H.264 encoders stop at 4096 pixels per side.
constexpr int WindowsHardwareEncoderMaxSide = 4096;
WindowsVideoPlan chooseWindowsVideoPlan(const QRect &captureRect,
                                        const WindowsCaptureCapabilities &capabilities,
                                        const QList<DxgiOutputInfo> &outputs,
                                        const WindowsVideoPlanRequest &request);
// Short form for logs, e.g. "ddagrab output 1 +0+0 1920x1080, h264_nvenc".
QString describeWindowsVideoPlan(const WindowsVideoPlan &plan);

// --- System audio alignment per active span --------------------------------

// `out_time_us=` value of a progress line in microseconds, or -1.
qint64 ffmpegProgressOutTimeUs(const QString &line);

// One active span of the loopback WAV (in sample frames) and the length of
// the video segment recorded meanwhile (-1 when unknown).
struct SystemAudioSpan {
    qint64 startFrame = 0;
    qint64 endFrame = 0;
    qint64 videoDurationMs = -1;
};
// Filter graph from input 1's audio to `[outLabel]`. Audio and video of a
// span stop together, but audio resumes right away while the next FFmpeg
// segment needs a moment to start, so each span is cut to its segment's
// length by dropping (or padding with silence) its start. A span without a
// known length is kept whole; the first one is shifted by `firstSpanLeadMs`
// instead (see systemAudioLeadMs). Spans without audio are left out.
QString systemAudioSpanFilter(const QList<SystemAudioSpan> &spans, int sampleRate,
                              qint64 firstSpanLeadMs, const QString &outLabel);
// systemAudioMuxArguments() aligning each active span on its own.
QStringList systemAudioSpanMuxArguments(const QString &videoPath, const QString &audioPath,
                                        int desktopVolume, bool videoHasAudio,
                                        const QList<SystemAudioSpan> &spans, int sampleRate,
                                        qint64 firstSpanLeadMs, const QString &outputPath);
