#include "AnnotationEngine.h"
#include "ImageEffects.h"
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QtMath>
#include <climits>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTransform>

namespace {
double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    if (len2 <= 0.0001)
        return QLineF(p, a).length();
    const double t = qBound(0.0, QPointF::dotProduct(p - a, ab) / len2, 1.0);
    const QPointF projection = a + ab * t;
    return QLineF(p, projection).length();
}

QPoint constrainedHighlighterPoint(const QPoint &start, const QPoint &pos)
{
    const QPoint delta = pos - start;
    return qAbs(delta.x()) >= qAbs(delta.y())
        ? QPoint(pos.x(), start.y())
        : QPoint(start.x(), pos.y());
}

// Shift with lines, arrows and the pen: the nearest multiple of 45 degrees.
QPoint snappedToAngle(const QPoint &from, const QPoint &to)
{
    const QPointF delta = to - from;
    const qreal length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    if (length < 0.5)
        return to;
    const qreal step = M_PI / 4.0;
    const qreal angle = qRound(qAtan2(delta.y(), delta.x()) / step) * step;
    return from + QPointF(qCos(angle) * length, qSin(angle) * length).toPoint();
}

// Box spanning two corners in any order; QRect(a, b).normalized() is two
// pixels smaller when the box was dragged up or to the left.
QRect boxFromCorners(const QPoint &a, const QPoint &b)
{
    return QRect(QPoint(qMin(a.x(), b.x()), qMin(a.y(), b.y())),
                 QPoint(qMax(a.x(), b.x()), qMax(a.y(), b.y())));
}

bool isBoxTool(AnnotationEngine::Tool tool)
{
    return tool == AnnotationEngine::Rectangle || tool == AnnotationEngine::Circle
        || tool == AnnotationEngine::SemiRect || tool == AnnotationEngine::Blur
        || tool == AnnotationEngine::Pixelate;
}

bool supportsFill(AnnotationEngine::Tool tool)
{
    return tool == AnnotationEngine::Rectangle || tool == AnnotationEngine::Circle;
}

// Direction of a box handle from the centre: -1, 0 or 1 per axis.
QPoint boxHandleSign(AnnotationEngine::ShapeHandle handle)
{
    using H = AnnotationEngine::ShapeHandle;
    switch (handle) {
    case H::TopLeft: return QPoint(-1, -1);
    case H::Top: return QPoint(0, -1);
    case H::TopRight: return QPoint(1, -1);
    case H::Right: return QPoint(1, 0);
    case H::BottomRight: return QPoint(1, 1);
    case H::Bottom: return QPoint(0, 1);
    case H::BottomLeft: return QPoint(-1, 1);
    case H::Left: return QPoint(-1, 0);
    default: return QPoint();
    }
}

QPointF rotatedVector(const QPointF &vector, qreal degrees)
{
    const qreal radians = qDegreesToRadians(degrees);
    return QPointF(vector.x() * qCos(radians) - vector.y() * qSin(radians),
                   vector.x() * qSin(radians) + vector.y() * qCos(radians));
}

// Skip pen/highlighter samples closer than ~2 px to keep the point lists small.
bool pointTooClose(const QPoint &a, const QPoint &b)
{
    const int dx = a.x() - b.x();
    const int dy = a.y() - b.y();
    return dx * dx + dy * dy < 4;
}

// Line with EShot's filled arrowhead; used by the Arrow tool and counter pointers.
void drawArrowShape(QPainter *painter, const QPointF &start, const QPointF &end,
                    const QColor &color, int penWidth)
{
    painter->setPen(QPen(color, penWidth, Qt::SolidLine, Qt::RoundCap));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(start, end);
    const CounterGeometry::ArrowHead head = CounterGeometry::arrowHead(start, end, penWidth);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPolygon(QPolygonF() << head.tip << head.left << head.right);
}
}

AnnotationEngine::AnnotationEngine(QObject *parent)
    : QObject(parent)
    , m_currentTool(None)
    , m_color(Qt::red)
    , m_penWidth(3)
    , m_textFontFamily(QStringLiteral("Segoe UI"))
    , m_textFontSize(18)
    , m_blurIntensity(16)
    , m_shiftHeld(false)
    , m_isDrawing(false)
    , m_counterValue(0)
    , m_selectedIndex(-1)
{
}

AnnotationEngine::~AnnotationEngine() {}

void AnnotationEngine::setCurrentTool(Tool tool)
{
    // Commit a stroke left open by a tool switch mid-drag while the old tool
    // is still active; otherwise m_isDrawing would leak into the new tool.
    if (m_isDrawing && tool != m_currentTool) {
        if (!m_currentAnnotation.points.isEmpty())
            endDraw(m_currentAnnotation.points.last());
        m_isDrawing = false;
        m_currentAnnotation = Annotation();
    }
    m_currentTool = tool;
}
void AnnotationEngine::setColor(const QColor &color) { m_color = color; }
void AnnotationEngine::setPenWidth(int width) { m_penWidth = qBound(1, width, 20); }
void AnnotationEngine::setTextFontFamily(const QString &family)
{
    if (!family.trimmed().isEmpty())
        m_textFontFamily = family.trimmed();
}
void AnnotationEngine::setTextFontSize(int size) { m_textFontSize = qBound(8, size, 72); }
void AnnotationEngine::setBlurIntensity(int intensity) { m_blurIntensity = qBound(4, intensity, 64); }
void AnnotationEngine::setCounterStartValue(int value)
{
    m_counterStartValue = qBound(CounterGeometry::MinStartValue, value, CounterGeometry::MaxStartValue);
}
void AnnotationEngine::setCounterDiameter(int diameter)
{
    m_counterDiameter = qBound(CounterGeometry::MinDiameter, diameter, CounterGeometry::MaxDiameter);
}
void AnnotationEngine::setShiftHeld(bool held) { m_shiftHeld = held; }

void AnnotationEngine::beginDraw(const QPoint &pos)
{
    if (m_currentTool == None || m_currentTool == Text)
        return;

    m_isDrawing = true;
    m_currentAnnotation = Annotation();
    m_currentAnnotation.tool = m_currentTool;
    m_currentAnnotation.color = m_color;
    m_currentAnnotation.penWidth = m_penWidth;
    m_currentAnnotation.blurIntensity = m_blurIntensity;
    m_currentAnnotation.filled = m_shapeFill && supportsFill(m_currentTool);
    m_currentAnnotation.points.append(pos);
    m_drawAnchor = pos;

    if (m_currentTool == Counter) {
        // The counter sits at the press point; dragging adds a pointer.
        m_currentAnnotation.counterValue = nextCounterValue();
        m_currentAnnotation.counterDiameter = m_counterDiameter;
    }

    if (m_currentTool == Highlighter)
        m_currentAnnotation.color = QColor(255, 193, 7, 185);
}

void AnnotationEngine::continueDraw(const QPoint &pos)
{
    if (!m_isDrawing || m_currentTool == None) return;

    if (m_currentTool == Counter) {
        updateCounterPointer(pos);
    } else if (m_currentTool == Pen && m_shiftHeld) {
        // Shift turns the stroke into a straight line from the press point.
        m_currentAnnotation.points = { m_drawAnchor, snappedToAngle(m_drawAnchor, pos) };
    } else if (m_currentTool == Pen || m_currentTool == Highlighter) {
        const QPoint drawPoint = m_currentTool == Highlighter && m_shiftHeld
            ? constrainedHighlighterPoint(m_currentAnnotation.points.first(), pos)
            : pos;
        if (!pointTooClose(m_currentAnnotation.points.last(), drawPoint))
            m_currentAnnotation.points.append(drawPoint);
    } else {
        m_currentAnnotation.points = shapePoints(pos);
    }
}

