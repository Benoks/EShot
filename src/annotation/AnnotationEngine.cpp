#include "AnnotationEngine.h"
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

void AnnotationEngine::setCurrentTool(Tool tool) { m_currentTool = tool; }
void AnnotationEngine::setColor(const QColor &color) { m_color = color; }
void AnnotationEngine::setPenWidth(int width) { m_penWidth = qBound(1, width, 20); }
void AnnotationEngine::setTextFontFamily(const QString &family)
{
    if (!family.trimmed().isEmpty())
        m_textFontFamily = family.trimmed();
}
void AnnotationEngine::setTextFontSize(int size) { m_textFontSize = qBound(8, size, 72); }
void AnnotationEngine::setBlurIntensity(int intensity) { m_blurIntensity = qBound(4, intensity, 64); }
void AnnotationEngine::setShiftHeld(bool held) { m_shiftHeld = held; }

void AnnotationEngine::beginDraw(const QPoint &pos)
{
    if (m_currentTool == None || m_currentTool == Text || m_currentTool == Counter)
        return;

    m_isDrawing = true;
    m_currentAnnotation = Annotation();
    m_currentAnnotation.tool = m_currentTool;
    m_currentAnnotation.color = m_color;
    m_currentAnnotation.penWidth = m_penWidth;
    m_currentAnnotation.points.append(pos);

    if (m_currentTool == Highlighter)
        m_currentAnnotation.color = QColor(255, 255, 0, 100);
}

void AnnotationEngine::continueDraw(const QPoint &pos)
{
    if (!m_isDrawing || m_currentTool == None) return;

    m_currentAnnotation.shiftConstrained = m_shiftHeld;

    if (m_currentTool == Pen || m_currentTool == Highlighter) {
        m_currentAnnotation.points.append(pos);
    } else {
        if (m_currentAnnotation.points.size() < 2)
            m_currentAnnotation.points.append(pos);
        else
            m_currentAnnotation.points.last() = pos;
    }
}

