#ifndef PINNEDWINDOW_H
#define PINNEDWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QMenu>

class PinnedWindow : public QWidget {
    Q_OBJECT

public:
    explicit PinnedWindow(const QPixmap &pixmap, const QPoint &screenPos, QWidget *parent = nullptr);
    ~PinnedWindow();

    QPixmap pixmap() const { return m_pixmap; }
    void setOpacity(double opacity);
    double opacity() const { return m_opacity; }

signals:
    void closed(PinnedWindow *window);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QPixmap m_pixmap;       // Original image (never mutated)
    double m_scale;         // Scale factor (1.0 = original size)
    QPoint m_dragOffset;
    bool m_dragging;
    bool m_hovered;
    double m_opacity;

    void updateWindowSize();

    // Logical (device-independent) size of the source image. The pixmap is
    // captured at full physical resolution and tagged with the target screen's
    // devicePixelRatio, so this is the size it should occupy on screen at 100%.
    QSizeF baseSize() const {
        qreal dpr = m_pixmap.devicePixelRatio();
        if (dpr <= 0.0) dpr = 1.0;
        return QSizeF(m_pixmap.width() / dpr, m_pixmap.height() / dpr);
    }

    // Resizing
    enum ResizeHandle { NoHandle, TopLeft, TopRight, BottomLeft, BottomRight };
    ResizeHandle m_resizeHandle;
    QRect handleRect(ResizeHandle handle) const;
    ResizeHandle getResizeHandle(const QPoint &pos) const;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
    double m_resizeStartScale;

    // Close button area
    QRect closeButtonRect() const;
};

#endif
