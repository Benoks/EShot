#pragma once

#include <QPoint>
#include <QRect>
#include <QVector>

enum class CaptureSelectionMode {
    FreeRegion,
    Window
};

QRect topmostWindowAt(const QVector<QRect> &windows,
                      const QPoint &point,
                      const QRect &overlayBounds);

bool isWindowSnapClick(const QPoint &pressPosition,
                       const QPoint &releasePosition,
                       int dragThreshold);

QRect windowSnapTargetForMode(CaptureSelectionMode mode,
                              const QVector<QRect> &windows,
                              const QPoint &point,
                              const QRect &overlayBounds);

bool allowsManualSelection(CaptureSelectionMode mode);
// windowModeAllowsDrag: a window capture also accepts a dragged region. On
// KDE the window list arrives from KWin asynchronously and may be missing,
// so dragging stays available there; Windows keeps clicks only.
bool allowsManualSelection(CaptureSelectionMode mode, bool windowModeAllowsDrag);

int windowSnapAnimationDurationMs();
QRect windowSnapTransitionRect(const QRect &from, const QRect &to, qreal progress);
