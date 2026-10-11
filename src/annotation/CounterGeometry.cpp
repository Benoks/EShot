#include "CounterGeometry.h"

#include <QLineF>
#include <QtGlobal>
#include <QtMath>

int CounterGeometry::nextValue(int startValue, int highestPlaced)
{
    return qMax(startValue, highestPlaced + 1);
}

bool CounterGeometry::isPointerDrag(const QPoint &press, const QPoint &release, int dragDistance)
{
    return (release - press).manhattanLength() >= qMax(1, dragDistance);
}

bool CounterGeometry::hasVisiblePointer(const QPointF &center, qreal radius, const QPointF &target)
{
    return QLineF(center, target).length() > radius;
}

QPointF CounterGeometry::pointerStart(const QPointF &center, qreal radius, const QPointF &target)
{
    const QPointF delta = target - center;
    const qreal length = qSqrt(QPointF::dotProduct(delta, delta));
    if (length <= 0.0001)
        return center;
    return center + delta * (radius / length);
}

qreal CounterGeometry::arrowHeadSize(int penWidth)
{
    return qMax(12, penWidth * 4);
}

CounterGeometry::ArrowHead CounterGeometry::arrowHead(const QPointF &from, const QPointF &tip,
                                                      int penWidth)
{
    const qreal angle = qAtan2(tip.y() - from.y(), tip.x() - from.x());
    const qreal size = arrowHeadSize(penWidth);
    ArrowHead head;
    head.tip = tip;
    head.left = tip - QPointF(qCos(angle - M_PI / 6) * size, qSin(angle - M_PI / 6) * size);
    head.right = tip - QPointF(qCos(angle + M_PI / 6) * size, qSin(angle + M_PI / 6) * size);
    return head;
}

QRectF CounterGeometry::pointerBounds(const QPointF &center, qreal radius, const QPointF &target,
                                      int penWidth)
{
    const QPointF start = pointerStart(center, radius, target);
    const ArrowHead head = arrowHead(start, target, penWidth);
    qreal left = start.x(), right = start.x(), top = start.y(), bottom = start.y();
    for (const QPointF &p : {head.tip, head.left, head.right}) {
        left = qMin(left, p.x());
        right = qMax(right, p.x());
        top = qMin(top, p.y());
        bottom = qMax(bottom, p.y());
    }
    const QRectF shape(QPointF(left, top), QPointF(right, bottom));
    const qreal stroke = penWidth / 2.0;
    return shape.adjusted(-stroke, -stroke, stroke, stroke);
}

qreal CounterGeometry::numberPointSize(int diameter, int value)
{
    const qreal size = 10.0 * qBound(MinDiameter, diameter, MaxDiameter) / DefaultDiameter;
    return qAbs(value) >= 100 ? size * 0.8 : size;
}