QVector<QPoint> AnnotationEngine::shapePoints(const QPoint &pos) const
{
    const QPoint anchor = m_drawAnchor;
    if (m_currentTool == Line || m_currentTool == Arrow)
        return { anchor, m_shiftHeld ? snappedToAngle(anchor, pos) : pos };
    if (!isBoxTool(m_currentTool))
        return { anchor, pos };
    // The constraints are stored in the points, so a finished box can be
    // resized freely later.
    QPoint delta = pos - anchor;
    if (m_shiftHeld && m_currentTool != Blur && m_currentTool != Pixelate) {
        const int side = qMin(qAbs(delta.x()), qAbs(delta.y()));
        delta = QPoint(delta.x() < 0 ? -side : side, delta.y() < 0 ? -side : side);
    }
    if (m_altHeld)
        return { anchor - delta, anchor + delta };
    return { anchor, anchor + delta };
}

void AnnotationEngine::updateCounterPointer(const QPoint &pos)
{
    // Short drags and targets hidden under the circle leave a plain counter.
    const QPoint center = m_currentAnnotation.points.first();
    m_currentAnnotation.points.resize(1);
    if (CounterGeometry::isPointerDrag(center, pos, m_counterDragDistance)
        && CounterGeometry::hasVisiblePointer(center, m_currentAnnotation.counterDiameter / 2.0, pos))
        m_currentAnnotation.points.append(pos);
}

void AnnotationEngine::endDraw(const QPoint &pos)
{
    if (!m_isDrawing || m_currentTool == None) return;

    if (m_currentTool == Counter) {
        updateCounterPointer(pos);
        m_annotations.append(m_currentAnnotation);
        m_counterValue = qMax(m_counterValue, m_currentAnnotation.counterValue);
        pushHistory(HistoryAction::Add, m_currentAnnotation, m_annotations.size() - 1);
        m_isDrawing = false;
        emit annotationAdded();
        return;
    }

    if (m_currentTool == Pen && m_shiftHeld) {
        m_currentAnnotation.points = { m_drawAnchor, snappedToAngle(m_drawAnchor, pos) };
    } else if (m_currentTool == Pen || m_currentTool == Highlighter) {
        const QPoint drawPoint = m_currentTool == Highlighter && m_shiftHeld
            ? constrainedHighlighterPoint(m_currentAnnotation.points.first(), pos)
            : pos;
        if (!pointTooClose(m_currentAnnotation.points.last(), drawPoint))
            m_currentAnnotation.points.append(drawPoint);
    } else {
        m_currentAnnotation.points = shapePoints(pos);
    }

    // Bounding rect
    if (!m_currentAnnotation.points.isEmpty()) {
        int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
        for (const QPoint &p : m_currentAnnotation.points) {
            minX = qMin(minX, p.x()); minY = qMin(minY, p.y());
            maxX = qMax(maxX, p.x()); maxY = qMax(maxY, p.y());
        }
        m_currentAnnotation.boundingRect = QRect(minX, minY, maxX - minX, maxY - minY);
    }

    if (m_currentAnnotation.points.size() >= 2) {
        QPoint diff = m_currentAnnotation.points.first() - m_currentAnnotation.points.last();
        if (m_currentTool == Pen || m_currentTool == Highlighter ||
            qAbs(diff.x()) > 3 || qAbs(diff.y()) > 3) {
            m_annotations.append(m_currentAnnotation);
            pushHistory(HistoryAction::Add, m_currentAnnotation, m_annotations.size() - 1);
            emit annotationAdded();
        }
    }

    m_isDrawing = false;
}

void AnnotationEngine::render(QPainter *painter, const QPoint &offset)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < m_annotations.size(); ++i) {
        if (i != m_hiddenIndex)
            drawAnnotation(painter, m_annotations[i], offset);
    }
    if (m_isDrawing)
        drawAnnotation(painter, m_currentAnnotation, offset);
    painter->restore();
}

void AnnotationEngine::drawAnnotation(QPainter *painter, const Annotation &ann, const QPoint &offset)
{
    if (ann.points.isEmpty()) return;
    painter->save();

    if (isRotatableTool(ann.tool) && !qFuzzyIsNull(ann.rotationDegrees)) {
        const QPointF center = rawAnnotationBounds(ann, 0).center() + offset;
        painter->translate(center);
        painter->rotate(ann.rotationDegrees);
        painter->translate(-center);
    }

    switch (ann.tool) {
    case Pen: {
        QPen pen(ann.color, ann.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        if (ann.points.size() > 2) {
            QPainterPath path;
            path.moveTo(ann.points.first() + offset);
            for (int i = 1; i < ann.points.size() - 1; i += 2) {
                QPoint p1 = ann.points[i] + offset;
                QPoint p2 = (i + 1 < ann.points.size()) ? ann.points[i+1] + offset : p1;
                path.quadTo(p1, p2);
            }
            if (ann.points.size() % 2 == 0)
                path.lineTo(ann.points.last() + offset);
            painter->drawPath(path);
        } else if (ann.points.size() == 2) {
            painter->drawLine(ann.points[0] + offset, ann.points[1] + offset);
        } else {
            painter->drawPoint(ann.points.first() + offset);
        }
        break;
    }
    case Arrow: {
        if (ann.points.size() < 2) break;
        drawArrowShape(painter, ann.points.first() + offset, ann.points.last() + offset,
                       ann.color, ann.penWidth);
        break;
    }
    case Rectangle: {
        if (ann.points.size() < 2) break;
        const QRect r = boxFromCorners(ann.points.first(), ann.points.last()).translated(offset);
        QPen pen(ann.color, ann.penWidth);
        painter->setPen(pen);
        painter->setBrush(ann.filled ? QBrush(ann.color) : QBrush(Qt::NoBrush));
        painter->drawRect(r);
        break;
    }
    case Circle: {
        if (ann.points.size() < 2) break;
        const QRect r = boxFromCorners(ann.points.first(), ann.points.last()).translated(offset);
        QPen pen(ann.color, ann.penWidth);
        painter->setPen(pen);
        painter->setBrush(ann.filled ? QBrush(ann.color) : QBrush(Qt::NoBrush));
        painter->drawEllipse(r);
        break;
    }
    case Line: {
        if (ann.points.size() < 2) break;
        QPoint start = ann.points.first() + offset;
        QPoint end = ann.points.last() + offset;
        QPen pen(ann.color, ann.penWidth);
        painter->setPen(pen);
        painter->drawLine(start, end);
        break;
    }
    case Highlighter: {
        QPainterPath path;
        if (ann.points.size() > 1) {
            path.moveTo(ann.points.first() + offset);
            for (int i = 1; i < ann.points.size(); ++i)
                path.lineTo(ann.points[i] + offset);
            QPen contrastPen(QColor(0, 0, 0, 55), ann.penWidth * 5 + 2, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
            painter->setPen(contrastPen);
            painter->setOpacity(0.45);
            painter->drawPath(path);

            QPen pen(ann.color, ann.penWidth * 5, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
            painter->setPen(pen);
            painter->setOpacity(0.72);
            painter->drawPath(path);
        }
        break;
    }
    case Blur:
    case Pixelate: {
        if (ann.points.size() < 2) break;
        const QRect r = boxFromCorners(ann.points.first(), ann.points.last());
        drawRedaction(painter, ann.tool, r, offset, ann.blurIntensity);
        break;
    }
    case Text: {
        if (ann.text.isEmpty() || ann.points.isEmpty()) break;
        QFont font(ann.fontFamily.isEmpty() ? QStringLiteral("Segoe UI") : ann.fontFamily,
                   qBound(8, ann.fontSize, 72));
        font.setBold(ann.textBold);
        QPoint tp = ann.points.first() + offset;

        QTextDocument doc;
        doc.setDefaultFont(font);
        doc.setPlainText(ann.text);
        QTextCursor cursor(&doc);
        cursor.select(QTextCursor::Document);
        QTextCharFormat fmt;
        fmt.setForeground(ann.color);
        cursor.mergeCharFormat(fmt);
        QSizeF docSize = doc.size();

        const QRect bg = textBaseBackgroundRect(ann).translated(offset);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 120));
        painter->save();
        painter->translate(tp);
        painter->scale(ann.textScaleX, ann.textScaleY);
        painter->translate(-tp);
        if (ann.textBackground == TextBox)
            painter->drawRoundedRect(bg, 3, 3);
        painter->translate(tp);
        if (ann.textBackground == TextOutline) {
            // A dark copy drawn around the text keeps it readable on light
            // and dark screenshots without covering them with a box.
            QTextDocument outline;
            outline.setDefaultFont(font);
            outline.setPlainText(ann.text);
            QTextCursor outlineCursor(&outline);
            outlineCursor.select(QTextCursor::Document);
            QTextCharFormat outlineFormat;
            outlineFormat.setForeground(QColor(0, 0, 0, 220));
            outlineCursor.mergeCharFormat(outlineFormat);
            const qreal w = qMax<qreal>(1.5, font.pointSizeF() / 12.0);
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0)
                        continue;
                    painter->translate(dx * w, dy * w);
                    outline.drawContents(painter);
                    painter->translate(-dx * w, -dy * w);
                }
            }
        }
        doc.drawContents(painter);
        painter->restore();
        break;
    }
    case Counter: {
        if (ann.points.isEmpty()) break;
        const QPointF center = ann.points.first() + offset;
        const qreal radius = ann.counterDiameter / 2.0;
        // The pointer starts at the circle's edge; the circle is drawn over it
        // so a large arrowhead close to the circle never covers the number.
        if (ann.points.size() >= 2) {
            const QPointF target = ann.points[1] + offset;
            drawArrowShape(painter, CounterGeometry::pointerStart(center, radius, target), target,
                           ann.color, ann.penWidth);
        }
        painter->setPen(QPen(Qt::white, qMax(2.0, ann.counterDiameter / 14.0)));
        painter->setBrush(ann.color);
        painter->drawEllipse(center, radius, radius);
        painter->setPen(Qt::white);
        QFont f("Segoe UI");
        f.setPointSizeF(CounterGeometry::numberPointSize(ann.counterDiameter, ann.counterValue));
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2),
                          Qt::AlignCenter, QString::number(ann.counterValue));
        break;
    }
    case SemiRect: {
        if (ann.points.size() < 2) break;
        const QRect r = boxFromCorners(ann.points.first(), ann.points.last()).translated(offset);
        QColor fillColor = ann.color;
        fillColor.setAlpha(80);
        painter->setPen(QPen(ann.color, ann.penWidth));
        painter->setBrush(fillColor);
        painter->drawRect(r);
        break;
    }
    default: break;
    }

    painter->restore();
}

