#include "ImageEffects.h"

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

// Floor division that also works for negative grid offsets.
int floorDiv(int value, int divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

// One box-blur pass along a line of `count` pixels spaced `stride` apart.
// Samples beyond the ends repeat the edge pixel.
void boxBlurLine(const QRgb *in, QRgb *out, int count, int stride, int radius)
{
    const int window = radius * 2 + 1;
    auto at = [&](int i) { return in[qBound(0, i, count - 1) * stride]; };

    std::array<int, 4> sum = {0, 0, 0, 0};
    for (int i = -radius; i <= radius; ++i) {
        const QRgb p = at(i);
        sum[0] += qAlpha(p);
        sum[1] += qRed(p);
        sum[2] += qGreen(p);
        sum[3] += qBlue(p);
    }
    for (int i = 0; i < count; ++i) {
        out[i * stride] = qRgba((sum[1] + window / 2) / window,
                                (sum[2] + window / 2) / window,
                                (sum[3] + window / 2) / window,
                                (sum[0] + window / 2) / window);
        const QRgb added = at(i + radius + 1);
        const QRgb removed = at(i - radius);
        sum[0] += qAlpha(added) - qAlpha(removed);
        sum[1] += qRed(added) - qRed(removed);
        sum[2] += qGreen(added) - qGreen(removed);
        sum[3] += qBlue(added) - qBlue(removed);
    }
}

void boxBlur(QImage &image, int radius)
{
    const int width = image.width();
    const int height = image.height();
    const int stride = image.bytesPerLine() / int(sizeof(QRgb));
    QRgb *pixels = reinterpret_cast<QRgb *>(image.bits());

    std::vector<QRgb> line(std::max(width, height));
    for (int y = 0; y < height; ++y) {
        QRgb *row = pixels + y * stride;
        boxBlurLine(row, line.data(), width, 1, radius);
        std::copy(line.begin(), line.begin() + width, row);
    }
    std::vector<QRgb> column(height);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y)
            column[y] = pixels[y * stride + x];
        boxBlurLine(column.data(), line.data(), height, 1, radius);
        for (int y = 0; y < height; ++y)
            pixels[y * stride + x] = line[y];
    }
}

}

namespace ImageEffects {

QImage pixelate(const QImage &source, int blockSize, const QPoint &gridOrigin)
{
    if (source.isNull() || blockSize <= 1)
        return source;

    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int width = image.width();
    const int height = image.height();
    const int firstX = floorDiv(gridOrigin.x(), blockSize) * blockSize - gridOrigin.x();
    const int firstY = floorDiv(gridOrigin.y(), blockSize) * blockSize - gridOrigin.y();

    for (int top = firstY; top < height; top += blockSize) {
        const int y0 = qMax(0, top);
        const int y1 = qMin(height, top + blockSize);
        for (int left = firstX; left < width; left += blockSize) {
            const int x0 = qMax(0, left);
            const int x1 = qMin(width, left + blockSize);
            if (x0 >= x1 || y0 >= y1)
                continue;

            quint64 a = 0, r = 0, g = 0, b = 0;
            for (int y = y0; y < y1; ++y) {
                const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                for (int x = x0; x < x1; ++x) {
                    a += qAlpha(row[x]);
                    r += qRed(row[x]);
                    g += qGreen(row[x]);
                    b += qBlue(row[x]);
                }
            }
            const quint64 n = quint64(x1 - x0) * quint64(y1 - y0);
            const QRgb average = qRgba(int((r + n / 2) / n), int((g + n / 2) / n),
                                       int((b + n / 2) / n), int((a + n / 2) / n));
            for (int y = y0; y < y1; ++y) {
                QRgb *row = reinterpret_cast<QRgb *>(image.scanLine(y));
                std::fill(row + x0, row + x1, average);
            }
        }
    }
    return image.convertToFormat(source.format());
}

QImage smoothBlur(const QImage &source, int radius)
{
    if (source.isNull() || radius <= 0)
        return source;

    // Large radii are blurred on a reduced copy: the result is visually the
    // same and it keeps the live preview responsive on big regions.
    const int factor = qMax(1, radius / 6);
    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (factor > 1) {
        image = image.scaled(qMax(1, image.width() / factor), qMax(1, image.height() / factor),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // Three box passes of radius r approximate a Gaussian with sigma ~ r.
    const int scaledRadius = qMax(1, qRound(double(radius) / factor));
    for (int pass = 0; pass < 3; ++pass)
        boxBlur(image, scaledRadius);

    if (factor > 1) {
        image = image.scaled(source.width(), source.height(),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return image.convertToFormat(source.format());
}

}
