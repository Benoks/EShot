#include "RecordingControlLayout.h"

#include <QtGlobal>

namespace {
constexpr int ScreenMargin = 8;
constexpr int ControlGap = 8;
constexpr int InsideInset = 12;

int clampedControlX(const QRect &captureRect, const QRect &screenRect, int width)
{
    const int minimum = screenRect.left() + ScreenMargin;
    const int maximum = screenRect.right() - ScreenMargin - width + 1;
    return qBound(minimum, captureRect.center().x() - (width - 1) / 2,
                  qMax(minimum, maximum));
}
}

RecordingControlLayout recordingControlLayout(const QRect &captureRect,
                                               const QRect &screenRect,
                                               const QSize &preferredSize,
                                               const QSize &compactSize,
                                               const QList<QRect> &alternativeScreens)
{
    RecordingControlLayout result;
    result.controlScreenRect = screenRect;
    const int availableWidth = qMax(1, screenRect.width() - ScreenMargin * 2);
    result.compact = preferredSize.width() > availableWidth;

    QSize size = result.compact ? compactSize : preferredSize;
    size.setWidth(qMin(size.width(), availableWidth));
    const int x = clampedControlX(captureRect, screenRect, size.width());

    const int belowTop = captureRect.bottom() + 1 + ControlGap;
    if (belowTop + size.height() - 1 <= screenRect.bottom() - ScreenMargin) {
        result.placement = RecordingControlPlacement::Below;
        result.controlRect = QRect(QPoint(x, belowTop), size);
        return result;
    }

    const int aboveTop = captureRect.top() - ControlGap - size.height();
    if (aboveTop >= screenRect.top() + ScreenMargin) {
        result.placement = RecordingControlPlacement::Above;
        result.controlRect = QRect(QPoint(x, aboveTop), size);
        return result;
    }

    for (const QRect &alternative : alternativeScreens) {
        if (!alternative.isValid() || alternative.intersects(captureRect))
            continue;
        const int alternativeWidth = qMax(1, alternative.width() - ScreenMargin * 2);
        result.compact = preferredSize.width() > alternativeWidth;
        size = result.compact ? compactSize : preferredSize;
        size.setWidth(qMin(size.width(), alternativeWidth));
        const int alternativeX = qBound(
            alternative.left() + ScreenMargin,
            alternative.center().x() - (size.width() - 1) / 2,
            qMax(alternative.left() + ScreenMargin,
                 alternative.right() - ScreenMargin - size.width() + 1));
        const int alternativeY = alternative.bottom() - ScreenMargin - size.height() + 1;
        result.placement = RecordingControlPlacement::Below;
        result.controlScreenRect = alternative;
        result.controlRect = QRect(QPoint(alternativeX, alternativeY), size);
        return result;
    }

    result.placement = RecordingControlPlacement::Inside;
    const int insideTop = qBound(screenRect.top() + ScreenMargin,
                                 captureRect.bottom() - InsideInset - size.height() + 1,
                                 qMax(screenRect.top() + ScreenMargin,
                                      screenRect.bottom() - ScreenMargin - size.height() + 1));
    result.controlRect = QRect(QPoint(x, insideTop), size);
    return result;
}

RecordingOverlayVisibility recordingOverlayVisibilityPolicy(
    RecordingControlPlacement placement,
    bool platformCanExcludeOverlay)
{
    if (placement == RecordingControlPlacement::Inside && !platformCanExcludeOverlay)
        return RecordingOverlayVisibility::RevealWhilePaused;
    return RecordingOverlayVisibility::AlwaysVisible;
}

RecordingOverlayVisibility recordingOverlayVisibilityPolicy(
    const QRect &controlRect,
    const QRect &captureRect,
    bool platformCanExcludeOverlay)
{
    if (!platformCanExcludeOverlay && controlRect.intersects(captureRect))
        return RecordingOverlayVisibility::RevealWhilePaused;
    return RecordingOverlayVisibility::AlwaysVisible;
}

bool recordingCaptureBackendCanExcludeOverlay(RecordingCaptureBackend backend)
{
    // WDA_EXCLUDEFROMCAPTURE is not honored by FFmpeg's GDI BitBlt capture.
    // Keep the positive case explicit for a future Windows Graphics Capture backend.
    return backend == RecordingCaptureBackend::WindowsGraphicsCapture;
}

bool recordingBorderStaysOnTop(bool locked)
{
    return locked;
}

QRect movedRecordingControlRect(const QRect &currentControlRect,
                                const QPoint &requestedTopLeft,
                                const QRect &screenRect)
{
    if (!currentControlRect.isValid() || !screenRect.isValid())
        return currentControlRect;
    const int x = qBound(screenRect.left() + ScreenMargin,
                         requestedTopLeft.x(),
                         qMax(screenRect.left() + ScreenMargin,
                              screenRect.right() - ScreenMargin
                                  - currentControlRect.width() + 1));
    const int y = qBound(screenRect.top() + ScreenMargin,
                         requestedTopLeft.y(),
                         qMax(screenRect.top() + ScreenMargin,
                              screenRect.bottom() - ScreenMargin
                                  - currentControlRect.height() + 1));
    return QRect(QPoint(x, y), currentControlRect.size());
}