void AnnotationEngine::clear()
{
    // Discard any in-progress annotaion so it won't appear in the next capture
    // this won't affect rememberLastAnnotationTool
    m_isDrawing = false;
    m_currentAnnotation = Annotation();
    m_selectedIndex = -1;
    m_hiddenIndex = -1;
    resetGestureState();

    m_annotations.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_counterValue = 0;
}

void AnnotationEngine::finishGestures()
{
    // A held gesture keeps its index; once undo/redo shifts the list it would
    // edit another annotation. Record it now so undo takes it back.
    endShapeResize();
    endTextResize();
    endRotate();
    endMove();
}

void AnnotationEngine::undo()
{
    finishGestures();
    if (m_undoStack.isEmpty()) return;

    HistoryAction action = m_undoStack.takeLast();
    if (action.type == HistoryAction::Add) {
        const int removeAt = action.index >= 0 && action.index < m_annotations.size()
            ? action.index : m_annotations.size() - 1;
        if (removeAt >= 0) {
            m_annotations.removeAt(removeAt);
            adjustSelectionForRemove(removeAt);
        }
    } else if (action.type == HistoryAction::Remove) {
        int insertAt = qBound(0, action.index, m_annotations.size());
        m_annotations.insert(insertAt, action.annotation);
        adjustSelectionForInsert(insertAt);
    } else if (action.type == HistoryAction::Resize
               && action.index >= 0 && action.index < m_annotations.size()) {
        m_annotations[action.index] = action.previousAnnotation;
    } else if (action.index >= 0 && action.index < m_annotations.size()) {
        m_annotations[action.index].rotationDegrees = action.previousRotationDegrees;
    }
    m_redoStack.append(action);
    recalculateCounterValue();
}

void AnnotationEngine::redo()
{
    finishGestures();
    if (m_redoStack.isEmpty()) return;

    HistoryAction action = m_redoStack.takeLast();
    if (action.type == HistoryAction::Add) {
        int insertAt = qBound(0, action.index, m_annotations.size());
        m_annotations.insert(insertAt, action.annotation);
        adjustSelectionForInsert(insertAt);
    } else if (action.type == HistoryAction::Remove) {
        const int removeAt = action.index >= 0 && action.index < m_annotations.size()
            ? action.index : m_annotations.size() - 1;
        if (removeAt >= 0) {
            m_annotations.removeAt(removeAt);
            adjustSelectionForRemove(removeAt);
        }
    } else if (action.type == HistoryAction::Resize
               && action.index >= 0 && action.index < m_annotations.size()) {
        m_annotations[action.index] = action.annotation;
    } else if (action.index >= 0 && action.index < m_annotations.size()) {
        m_annotations[action.index].rotationDegrees = action.rotationDegrees;
    }
    appendHistoryAction(action);
    recalculateCounterValue();
}

void AnnotationEngine::addTextAnnotation(const QPoint &pos, const QString &text)
{
    if (text.isEmpty()) return;
    Annotation a;
    a.tool = Text; a.color = m_color; a.penWidth = m_penWidth;
    a.fontFamily = m_textFontFamily;
    a.fontSize = m_textFontSize;
    a.textBold = m_textBold;
    a.textBackground = m_textBackground;
    a.points.append(pos); a.text = text;
    m_annotations.append(a);
    pushHistory(HistoryAction::Add, a, m_annotations.size() - 1);
    emit annotationAdded();
}

AnnotationEngine::TextStyle AnnotationEngine::currentTextStyle() const
{
    TextStyle style;
    style.fontFamily = m_textFontFamily;
    style.fontSize = m_textFontSize;
    style.color = m_color;
    style.bold = m_textBold;
    style.background = m_textBackground;
    return style;
}

QString AnnotationEngine::textOf(int index) const
{
    return isTextAnnotation(index) ? m_annotations[index].text : QString();
}

AnnotationEngine::TextStyle AnnotationEngine::textStyleOf(int index) const
{
    if (!isTextAnnotation(index))
        return currentTextStyle();
    const Annotation &ann = m_annotations[index];
    TextStyle style;
    style.fontFamily = ann.fontFamily;
    style.fontSize = ann.fontSize;
    style.color = ann.color;
    style.bold = ann.textBold;
    style.background = ann.textBackground;
    return style;
}

QPoint AnnotationEngine::textAnchorOf(int index) const
{
    return isTextAnnotation(index) ? m_annotations[index].points.first() : QPoint();
}

