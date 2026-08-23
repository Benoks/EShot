#include "AnnotationRotationGeometry.h"

#include <QtGlobal>
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
    // Legacy behavior: atan2(0, 0) collapses to 0 when the cursor sits on the
    // rotation center.
    qreal degrees = 0.0;
    angleAt(center, cursor, degrees);
    return degrees;
}

bool AnnotationRotationGeometry::angleAt(const QPointF &center, const QPointF &cursor,
                                         qreal &outDegrees)
{
    const qreal dx = cursor.x() - center.x();
    const qreal dy = cursor.y() - center.y();
    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy))
        return false; // angle is undefined on the rotation center
    outDegrees = qRadiansToDegrees(qAtan2(dy, dx));
    return true;
}

qreal AnnotationRotationGeometry::snappedAngle(qreal degrees, bool snap)
{
    return snap ? qRound(degrees / kSnapDegrees) * kSnapDegrees : degrees;
}
