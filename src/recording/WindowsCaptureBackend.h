#pragma once

#include "RecordingControlLayout.h"
#include "WindowsRecordingSupport.h"

#include <QList>
#include <QRect>
#include <QString>

class QProcess;

// Runtime side of the Windows capture backend selection: the FFmpeg probe
// (cached per ffmpeg.exe build) and the DXGI monitor list. The argument
// building and the choice itself are in WindowsRecordingSupport.h. Outside
// Windows these report "not ready" / gdigrab and do nothing.

// QSettings keys (no UI yet):
// "recordingWindowsCaptureBackend": "auto" (default) prefers ddagrab where
//     it works, "gdigrab" always records with GDI.
// "videoHardwareEncoding": bool, default true; false keeps libx264 for
//     ddagrab captures.
inline constexpr char RecordingWindowsCaptureBackendKey[] = "recordingWindowsCaptureBackend";
inline constexpr char RecordingHardwareEncodingKey[] = "videoHardwareEncoding";

// Starts the probe on a worker thread unless a result is cached (in memory
// or in QSettings) or a probe is running. Safe to call often.
void requestWindowsCaptureProbe(const QString &ffmpegPath);
// The probe result; not ready (gdigrab + libx264) while it is still running.
// Never waits.
WindowsCaptureCapabilities cachedWindowsCaptureCapabilities(const QString &ffmpegPath);
// ddagrab failed to start a recording: use gdigrab for the rest of this run.
void disableWindowsDdaGrabForSession(const QString &ffmpegPath);
// Attached DXGI outputs of all adapters.
QList<DxgiOutputInfo> enumerateDxgiOutputs();
// Plan for a recording of `captureRect` (physical desktop pixels) using the
// settings above, the cached probe result and the current monitors.
WindowsVideoPlan windowsVideoPlanForRecording(const QString &ffmpegPath, const QRect &captureRect,
                                              bool losslessIntermediate);
// Capture backend reported to the recording indicator.
RecordingCaptureBackend recordingCaptureBackendForPlan(const WindowsVideoPlan &plan);
// Puts a started recording process in a kill-on-close job object, so it
// does not keep capturing when EShot exits or crashes. No-op elsewhere.
void tieRecordingProcessToApplication(QProcess *process);
// Hides `.eshot_*` intermediates (dot files are not hidden on Windows) and
// unhides a file once it has a visible name. No-op elsewhere.
void syncRecordingFileHiddenAttribute(const QString &path);