void AnnotationEngine::replaceAnnotation(int index, const Annotation &updated)
{
    HistoryAction action;
    action.type = HistoryAction::Resize;
    action.index = index;
    action.previousAnnotation = m_annotations[index];
    action.annotation = updated;
    m_annotations[index] = updated;
    appendHistoryAction(action);
    m_redoStack.clear();
}

bool AnnotationEngine::updateTextAnnotation(int index, const QString &text, const TextStyle &style,
                                            const QPoint &offset)
{
    if (!isTextAnnotation(index))
        return false;
    if (text.isEmpty())
        return removeAnnotation(index);

    Annotation updated = m_annotations[index];
    updated.text = text;
    updated.fontFamily = style.fontFamily;
    updated.fontSize = style.fontSize;
    updated.color = style.color;
    updated.textBold = style.bold;
    updated.textBackground = style.background;
    for (QPoint &point : updated.points)
        point += offset;
    const Annotation &current = m_annotations[index];
    if (updated.text == current.text && updated.fontFamily == current.fontFamily
        && updated.fontSize == current.fontSize && updated.color == current.color
        && updated.textBold == current.textBold
        && updated.textBackground == current.textBackground
        && updated.points == current.points) {
        return false;
    }
    replaceAnnotation(index, updated);
    return true;
}

bool AnnotationEngine::setAnnotationColor(int index, const QColor &color)
{
    if (index < 0 || index >= m_annotations.size() || !color.isValid())
        return false;
    const Tool tool = m_annotations[index].tool;
    if (tool == Blur || tool == Pixelate || m_annotations[index].color == color)
        return false;
    Annotation updated = m_annotations[index];
    updated.color = color;
    replaceAnnotation(index, updated);
    return true;
}

bool AnnotationEngine::removeAnnotation(int index)
{
    if (index < 0 || index >= m_annotations.size())
        return false;
    const Annotation removed = m_annotations[index];
    m_annotations.removeAt(index);
    adjustSelectionForRemove(index);
    pushHistory(HistoryAction::Remove, removed, index);
    recalculateCounterValue();
    return true;
}

void AnnotationEngine::addCounterAnnotation(const QPoint &pos)
{
    Annotation ann;
    ann.tool = Counter;
    ann.color = m_color;
    ann.penWidth = m_penWidth;
    ann.points.append(pos);
    ann.counterValue = nextCounterValue();
    ann.counterDiameter = m_counterDiameter;
    m_counterValue = ann.counterValue;
    m_annotations.append(ann);
    pushHistory(HistoryAction::Add, ann, m_annotations.size() - 1);
    emit annotationAdded();
}

bool AnnotationEngine::eraseAnnotationAt(const QPoint &pos)
{
    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        const Annotation &ann = m_annotations[i];
        if (ann.points.isEmpty()) continue;

        if (annotationContainsPoint(ann, pos, 8))
            return removeAnnotation(i);
    }
    return false;
}

int AnnotationEngine::findAnnotationAt(const QPoint &pos)
{
    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        const Annotation &ann = m_annotations[i];
        if (ann.points.isEmpty()) continue;

        if (annotationContainsPoint(ann, pos, ann.tool == Text ? 3 : 10))
            return i;
    }
    return -1;
}

void AnnotationEngine::beginMove(int index)
{
    if (index < 0 || index >= m_annotations.size())
        return;
    if (m_moveGestureIndex != index) {
        m_moveGestureIndex = index;
        m_moveGestureOriginal = m_annotations[index];
        m_moveGestureHistoryStarted = false;
    }
}

void AnnotationEngine::moveAnnotation(int index, const QPoint &delta)
{
    if (index < 0 || index >= m_annotations.size())
        return;
    beginMove(index); // no-op when the caller manages the gesture explicitly

    Annotation &ann = m_annotations[index];
    for (int i = 0; i < ann.points.size(); ++i) {
        ann.points[i] += delta;
    }
    if (!m_moveGestureHistoryStarted
        || m_undoStack.isEmpty()
        || m_undoStack.size() != m_moveHistorySize
        || m_undoStack.last().index != index) {
        // Record the move as a single undo action (Resize actions restore the
        // whole annotation), keeping the pre-move snapshot as previous state.
        HistoryAction action;
        action.type = HistoryAction::Resize;
        action.index = index;
        action.previousAnnotation = m_moveGestureOriginal;
        action.annotation = ann;
        appendHistoryAction(action);
        m_redoStack.clear();
        m_moveGestureHistoryStarted = true;
        m_moveHistorySize = m_undoStack.size();
    } else {
        m_undoStack.last().annotation = ann;
    }
}

void AnnotationEngine::endMove()
{
    m_moveGestureIndex = -1;
    m_moveGestureHistoryStarted = false;
    m_moveHistorySize = -1;
}

bool AnnotationEngine::isRotatableTool(Tool tool)
{
    return tool == Text || tool == Arrow || tool == Line || tool == Rectangle
        || tool == Circle || tool == SemiRect;
}

bool AnnotationEngine::isRotatable(int index) const
{
    return index >= 0 && index < m_annotations.size()
        && isRotatableTool(m_annotations[index].tool);
}

bool AnnotationEngine::isTextAnnotation(int index) const
{
    return index >= 0 && index < m_annotations.size()
        && m_annotations[index].tool == Text;
}

AnnotationEngine::Tool AnnotationEngine::toolOf(int index) const
{
    return index >= 0 && index < m_annotations.size() ? m_annotations[index].tool : None;
}

qreal AnnotationEngine::rotationDegreesOf(int index) const
{
    return index >= 0 && index < m_annotations.size()
        ? m_annotations[index].rotationDegrees
        : 0.0;
}

void AnnotationEngine::beginRotate(int index)
{
    if (!isRotatable(index))
        return;
    m_rotateIndex = index;
    m_rotateOriginalDegrees = m_annotations[index].rotationDegrees;
}

void AnnotationEngine::endRotate()
{
    if (!isRotatable(m_rotateIndex)) {
        m_rotateIndex = -1;
        return;
    }

    const Annotation &rotated = m_annotations[m_rotateIndex];
    if (!qFuzzyCompare(rotated.rotationDegrees, m_rotateOriginalDegrees)) {
        HistoryAction action;
        action.type = HistoryAction::Rotate;
        action.index = m_rotateIndex;
        action.previousRotationDegrees = m_rotateOriginalDegrees;
        action.rotationDegrees = rotated.rotationDegrees;
        appendHistoryAction(action);
        m_redoStack.clear();
    }
    m_rotateIndex = -1;
}

void AnnotationEngine::rotateAnnotation(int index, qreal degrees)
{
    if (!isRotatable(index))
        return;

    Annotation &ann = m_annotations[index];
    if (qFuzzyCompare(ann.rotationDegrees + 1.0, degrees + 1.0))
        return;

    if (index != m_rotateIndex) {
        // Legacy per-move call path: fold consecutive rotations of the same
        // annotation into one undo action instead of one entry per degree.
        if (!m_undoStack.isEmpty()
            && m_undoStack.last().type == HistoryAction::Rotate
            && m_undoStack.last().index == index) {
            ann.rotationDegrees = degrees;
            m_undoStack.last().rotationDegrees = degrees;
            m_redoStack.clear();
            return;
        }
        HistoryAction action;
        action.type = HistoryAction::Rotate;
        action.index = index;
        action.previousRotationDegrees = ann.rotationDegrees;
        action.rotationDegrees = degrees;
        appendHistoryAction(action);
        m_redoStack.clear();
        ann.rotationDegrees = degrees;
        return;
    }
    // Explicit begin/end gesture: history is recorded once in endRotate().
    ann.rotationDegrees = degrees;
}