void AnnotationEngine::endDraw(const QPoint &pos)
{
    if (!m_isDrawing || m_currentTool == None) return;

    m_currentAnnotation.shiftConstrained = m_shiftHeld;

    if (m_currentTool == Pen || m_currentTool == Highlighter) {
        m_currentAnnotation.points.append(pos);
    } else {
        if (m_currentAnnotation.points.size() < 2)
            m_currentAnnotation.points.append(pos);
        else
            m_currentAnnotation.points.last() = pos;
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
    for (const Annotation &a : m_annotations)
        drawAnnotation(painter, a, offset);
    if (m_isDrawing)
        drawAnnotation(painter, m_currentAnnotation, offset);
    painter->restore();
}

void AnnotationEngine::drawAnnotation(QPainter *painter, const Annotation &ann, const QPoint &offset)
{
    if (ann.points.isEmpty()) return;
    painter->save();

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
        QPoint start = ann.points.first() + offset;
        QPoint end = ann.points.last() + offset;
        QPen pen(ann.color, ann.penWidth, Qt::SolidLine, Qt::RoundCap);
        painter->setPen(pen);
        painter->drawLine(start, end);
        double angle = qAtan2(end.y() - start.y(), end.x() - start.x());
        int arrowSize = qMax(12, ann.penWidth * 4);
        QPointF a1 = end - QPointF(qCos(angle - M_PI/6)*arrowSize, qSin(angle - M_PI/6)*arrowSize);
        QPointF a2 = end - QPointF(qCos(angle + M_PI/6)*arrowSize, qSin(angle + M_PI/6)*arrowSize);
        painter->setPen(Qt::NoPen);
        painter->setBrush(ann.color);
        painter->drawPolygon(QPolygonF() << QPointF(end) << a1 << a2);
        break;
    }
    case Rectangle: {
        if (ann.points.size() < 2) break;
        QPoint start = ann.points.first() + offset;
        QPoint end = ann.points.last() + offset;
        QRect r(start, end);
        if (ann.shiftConstrained) {
            int side = qMin(qAbs(r.width()), qAbs(r.height()));
            r.setWidth(r.width() < 0 ? -side : side);
            r.setHeight(r.height() < 0 ? -side : side);
        }
        QPen pen(ann.color, ann.penWidth);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.normalized());
        break;
    }
    case Circle: {
        if (ann.points.size() < 2) break;
        QPoint start = ann.points.first() + offset;
        QPoint end = ann.points.last() + offset;
        QRect r(start, end);
        if (ann.shiftConstrained) {
            int side = qMin(qAbs(r.width()), qAbs(r.height()));
            r.setWidth(r.width() < 0 ? -side : side);
            r.setHeight(r.height() < 0 ? -side : side);
        }
        QPen pen(ann.color, ann.penWidth);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(r.normalized());
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
        QPen pen(ann.color, ann.penWidth * 5, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
        painter->setPen(pen);
        painter->setOpacity(0.4);
        if (ann.points.size() > 1) {
            QPainterPath path;
            path.moveTo(ann.points.first() + offset);
            for (int i = 1; i < ann.points.size(); ++i)
                path.lineTo(ann.points[i] + offset);
            painter->drawPath(path);
        }
        break;
    }
    case Blur: {
        if (ann.points.size() < 2) break;
        QRect r = QRect(ann.points.first(), ann.points.last()).normalized();
        drawBlurEffect(painter, r, offset);
        break;
    }
    case Text: {
        if (ann.text.isEmpty() || ann.points.isEmpty()) break;
        QFont font(ann.fontFamily.isEmpty() ? QStringLiteral("Segoe UI") : ann.fontFamily,
                   qBound(8, ann.fontSize, 72));
        font.setBold(true);
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

        // Background
        QRect bg(tp.x() - 4, tp.y() - 4,
                 static_cast<int>(docSize.width()) + 8,
                 static_cast<int>(docSize.height()) + 8);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 120));
        painter->drawRoundedRect(bg, 3, 3);

        // Text
        painter->save();
        painter->translate(tp);
        doc.drawContents(painter);
        painter->restore();
        break;
    }
    case Counter: {
        if (ann.points.isEmpty()) break;
        QPoint center = ann.points.first() + offset;
        int radius = 14;
        painter->setPen(QPen(Qt::white, 2));
        painter->setBrush(ann.color);
        painter->drawEllipse(center, radius, radius);
        painter->setPen(Qt::white);
        QFont f("Segoe UI", 10); f.setBold(true);
        painter->setFont(f);
        painter->drawText(QRect(center.x()-radius, center.y()-radius, radius*2, radius*2),
                          Qt::AlignCenter, QString::number(ann.counterValue));
        break;
    }
    case SemiRect: {
        if (ann.points.size() < 2) break;
        QPoint start = ann.points.first() + offset;
        QPoint end = ann.points.last() + offset;
        QRect r(start, end);
        if (ann.shiftConstrained) {
            int side = qMin(qAbs(r.width()), qAbs(r.height()));
            r.setWidth(r.width() < 0 ? -side : side);
            r.setHeight(r.height() < 0 ? -side : side);
        }
        QColor fillColor = ann.color;
        fillColor.setAlpha(80);
        painter->setPen(QPen(ann.color, ann.penWidth));
        painter->setBrush(fillColor);
        painter->drawRect(r.normalized());
        break;
    }
    default: break;
    }

    painter->restore();
}

void AnnotationEngine::clear()
{
    m_annotations.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_counterValue = 0;
}

void AnnotationEngine::undo()
{
    if (m_undoStack.isEmpty()) return;

    HistoryAction action = m_undoStack.takeLast();
    if (action.type == HistoryAction::Add) {
        if (action.index >= 0 && action.index < m_annotations.size())
            m_annotations.removeAt(action.index);
        else if (!m_annotations.isEmpty())
            m_annotations.removeLast();
    } else {
        int insertAt = qBound(0, action.index, m_annotations.size());
        m_annotations.insert(insertAt, action.annotation);
    }
    m_redoStack.append(action);
    recalculateCounterValue();
}

void AnnotationEngine::redo()
{
    if (m_redoStack.isEmpty()) return;

    HistoryAction action = m_redoStack.takeLast();
    if (action.type == HistoryAction::Add) {
        int insertAt = qBound(0, action.index, m_annotations.size());
        m_annotations.insert(insertAt, action.annotation);
    } else {
        if (action.index >= 0 && action.index < m_annotations.size())
            m_annotations.removeAt(action.index);
        else if (!m_annotations.isEmpty())
            m_annotations.removeLast();
    }
    m_undoStack.append(action);
    recalculateCounterValue();
}

void AnnotationEngine::addTextAnnotation(const QPoint &pos, const QString &text)
{
    if (text.isEmpty()) return;
    Annotation a;
    a.tool = Text; a.color = m_color; a.penWidth = m_penWidth;
    a.fontFamily = m_textFontFamily;
    a.fontSize = m_textFontSize;
    a.points.append(pos); a.text = text;
    m_annotations.append(a);
    pushHistory(HistoryAction::Add, a, m_annotations.size() - 1);
    emit annotationAdded();
}

