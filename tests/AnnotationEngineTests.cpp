#include <QtTest>
#include <QPainter>

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
    void releasesScreenSnapshotExplicitly();
    void capsUndoHistoryAtTwoHundredActions();
    void evictingOldestActionKeepsRemainingHistory();
    void blurKeepsTheIntensityItWasDrawnWith();
    void finalBlurSamplesTheScreenshotNotEarlierAnnotations();
    void scaledTextHitTestingMatchesTheVisibleBackground();
    void resizingRotatedTextStartsWithoutAJump();
    void undoRedoOfRemovalKeepsSelectionOnTheSameAnnotation();
    void shiftConstrainedCircleHitTestingUsesTheRenderedCircle();
    void clearResetsInProgressGestures();
    void switchingToolMidStrokeFinishesTheStroke();
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
    // QTextDocument font metrics differ slightly between supported Qt builds.
    // The resize contract is preserved when all four bounds remain within one
    // logical pixel of the requested geometry.
    const auto nearlyEqualBounds = [](const QRectF &actual, const QRectF &expected) {
        return qAbs(actual.left() - expected.left()) <= 1.0
            && qAbs(actual.top() - expected.top()) <= 1.0
            && qAbs(actual.width() - expected.width()) <= 1.0
            && qAbs(actual.height() - expected.height()) <= 1.0;
    };

    engine.beginTextResize(0);
    engine.resizeTextAnnotation(0, stretchedBounds);
    engine.endTextResize();
    QVERIFY(nearlyEqualBounds(engine.rotatedBoundingRectOf(0), stretchedBounds));

    engine.undo();
    QCOMPARE(engine.rotatedBoundingRectOf(0), originalBounds);

    engine.redo();
    QVERIFY(nearlyEqualBounds(engine.rotatedBoundingRectOf(0), stretchedBounds));
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

void AnnotationEngineTests::releasesScreenSnapshotExplicitly()
{
    AnnotationEngine engine;
    engine.setScreenSnapshot(QPixmap(1920, 1080));
    QVERIFY(!engine.screenSnapshot().isNull());

    engine.releaseScreenSnapshot();

    QVERIFY(engine.screenSnapshot().isNull());
}

void AnnotationEngineTests::capsUndoHistoryAtTwoHundredActions()
{
    AnnotationEngine engine;
    for (int i = 0; i < 201; ++i)
        engine.addTextAnnotation(QPoint(i, i), QString::number(i));

    for (int i = 0; i < 200; ++i)
        engine.undo();

    QVERIFY(!engine.canUndo());
    QVERIFY(engine.hasAnnotations());
}

void AnnotationEngineTests::evictingOldestActionKeepsRemainingHistory()
{
    AnnotationEngine engine;
    for (int i = 0; i < 201; ++i)
        engine.addTextAnnotation(QPoint(i, i), QString::number(i));

    // Only the oldest entry is dropped; the other 199 undos stay available.
    for (int i = 0; i < 199; ++i)
        engine.undo();
    QVERIFY(engine.canUndo());
    engine.undo();
    QVERIFY(!engine.canUndo());
    QVERIFY(engine.canRedo());
}

namespace {
QPixmap gradientSnapshot()
{
    QImage image(200, 200, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x)
            image.setPixelColor(x, y, QColor(x, y, (x * y) % 256));
    }
    return QPixmap::fromImage(image);
}

QImage renderEngine(AnnotationEngine &engine, const QPixmap &base, const QPoint &offset = QPoint())
{
    QPixmap result = base;
    QPainter painter(&result);
    engine.render(&painter, offset);
    painter.end();
    return result.toImage();
}

void drawBlur(AnnotationEngine &engine, const QRect &rect)
{
    engine.setCurrentTool(AnnotationEngine::Blur);
    engine.beginDraw(rect.topLeft());
    engine.endDraw(rect.bottomRight());
}
}

void AnnotationEngineTests::blurKeepsTheIntensityItWasDrawnWith()
{
    const QPixmap snapshot = gradientSnapshot();
    AnnotationEngine reference;
    reference.setScreenSnapshot(snapshot);
    reference.setBlurIntensity(8);
    drawBlur(reference, QRect(0, 0, 64, 64));

    AnnotationEngine engine;
    engine.setScreenSnapshot(snapshot);
    engine.setBlurIntensity(8);
    drawBlur(engine, QRect(0, 0, 64, 64));
    engine.setBlurIntensity(64);

    QCOMPARE(renderEngine(engine, snapshot), renderEngine(reference, snapshot));
}

void AnnotationEngineTests::finalBlurSamplesTheScreenshotNotEarlierAnnotations()
{
    QPixmap snapshot(200, 200);
    snapshot.fill(Qt::blue);
    AnnotationEngine engine;
    engine.setScreenSnapshot(snapshot);
    engine.setColor(Qt::red);
    engine.setCurrentTool(AnnotationEngine::SemiRect);
    engine.beginDraw(QPoint(40, 40));
    engine.endDraw(QPoint(80, 80));
    drawBlur(engine, QRect(20, 20, 100, 100));

    // Mirror getSelectedPixmap(): crop the selection and render with -topLeft.
    const QRect selection(10, 10, 150, 150);
    const QImage result = renderEngine(engine, snapshot.copy(selection), -selection.topLeft());
    QCOMPARE(result.pixelColor(QPoint(60, 60) - selection.topLeft()), QColor(Qt::blue));
}