void AnnotationEngine::beginTextResize(int index)
{
    if (!isTextAnnotation(index))
        return;
    m_textResizeIndex = index;
    m_textResizeOriginal = m_annotations[index];
}

void AnnotationEngine::resizeTextAnnotation(int index, const QRectF &bounds)
{
    if (!isTextAnnotation(index) || index != m_textResizeIndex)
        return;

    Annotation &ann = m_annotations[index];
    const QRect baseBounds = textBaseBackgroundRect(ann);
    if (baseBounds.isEmpty() || bounds.width() < 2.0 || bounds.height() < 2.0)
        return;

    if (!qFuzzyIsNull(m_textResizeOriginal.rotationDegrees)) {
        // Rotated text: callers drag the axis-aligned box of the rotated text
        // (rotatedBoundingRectOf at gesture start). Translate that box's
        // stretch into the text's local axes and keep the rotated text
        // centred in the requested box, instead of treating it as the
        // unrotated background.
        const QRectF startBounds = rotatedAnnotationBounds(m_textResizeOriginal, 0);
        if (startBounds.width() <= 0.0 || startBounds.height() <= 0.0)
            return;
        const qreal kx = bounds.width() / startBounds.width();
        const qreal ky = bounds.height() / startBounds.height();
        const qreal radians = qDegreesToRadians(m_textResizeOriginal.rotationDegrees);
        const qreal cos2 = qCos(radians) * qCos(radians);
        const qreal sin2 = 1.0 - cos2;
        ann.textScaleX = qBound(0.1, m_textResizeOriginal.textScaleX * (kx * cos2 + ky * sin2), 20.0);
        ann.textScaleY = qBound(0.1, m_textResizeOriginal.textScaleY * (kx * sin2 + ky * cos2), 20.0);
        // Rotation pivots on the scaled background's centre, so placing that
        // centre on the requested centre keeps the rotated box inside bounds.
        const QPointF center = bounds.center();
        ann.points[0] = QPoint(qRound(center.x() - baseBounds.width() * ann.textScaleX / 2.0
                                      + 4.0 * ann.textScaleX),
                               qRound(center.y() - baseBounds.height() * ann.textScaleY / 2.0
                                      + 4.0 * ann.textScaleY));
        return;
    }

    ann.textScaleX = qBound(0.1, bounds.width() / baseBounds.width(), 20.0);
    ann.textScaleY = qBound(0.1, bounds.height() / baseBounds.height(), 20.0);
    ann.points[0] = QPoint(qRound(bounds.left() + 4.0 * ann.textScaleX),
                           qRound(bounds.top() + 4.0 * ann.textScaleY));
}

void AnnotationEngine::endTextResize()
{
    if (!isTextAnnotation(m_textResizeIndex)) {
        m_textResizeIndex = -1;
        return;
    }

    const Annotation &resized = m_annotations[m_textResizeIndex];
    if (!qFuzzyCompare(resized.textScaleX, m_textResizeOriginal.textScaleX)
        || !qFuzzyCompare(resized.textScaleY, m_textResizeOriginal.textScaleY)
        || resized.points != m_textResizeOriginal.points) {
        HistoryAction action;
        action.type = HistoryAction::Resize;
        action.index = m_textResizeIndex;
        action.previousAnnotation = m_textResizeOriginal;
        action.annotation = resized;
        appendHistoryAction(action);
        m_redoStack.clear();
    }
    m_textResizeIndex = -1;
}

QVector<AnnotationEngine::ShapeHandlePoint> AnnotationEngine::shapeHandlesOf(int index) const
{
    QVector<ShapeHandlePoint> handles;
    if (index < 0 || index >= m_annotations.size())
        return handles;
    const Annotation &ann = m_annotations[index];
    if (ann.points.size() < 2)
        return handles;

    if (ann.tool == Counter) {
        handles.append({ShapeHandle::End, QPointF(ann.points[1])});
        return handles;
    }

    // Shapes rotate around the centre of their unrotated bounds.
    const QPointF center = rawAnnotationBounds(ann, 0).center();
    const auto onScreen = [&](const QPointF &point) {
        return center + rotatedVector(point - center, ann.rotationDegrees);
    };
    if (ann.tool == Line || ann.tool == Arrow) {
        handles.append({ShapeHandle::Start, onScreen(ann.points.first())});
        handles.append({ShapeHandle::End, onScreen(ann.points.last())});
        return handles;
    }
    if (!isBoxTool(ann.tool))
        return handles;

    const QRectF box = QRectF(QPointF(ann.points.first()), QPointF(ann.points.last())).normalized();
    // Edge handles only where the side is long enough to tell them apart.
    constexpr qreal MinEdgeHandleSide = 30.0;
    using H = ShapeHandle;
    for (H handle : {H::TopLeft, H::Top, H::TopRight, H::Right, H::BottomRight, H::Bottom,
                     H::BottomLeft, H::Left}) {
        const QPoint sign = boxHandleSign(handle);
        if ((sign.x() == 0 && box.width() < MinEdgeHandleSide)
            || (sign.y() == 0 && box.height() < MinEdgeHandleSide))
            continue;
        const QPointF local(box.center().x() + sign.x() * box.width() / 2.0,
                            box.center().y() + sign.y() * box.height() / 2.0);
        handles.append({handle, onScreen(local)});
    }
    return handles;
}

void AnnotationEngine::beginShapeResize(int index, ShapeHandle handle)
{
    if (index < 0 || index >= m_annotations.size() || handle == ShapeHandle::None)
        return;
    m_shapeResizeIndex = index;
    m_shapeResizeHandle = handle;
    m_shapeResizeOriginal = m_annotations[index];

    Annotation &ann = m_annotations[index];
    if ((ann.tool == Line || ann.tool == Arrow) && !qFuzzyIsNull(ann.rotationDegrees)
        && ann.points.size() >= 2) {
        // A rotated line is the same line with rotated ends; storing it that
        // way lets each end follow the mouse directly.
        const QPointF center = rawAnnotationBounds(ann, 0).center();
        for (QPoint &point : ann.points)
            point = (center + rotatedVector(point - center, ann.rotationDegrees)).toPoint();
        ann.rotationDegrees = 0.0;
    }
}

void AnnotationEngine::resizeShape(const QPointF &pos, bool keepRatio)
{
    if (m_shapeResizeIndex < 0 || m_shapeResizeIndex >= m_annotations.size())
        return;
    Annotation &ann = m_annotations[m_shapeResizeIndex];
    if (ann.points.size() < 2)
        return;

    if (ann.tool == Counter) {
        const QPoint target = pos.toPoint();
        if (CounterGeometry::hasVisiblePointer(ann.points.first(), ann.counterDiameter / 2.0, target))
            ann.points[1] = target;
        return;
    }
    if (ann.tool == Line || ann.tool == Arrow) {
        const bool start = m_shapeResizeHandle == ShapeHandle::Start;
        const QPoint fixed = start ? ann.points.last() : ann.points.first();
        const QPoint moved = keepRatio ? snappedToAngle(fixed, pos.toPoint()) : pos.toPoint();
        if (start)
            ann.points.first() = moved;
        else
            ann.points.last() = moved;
        return;
    }
    if (!isBoxTool(ann.tool))
        return;

    // Work in the box's own axes: the handle opposite the dragged one stays
    // where it is on screen, also when the box is rotated.
    const Annotation &original = m_shapeResizeOriginal;
    const QRectF box = QRectF(QPointF(original.points.first()),
                              QPointF(original.points.last())).normalized();
    const qreal angle = original.rotationDegrees;
    const QPointF center = box.center();
    const QPoint sign = boxHandleSign(m_shapeResizeHandle);
    const QPointF anchorLocal(-sign.x() * box.width() / 2.0, -sign.y() * box.height() / 2.0);
    const QPointF anchor = center + rotatedVector(anchorLocal, angle);
    const QPointF reach = rotatedVector(pos - anchor, -angle);

    constexpr qreal MinSide = 4.0;
    qreal width = sign.x() != 0 ? qMax(MinSide, sign.x() * reach.x()) : box.width();
    qreal height = sign.y() != 0 ? qMax(MinSide, sign.y() * reach.y()) : box.height();
    if (keepRatio && sign.x() != 0 && sign.y() != 0 && box.width() > 0.0 && box.height() > 0.0) {
        const qreal scale = qMax(width / box.width(), height / box.height());
        width = qMax(MinSide, box.width() * scale);
        height = qMax(MinSide, box.height() * scale);
    }
    const QPointF centerOffset(sign.x() * width / 2.0, sign.y() * height / 2.0);
    const QPointF newCenter = anchor + rotatedVector(centerOffset, angle);
    ann.points.first() = QPointF(newCenter.x() - width / 2.0, newCenter.y() - height / 2.0).toPoint();
    ann.points.last() = QPointF(newCenter.x() + width / 2.0, newCenter.y() + height / 2.0).toPoint();
}

