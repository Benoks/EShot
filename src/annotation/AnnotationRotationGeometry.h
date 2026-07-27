#ifndef ANNOTATIONROTATIONGEOMETRY_H
#define ANNOTATIONROTATIONGEOMETRY_H

#include <QPointF>
#include <QRectF>

class AnnotationRotationGeometry {
public:
    static QPointF handleCenter(const QRectF &bounds);
    static qreal angleAt(const QPointF &center, const QPointF &cursor);
    static qreal snappedAngle(qreal degrees, bool snap);
};

#endif
