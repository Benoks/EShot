#include <QtTest>

#include "annotation/AnnotationEngine.h"

class AnnotationEngineTests : public QObject {
    Q_OBJECT

private slots:
    void rotatesRectangleAroundItsCenter();
    void undoRedoRestoresRotationAngle();
    void rotatedTextHitTestingUsesTransformedBounds();
    void textHitTestingDoesNotExtendBeyondVisibleBackground();
    void textStretchAnchorsTheOppositeCornerAndSupportsUndoRedo();
    void shiftConstrainedCircleBoundsMatchTheRenderedCircle();
    void shiftConstrainedHighlighterSnapsToItsDominantAxis();
    void exposesOnlyRequestedAnnotationTypesAsRotatable();
};

void AnnotationEngineTests::rotatesRectangleAroundItsCenter()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Rectangle);
    engine.beginDraw(QPoint(10, 10));
    engine.endDraw(QPoint(50, 30));

    engine.rotateAnnotation(0, 90.0);

    QCOMPARE(engine.rotatedBoundingRectOf(0), QRectF(20, 0, 21, 41));
}

void AnnotationEngineTests::undoRedoRestoresRotationAngle()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Line);
    engine.beginDraw(QPoint(10, 10));
    engine.endDraw(QPoint(50, 10));
    const QRectF originalBounds = engine.rotatedBoundingRectOf(0);

    engine.rotateAnnotation(0, 90.0);
    const QRectF rotatedBounds = engine.rotatedBoundingRectOf(0);
    QVERIFY(rotatedBounds != originalBounds);

    engine.undo();
    QCOMPARE(engine.rotatedBoundingRectOf(0), originalBounds);

    engine.redo();
    QCOMPARE(engine.rotatedBoundingRectOf(0), rotatedBounds);
}

void AnnotationEngineTests::rotatedTextHitTestingUsesTransformedBounds()
{
    AnnotationEngine engine;
    engine.addTextAnnotation(QPoint(40, 40), QStringLiteral("Rotate me"));
    engine.rotateAnnotation(0, 45.0);

    const QPoint hitPoint = engine.rotatedBoundingRectOf(0).center().toPoint();
    QCOMPARE(engine.findAnnotationAt(hitPoint), 0);
}

void AnnotationEngineTests::textHitTestingDoesNotExtendBeyondVisibleBackground()
{
    AnnotationEngine engine;
    engine.addTextAnnotation(QPoint(100, 100), QStringLiteral("bu text"));

    const QRect visibleBounds = engine.boundingRectOf(0);
    QVERIFY(!visibleBounds.contains(QPoint(visibleBounds.left() - 5, visibleBounds.top() - 5)));
    QCOMPARE(engine.findAnnotationAt(QPoint(visibleBounds.left() - 5,
                                            visibleBounds.top() - 5)), -1);
}

void AnnotationEngineTests::textStretchAnchorsTheOppositeCornerAndSupportsUndoRedo()
{
    AnnotationEngine engine;
    engine.setTextFontSize(18);
    engine.addTextAnnotation(QPoint(100, 100), QStringLiteral("Resize me"));
    const QRectF originalBounds = engine.rotatedBoundingRectOf(0);
    const QRectF stretchedBounds(originalBounds.left() - 30, originalBounds.top() - 12,
                                 originalBounds.width() + 30, originalBounds.height() + 12);

    engine.beginTextResize(0);
    engine.resizeTextAnnotation(0, stretchedBounds);
    engine.endTextResize();
    QCOMPARE(engine.rotatedBoundingRectOf(0), stretchedBounds);

    engine.undo();
    QCOMPARE(engine.rotatedBoundingRectOf(0), originalBounds);

    engine.redo();
    QCOMPARE(engine.rotatedBoundingRectOf(0), stretchedBounds);
}

void AnnotationEngineTests::shiftConstrainedCircleBoundsMatchTheRenderedCircle()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Circle);
    engine.setShiftHeld(true);
    engine.beginDraw(QPoint(10, 10));
    engine.endDraw(QPoint(60, 40));

    QCOMPARE(engine.rotatedBoundingRectOf(0), QRectF(10, 10, 31, 31));
}

void AnnotationEngineTests::shiftConstrainedHighlighterSnapsToItsDominantAxis()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Highlighter);
    engine.setShiftHeld(true);
    engine.beginDraw(QPoint(10, 10));
    engine.continueDraw(QPoint(40, 28));
    engine.endDraw(QPoint(60, 34));

    QCOMPARE(engine.rotatedBoundingRectOf(0), QRectF(10, 10, 51, 1));
}

void AnnotationEngineTests::exposesOnlyRequestedAnnotationTypesAsRotatable()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Rectangle);
    engine.beginDraw(QPoint(0, 0));
    engine.endDraw(QPoint(20, 20));
    QVERIFY(engine.isRotatable(0));

    engine.setCurrentTool(AnnotationEngine::Pen);
    engine.beginDraw(QPoint(40, 0));
    engine.endDraw(QPoint(60, 20));
    QVERIFY(!engine.isRotatable(1));
}

QTEST_MAIN(AnnotationEngineTests)

#include "AnnotationEngineTests.moc"
