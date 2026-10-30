#include "WindowSnapPolicy.h"

QRect topmostWindowAt(const QVector<QRect> &windows,
                      const QPoint &point,
                      const QRect &overlayBounds)
{
    if (!overlayBounds.isValid() || !overlayBounds.contains(point))
        return {};

    for (const QRect &window : windows) {
        if (!window.isValid())
            continue;
        const QRect visible = window.intersected(overlayBounds);
        if (visible.isValid() && visible.contains(point))
            return visible;
    }
    return {};
}

bool isWindowSnapClick(const QPoint &pressPosition,
                       const QPoint &releasePosition,
                       int dragThreshold)
{
    return dragThreshold > 0
        && (releasePosition - pressPosition).manhattanLength() < dragThreshold;
}

QRect windowSnapTargetForMode(CaptureSelectionMode mode,
                              const QVector<QRect> &windows,
                              const QPoint &point,
                              const QRect &overlayBounds)
{
    if (mode != CaptureSelectionMode::Window)
        return {};
    return topmostWindowAt(windows, point, overlayBounds);
}

bool allowsManualSelection(CaptureSelectionMode mode)
{
    return allowsManualSelection(mode, false);
}

bool allowsManualSelection(CaptureSelectionMode mode, bool windowModeAllowsDrag)
{
    return mode == CaptureSelectionMode::FreeRegion
        || (mode == CaptureSelectionMode::Window && windowModeAllowsDrag);
}

int windowSnapAnimationDurationMs()
{
    return 120;
}

QRect windowSnapTransitionRect(const QRect &from, const QRect &to, qreal progress)
{
    const qreal bounded = qBound(0.0, progress, 1.0);
    const qreal remaining = 1.0 - bounded;
    const qreal eased = 1.0 - remaining * remaining * remaining;
    const auto interpolate = [eased](int start, int end) {
        return qRound(start + (end - start) * eased);
    };
    return QRect(interpolate(from.x(), to.x()),
                 interpolate(from.y(), to.y()),
                 interpolate(from.width(), to.width()),
                 interpolate(from.height(), to.height()));
}