void AnnotationEngine::addCounterAnnotation(const QPoint &pos)
{
    Annotation ann;
    ann.tool = Counter;
    ann.color = m_color;
    ann.penWidth = m_penWidth;
    ann.points.append(pos);
    ann.counterValue = ++m_counterValue;
    m_annotations.append(ann);
    pushHistory(HistoryAction::Add, ann, m_annotations.size() - 1);
    emit annotationAdded();
}

bool AnnotationEngine::eraseAnnotationAt(const QPoint &pos)
{
    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        const Annotation &ann = m_annotations[i];
        if (ann.points.isEmpty()) continue;

        if (annotationContainsPoint(ann, pos, 8)) {
            Annotation removed = m_annotations[i];
            m_annotations.removeAt(i);
            pushHistory(HistoryAction::Remove, removed, i);
            recalculateCounterValue();
            return true;
        }
    }
    return false;
}

int AnnotationEngine::findAnnotationAt(const QPoint &pos)
{
    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        const Annotation &ann = m_annotations[i];
        if (ann.points.isEmpty()) continue;

        if (annotationContainsPoint(ann, pos, ann.tool == Text ? 18 : 10))
            return i;
    }
    return -1;
}

void AnnotationEngine::moveAnnotation(int index, const QPoint &delta)
{
    if (index < 0 || index >= m_annotations.size()) return;
    Annotation &ann = m_annotations[index];
    for (int i = 0; i < ann.points.size(); ++i) {
        ann.points[i] += delta;
    }
}

void AnnotationEngine::setSelectedIndex(int index)
{
    m_selectedIndex = index;
}

QRect AnnotationEngine::boundingRectOf(int index) const
{
    if (index < 0 || index >= m_annotations.size()) return QRect();
    return annotationBounds(m_annotations[index], 0);
}

