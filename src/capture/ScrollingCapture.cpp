#include "ScrollingCapture.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QPainter>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ScrollingCapture::ScrollingCapture(QObject *parent) : QObject(parent) {}

ScrollingCapture::~ScrollingCapture()
{
    if (m_running) stop();
}

void ScrollingCapture::start(const QRect &captureRect, int scrollAmount, int intervalMs, int maxFrames)
{
    if (m_running) return;
    if (captureRect.width() < 16 || captureRect.height() < 16) {
        emit failed(QStringLiteral("region too small"));
        return;
    }
    if (scrollAmount < 10) scrollAmount = 60;
    if (intervalMs < 30)  intervalMs = 150;
    if (maxFrames < 2)    maxFrames = 20;
    if (maxFrames > 60)   maxFrames = 60;

    m_captureRect = captureRect;
    m_scrollAmount = scrollAmount;
    m_intervalMs = intervalMs;
    m_maxFrames = maxFrames;
    m_frames.clear();

    m_timer = new QTimer(this);
    m_timer->setInterval(m_intervalMs);
    connect(m_timer, &QTimer::timeout, this, &ScrollingCapture::captureNext);

    m_running = true;
    emit started();
    QTimer::singleShot(250, this, [this]() {
        if (!m_running || !m_timer) return;
        captureNext();
        if (m_running && m_timer) m_timer->start();
    });
}

void ScrollingCapture::stop(const QString &reason)
{
    if (!m_running) return;
    m_running = false;
    if (m_timer) { m_timer->stop(); m_timer->deleteLater(); m_timer = nullptr; }

    if (!reason.isEmpty()) {
        emit failed(reason);
        return;
    }
    if (m_frames.isEmpty()) {
        emit failed(QStringLiteral("no frames captured"));
        return;
    }
    QImage result = stitchFrames();
    emit finished(result);
}

void ScrollingCapture::captureNext()
{
    if (!m_running) return;

    QImage frame = grabScreenRegion(m_captureRect);
    if (frame.isNull()) {
        stop(QStringLiteral("capture failed"));
        return;
    }

    if (!m_frames.isEmpty()) {
        const QImage &prev = m_frames.last();
        int diff = 0;
        int sampleW = qMin(64, prev.width());
        int sampleH = qMin(64, prev.height());
        for (int y = 0; y < sampleH; y += 4) {
            const quint32 *p1 = reinterpret_cast<const quint32 *>(prev.constScanLine(y));
            const quint32 *p2 = reinterpret_cast<const quint32 *>(frame.constScanLine(y));
            for (int x = 0; x < sampleW; x += 4) {
                if (p1[x] != p2[x]) ++diff;
            }
        }
        if (diff == 0) {
            // No change, page may have stopped scrolling
        }
    }

    m_frames.append(frame);
    emit frameAdded(m_frames.size());
    emit progress(m_frames.size());

    if (m_frames.size() >= m_maxFrames) {
        stop();
        return;
    }

    scrollContent(m_scrollAmount);
}

