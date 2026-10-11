#include <QtTest>
#include <QPainter>

#include "annotation/AnnotationEngine.h"
#include "annotation/ImageEffects.h"

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
    void pixelateAveragesEachBlock();
    void pixelateGridStaysFixedWhenTheRegionMoves();
    void smoothBlurKeepsFlatColoursAndSoftensEdges();
    void blurAndPixelateAreDifferentTools();
    void editingTextIsOneUndoStep();
    void emptyingTextRemovesIt();
    void recolouringAndDeletingCanBeUndone();
    void textStyleIsStoredPerLabel();
    void countersNumberFromTheStartValue();
    void counterNumberingStaysConsistentAfterUndoAndErase();
    void counterClickHasNoPointer();
    void counterDragAddsAPointerThatIsPartOfTheCounter();
    void counterPointerMovesAndUndoesWithTheCounter();
    void counterPointerRendersInTheExportedImage();
    void counterSizeIsStoredPerCounter();
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

void AnnotationEngineTests::pixelateAveragesEachBlock()
{
    // A 4x4 block with one white and three black columns averages to 64 grey,
    // instead of taking the colour of a single sampled pixel.
    QImage image(8, 4, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    for (int y = 0; y < 4; ++y)
        image.setPixelColor(0, y, Qt::white);
    const QImage result = ImageEffects::pixelate(image, 4);
    QCOMPARE(result.pixelColor(0, 0), QColor(64, 64, 64));
    QCOMPARE(result.pixelColor(3, 3), QColor(64, 64, 64));
    QCOMPARE(result.pixelColor(4, 0), QColor(Qt::black));
}

void AnnotationEngineTests::pixelateGridStaysFixedWhenTheRegionMoves()
{
    const QImage full = gradientSnapshot().toImage();
    const QImage whole = ImageEffects::pixelate(full, 10);
    // A region cut at an odd offset keeps the blocks of the full image.
    const QRect region(7, 13, 60, 40);
    const QImage part = ImageEffects::pixelate(full.copy(region), 10, region.topLeft());
    QCOMPARE(part.pixelColor(20, 20), whole.pixelColor(region.topLeft() + QPoint(20, 20)));

    AnnotationEngine engine;
    engine.setScreenSnapshot(gradientSnapshot());
    engine.setCurrentTool(AnnotationEngine::Pixelate);
    engine.setBlurIntensity(10);
    engine.beginDraw(QPoint(3, 5));
    engine.endDraw(QPoint(90, 70));
    const QImage rendered = renderEngine(engine, gradientSnapshot());
    QCOMPARE(rendered.pixelColor(35, 35), whole.pixelColor(35, 35));
}

void AnnotationEngineTests::smoothBlurKeepsFlatColoursAndSoftensEdges()
{
    QImage flat(40, 40, QImage::Format_ARGB32_Premultiplied);
    flat.fill(QColor(30, 120, 200));
    QCOMPARE(ImageEffects::smoothBlur(flat, 5).pixelColor(20, 20), QColor(30, 120, 200));
    QCOMPARE(ImageEffects::smoothBlur(flat, 20).pixelColor(0, 0), QColor(30, 120, 200));

    QImage edge(40, 10, QImage::Format_ARGB32_Premultiplied);
    edge.fill(Qt::black);
    for (int y = 0; y < 10; ++y)
        for (int x = 20; x < 40; ++x)
            edge.setPixelColor(x, y, Qt::white);
    const QImage blurred = ImageEffects::smoothBlur(edge, 4);
    const int middle = blurred.pixelColor(20, 5).red();
    QVERIFY(middle > 60 && middle < 200);
    QVERIFY(blurred.pixelColor(17, 5).red() < middle);
    QVERIFY(blurred.pixelColor(23, 5).red() > middle);
    QCOMPARE(blurred.pixelColor(0, 5), QColor(Qt::black));
}

void AnnotationEngineTests::blurAndPixelateAreDifferentTools()
{
    const QPixmap snapshot = gradientSnapshot();
    AnnotationEngine blur;
    blur.setScreenSnapshot(snapshot);
    blur.setBlurIntensity(16);
    drawBlur(blur, QRect(20, 20, 120, 120));

    AnnotationEngine pixelate;
    pixelate.setScreenSnapshot(snapshot);
    pixelate.setBlurIntensity(16);
    pixelate.setCurrentTool(AnnotationEngine::Pixelate);
    pixelate.beginDraw(QPoint(20, 20));
    pixelate.endDraw(QPoint(139, 139));

    const QImage blurred = renderEngine(blur, snapshot);
    const QImage pixelated = renderEngine(pixelate, snapshot);
    QVERIFY(blurred != pixelated);
    // Pixelate paints flat blocks; the smooth blur changes from pixel to pixel.
    QCOMPARE(pixelated.pixelColor(33, 33), pixelated.pixelColor(34, 33));
    QVERIFY(blurred.pixelColor(33, 80) != blurred.pixelColor(38, 80));
    // Outside the region nothing changes.
    QCOMPARE(blurred.pixelColor(5, 5), snapshot.toImage().pixelColor(5, 5));
}

void AnnotationEngineTests::editingTextIsOneUndoStep()
{
    AnnotationEngine engine;
    engine.addTextAnnotation(QPoint(20, 20), QStringLiteral("before"));
    AnnotationEngine::TextStyle style = engine.textStyleOf(0);
    style.color = Qt::green;
    style.bold = false;
    QVERIFY(engine.updateTextAnnotation(0, QStringLiteral("after"), style));
    QCOMPARE(engine.textOf(0), QStringLiteral("after"));
    QCOMPARE(engine.textStyleOf(0).color, QColor(Qt::green));
    QVERIFY(!engine.textStyleOf(0).bold);

    engine.undo();
    QCOMPARE(engine.textOf(0), QStringLiteral("before"));
    QVERIFY(engine.textStyleOf(0).bold);
    engine.redo();
    QCOMPARE(engine.textOf(0), QStringLiteral("after"));
    // An unchanged edit adds no history entry.
    QVERIFY(!engine.updateTextAnnotation(0, QStringLiteral("after"), engine.textStyleOf(0)));
}

void AnnotationEngineTests::emptyingTextRemovesIt()
{
    AnnotationEngine engine;
    engine.addTextAnnotation(QPoint(20, 20), QStringLiteral("gone"));
    QVERIFY(engine.updateTextAnnotation(0, QString(), engine.textStyleOf(0)));
    QVERIFY(!engine.hasAnnotations());
    engine.undo();
    QCOMPARE(engine.textOf(0), QStringLiteral("gone"));
}

void AnnotationEngineTests::recolouringAndDeletingCanBeUndone()
{
    AnnotationEngine engine;
    engine.setColor(Qt::red);
    engine.setCurrentTool(AnnotationEngine::Rectangle);
    engine.beginDraw(QPoint(10, 10));
    engine.endDraw(QPoint(60, 60));
    engine.setSelectedIndex(0);

    QVERIFY(engine.setAnnotationColor(0, Qt::blue));
    QVERIFY(!engine.setAnnotationColor(0, Qt::blue));
    QVERIFY(engine.removeAnnotation(0));
    QVERIFY(!engine.hasAnnotations());
    QCOMPARE(engine.selectedIndex(), -1);

    engine.undo();
    QVERIFY(engine.hasAnnotations());
    engine.undo();
    // Back to red: rendering the rectangle edge shows the original colour.
    QPixmap canvas(80, 80);
    canvas.fill(Qt::white);
    const QImage image = renderEngine(engine, canvas);
    QCOMPARE(image.pixelColor(35, 10).red(), 255);
    QVERIFY(image.pixelColor(35, 10).blue() < 100);

    // Blur regions have no colour to change.
    engine.setCurrentTool(AnnotationEngine::Blur);
    engine.beginDraw(QPoint(0, 0));
    engine.endDraw(QPoint(20, 20));
    QVERIFY(!engine.setAnnotationColor(1, Qt::green));
}

void AnnotationEngineTests::textStyleIsStoredPerLabel()
{
    AnnotationEngine engine;
    engine.setTextBold(false);
    engine.setTextBackground(AnnotationEngine::TextOutline);
    engine.addTextAnnotation(QPoint(10, 10), QStringLiteral("one"));
    engine.setTextBold(true);
    engine.setTextBackground(AnnotationEngine::TextPlain);
    engine.addTextAnnotation(QPoint(10, 60), QStringLiteral("two"));

    QVERIFY(!engine.textStyleOf(0).bold);
    QCOMPARE(engine.textStyleOf(0).background, AnnotationEngine::TextOutline);
    QVERIFY(engine.textStyleOf(1).bold);
    QCOMPARE(engine.textStyleOf(1).background, AnnotationEngine::TextPlain);

    // A label being edited in place is not drawn twice.
    QPixmap canvas(200, 120);
    canvas.fill(Qt::white);
    const QImage visible = renderEngine(engine, canvas);
    engine.setHiddenIndex(0);
    const QImage hidden = renderEngine(engine, canvas);
    QVERIFY(visible != hidden);
    engine.setHiddenIndex(-1);
}

namespace {
void placeCounter(AnnotationEngine &engine, const QPoint &press, const QPoint &release)
{
    engine.setCurrentTool(AnnotationEngine::Counter);
    engine.beginDraw(press);
    engine.continueDraw(release);
    engine.endDraw(release);
}
}

void AnnotationEngineTests::countersNumberFromTheStartValue()
{
    AnnotationEngine engine;
    engine.setCounterStartValue(5);
    placeCounter(engine, QPoint(20, 20), QPoint(20, 20));
    engine.addCounterAnnotation(QPoint(60, 20));
    QCOMPARE(engine.counterValueOf(0), 5);
    QCOMPARE(engine.counterValueOf(1), 6);

    // Clearing for a new capture keeps the start value.
    engine.clear();
    placeCounter(engine, QPoint(20, 20), QPoint(20, 20));
    QCOMPARE(engine.counterValueOf(0), 5);

    engine.setCounterStartValue(0);
    QCOMPARE(engine.counterStartValue(), 1);
    engine.setCounterStartValue(5000);
    QCOMPARE(engine.counterStartValue(), 999);
}

void AnnotationEngineTests::counterNumberingStaysConsistentAfterUndoAndErase()
{
    AnnotationEngine engine;
    engine.setCounterStartValue(3);
    placeCounter(engine, QPoint(20, 20), QPoint(20, 20));
    placeCounter(engine, QPoint(80, 20), QPoint(80, 20));
    placeCounter(engine, QPoint(140, 20), QPoint(140, 20));
    QCOMPARE(engine.nextCounterValue(), 6);

    engine.undo();
    QCOMPARE(engine.nextCounterValue(), 5);
    engine.redo();
    QCOMPARE(engine.nextCounterValue(), 6);

    engine.setCurrentTool(AnnotationEngine::Eraser);
    QVERIFY(engine.eraseAnnotationAt(QPoint(140, 20)));
    QCOMPARE(engine.nextCounterValue(), 5);
    QVERIFY(engine.eraseAnnotationAt(QPoint(80, 20)));
    QVERIFY(engine.eraseAnnotationAt(QPoint(20, 20)));
    QCOMPARE(engine.nextCounterValue(), 3);
}

void AnnotationEngineTests::counterClickHasNoPointer()
{
    AnnotationEngine engine;
    engine.setCounterDragDistance(10);
    placeCounter(engine, QPoint(50, 50), QPoint(53, 52));
    QCOMPARE(engine.boundingRectOf(0), QRect(36, 36, 28, 28));
    QCOMPARE(engine.findAnnotationAt(QPoint(120, 50)), -1);
}

void AnnotationEngineTests::counterDragAddsAPointerThatIsPartOfTheCounter()
{
    AnnotationEngine engine;
    engine.setCounterDragDistance(10);
    engine.setPenWidth(3);
    placeCounter(engine, QPoint(50, 50), QPoint(150, 50));
    QCOMPARE(engine.counterValueOf(0), 1);

    const QRect bounds = engine.boundingRectOf(0);
    QCOMPARE(bounds.left(), 36);
    QVERIFY(bounds.right() >= 150);
    QCOMPARE(engine.findAnnotationAt(QPoint(110, 50)), 0);
    QCOMPARE(engine.findAnnotationAt(QPoint(148, 52)), 0);
    QCOMPARE(engine.findAnnotationAt(QPoint(110, 90)), -1);

    // Erasing the pointer erases the whole counter, and undo brings both back.
    engine.setCurrentTool(AnnotationEngine::Eraser);
    QVERIFY(engine.eraseAnnotationAt(QPoint(110, 50)));
    QVERIFY(!engine.hasAnnotations());
    engine.undo();
    QCOMPARE(engine.boundingRectOf(0), bounds);
}

void AnnotationEngineTests::counterPointerMovesAndUndoesWithTheCounter()
{
    AnnotationEngine engine;
    engine.setCounterDragDistance(10);
    placeCounter(engine, QPoint(50, 50), QPoint(50, 150));
    const QRect original = engine.boundingRectOf(0);

    engine.beginMove(0);
    engine.moveAnnotation(0, QPoint(30, 10));
    engine.endMove();
    QCOMPARE(engine.boundingRectOf(0), original.translated(30, 10));
    QCOMPARE(engine.findAnnotationAt(QPoint(80, 140)), 0);

    engine.undo();
    QCOMPARE(engine.boundingRectOf(0), original);
    engine.undo();
    QVERIFY(!engine.hasAnnotations());
    engine.redo();
    QCOMPARE(engine.boundingRectOf(0), original);
}

void AnnotationEngineTests::counterPointerRendersInTheExportedImage()
{
    AnnotationEngine engine;
    engine.setColor(Qt::blue);
    engine.setPenWidth(4);
    engine.setCounterDragDistance(10);
    placeCounter(engine, QPoint(60, 60), QPoint(160, 60));

    QPixmap canvas(200, 120);
    canvas.fill(Qt::white);
    const QImage screen = renderEngine(engine, canvas);
    QCOMPARE(screen.pixelColor(110, 60), QColor(Qt::blue));
    QCOMPARE(screen.pixelColor(110, 90), QColor(Qt::white));

    // The export crops the selection and renders with a negative offset.
    QPixmap crop(150, 80);
    crop.fill(Qt::white);
    const QImage exported = renderEngine(engine, crop, QPoint(-40, -30));
    QCOMPARE(exported.pixelColor(70, 30), QColor(Qt::blue));
    QCOMPARE(exported, screen.copy(40, 30, 150, 80));
}

void AnnotationEngineTests::counterSizeIsStoredPerCounter()
{
    AnnotationEngine engine;
    engine.setCounterDiameter(40);
    placeCounter(engine, QPoint(50, 50), QPoint(50, 50));
    engine.setCounterDiameter(20);
    placeCounter(engine, QPoint(150, 50), QPoint(150, 50));

    QCOMPARE(engine.boundingRectOf(0), QRect(30, 30, 40, 40));
    QCOMPARE(engine.boundingRectOf(1), QRect(140, 40, 20, 20));
    engine.setCounterDiameter(1);
    QCOMPARE(engine.counterDiameter(), 16);
}

QTEST_MAIN(AnnotationEngineTests)

#include "AnnotationEngineTests.moc"