void AnnotationEngine::endShapeResize()
{
    if (m_shapeResizeIndex < 0 || m_shapeResizeIndex >= m_annotations.size()) {
        m_shapeResizeIndex = -1;
        return;
    }
    const Annotation &resized = m_annotations[m_shapeResizeIndex];
    if (resized.points != m_shapeResizeOriginal.points
        || !qFuzzyCompare(resized.rotationDegrees + 1.0, m_shapeResizeOriginal.rotationDegrees + 1.0)) {
        HistoryAction action;
        action.type = HistoryAction::Resize;
        action.index = m_shapeResizeIndex;
        action.previousAnnotation = m_shapeResizeOriginal;
        action.annotation = resized;
        appendHistoryAction(action);
        m_redoStack.clear();
    }
    m_shapeResizeIndex = -1;
    m_shapeResizeHandle = ShapeHandle::None;
}

void AnnotationEngine::editAnnotationProperty(int index, const Annotation &updated, PropertyEdit edit)
{
    if (m_propertyEditOpen && !m_undoStack.isEmpty()) {
        HistoryAction &last = m_undoStack.last();
        if (last.type == HistoryAction::Resize && last.index == index && last.propertyEdit == edit) {
            last.annotation = updated;
            m_annotations[index] = updated;
            m_redoStack.clear();
            return;
        }
    }
    replaceAnnotation(index, updated);
    m_undoStack.last().propertyEdit = edit;
    m_propertyEditOpen = true;
}

bool AnnotationEngine::setAnnotationPenWidth(int index, int width)
{
    if (index < 0 || index >= m_annotations.size())
        return false;
    const Tool tool = m_annotations[index].tool;
    // Text has a font size; blur and pixelate have no outline.
    if (tool == Text || tool == Blur || tool == Pixelate)
        return false;
    width = qBound(1, width, 20);
    if (m_annotations[index].penWidth == width)
        return false;
    Annotation updated = m_annotations[index];
    updated.penWidth = width;
    editAnnotationProperty(index, updated, PenWidthEdit);
    return true;
}

bool AnnotationEngine::setAnnotationBlurIntensity(int index, int intensity)
{
    if (index < 0 || index >= m_annotations.size())
        return false;
    const Tool tool = m_annotations[index].tool;
    intensity = qBound(4, intensity, 64);
    if ((tool != Blur && tool != Pixelate) || m_annotations[index].blurIntensity == intensity)
        return false;
    Annotation updated = m_annotations[index];
    updated.blurIntensity = intensity;
    editAnnotationProperty(index, updated, BlurIntensityEdit);
    return true;
}

bool AnnotationEngine::setAnnotationFilled(int index, bool filled)
{
    if (index < 0 || index >= m_annotations.size()
        || !supportsFill(m_annotations[index].tool) || m_annotations[index].filled == filled)
        return false;
    Annotation updated = m_annotations[index];
    updated.filled = filled;
    editAnnotationProperty(index, updated, FillEdit);
    return true;
}

bool AnnotationEngine::isFilled(int index) const
{
    return index >= 0 && index < m_annotations.size() && m_annotations[index].filled;
}

int AnnotationEngine::duplicateAnnotation(int index, const QPoint &offset)
{
    if (index < 0 || index >= m_annotations.size())
        return -1;
    Annotation copy = m_annotations[index];
    for (QPoint &point : copy.points)
        point += offset;
    // A copied counter continues the numbering instead of repeating it.
    if (copy.tool == Counter) {
        copy.counterValue = nextCounterValue();
        m_counterValue = copy.counterValue;
    }
    m_annotations.append(copy);
    pushHistory(HistoryAction::Add, copy, m_annotations.size() - 1);
    emit annotationAdded();
    return m_annotations.size() - 1;
}

void AnnotationEngine::setSelectedIndex(int index)
{
    if (index != m_selectedIndex)
        m_propertyEditOpen = false;
    m_selectedIndex = index;
}

QRect AnnotationEngine::boundingRectOf(int index) const
{
    return rotatedBoundingRectOf(index).toAlignedRect();
}

QRectF AnnotationEngine::rotatedBoundingRectOf(int index) const
{
    if (index < 0 || index >= m_annotations.size())
        return QRectF();
    return rotatedAnnotationBounds(m_annotations[index], 0);
}

QRect AnnotationEngine::paintedBoundsOf(int index) const
{
    if (index < 0 || index >= m_annotations.size())
        return QRect();
    return paintedBounds(m_annotations[index]);
}

QRect AnnotationEngine::currentPaintedBounds() const
{
    return m_isDrawing ? paintedBounds(m_currentAnnotation) : QRect();
}

QRect AnnotationEngine::paintedBounds(const Annotation &ann) const
{
    if (ann.points.isEmpty())
        return QRect();
    // Room for half the widest stroke (a highlighter is five pens wide), arrow
    // heads reaching sideways and antialiasing.
    const int margin = ann.penWidth * 5 + 16;
    return rotatedAnnotationBounds(ann, margin).toAlignedRect();
}

QRect AnnotationEngine::rawAnnotationBounds(const Annotation &ann, int padding) const
{
    if (ann.points.isEmpty()) return QRect();

    QRect bounds;
    if (ann.tool == Text) {
        bounds = textBackgroundRect(ann);
    } else if (ann.tool == Counter) {
        bounds = counterCircleRect(ann);
        if (ann.points.size() >= 2) {
            bounds |= CounterGeometry::pointerBounds(ann.points.first(), ann.counterDiameter / 2.0,
                                                     ann.points[1], ann.penWidth)
                          .toAlignedRect();
        }
    } else if (ann.tool == Blur || ann.tool == Pixelate || ann.tool == SemiRect || ann.tool == Rectangle || ann.tool == Circle || ann.tool == Line || ann.tool == Arrow) {
        if (ann.points.size() < 2)
            bounds = QRect(ann.points.first(), QSize(1, 1));
        else {
            bounds = boxFromCorners(ann.points.first(), ann.points.last());
        }
    } else if (ann.points.size() == 1) {
        bounds = QRect(ann.points.first(), QSize(1, 1));
    } else {
        int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
        for (const QPoint &p : ann.points) {
            minX = qMin(minX, p.x()); minY = qMin(minY, p.y());
            maxX = qMax(maxX, p.x()); maxY = qMax(maxY, p.y());
        }
        bounds = QRect(QPoint(minX, minY), QPoint(maxX, maxY)).normalized();
    }
    bounds.adjust(-padding, -padding, padding, padding);
    return bounds;
}

