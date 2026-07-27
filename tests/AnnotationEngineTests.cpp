#include <QtTest>

#include "annotation/AnnotationEngine.h"

class AnnotationEngineTests : public QObject {
    Q_OBJECT

private slots:
    void rotatesRectangleAroundItsCenter();
    void undoRedoRestoresRotationAngle();
    void rotatedTextHitTestingUsesTransformedBounds();
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
