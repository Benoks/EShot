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