QRect AnnotationEngine::textBaseBackgroundRect(const Annotation &ann) const
{
    if (ann.tool != Text || ann.points.isEmpty() || ann.text.isEmpty())
        return QRect();

    QFont font(ann.fontFamily.isEmpty() ? QStringLiteral("Segoe UI") : ann.fontFamily,
               qBound(8, ann.fontSize, 72));
    font.setBold(ann.textBold);
    QTextDocument doc;
    doc.setDefaultFont(font);
    doc.setPlainText(ann.text);
    const QSize documentSize = doc.size().toSize();
    return QRect(ann.points.first() - QPoint(4, 4), documentSize + QSize(8, 8));
}

QRect AnnotationEngine::textBackgroundRect(const Annotation &ann) const
{
    const QRect baseBounds = textBaseBackgroundRect(ann);
    if (baseBounds.isEmpty())
        return QRect();

    const QPoint anchor = ann.points.first();
    const qreal left = anchor.x() + (baseBounds.left() - anchor.x()) * ann.textScaleX;
    const qreal top = anchor.y() + (baseBounds.top() - anchor.y()) * ann.textScaleY;
    return QRect(qFloor(left), qFloor(top),
                 qCeil(baseBounds.width() * ann.textScaleX),
                 qCeil(baseBounds.height() * ann.textScaleY));
}

QRectF AnnotationEngine::rotatedAnnotationBounds(const Annotation &ann, int padding) const
{
    const QRectF bounds = rawAnnotationBounds(ann, padding);
    if (!isRotatableTool(ann.tool) || qFuzzyIsNull(ann.rotationDegrees))
        return bounds;

    QTransform transform;
    transform.translate(bounds.center().x(), bounds.center().y());
    transform.rotate(ann.rotationDegrees);
    transform.translate(-bounds.center().x(), -bounds.center().y());
    return transform.mapRect(bounds);
}

bool AnnotationEngine::annotationContainsPoint(const Annotation &ann, const QPoint &pos, int padding) const
{
    if (ann.points.isEmpty())
        return false;

    QPointF transformedPos = pos;
    if (isRotatableTool(ann.tool) && !qFuzzyIsNull(ann.rotationDegrees)) {
        const QPointF center = rawAnnotationBounds(ann, 0).center();
        QTransform inverseTransform;
        inverseTransform.translate(center.x(), center.y());
        inverseTransform.rotate(-ann.rotationDegrees);
        inverseTransform.translate(-center.x(), -center.y());
        transformedPos = inverseTransform.map(transformedPos);
    }
    if (ann.tool == Text
        && (!qFuzzyCompare(ann.textScaleX, 1.0) || !qFuzzyCompare(ann.textScaleY, 1.0))) {
        // Invert the text scale around its anchor after the inverse rotation,
        // mirroring the R∘S transform used when drawing scaled+rotated text.
        const QPointF anchor = ann.points.first();
        transformedPos = QPointF(anchor.x() + (transformedPos.x() - anchor.x()) / ann.textScaleX,
                                 anchor.y() + (transformedPos.y() - anchor.y()) / ann.textScaleY);
    }
    const QPoint hitPos = transformedPos.toPoint();

    const int tolerance = qMax(padding, ann.penWidth + 5);

    switch (ann.tool) {
    case Pen:
    case Highlighter: {
        if (ann.points.size() == 1)
            return QRect(ann.points.first() - QPoint(tolerance, tolerance), QSize(tolerance * 2, tolerance * 2)).contains(hitPos);
        const int strokeTolerance = ann.tool == Highlighter
            ? qMax(tolerance, ann.penWidth * 3)
            : tolerance;
        for (int i = 1; i < ann.points.size(); ++i) {
            if (distanceToSegment(hitPos, ann.points[i - 1], ann.points[i]) <= strokeTolerance)
                return true;
        }
        return false;
    }
    case Line:
    case Arrow:
        if (ann.points.size() < 2)
            return rawAnnotationBounds(ann, tolerance).contains(hitPos);
        return distanceToSegment(hitPos, ann.points.first(), ann.points.last()) <= tolerance;

    case Rectangle: {
        if (ann.points.size() < 2)
            return rawAnnotationBounds(ann, tolerance).contains(hitPos);
        const QRect r = boxFromCorners(ann.points.first(), ann.points.last());
        if (!r.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(hitPos))
            return false;
        if (ann.filled)
            return true;
        const int left = qAbs(hitPos.x() - r.left());
        const int right = qAbs(hitPos.x() - r.right());
        const int top = qAbs(hitPos.y() - r.top());
        const int bottom = qAbs(hitPos.y() - r.bottom());
        return qMin(qMin(left, right), qMin(top, bottom)) <= tolerance;
    }
    case Circle: {
        if (ann.points.size() < 2)
            return rawAnnotationBounds(ann, tolerance).contains(hitPos);
        const QRect r = boxFromCorners(ann.points.first(), ann.points.last());
        if (r.width() <= 0 || r.height() <= 0)
            return rawAnnotationBounds(ann, tolerance).contains(hitPos);
        if (!r.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(hitPos))
            return false;
        const QPointF center = r.center();
        const double rx = r.width() / 2.0;
        const double ry = r.height() / 2.0;
        const double nx = (hitPos.x() - center.x()) / rx;
        const double ny = (hitPos.y() - center.y()) / ry;
        const double edge = qSqrt(nx * nx + ny * ny);
        const double normalizedTolerance = tolerance / qMax(1.0, qMin(rx, ry));
        if (ann.filled)
            return edge <= 1.0 + normalizedTolerance;
        return qAbs(edge - 1.0) <= normalizedTolerance;
    }
    case Text:
        // hitPos is already mapped back into unscaled text space above, so
        // compare against the unscaled background.
        return textBaseBackgroundRect(ann)
            .adjusted(-padding, -padding, padding, padding).contains(hitPos);
    case Counter: {
        if (counterCircleRect(ann).adjusted(-padding, -padding, padding, padding).contains(hitPos))
            return true;
        if (ann.points.size() < 2)
            return false;
        const QPointF target = ann.points[1];
        const QPointF start = CounterGeometry::pointerStart(ann.points.first(),
                                                            ann.counterDiameter / 2.0, target);
        if (distanceToSegment(hitPos, start, target) <= tolerance)
            return true;
        const CounterGeometry::ArrowHead head = CounterGeometry::arrowHead(start, target, ann.penWidth);
        return QPolygonF({head.tip, head.left, head.right}).containsPoint(hitPos, Qt::OddEvenFill);
    }
    case Blur:
    case Pixelate:
    case SemiRect:
        return rawAnnotationBounds(ann, padding).contains(hitPos);

    default:
        return rawAnnotationBounds(ann, padding).contains(hitPos);
    }
}

void AnnotationEngine::pushHistory(HistoryAction::Type type, const Annotation &annotation, int index)
{
    HistoryAction action;
    action.type = type;
    action.annotation = annotation;
    action.index = index;
    appendHistoryAction(action);
    m_redoStack.clear();
}

void AnnotationEngine::appendHistoryAction(const HistoryAction &action)
{
    constexpr qsizetype MaxUndoActions = 200;
    if (m_undoStack.size() >= MaxUndoActions) {
        // Every later action was recorded on top of the evicted one, so their
        // indices stay valid; the evicted change just becomes permanent.
        m_undoStack.removeFirst();
    }
    m_undoStack.append(action);
}

int AnnotationEngine::nextCounterValue() const
{
    return CounterGeometry::nextValue(m_counterStartValue, m_counterValue);
}

