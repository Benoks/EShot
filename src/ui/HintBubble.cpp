#include "HintBubble.h"

#include "core/TranslationManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int ArrowSize = 9;
constexpr int AutoHideMs = 12000;
}

HintBubble::HintBubble(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_AlwaysShowToolTips);
    setFocusPolicy(Qt::NoFocus);

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setStyleSheet(QStringLiteral("color: #f5f5f5; font-size: 13px; background: transparent;"));
    m_label->setMaximumWidth(280);

    m_button = new QPushButton(TranslationManager::tr("tipGotIt"), this);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->setFocusPolicy(Qt::NoFocus);
    m_button->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3a3a3a; color: #f5f5f5; border: 1px solid #505050;"
        " border-radius: 8px; padding: 4px 14px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background: #454545; border-color: #606060; }"
        "QPushButton:pressed { background: #333333; }"));
    connect(m_button, &QPushButton::clicked, this, &HintBubble::dismiss);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(m_button);
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->addWidget(m_label);
    layout->addLayout(buttonRow);

    m_autoHide.setSingleShot(true);
    connect(&m_autoHide, &QTimer::timeout, this, &HintBubble::dismiss);
    hide();
}

void HintBubble::showTip(const QString &text, const QRect &anchor, Pointer pointer,
                         const QRect &bounds)
{
    m_pointer = pointer;
    m_label->setText(text);
    // Leave room for the arrow on the side that points at the anchor.
    const int m = 12;
    auto *box = static_cast<QVBoxLayout *>(layout());
    box->setContentsMargins(m + (pointer == Pointer::Left ? ArrowSize : 0),
                            m + (pointer == Pointer::Up ? ArrowSize : 0), m,
                            m + (pointer == Pointer::Down ? ArrowSize : 0));
    adjustSize();

    QPoint topLeft;
    switch (pointer) {
    case Pointer::Left:
        topLeft = QPoint(anchor.right() + 4, anchor.center().y() - height() / 2);
        break;
    case Pointer::Up:
        topLeft = QPoint(anchor.center().x() - width() / 2, anchor.bottom() + 4);
        break;
    case Pointer::Down:
        topLeft = QPoint(anchor.center().x() - width() / 2, anchor.top() - height() - 4);
        break;
    }
    const QRect area = bounds.adjusted(6, 6, -6, -6);
    topLeft.setX(qBound(area.left(), topLeft.x(), qMax(area.left(), area.right() - width())));
    topLeft.setY(qBound(area.top(), topLeft.y(), qMax(area.top(), area.bottom() - height())));
    move(topLeft);

    // Keep the arrow on the anchor even when the bubble was pushed inside bounds.
    m_arrowOffset = pointer == Pointer::Left
        ? qBound(14, anchor.center().y() - topLeft.y(), height() - 14)
        : qBound(14, anchor.center().x() - topLeft.x(), width() - 14);

    show();
    raise();
    m_autoHide.start(AutoHideMs);
    update();
}

void HintBubble::dismiss()
{
    m_autoHide.stop();
    if (!isVisible())
        return;
    hide();
    emit dismissed();
}

void HintBubble::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPolygonF arrow;
    switch (m_pointer) {
    case Pointer::Left:
        body.setLeft(body.left() + ArrowSize);
        arrow << QPointF(body.left() + 1, m_arrowOffset - ArrowSize)
              << QPointF(0.5, m_arrowOffset)
              << QPointF(body.left() + 1, m_arrowOffset + ArrowSize);
        break;
    case Pointer::Up:
        body.setTop(body.top() + ArrowSize);
        arrow << QPointF(m_arrowOffset - ArrowSize, body.top() + 1)
              << QPointF(m_arrowOffset, 0.5)
              << QPointF(m_arrowOffset + ArrowSize, body.top() + 1);
        break;
    case Pointer::Down:
        body.setBottom(body.bottom() - ArrowSize);
        arrow << QPointF(m_arrowOffset - ArrowSize, body.bottom() - 1)
              << QPointF(m_arrowOffset, height() - 0.5)
              << QPointF(m_arrowOffset + ArrowSize, body.bottom() - 1);
        break;
    }

    QPainterPath path;
    path.addRoundedRect(body, 10, 10);
    QPainterPath pointerPath;
    pointerPath.addPolygon(arrow);
    pointerPath.closeSubpath();
    path = path.united(pointerPath);

    // Same greys as the overlay toolbar and panels.
    painter.setPen(QPen(QColor(0x50, 0x50, 0x50), 1.2));
    painter.setBrush(QColor(0x2d, 0x2d, 0x2d, 250));
    painter.drawPath(path);
}
