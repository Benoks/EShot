#ifndef COUNTERGEOMETRY_H
#define COUNTERGEOMETRY_H

#include <QPoint>
#include <QPointF>
#include <QRectF>

// Numbering and pointer geometry of the Counter tool, kept free of painting so
// it can be tested on its own.
class CounterGeometry {
public:
    static constexpr int DefaultDiameter = 28;
    static constexpr int MinDiameter = 16;
    static constexpr int MaxDiameter = 96;
    static constexpr int MinStartValue = 1;
    static constexpr int MaxStartValue = 999;

    struct ArrowHead {
        QPointF tip;
        QPointF left;
        QPointF right;
    };

    // Number for the next counter: one past the highest placed counter, but
    // never below the chosen start value. highestPlaced is 0 with no counters.
    static int nextValue(int startValue, int highestPlaced);

    // A press/release pair further apart than the platform drag distance is a
    // drag (counter + pointer); anything shorter is a plain click.
    static bool isPointerDrag(const QPoint &press, const QPoint &release, int dragDistance);
    // A pointer whose target lies inside the circle would be hidden by it.
    static bool hasVisiblePointer(const QPointF &center, qreal radius, const QPointF &target);
    // Where the pointer leaves the circle: on its edge, facing the target.
    static QPointF pointerStart(const QPointF &center, qreal radius, const QPointF &target);

    // EShot's arrowhead: a filled 60 degree triangle whose size follows the
    // stroke width. Shared with the Arrow tool so pointers look the same.
    static qreal arrowHeadSize(int penWidth);
    static ArrowHead arrowHead(const QPointF &from, const QPointF &tip, int penWidth);
    // Area covered by the pointer line and its arrowhead, stroke included.
    static QRectF pointerBounds(const QPointF &center, qreal radius, const QPointF &target,
                                int penWidth);

    // Number size in points; 10 pt in the default 28 px circle, scaled with the
    // diameter and shrunk for three or more digits so they fit.
    static qreal numberPointSize(int diameter, int value);
};

#endif
