#include <QtTest>

#include "annotation/AnnotationRotationGeometry.h"

class AnnotationRotationGeometryTests : public QObject {
    Q_OBJECT

private slots:
    void placesHandleBelowSelectionBounds();
    void measuresClockwiseAngleFromAnnotationCenter();
    void snapsOnlyWhenRequested();
};

void AnnotationRotationGeometryTests::placesHandleBelowSelectionBounds()
{
    QCOMPARE(AnnotationRotationGeometry::handleCenter(QRectF(10, 20, 100, 40)), QPointF(60, 90));
}

void AnnotationRotationGeometryTests::measuresClockwiseAngleFromAnnotationCenter()
{
    QCOMPARE(AnnotationRotationGeometry::angleAt(QPointF(60, 40), QPointF(60, 80)), 90.0);
}

void AnnotationRotationGeometryTests::snapsOnlyWhenRequested()
{
    QCOMPARE(AnnotationRotationGeometry::snappedAngle(22.0, false), 22.0);
    QCOMPARE(AnnotationRotationGeometry::snappedAngle(22.0, true), 15.0);
}

QTEST_MAIN(AnnotationRotationGeometryTests)

#include "AnnotationRotationGeometryTests.moc"
