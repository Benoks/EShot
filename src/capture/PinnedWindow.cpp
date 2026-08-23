#include "PinnedWindow.h"

#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QScreen>
#include <QFileDialog>
#include <QDebug>
#include "../core/TranslationManager.h"

static constexpr int BAR_HEIGHT = 32;
static constexpr int CLOSE_BTN_SIZE = 20;
static constexpr int HANDLE_SIZE = 8;
static constexpr double MIN_SCALE = 0.1;
static constexpr double MAX_SCALE = 5.0;

PinnedWindow::PinnedWindow(const QPixmap &pixmap, const QPoint &screenPos, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_pixmap(pixmap)
    , m_scale(1.0)
    , m_dragging(false)
    , m_hovered(false)
    , m_opacity(1.0)
    , m_resizeHandle(NoHandle)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setCursor(Qt::SizeAllCursor);

    // The pixmap holds the selection at full physical resolution. Tag it with the
    // devicePixelRatio of the screen it appears on so it is displayed at the same
    // size it was captured (otherwise it renders DPR-times too large on HiDPI).
    qreal dpr = 1.0;
    if (QScreen *screen = QGuiApplication::screenAt(screenPos))
        dpr = screen->devicePixelRatio();
    else if (QScreen *screen = QGuiApplication::primaryScreen())
        dpr = screen->devicePixelRatio();
    if (dpr > 0.0)
        m_pixmap.setDevicePixelRatio(dpr);

    updateWindowSize();
    move(screenPos);
    show();

    qDebug() << "[PinnedWindow] Created at" << screenPos << "size:" << m_pixmap.size();
}

PinnedWindow::~PinnedWindow()
{
    qDebug() << "[PinnedWindow] Destroyed";
}

void PinnedWindow::updateWindowSize()
{
    const QSizeF base = baseSize();
    int w = qMax(1, static_cast<int>(base.width() * m_scale));
    int h = qMax(1, static_cast<int>(base.height() * m_scale));
    setFixedSize(w + 2, h + BAR_HEIGHT + 2);
}

void PinnedWindow::setOpacity(double opacity)
{
    m_opacity = qBound(0.1, opacity, 1.0);
    setWindowOpacity(m_opacity);
    update();
}

QRect PinnedWindow::closeButtonRect() const
{
    int x = width() - CLOSE_BTN_SIZE - 8;
    int y = (BAR_HEIGHT - CLOSE_BTN_SIZE) / 2 + 1;
    return QRect(x, y, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE);
}

QRect PinnedWindow::handleRect(ResizeHandle handle) const
{
    int s = HANDLE_SIZE;
    switch (handle) {
        case TopLeft:      return QRect(0, 0, s, s);
        case TopRight:     return QRect(width() - s, 0, s, s);
        case BottomLeft:   return QRect(0, height() - s, s, s);
        case BottomRight:  return QRect(width() - s, height() - s, s, s);
        default:           return QRect();
    }
}

PinnedWindow::ResizeHandle PinnedWindow::getResizeHandle(const QPoint &pos) const
{
    if (handleRect(TopLeft).contains(pos))     return TopLeft;
    if (handleRect(TopRight).contains(pos))    return TopRight;
    if (handleRect(BottomLeft).contains(pos))  return BottomLeft;
    if (handleRect(BottomRight).contains(pos)) return BottomRight;
    return NoHandle;
}

void PinnedWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::transparent);

    // Draw original image at scaled size
    const QSizeF base = baseSize();
    int drawW = qMax(1, static_cast<int>(base.width() * m_scale));
    int drawH = qMax(1, static_cast<int>(base.height() * m_scale));
    painter.drawPixmap(1, BAR_HEIGHT + 1, drawW, drawH, m_pixmap);

    // Top bar
    QPainterPath barPath;
    barPath.addRoundedRect(QRectF(1, 1, width() - 2, BAR_HEIGHT), 6, 6);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 20, 210));
    painter.drawPath(barPath);

    if (m_hovered) {
        // Close button
        QRect closeRect = closeButtonRect();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(70, 70, 70, 200));
        painter.drawRoundedRect(closeRect, 3, 3);

        painter.setPen(QPen(QColor(200, 200, 200), 1.5));
        int m = 5;
        painter.drawLine(closeRect.left() + m, closeRect.top() + m,
                         closeRect.right() - m, closeRect.bottom() - m);
        painter.drawLine(closeRect.right() - m, closeRect.top() + m,
                         closeRect.left() + m, closeRect.bottom() - m);

        // Pin icon
        int iconX = 14;
        int iconY = BAR_HEIGHT / 2;
        painter.setPen(QPen(QColor(0, 122, 204), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPoint(iconX, iconY - 2), 3, 3);
        painter.drawLine(iconX, iconY + 1, iconX, iconY + 7);

        // Size info
        QString sizeText = QString("%1x%2 (%3%)")
            .arg(qRound(base.width())).arg(qRound(base.height()))
            .arg(qRound(m_scale * 100));
        painter.setPen(QColor(180, 180, 180));
        QFont f("Segoe UI", 9);
        f.setBold(true);
        painter.setFont(f);
        painter.drawText(iconX + 8, 0, width() - iconX - 8 - CLOSE_BTN_SIZE - 14, BAR_HEIGHT,
                         Qt::AlignVCenter, sizeText);

        // Resize handles
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 122, 204, 180));
        for (int i = TopLeft; i <= BottomRight; ++i) {
            QRect hr = handleRect(static_cast<ResizeHandle>(i));
            painter.drawRoundedRect(hr, 2, 2);
        }
    }

    // Border
    QPainterPath borderPath;
    borderPath.addRoundedRect(QRectF(1, 1, width() - 2, height() - 2), 7, 7);
    painter.setPen(QPen(QColor(0, 122, 204), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(borderPath);
}

void PinnedWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_hovered && closeButtonRect().contains(event->pos())) {
            close();
            return;
        }

        ResizeHandle handle = getResizeHandle(event->pos());
        if (handle != NoHandle) {
            m_resizeHandle = handle;
            m_resizeStartGlobal = event->globalPosition().toPoint();
            m_resizeStartSize = QSize(width(), height());
            m_resizeStartScale = m_scale;
            return;
        }

        m_dragging = true;
        m_dragOffset = event->pos();
    }
}

void PinnedWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizeHandle != NoHandle) {
        QPoint current = event->globalPosition().toPoint();
        QPoint delta = current - m_resizeStartGlobal;

        // Calculate scale per corner
        double newW, newH;
        switch (m_resizeHandle) {
            case BottomRight:
                newW = m_resizeStartSize.width() + delta.x();
                newH = m_resizeStartSize.height() + delta.y();
                break;
            case BottomLeft:
                newW = m_resizeStartSize.width() - delta.x();
                newH = m_resizeStartSize.height() + delta.y();
                break;
            case TopRight:
                newW = m_resizeStartSize.width() + delta.x();
                newH = m_resizeStartSize.height() - delta.y();
                break;
            case TopLeft:
                newW = m_resizeStartSize.width() - delta.x();
                newH = m_resizeStartSize.height() - delta.y();
                break;
            default:
                return;
        }

        // Keep aspect ratio: scale from the diagonal length ratio so both
        // axes contribute proportionally
        QLineF startDiag(0.0, 0.0, m_resizeStartSize.width(), m_resizeStartSize.height());
        QLineF newDiag(0.0, 0.0, newW, newH);
        double ratio = startDiag.length() > 0.0 ? newDiag.length() / startDiag.length() : 1.0;
        m_scale = qBound(MIN_SCALE, m_resizeStartScale * ratio, MAX_SCALE);

        updateWindowSize();
        update();
        return;
    }

    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }

    // Update cursor
    if (m_hovered) {
        ResizeHandle handle = getResizeHandle(event->pos());
        switch (handle) {
            case TopLeft:
            case BottomRight:
                setCursor(Qt::SizeFDiagCursor); break;
            case TopRight:
            case BottomLeft:
                setCursor(Qt::SizeBDiagCursor); break;
            default:
                setCursor(Qt::SizeAllCursor); break;
        }
    }
}

void PinnedWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_resizeHandle = NoHandle;
    }
}

void PinnedWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    }
}

void PinnedWindow::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_hovered = true;
    update();
}

void PinnedWindow::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hovered = false;
    setCursor(Qt::SizeAllCursor);
    update();
}

void PinnedWindow::wheelEvent(QWheelEvent *event)
{
    double delta = (event->angleDelta().y() / 120.0) * 0.05;
    if (event->modifiers() & Qt::ShiftModifier) {
        // Shift + Scroll → scale (zoom)
        m_scale = qBound(MIN_SCALE, m_scale + delta, MAX_SCALE);
        updateWindowSize();
    } else {
        // Normal Scroll → opacity
        setOpacity(m_opacity + delta);
    }
    update();
}

void PinnedWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *copyAction = menu.addAction(TranslationManager::pinnedCopy());
    connect(copyAction, &QAction::triggered, [this]() {
        QGuiApplication::clipboard()->setPixmap(m_pixmap);
    });

    QAction *saveAction = menu.addAction(TranslationManager::pinnedSave());
    connect(saveAction, &QAction::triggered, [this]() {
        QString path = QFileDialog::getSaveFileName(this, TranslationManager::pinnedSave(),
            QString(), "PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)");
        if (!path.isEmpty()) {
            if (path.endsWith(".jpg", Qt::CaseInsensitive))
                m_pixmap.save(path, "JPEG", 95);
            else if (path.endsWith(".bmp", Qt::CaseInsensitive))
                m_pixmap.save(path, "BMP");
            else
                m_pixmap.save(path, "PNG");
        }
    });

    menu.addSeparator();

    QAction *closeAction = menu.addAction(TranslationManager::pinnedClose());
    connect(closeAction, &QAction::triggered, this, &QWidget::close);

    menu.exec(event->globalPos());
}
