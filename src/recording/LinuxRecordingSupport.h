#pragma once

#include <QList>
#include <QPair>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStringList>

struct PortalCropGeometry {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    // Size of the cropped region in stream pixels. It is always even so the
    // encoder never needs a W -> W-1 rescale.
    QSize cropSize;
    QSize outputSize;
    // True only when the output is smaller than the crop (e.g. a bounded GIF).
    bool needsScale = false;
    bool valid = false;
};

QString pipeWireSourcePath(uint nodeId, quint64 pipewireSerial = 0);
// Maps the capture region into the portal stream. A crop with an odd width
// or height grows by one pixel on the right/bottom (or shrinks when it
// already touches that edge) so the crop matches the even encoder size.
PortalCropGeometry portalCropGeometry(const QRect &captureRect,
                                      const QRect &displayRect,
                                      const QPoint &streamPosition,
                                      const QSize &streamDisplaySize,
                                      const QSize &requestedOutputSize);
QSize evenRecordingSize(const QSize &size);
QString preferredGstAacEncoder(const QStringList &availableElements);
// x264enc, or openh264enc when x264enc is not installed; empty when neither.
QString preferredGstH264Encoder(const QStringList &availableElements);
// Element names from `gst-inspect-1.0` catalog output ("plugin:  element: description").
QStringList gstElementNamesFromInspect(const QString &inspectOutput);
// Required elements that are missing from `availableElements`. Groups with
// alternatives (H.264 and AAC encoders) are reported by their preferred name.
QStringList missingPortalRecordingElements(const QStringList &availableElements,
                                           bool withAudio, bool mixesAudio,
                                           bool joinsSegments);
// "missing gstreamer elements: x264enc, mp4mux"
QString gstMissingElementsReason(const QStringList &missingElements);
// Reported instead of a failure when the user closes the portal picker.
QString portalCanceledReason();
// gst-launch prints progress ("Setting pipeline to PAUSED ...") before the
// real error; returns the ERROR lines, or the last line when there are none.
QString gstFailureReason(const QString &log);
// True once gst-launch reported the switch to PLAYING (run it with LANGUAGE=en).
bool gstPipelineReachedPlaying(const QString &log);
// Cached element catalog of the given gst-inspect-1.0 (one process per app
// run). *ok is false when gst-inspect could not be run.
QStringList cachedGstElements(const QString &inspectProgram, bool *ok = nullptr);
// Drops the cache, e.g. after a check found missing elements that the user
// may install before the next attempt.
void invalidateGstElementCache();
QString discoverGstAacEncoder(const QString &inspectProgram = QString());
// gst-inspect-1.0 next to the given gst-launch-1.0 (bundled runtimes), else
// the one on PATH.
QString gstInspectPathFor(const QString &gstLaunchPath);

struct PortalVideoPipelineOptions {
    int pipewireFd = -1;
    QString sourcePath;
    PortalCropGeometry crop;
    int fps = 30;
    int crf = 24;
    // x264enc, openh264enc or a hardware encoder (vah264enc, vah264lpenc,
    // nvh264enc); see selectPortalH264Encoder().
    QString h264Encoder = QStringLiteral("x264enc");
    // Near-lossless intermediate for the GIF conversion.
    bool losslessIntermediate = false;
};

// pipewiresrc keepalive so static screens still produce frames.
int pipeWireKeepaliveMs(int fps);
// Raw format the encoder is fed.
QString portalEncoderInputFormat(const QString &encoder, bool lossless, const QSize &size);
QStringList gstH264EncoderArguments(const QString &encoder, int fps, int crf,
                                    const QSize &outputSize, bool lossless);

// ── Hardware H.264 encoding ──
// Optional hardware encoders in preference order: VA-API (vah264enc, else
// the low-power vah264lpenc), then NVENC (nvh264enc). x264enc/openh264enc
// stay the required software fallback.
QStringList hardwareGstH264Encoders();
bool isHardwareGstH264Encoder(const QString &encoder);
// Elements the encoder needs to receive frames (vapostproc, cudaupload ...).
QStringList gstH264EncoderHelperElements(const QString &encoder);
// Hardware encoders whose element and helper elements are installed.
QStringList installedHardwareGstH264Encoders(const QStringList &availableElements);
// Every optional element, for runtime checks that list them as extras.
QStringList optionalPortalRecordingElements();
// Frame sizes the encoder accepts (VA-API 128..4096, NVENC 160x64..4096).
bool gstH264EncoderSupportsSize(const QString &encoder, const QSize &size);
// EShot's CRF (18..32) as a constant quantizer for the hardware encoders.
// Hardware CQP spends bits evenly instead of masking them like x264's CRF,
// so P frames use crf + 2; I frames keep crf.
int hardwareH264QpForCrf(int crf);
// Elements between videocrop and the encoder, ending with the input caps:
// "videoconvert [! videoscale] ! video/x-raw,..." for software encoders,
// "vapostproc ! video/x-raw(memory:VAMemory),..." for VA-API and
// "cudaupload ! cudaconvertscale ! video/x-raw(memory:CUDAMemory),..." for NVENC.
QStringList gstH264EncoderInputArguments(const QString &encoder, int fps, const QSize &outputSize,
                                         bool needsScale, bool lossless);