QImage ScrollingCapture::grabScreenRegion(const QRect &rect) const
{
#ifdef Q_OS_WIN
    int sx = rect.x();
    int sy = rect.y();
    int sw = rect.width();
    int sh = rect.height();

    HDC hScreenDC = GetDC(nullptr);
    if (!hScreenDC) return QImage();
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) { ReleaseDC(nullptr, hScreenDC); return QImage(); }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, sw, sh);
    if (!hBitmap) { DeleteDC(hMemDC); ReleaseDC(nullptr, hScreenDC); return QImage(); }

    HGDIOBJ hOld = SelectObject(hMemDC, hBitmap);
    BitBlt(hMemDC, 0, 0, sw, sh, hScreenDC, sx, sy, SRCCOPY | CAPTUREBLT);
    SelectObject(hMemDC, hOld);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = sw;
    bi.biHeight = -sh;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage img(sw, sh, QImage::Format_ARGB32);
    GetDIBits(hMemDC, hBitmap, 0, sh, img.bits(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);

    return img.convertToFormat(QImage::Format_RGB32);
#else
    QScreen *screen = QGuiApplication::screenAt(rect.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return QImage();

    int sx = rect.x() - screen->geometry().x();
    int sy = rect.y() - screen->geometry().y();
    QPixmap pix = screen->grabWindow(0, sx, sy, rect.width(), rect.height());
    return pix.toImage().convertToFormat(QImage::Format_RGB32);
#endif
}

void ScrollingCapture::scrollContent(int amount)
{
    if (amount <= 0) return;
#ifdef Q_OS_WIN
    // Find window at center of capture rect and send WM_MOUSEWHEEL directly
    QPoint center = m_captureRect.center();
    HWND hWnd = WindowFromPoint({center.x(), center.y()});
    if (hWnd) {
        // Convert screen coords to window client coords
        POINT pt = {center.x(), center.y()};
        ScreenToClient(hWnd, &pt);
        LPARAM lParam = MAKELPARAM(pt.x, pt.y);
        WPARAM wParam = MAKEWPARAM(0, -amount); // WHEEL_DELTA negative = scroll down
        PostMessage(hWnd, WM_MOUSEWHEEL, wParam, lParam);
    }
#else
    Q_UNUSED(amount)
#endif
}

QImage ScrollingCapture::stitchFrames() const
{
    if (m_frames.isEmpty()) return QImage();
    if (m_frames.size() == 1) return m_frames.first();

    const int w = m_frames.first().width();
    const int h = m_frames.first().height();

    // Detect vertical overlap between consecutive frames. A coarse pass keeps
    // finalization responsive for long 4K captures, then a local full-step
    // refinement preserves pixel-level placement around the best candidate.
    QList<int> frameOffsets;
    frameOffsets.append(0);
    for (int i = 0; i + 1 < m_frames.size(); ++i) {
        const auto scoreOffset = [&](int off) {
            int matched = 0;
            int samples = 0;
            for (int y = 0; y < h - off; y += 8) {
                const quint32 *a = reinterpret_cast<const quint32 *>(m_frames[i].constScanLine(y + off));
                const quint32 *b = reinterpret_cast<const quint32 *>(m_frames[i + 1].constScanLine(y));
                for (int x = 0; x < w; x += 16) {
                    const QRgb ca = static_cast<QRgb>(a[x]);
                    const QRgb cb = static_cast<QRgb>(b[x]);
                    if (std::abs(qRed(ca) - qRed(cb)) < 20
                        && std::abs(qGreen(ca) - qGreen(cb)) < 20
                        && std::abs(qBlue(ca) - qBlue(cb)) < 20) {
                        ++matched;
                    }
                    ++samples;
                }
            }
            return samples > 0 ? (matched * 100) / samples : 0;
        };

        const int maxOffset = qMax(1, (h * 4) / 5);
        constexpr int CoarseStep = 8;
        int bestOverlap = 0;
        int bestOffset = 0;
        for (int off = 1; off < maxOffset; off += CoarseStep) {
            const int score = scoreOffset(off);
            if (score > bestOverlap) {
                bestOverlap = score;
                bestOffset = off;
            }
        }
        const int refineStart = qMax(1, bestOffset - CoarseStep);
        const int refineEnd = qMin(maxOffset - 1, bestOffset + CoarseStep);
        for (int off = refineStart; off <= refineEnd; ++off) {
            const int score = scoreOffset(off);
            if (score > bestOverlap) {
                bestOverlap = score;
                bestOffset = off;
            }
        }
        const int stepOverlap = bestOverlap > 60 ? bestOffset : 0;
        frameOffsets.append(frameOffsets.last() + h - stepOverlap);
    }

    const int stitchedH = frameOffsets.last() + h;

    QImage result(w, stitchedH, QImage::Format_RGB32);
    result.fill(Qt::white);
    QPainter p(&result);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (int i = 0; i < m_frames.size(); ++i) {
        p.drawImage(0, frameOffsets[i], m_frames[i]);
    }
    p.end();
    return result;
}