int AnnotationEngine::counterValueOf(int index) const
{
    return index >= 0 && index < m_annotations.size() && m_annotations[index].tool == Counter
        ? m_annotations[index].counterValue : 0;
}

QRect AnnotationEngine::counterCircleRect(const Annotation &ann)
{
    const int half = (ann.counterDiameter + 1) / 2;
    return QRect(ann.points.first() - QPoint(half, half), QSize(half * 2, half * 2));
}

void AnnotationEngine::recalculateCounterValue()
{
    int maxCounter = 0;
    for (const Annotation &ann : m_annotations) {
        if (ann.tool == Counter)
            maxCounter = qMax(maxCounter, ann.counterValue);
    }
    m_counterValue = maxCounter;
}

// The selected and the hidden (being edited) annotation must keep pointing at
// the same annotation when another one is inserted or removed before them.
void AnnotationEngine::adjustSelectionForInsert(int index)
{
    if (m_selectedIndex >= index)
        ++m_selectedIndex;
    if (m_hiddenIndex >= index)
        ++m_hiddenIndex;
}

void AnnotationEngine::adjustSelectionForRemove(int index)
{
    if (m_selectedIndex == index)
        m_selectedIndex = -1;
    else if (m_selectedIndex > index)
        --m_selectedIndex;
    if (m_hiddenIndex == index)
        m_hiddenIndex = -1;
    else if (m_hiddenIndex > index)
        --m_hiddenIndex;
}

void AnnotationEngine::resetGestureState()
{
    m_textResizeIndex = -1;
    m_textResizeOriginal = Annotation();
    m_rotateIndex = -1;
    m_rotateOriginalDegrees = 0.0;
    m_moveGestureIndex = -1;
    m_moveGestureOriginal = Annotation();
    m_moveGestureHistoryStarted = false;
    m_moveHistorySize = -1;
    m_shapeResizeIndex = -1;
    m_shapeResizeHandle = ShapeHandle::None;
    m_shapeResizeOriginal = Annotation();
    m_propertyEditOpen = false;
}

void AnnotationEngine::setScreenSnapshot(const QPixmap &snapshot)
{
    m_screenSnapshot = snapshot;
    m_redactionCache.clear();
}

void AnnotationEngine::releaseScreenSnapshot()
{
    m_screenSnapshot = QPixmap();
    m_redactionCache.clear();
}

namespace {
int redactionPixels(AnnotationEngine::Tool tool, int intensity, qreal scale)
{
    // The slider is the pixelate block size; a smooth blur of half that
    // radius hides text about as well. Both are logical pixels.
    const qreal logical = tool == AnnotationEngine::Pixelate ? intensity : intensity / 2.0;
    return qMax(1, qRound(logical * scale));
}

QImage applyRedaction(AnnotationEngine::Tool tool, const QImage &image, int pixels,
                      const QPoint &gridOrigin)
{
    return tool == AnnotationEngine::Pixelate
        ? ImageEffects::pixelate(image, pixels, gridOrigin)
        : ImageEffects::smoothBlur(image, pixels);
}

// Area to read around a region so its edges are computed from real
// neighbouring pixels: whole grid blocks for pixelate, the blur reach for blur.
QRect redactionSampleRect(AnnotationEngine::Tool tool, const QRect &rect, int pixels)
{
    if (tool == AnnotationEngine::Pixelate) {
        const auto down = [pixels](int v) {
            return (v >= 0 ? v / pixels : -((-v + pixels - 1) / pixels)) * pixels;
        };
        const int left = down(rect.left());
        const int top = down(rect.top());
        const int right = down(rect.left() + rect.width() - 1) + pixels;
        const int bottom = down(rect.top() + rect.height() - 1) + pixels;
        return QRect(left, top, right - left, bottom - top);
    }
    const int margin = pixels * 3;
    return rect.adjusted(-margin, -margin, margin, margin);
}
}

QPixmap AnnotationEngine::redactedSnapshotRegion(Tool tool, const QRect &physicalRect,
                                                 int intensity) const
{
    const int pixels = redactionPixels(tool, intensity, m_snapshotScale);
    const QString key = QStringLiteral("%1:%2:%3:%4:%5:%6:%7")
        .arg(m_screenSnapshot.cacheKey()).arg(int(tool)).arg(pixels)
        .arg(physicalRect.x()).arg(physicalRect.y())
        .arg(physicalRect.width()).arg(physicalRect.height());
    const auto cached = m_redactionCache.constFind(key);
    if (cached != m_redactionCache.constEnd())
        return cached.value();

    // Pixelate blocks follow a grid anchored to the screenshot, so they stay
    // put while a region is drawn or resized.
    const QRect sample = redactionSampleRect(tool, physicalRect, pixels)
                             .intersected(m_screenSnapshot.rect());
    const QImage processed = applyRedaction(tool, m_screenSnapshot.copy(sample).toImage(),
                                            pixels, sample.topLeft());
    const QPixmap result = QPixmap::fromImage(
        processed.copy(physicalRect.translated(-sample.topLeft())));

    if (m_redactionCache.size() > 64)
        m_redactionCache.clear();
    m_redactionCache.insert(key, result);
    return result;
}

void AnnotationEngine::drawRedaction(QPainter *painter, Tool tool, const QRect &rect,
                                     const QPoint &offset, int intensity)
{
    const QRect target = rect.translated(offset);
    if (target.isEmpty())
        return;

    painter->save();

    // Sample the untouched screenshot for both the live preview and the final
    // capture, so earlier annotations never bleed into the effect. rect is in
    // logical overlay coordinates (before offset); the snapshot is physical
    // pixels scaled by m_snapshotScale. The destination goes through the
    // painter transform (e.g. the high-DPI scale of the final capture).
    if (!m_screenSnapshot.isNull()) {
        const qreal s = m_snapshotScale;
        const QRect sourceRect(qRound(rect.x() * s), qRound(rect.y() * s),
                               qRound(rect.width() * s), qRound(rect.height() * s));
        const QRect clamped = sourceRect.intersected(m_screenSnapshot.rect());
        if (!clamped.isEmpty()) {
            const QPixmap region = redactedSnapshotRegion(tool, clamped, intensity);
            if (!region.isNull()) {
                // Map the clamped physical region back to its logical destination.
                const QRectF dst(clamped.x() / s + offset.x(), clamped.y() / s + offset.y(),
                                 clamped.width() / s, clamped.height() / s);
                painter->drawPixmap(dst, region, QRectF(region.rect()));
                painter->restore();
                return;
            }
        }
    }

    // No snapshot: fall back to processing what the painter's QPixmap device
    // already holds. The painter may carry a scale transform on high-DPI
    // displays, so map the target into the device's physical pixels.
    QPixmap *dev = dynamic_cast<QPixmap*>(painter->device());
    if (dev) {
        const QRect clamped = painter->transform().mapRect(target).intersected(dev->rect());
        if (!clamped.isEmpty()) {
            const int pixels = redactionPixels(tool, intensity, painter->transform().m11());
            const QRect sample = redactionSampleRect(tool, clamped, pixels).intersected(dev->rect());
            const QImage processed = applyRedaction(tool, dev->copy(sample).toImage(),
                                                    pixels, sample.topLeft());
            // clamped is in device pixels; draw it back bypassing the transform.
            painter->resetTransform();
            painter->drawImage(clamped.topLeft(),
                               processed.copy(clamped.translated(-sample.topLeft())));
            painter->restore();
            return;
        }
    }

    // Fallback: gray fill
    painter->fillRect(target, QColor(128, 128, 128, 180));
    painter->restore();
}
