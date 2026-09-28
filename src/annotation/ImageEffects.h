#pragma once

#include <QImage>
#include <QPoint>

// Redaction effects shared by the live preview and the saved capture.
namespace ImageEffects {

// Mosaic with square blocks of blockSize pixels. Each block shows the average
// colour of the pixels it covers. Block edges fall on multiples of blockSize
// counted from -gridOrigin, so a region cut from a larger image keeps the same
// grid as its neighbours and blocks do not shift while a selection is resized.
QImage pixelate(const QImage &source, int blockSize, const QPoint &gridOrigin = QPoint());

// Smooth blur approximating a Gaussian with the given radius in pixels
// (three box-blur passes). Edge pixels are extended, so pass a region with
// some margin around the area that must look right.
QImage smoothBlur(const QImage &source, int radius);

}
