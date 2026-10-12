#ifndef RECORDINGCONTROLLAYOUT_H
#define RECORDINGCONTROLLAYOUT_H

#include <QRect>
#include <QList>
#include <QSize>
#include <QString>

enum class RecordingControlPlacement {
    Below,
    Above,
    Inside
};

enum class RecordingOverlayVisibility {
    AlwaysVisible,
    RevealWhilePaused
};

enum class RecordingCaptureBackend {
    WindowsGdiGrab,
    // FFmpeg ddagrab (Desktop Duplication API).
    WindowsDdaGrab,
    WindowsGraphicsCapture,
    LinuxPortal
};

// What a recording on this platform uses unless the recorder says otherwise.
inline RecordingCaptureBackend defaultRecordingCaptureBackend()
{
#ifdef Q_OS_WIN
    return RecordingCaptureBackend::WindowsGdiGrab;
#else
    return RecordingCaptureBackend::LinuxPortal;
#endif
}

// Where the pause/stop/cancel controls of a running recording live. With a
// system tray they are in the tray menu and only the border is drawn; a
// floating control bar is kept for desktops without a tray (stock GNOME
// without an AppIndicator extension) so the recording can still be stopped.
enum class RecordingControlsPresentation {
    ControlBar,
    TrayMenu
};

RecordingControlsPresentation recordingControlsPresentation(bool systemTrayAvailable,
                                                            bool trayIconVisible);

// The "control the recording from the tray" notification: at most once per
// session, never after "Don't show again", and only for the first few
// sessions so it does not turn into noise.
constexpr int RecordingTrayTipMaxSessions = 3;
bool shouldShowRecordingTrayTip(bool shownThisSession, bool dismissed, int sessionsShown);

struct RecordingControlLayout {
    QRect controlRect;
    QRect controlScreenRect;
    RecordingControlPlacement placement = RecordingControlPlacement::Below;
    bool compact = false;
};

RecordingControlLayout recordingControlLayout(const QRect &captureRect,
                                               const QRect &screenRect,
                                               const QSize &preferredSize,
                                               const QSize &compactSize,
                                               const QList<QRect> &alternativeScreens = {});

RecordingOverlayVisibility recordingOverlayVisibilityPolicy(
    RecordingControlPlacement placement,
    bool platformCanExcludeOverlay);
RecordingOverlayVisibility recordingOverlayVisibilityPolicy(
    const QRect &controlRect,
    const QRect &captureRect,
    bool platformCanExcludeOverlay);
// First Windows build (10 2004) where WDA_EXCLUDEFROMCAPTURE removes a
// window from desktop-DC BitBlt captures such as FFmpeg's gdigrab and from
// Desktop Duplication (ddagrab).
constexpr int RecordingWindowsCaptureExclusionMinimumBuild = 19041;

// windowsBuild and displayAffinityConfirmed describe the running system:
// the Windows build number (0 elsewhere) and whether GetWindowDisplayAffinity
// reported WDA_EXCLUDEFROMCAPTURE for the control window.
bool recordingCaptureBackendCanExcludeOverlay(RecordingCaptureBackend backend,
                                              int windowsBuild = 0,
                                              bool displayAffinityConfirmed = false);
bool recordingBorderStaysOnTop(bool locked);
// Controls outside the recorded area, or excluded from the capture, can stay
// above other windows without ending up in the video.
bool recordingControlBarStaysOnTop(const QRect &controlRect,
                                   const QRect &captureRect,
                                   bool platformCanExcludeOverlay);
// Controls hidden for the whole recording need another way to stop it.
bool recordingControlsNeedHiddenHint(RecordingOverlayVisibility visibility);
QRect movedRecordingControlRect(const QRect &currentControlRect,
                                const QPoint &requestedTopLeft,
                                const QRect &screenRect);


// "mm:ss", or "mm:ss / mm:ss" when the recording has a time limit.
QString recordingStatusTimeText(int elapsedSeconds, int limitSeconds);
// True in the last five seconds before the time limit stops the recording.
// remainingSeconds < 0 means "unknown"; it is then derived from the limit.
bool recordingTimeLimitWarning(int elapsedSeconds, int limitSeconds,
                               int remainingSeconds = -1);

#endif