QRect AnnotationEngine::annotationBounds(const Annotation &ann, int padding) const
{
    if (ann.points.isEmpty()) return QRect();

    QRect bounds;
    if (ann.tool == Text) {
        QFont font(ann.fontFamily.isEmpty() ? QStringLiteral("Segoe UI") : ann.fontFamily,
                   qBound(8, ann.fontSize, 72));
        font.setBold(true);
        QTextDocument doc;
        doc.setDefaultFont(font);
        doc.setPlainText(ann.text);
        QSize docSize = doc.size().toSize();
        bounds = QRect(ann.points.first(), docSize + QSize(8, 8));
    } else if (ann.tool == Counter) {
        const int radius = 14;
        bounds = QRect(ann.points.first() - QPoint(radius, radius), QSize(radius * 2, radius * 2));
    } else if (ann.tool == Blur || ann.tool == SemiRect || ann.tool == Rectangle || ann.tool == Circle || ann.tool == Line || ann.tool == Arrow) {
        if (ann.points.size() < 2)
            bounds = QRect(ann.points.first(), QSize(1, 1));
        else
            bounds = QRect(ann.points.first(), ann.points.last()).normalized();
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

bool AnnotationEngine::annotationContainsPoint(const Annotation &ann, const QPoint &pos, int padding) const
{
    if (ann.points.isEmpty())
        return false;

    const int tolerance = qMax(padding, ann.penWidth + 5);

    switch (ann.tool) {
    case Pen:
    case Highlighter: {
        if (ann.points.size() == 1)
            return QRect(ann.points.first() - QPoint(tolerance, tolerance), QSize(tolerance * 2, tolerance * 2)).contains(pos);
        const int strokeTolerance = ann.tool == Highlighter
            ? qMax(tolerance, ann.penWidth * 3)
            : tolerance;
        for (int i = 1; i < ann.points.size(); ++i) {
            if (distanceToSegment(pos, ann.points[i - 1], ann.points[i]) <= strokeTolerance)
                return true;
        }
        return false;
    }
    case Line:
    case Arrow:
        if (ann.points.size() < 2)
            return annotationBounds(ann, tolerance).contains(pos);
        return distanceToSegment(pos, ann.points.first(), ann.points.last()) <= tolerance;

    case Rectangle: {
        if (ann.points.size() < 2)
            return annotationBounds(ann, tolerance).contains(pos);
        const QRect r = QRect(ann.points.first(), ann.points.last()).normalized();
        if (!r.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(pos))
            return false;
        const int left = qAbs(pos.x() - r.left());
        const int right = qAbs(pos.x() - r.right());
        const int top = qAbs(pos.y() - r.top());
        const int bottom = qAbs(pos.y() - r.bottom());
        return qMin(qMin(left, right), qMin(top, bottom)) <= tolerance;
    }
    case Circle: {
        if (ann.points.size() < 2)
            return annotationBounds(ann, tolerance).contains(pos);
        const QRect r = QRect(ann.points.first(), ann.points.last()).normalized();
        if (r.width() <= 0 || r.height() <= 0)
            return annotationBounds(ann, tolerance).contains(pos);
        if (!r.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(pos))
            return false;
        const QPointF center = r.center();
        const double rx = r.width() / 2.0;
        const double ry = r.height() / 2.0;
        const double nx = (pos.x() - center.x()) / rx;
        const double ny = (pos.y() - center.y()) / ry;
        const double edge = qSqrt(nx * nx + ny * ny);
        const double normalizedTolerance = tolerance / qMax(1.0, qMin(rx, ry));
        return qAbs(edge - 1.0) <= normalizedTolerance;
    }
    case Text:
    case Blur:
    case SemiRect:
    case Counter:
        return annotationBounds(ann, padding).contains(pos);

    default:
        return annotationBounds(ann, padding).contains(pos);
    }
}

void AnnotationEngine::pushHistory(HistoryAction::Type type, const Annotation &annotation, int index)
{
    HistoryAction action;
    action.type = type;
    action.annotation = annotation;
    action.index = index;
    m_undoStack.append(action);
    m_redoStack.clear();
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

void AnnotationEngine::setScreenSnapshot(const QPixmap &snapshot)
{
    m_screenSnapshot = snapshot;
}

void AnnotationEngine::setSelectionRect(const QRect &rect)
{
    m_selectionRect = rect;
}

void AnnotationEngine::drawBlurEffect(QPainter *painter, const QRect &rect, const QPoint &offset)
{
    QRect target = rect.translated(offset);
    painter->save();

    if (target.isEmpty()) {
        painter->restore();
        return;
    }

    // Final capture: painter draws on a QPixmap (the cropped result). The
    // painter may carry a scale transform on high-DPI displays, so map the blur
    // target into the device's physical pixels before sampling it back.
    QPixmap *dev = dynamic_cast<QPixmap*>(painter->device());
    if (dev) {
        QRect clamped = painter->transform().mapRect(target).intersected(dev->rect());
        if (!clamped.isEmpty()) {
            QPixmap region = dev->copy(clamped);
            if (!region.isNull()) {
                int ps = m_blurIntensity;
                QImage img = region.toImage();
                QImage scaled = img.scaled(qMax(1, img.width() / ps), qMax(1, img.height() / ps),
                                           Qt::IgnoreAspectRatio, Qt::FastTransformation);
                QImage mosaic = scaled.scaled(img.width(), img.height(),
                                              Qt::IgnoreAspectRatio, Qt::FastTransformation);
                // clamped is in device pixels; draw it back bypassing the transform.
                painter->save();
                painter->resetTransform();
                painter->drawPixmap(clamped.topLeft(), QPixmap::fromImage(mosaic));
                painter->restore();
                painter->restore();
                return;
            }
        }
    }

    // Live preview: take from m_screenSnapshot. target is in logical overlay
    // coordinates; the snapshot is physical pixels scaled by m_snapshotScale.
    if (!m_screenSnapshot.isNull()) {
        const qreal s = m_snapshotScale;
        QRect sourceRect(qRound(target.x() * s), qRound(target.y() * s),
                         qRound(target.width() * s), qRound(target.height() * s));
        QRect clamped = sourceRect.intersected(m_screenSnapshot.rect());
        if (!clamped.isEmpty()) {
            QPixmap region = m_screenSnapshot.copy(clamped);
            if (!region.isNull()) {
                int ps = m_blurIntensity;
                QImage img = region.toImage();
                QImage scaled = img.scaled(qMax(1, img.width() / ps), qMax(1, img.height() / ps),
                                           Qt::IgnoreAspectRatio, Qt::FastTransformation);
                QImage mosaic = scaled.scaled(img.width(), img.height(),
                                              Qt::IgnoreAspectRatio, Qt::FastTransformation);
                // Map the clamped physical region back to its logical destination.
                QRectF dst(clamped.x() / s, clamped.y() / s,
                           clamped.width() / s, clamped.height() / s);
                painter->drawPixmap(dst, QPixmap::fromImage(mosaic), QRectF(mosaic.rect()));
                painter->restore();
                return;
            }
        }
    }

    // Fallback: gray fill
    painter->fillRect(target, QColor(128, 128, 128, 180));
    painter->restore();
}
