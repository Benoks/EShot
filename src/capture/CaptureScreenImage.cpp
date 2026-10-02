#include "CaptureScreenImage.h"
#include "CaptureGeometry.h"

#include <QtMath>

namespace {
bool isUsable(const CaptureScreenImage &screen)
{
    return screen.logical.isValid() && !screen.image.isNull()
        && qIsFinite(screen.scale) && screen.scale > 0.0;
}
}

qreal captureSelectionScale(const QVector<CaptureScreenImage> &screens,
                            const QRect &logicalSelection)
{
    qreal scale = 0.0;
    for (const CaptureScreenImage &screen : screens) {
        if (isUsable(screen) && screen.logical.intersects(logicalSelection))
            scale = qMax(scale, screen.scale);
    }
    return scale;
}

QSize captureSelectionSize(const QVector<CaptureScreenImage> &screens,
                          const QRect &logicalSelection)
{
    const qreal scale = captureSelectionScale(screens, logicalSelection);
    return scale > 0.0
        ? QSize(qRound(logicalSelection.width() * scale),
                qRound(logicalSelection.height() * scale))
        : QSize();
}

QImage composeCaptureImages(const QVector<CaptureScreenImage> &screens,
                            const QRect &logicalSelection)
{
    const qreal scale = captureSelectionScale(screens, logicalSelection);
    const QSize size = captureSelectionSize(screens, logicalSelection);
    if (size.isEmpty())
        return {};

    // A selection contained in one output is a direct native-pixel crop:
    // no resampling, even when another output has a higher scale.
    for (const CaptureScreenImage &screen : screens) {
        if (isUsable(screen) && qFuzzyCompare(screen.scale, scale)
            && screen.logical.contains(logicalSelection)) {
            const QRect source = physicalRectFromLogical(
                logicalSelection.translated(-screen.logical.topLeft()), screen.scale);
            QImage result = screen.image.copy(source);
            result.setDevicePixelRatio(1.0);
            return result;
        }
    }

    // A cross-output image needs one pixel density. Use the highest density
    // actually touched by the selection, with nearest-neighbour enlargement
    // of lower-density outputs rather than introducing interpolation blur.
    QImage result(size, QImage::Format_ARGB32_Premultiplied);
    if (result.isNull())
        return {};
    result.fill(Qt::black);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const CaptureScreenImage &screen : screens) {
        if (!isUsable(screen))
            continue;
        const QRect intersection = screen.logical.intersected(logicalSelection);
        if (intersection.isEmpty())
            continue;
        const QRect source = physicalRectFromLogical(
            intersection.translated(-screen.logical.topLeft()), screen.scale);
        const QRect destination = physicalRectFromLogical(
            intersection.translated(-logicalSelection.topLeft()), scale);
        painter.drawImage(destination, screen.image, source);
    }
    return result;
}

void paintCaptureImages(QPainter *painter,
                        const QVector<CaptureScreenImage> &screens,
                        const QPoint &canvasOrigin)
{
    painter->save();
    // A proxy widget may be painted into multiple views. The QWidget's own
    // DPR is not authoritative; the redirected painter's device transform is.
    const QTransform transform = painter->deviceTransform();
    const qreal paintScale = qSqrt(transform.m11() * transform.m11()
                                  + transform.m12() * transform.m12());
    for (const CaptureScreenImage &screen : screens) {
        if (!isUsable(screen))
            continue;
        // Draw each output independently. In particular a 100% output is
        // blitted 1:1 instead of upscaling to the workspace's maximum DPR
        // and then downscaling again for the preview.
        painter->setRenderHint(QPainter::SmoothPixmapTransform,
                              screen.scale > paintScale);
        painter->drawImage(screen.logical.translated(-canvasOrigin),
                           screen.image, screen.image.rect());
    }
    painter->restore();
}
