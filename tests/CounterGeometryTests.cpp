#include <QtTest>
#include <QtMath>

#include "annotation/CounterGeometry.h"

class CounterGeometryTests : public QObject {
    Q_OBJECT

private slots:
    void numbersFromTheStartValue();
    void continuesAfterTheHighestPlacedCounter();
    void shortDragsArePlainClicks();
    void pointerStartsOnTheCircleEdgeTowardTheTarget();
    void pointerIsHiddenWhenTheTargetIsInsideTheCircle();
    void arrowHeadMatchesTheArrowTool();
    void arrowHeadGrowsWithThickness();
    void pointerBoundsCoverLineAndHead();
    void numberSizeFollowsTheDiameter();
};

void CounterGeometryTests::numbersFromTheStartValue()
{
    QCOMPARE(CounterGeometry::nextValue(1, 0), 1);
    QCOMPARE(CounterGeometry::nextValue(7, 0), 7);
}

void CounterGeometryTests::continuesAfterTheHighestPlacedCounter()
{
    QCOMPARE(CounterGeometry::nextValue(1, 3), 4);
    QCOMPARE(CounterGeometry::nextValue(5, 6), 7);
    // Raising the start value above the placed counters jumps ahead.
    QCOMPARE(CounterGeometry::nextValue(10, 3), 10);
}

void CounterGeometryTests::shortDragsArePlainClicks()
{
    QVERIFY(!CounterGeometry::isPointerDrag(QPoint(10, 10), QPoint(10, 10), 10));
    QVERIFY(!CounterGeometry::isPointerDrag(QPoint(10, 10), QPoint(14, 14), 10));
    QVERIFY(CounterGeometry::isPointerDrag(QPoint(10, 10), QPoint(20, 10), 10));
    QVERIFY(CounterGeometry::isPointerDrag(QPoint(10, 10), QPoint(4, 2), 10));
}

void CounterGeometryTests::pointerStartsOnTheCircleEdgeTowardTheTarget()
{
    QCOMPARE(CounterGeometry::pointerStart(QPointF(50, 50), 14, QPointF(150, 50)), QPointF(64, 50));
    QCOMPARE(CounterGeometry::pointerStart(QPointF(50, 50), 14, QPointF(50, 0)), QPointF(50, 36));
    const QPointF diagonal = CounterGeometry::pointerStart(QPointF(0, 0), 10, QPointF(30, 40));
    QCOMPARE(diagonal, QPointF(6, 8));
    // Degenerate: target on the center keeps the start on the center.
    QCOMPARE(CounterGeometry::pointerStart(QPointF(5, 5), 10, QPointF(5, 5)), QPointF(5, 5));
}

void CounterGeometryTests::pointerIsHiddenWhenTheTargetIsInsideTheCircle()
{
    QVERIFY(!CounterGeometry::hasVisiblePointer(QPointF(0, 0), 14, QPointF(10, 0)));
    QVERIFY(!CounterGeometry::hasVisiblePointer(QPointF(0, 0), 14, QPointF(14, 0)));
    QVERIFY(CounterGeometry::hasVisiblePointer(QPointF(0, 0), 14, QPointF(30, 0)));
}

void CounterGeometryTests::arrowHeadMatchesTheArrowTool()
{
    // Pointing right with width 3: 12 px long, 30 degrees either side.
    const CounterGeometry::ArrowHead head =
        CounterGeometry::arrowHead(QPointF(0, 0), QPointF(100, 0), 3);
    QCOMPARE(head.tip, QPointF(100, 0));
    QVERIFY(qAbs(head.left.x() - (100 - 12 * qCos(M_PI / 6))) < 1e-9);
    QVERIFY(qAbs(head.left.y() - 6.0) < 1e-9);
    QVERIFY(qAbs(head.right.x() - head.left.x()) < 1e-9);
    QVERIFY(qAbs(head.right.y() + 6.0) < 1e-9);
}

void CounterGeometryTests::arrowHeadGrowsWithThickness()
{
    QCOMPARE(CounterGeometry::arrowHeadSize(1), 12.0);
    QCOMPARE(CounterGeometry::arrowHeadSize(3), 12.0);
    QCOMPARE(CounterGeometry::arrowHeadSize(8), 32.0);
}

void CounterGeometryTests::pointerBoundsCoverLineAndHead()
{
    const QRectF bounds = CounterGeometry::pointerBounds(QPointF(50, 50), 14, QPointF(150, 50), 4);
    // From the circle edge (64) to the tip (150), stroke half-width 2 added.
    QCOMPARE(bounds.left(), 62.0);
    QCOMPARE(bounds.right(), 152.0);
    // Head is 16 px long, so it spreads 8 px above and below the line.
    QVERIFY(qAbs(bounds.top() - 40.0) < 1e-9);
    QVERIFY(qAbs(bounds.bottom() - 60.0) < 1e-9);
}

void CounterGeometryTests::numberSizeFollowsTheDiameter()
{
    QCOMPARE(CounterGeometry::numberPointSize(28, 1), 10.0);
    QCOMPARE(CounterGeometry::numberPointSize(28, 42), 10.0);
    QCOMPARE(CounterGeometry::numberPointSize(56, 1), 20.0);
    QVERIFY(CounterGeometry::numberPointSize(28, 100) < 10.0);
    // Out-of-range diameters are clamped.
    QCOMPARE(CounterGeometry::numberPointSize(1000, 1),
             CounterGeometry::numberPointSize(CounterGeometry::MaxDiameter, 1));
}

QTEST_MAIN(CounterGeometryTests)

#include "CounterGeometryTests.moc"
