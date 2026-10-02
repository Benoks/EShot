#pragma once

#include <QImage>
#include <QPainter>
#include <QRect>
#include <QVector>

// KWin's CaptureScreen returns the output's actual scale. Under XWayland,
// QScreen::devicePixelRatio() can instead report 1 for every output.
struct CaptureScreenImage
{
    QRect logical;
    QImage image;
    qreal scale = 1.0;
};

qreal captureSelectionScale(const QVector<CaptureScreenImage> &screens,
                            const QRect &logicalSelection);
QSize captureSelectionSize(const QVector<CaptureScreenImage> &screens,
                          const QRect &logicalSelection);
QImage composeCaptureImages(const QVector<CaptureScreenImage> &screens,
                            const QRect &logicalSelection);
void paintCaptureImages(QPainter *painter,
                        const QVector<CaptureScreenImage> &screens,
                        const QPoint &canvasOrigin);
