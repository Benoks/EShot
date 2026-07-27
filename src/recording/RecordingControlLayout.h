#ifndef RECORDINGCONTROLLAYOUT_H
#define RECORDINGCONTROLLAYOUT_H

#include <QRect>
#include <QList>
#include <QSize>

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
    WindowsGraphicsCapture,
    LinuxPortal
};

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
bool recordingCaptureBackendCanExcludeOverlay(RecordingCaptureBackend backend);
bool recordingBorderStaysOnTop(bool locked);
QRect movedRecordingControlRect(const QRect &currentControlRect,
                                const QPoint &requestedTopLeft,
                                const QRect &screenRect);

#endif
