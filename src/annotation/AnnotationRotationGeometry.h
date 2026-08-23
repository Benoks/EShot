#ifndef ANNOTATIONROTATIONGEOMETRY_H
#define ANNOTATIONROTATIONGEOMETRY_H

#include <QPointF>
#include <QRectF>

class AnnotationRotationGeometry {
public:
    static QPointF handleCenter(const QRectF &bounds);
    static qreal angleAt(const QPointF &center, const QPointF &cursor);
    // Out-param overload: returns false when the cursor sits on the rotation
    // center (angle undefined) so callers can keep the previous angle.
    static bool angleAt(const QPointF &center, const QPointF &cursor, qreal &outDegrees);
    static qreal snappedAngle(qreal degrees, bool snap);
};

#endif