void AnnotationEngineTests::scaledTextHitTestingMatchesTheVisibleBackground()
{
    AnnotationEngine engine;
    engine.addTextAnnotation(QPoint(100, 100), QStringLiteral("Scale me"));
    const QRectF original = engine.rotatedBoundingRectOf(0);

    engine.beginTextResize(0);
    engine.resizeTextAnnotation(0, QRectF(original.topLeft(), original.size() * 2.0));
    engine.endTextResize();
    const QRect scaled = engine.boundingRectOf(0);

    // Inside the scaled background but beyond the unscaled one.
    QCOMPARE(engine.findAnnotationAt(scaled.bottomRight() - QPoint(4, 4)), 0);
    QCOMPARE(engine.findAnnotationAt(scaled.bottomRight() + QPoint(20, 20)), -1);
}

void AnnotationEngineTests::resizingRotatedTextStartsWithoutAJump()
{
    AnnotationEngine engine;
    engine.addTextAnnotation(QPoint(100, 100), QStringLiteral("Rotated text"));
    engine.rotateAnnotation(0, 90.0);
    const QRectF startBounds = engine.rotatedBoundingRectOf(0);

    engine.beginTextResize(0);
    engine.resizeTextAnnotation(0, startBounds);
    QVERIFY(qAbs(engine.rotatedBoundingRectOf(0).left() - startBounds.left()) <= 1.0);
    QVERIFY(qAbs(engine.rotatedBoundingRectOf(0).top() - startBounds.top()) <= 1.0);
    QVERIFY(qAbs(engine.rotatedBoundingRectOf(0).width() - startBounds.width()) <= 1.0);
    QVERIFY(qAbs(engine.rotatedBoundingRectOf(0).height() - startBounds.height()) <= 1.0);

    // Dragging the bottom-right corner of the on-screen box grows the text
    // into that box instead of reinterpreting it as the unrotated background.
    const QRectF grown(startBounds.topLeft(), startBounds.size() * 2.0);
    engine.resizeTextAnnotation(0, grown);
    engine.endTextResize();
    const QRectF actual = engine.rotatedBoundingRectOf(0);
    QVERIFY(qAbs(actual.left() - grown.left()) <= 2.0);
    QVERIFY(qAbs(actual.top() - grown.top()) <= 2.0);
    QVERIFY(qAbs(actual.width() - grown.width()) <= 2.0);
    QVERIFY(qAbs(actual.height() - grown.height()) <= 2.0);
    QCOMPARE(engine.rotationDegreesOf(0), 90.0);
}

void AnnotationEngineTests::undoRedoOfRemovalKeepsSelectionOnTheSameAnnotation()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Rectangle);
    engine.beginDraw(QPoint(10, 10));
    engine.endDraw(QPoint(40, 40));
    engine.beginDraw(QPoint(100, 100));
    engine.endDraw(QPoint(140, 140));

    engine.setSelectedIndex(1);
    QVERIFY(engine.eraseAnnotationAt(QPoint(10, 20)));
    QCOMPARE(engine.selectedIndex(), 0);

    engine.undo();
    QCOMPARE(engine.selectedIndex(), 1);
    engine.redo();
    QCOMPARE(engine.selectedIndex(), 0);

    engine.undo();
    engine.setSelectedIndex(0);
    engine.redo();
    QCOMPARE(engine.selectedIndex(), -1);
}

void AnnotationEngineTests::shiftConstrainedCircleHitTestingUsesTheRenderedCircle()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Circle);
    engine.setShiftHeld(true);
    engine.beginDraw(QPoint(10, 10));
    engine.endDraw(QPoint(60, 40));

    // The rendered circle spans 10..40; its right edge is at x = 40.
    QCOMPARE(engine.findAnnotationAt(QPoint(40, 25)), 0);
    QCOMPARE(engine.findAnnotationAt(QPoint(60, 25)), -1);
}

void AnnotationEngineTests::clearResetsInProgressGestures()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Rectangle);
    engine.beginDraw(QPoint(10, 10));
    engine.endDraw(QPoint(40, 40));
    engine.beginMove(0);
    engine.moveAnnotation(0, QPoint(5, 5));

    engine.clear();
    engine.beginDraw(QPoint(100, 100));
    engine.endDraw(QPoint(150, 150));
    const QRectF drawnBounds = engine.rotatedBoundingRectOf(0);

    engine.moveAnnotation(0, QPoint(5, 5));
    engine.endMove();
    engine.undo();
    QCOMPARE(engine.rotatedBoundingRectOf(0), drawnBounds);
}

void AnnotationEngineTests::switchingToolMidStrokeFinishesTheStroke()
{
    AnnotationEngine engine;
    engine.setCurrentTool(AnnotationEngine::Rectangle);
    engine.beginDraw(QPoint(10, 10));
    engine.continueDraw(QPoint(50, 50));

    engine.setCurrentTool(AnnotationEngine::Pen);
    QVERIFY(engine.hasAnnotations());
    QVERIFY(engine.canUndo());

    // The stale release from the old gesture must not create a pen stroke.
    engine.endDraw(QPoint(80, 80));
    engine.undo();
    QVERIFY(!engine.hasAnnotations());
}

QTEST_MAIN(AnnotationEngineTests)

#include "AnnotationEngineTests.moc"
