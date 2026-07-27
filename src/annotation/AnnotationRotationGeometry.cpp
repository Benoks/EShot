#include "AnnotationRotationGeometry.h"

#include <QtMath>

namespace {
constexpr qreal kHandleGap = 30.0;
constexpr qreal kSnapDegrees = 15.0;
}

QPointF AnnotationRotationGeometry::handleCenter(const QRectF &bounds)
{
    return QPointF(bounds.center().x(), bounds.bottom() + kHandleGap);
}

qreal AnnotationRotationGeometry::angleAt(const QPointF &center, const QPointF &cursor)
{
    return qRadiansToDegrees(qAtan2(cursor.y() - center.y(), cursor.x() - center.x()));
}

qreal AnnotationRotationGeometry::snappedAngle(qreal degrees, bool snap)
{
    return snap ? qRound(degrees / kSnapDegrees) * kSnapDegrees : degrees;
}
