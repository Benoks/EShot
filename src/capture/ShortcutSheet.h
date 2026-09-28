#pragma once

#include <QRect>
#include <QWidget>

class QPainter;

// Full-size layer that shows the shortcut sheet above every other overlay
// control (toolbar, action panel, Quick Settings). A click closes it.
class ShortcutSheetLayer : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutSheetLayer(QWidget *parent);
    // Covers the parent and centres the card on monitorRect.
    void open(const QRect &monitorRect);

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QRect m_monitorRect;
};

// The "?" overlay card listing every capture shortcut and mouse gesture.
// Paints a card centred on monitorRect using the user's configured keys.
void drawShortcutSheet(QPainter &painter, const QRect &monitorRect);
