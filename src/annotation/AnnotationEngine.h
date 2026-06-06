#ifndef ANNOTATIONENGINE_H
#define ANNOTATIONENGINE_H

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QColor>
#include <QVector>
#include <QPixmap>
#include <QString>
#include <QFont>

class AnnotationEngine : public QObject {
    Q_OBJECT

public:
    enum Tool {
        None = 0,
        Pen,
        Arrow,
        Rectangle,
        Circle,
        Text,
        Blur,
        Highlighter,
        Counter,
        Eraser,
        Line,
        SemiRect
    };
    Q_ENUM(Tool)

    explicit AnnotationEngine(QObject *parent = nullptr);
    ~AnnotationEngine();

    void setCurrentTool(Tool tool);
    Tool currentTool() const { return m_currentTool; }

    void setColor(const QColor &color);
    QColor color() const { return m_color; }

    void setPenWidth(int width);
    int penWidth() const { return m_penWidth; }

    void setTextFontFamily(const QString &family);
    QString textFontFamily() const { return m_textFontFamily; }
    void setTextFontSize(int size);
    int textFontSize() const { return m_textFontSize; }

    void setBlurIntensity(int intensity);
    int blurIntensity() const { return m_blurIntensity; }

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    void setShiftHeld(bool held);

    void beginDraw(const QPoint &pos);
    void continueDraw(const QPoint &pos);
    void endDraw(const QPoint &pos);

    void render(QPainter *painter, const QPoint &offset);
    bool hasAnnotations() const { return !m_annotations.isEmpty(); }

    void clear();
    void undo();
    void redo();

    void addTextAnnotation(const QPoint &pos, const QString &text);
    void addCounterAnnotation(const QPoint &pos);
    bool eraseAnnotationAt(const QPoint &pos);

    // Annotation move
    int findAnnotationAt(const QPoint &pos);
    void moveAnnotation(int index, const QPoint &delta);
    void setSelectedIndex(int index);
    int selectedIndex() const { return m_selectedIndex; }
    QRect boundingRectOf(int index) const;

    void setScreenSnapshot(const QPixmap &snapshot);
    // Ratio of physical snapshot pixels to logical annotation coordinates, so
    // the blur tool samples the correct snapshot region on high-DPI displays.
    void setSnapshotScale(qreal scale) { m_snapshotScale = scale; }
    void setSelectionRect(const QRect &rect);
    QPixmap screenSnapshot() const { return m_screenSnapshot; }

signals:
    void annotationAdded();

private:
    struct Annotation {
        Tool tool;
        QColor color;
        int penWidth;
        QVector<QPoint> points;
        QString text;
        QString fontFamily;
        int fontSize = 18;
        QRect boundingRect;
        int counterValue = 0;
        bool shiftConstrained = false;
    };

    struct HistoryAction {
        enum Type { Add, Remove } type = Add;
        Annotation annotation;
        int index = -1;
    };

    void drawAnnotation(QPainter *painter, const Annotation &ann, const QPoint &offset);
    void drawBlurEffect(QPainter *painter, const QRect &rect, const QPoint &offset);
    QRect annotationBounds(const Annotation &ann, int padding = 10) const;
    bool annotationContainsPoint(const Annotation &ann, const QPoint &pos, int padding = 8) const;
    void pushHistory(HistoryAction::Type type, const Annotation &annotation, int index);
    void recalculateCounterValue();

    Tool m_currentTool;
    QColor m_color;
    int m_penWidth;
    QString m_textFontFamily;
    int m_textFontSize;
    int m_blurIntensity;
    bool m_shiftHeld;

    QVector<Annotation> m_annotations;
    QVector<HistoryAction> m_undoStack;
    QVector<HistoryAction> m_redoStack;
    Annotation m_currentAnnotation;
    bool m_isDrawing;
    int m_counterValue;
    int m_selectedIndex;
    QPixmap m_screenSnapshot;
    QRect m_selectionRect;
    qreal m_snapshotScale = 1.0;
};

#endif