// The encoder to use for one recording. Lossless intermediates and disabled
// hardware encoding always use `softwareEncoder`; otherwise the first
// hardware encoder from `workingHardwareEncoders` (probe results, preference
// order) that accepts the frame size.
QString selectPortalH264Encoder(const QStringList &workingHardwareEncoders,
                                const QString &softwareEncoder, bool hardwareEnabled,
                                bool lossless, const QSize &outputSize);
// gst-launch arguments of a short real encode used to verify an encoder:
// videotestsrc num-buffers=5 at 320x240 through the same input chain and
// encoder properties as a recording.
QStringList gstH264EncoderProbeArguments(const QString &encoder);
// How long one probe may run before it counts as failed.
int gstH264EncoderProbeTimeoutMs();
// Probe results ("encoder" -> worked) keyed by a signature of the GStreamer
// installation. Results recorded for another signature are discarded.
struct GstH264ProbeState {
    QString signature;
    QStringList working;
    QStringList failed;
};
GstH264ProbeState gstH264ProbeStateForSignature(const GstH264ProbeState &stored,
                                                const QString &currentSignature);
// Next hardware encoder to probe, or empty when done: stops at the first
// working encoder so the dGPU is not woken when VA-API works; vah264lpenc is
// only tried when vah264enc failed.
QString nextGstH264EncoderToProbe(const GstH264ProbeState &state);
// Signature from the gst-launch path and mtime, the registry mtimes and the
// GPU driver versions: any plugin or driver update re-runs the probe.
QString gstH264ProbeSignature(const QString &gstLaunchPath, qint64 gstLaunchMtime,
                              const QList<qint64> &registryMtimes,
                              const QString &driverVersions);
// Paused portal recordings are joined with FFmpeg's concat demuxer when
// FFmpeg is installed. splitmuxsrc is only the fallback: it measures each
// part by seeking near its end, and qtdemux's fragmented seek loses the audio
// track when video fragments precede the audio ones in the file, which is the
// normal layout with low-latency (hardware) encoders. The joined file then
// ends after the first part's audio.
bool joinPortalSegmentsWithFfmpeg(bool ffmpegAvailable);
// A hardware encoder whose first pipeline of a recording failed during
// startup (before PLAYING or within 5 s), before any segment was kept, is
// dropped and the start is retried once with the software encoder.
bool shouldRetryPortalStartWithSoftwareEncoder(const QString &encoder, bool firstPipeline,
                                               bool keptAnySegment, bool expectedStop,
                                               bool reachedPlaying, qint64 runMs,
                                               bool alreadyRetried);
// pipewiresrc ... ! h264parse
QStringList portalVideoBranchArguments(const PortalVideoPipelineOptions &options);
// Complete gst-launch arguments: fragmented mp4mux (a killed recording stays
// playable), the video branch and the optional audio branch.
QStringList portalRecordingPipelineArguments(const PortalVideoPipelineOptions &options,
                                             const QString &outputPath,
                                             const QStringList &audioArguments = {});
// How long a stopped gst-launch may take to finalize before it is killed.
int gstStopGraceMs(qint64 recordedMs);

QStringList waylandRecordingAudioArguments(bool desktopEnabled, int desktopVolume,
                                           const QString &desktopDevice,
                                           bool microphoneEnabled, int microphoneVolume,
                                           const QString &microphoneDevice,
                                           const QString &aacEncoder);
// PulseAudio names that work on PulseAudio and pipewire-pulse alike.
QString linuxDesktopAudioDevice(const QString &configuredDevice);
struct LinuxMicrophoneResolution {
    QString device;
    bool missing = false;
};
// "default" maps to @DEFAULT_SOURCE@. When the device list could be read
// (discoveryOk), a missing default or named microphone is reported.
LinuxMicrophoneResolution resolveLinuxMicrophoneDevice(
    const QString &configuredDevice,
    const QList<QPair<QString, QString>> &discoveredDevices,
    bool discoveryOk);
QList<QPair<QString, QString>> linuxMicrophoneDevices(const QString &pactlSources);
QList<QPair<QString, QString>> discoverLinuxMicrophoneDevices(bool *ok = nullptr);
