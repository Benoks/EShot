#include <QtTest>
#include <QSettings>
#include "capture/CaptureInteractionPolicy.h"
#include "core/TranslationManager.h"

class CaptureInteractionPolicyTests : public QObject {
    Q_OBJECT
private slots:
    void resizeHandleReleasesActiveTool()
    {
        QVERIFY(shouldReleaseToolForResize(true, 3, 0));
        QVERIFY(!shouldReleaseToolForResize(false, 3, 0));
        QVERIFY(!shouldReleaseToolForResize(true, 0, 0));
    }

    void toolPersistenceDefaultsToNone()
    {
        QCOMPARE(initialAnnotationTool(false, 5, 0), 0);
        QCOMPARE(initialAnnotationTool(true, 5, 0), 5);
    }

    void captureHintsOnlyAppearBeforeInteraction()
    {
        QVERIFY(shouldShowCaptureHints(true, false, false, false));
        QVERIFY(!shouldShowCaptureHints(false, false, false, false));
        QVERIFY(!shouldShowCaptureHints(true, true, false, false));
        QVERIFY(!shouldShowCaptureHints(true, false, true, false));
        QVERIFY(!shouldShowCaptureHints(true, false, false, true));
    }

    void captureHintStaysCenteredInsideTheActiveMonitor()
    {
        QCOMPARE(captureHintRect(QRect(1920, 0, 1920, 1080), QSize(640, 76)),
                 QRect(2560, 24, 640, 76));
        QCOMPARE(captureHintRect(QRect(0, 0, 480, 320), QSize(640, 76)),
                 QRect(16, 24, 448, 76));
    }

    void quickSettingsTabFitsTranslatedTextAndScreen()
    {
        QCOMPARE(quickSettingsTabHeight(70, 1080), 118);
        QCOMPARE(quickSettingsTabHeight(136, 1080), 168);
        QCOMPARE(quickSettingsTabHeight(400, 300), 236);
    }

    void overlayInputTakesPriorityOverInlineTextEditing()
    {
        QCOMPARE(managedProxyKeyDestination(false, false),
                 ManagedProxyKeyDestination::CaptureOverlay);
        QCOMPARE(managedProxyKeyDestination(true, false),
                 ManagedProxyKeyDestination::TextEditor);
        QCOMPARE(managedProxyKeyDestination(false, true),
                 ManagedProxyKeyDestination::OverlayInput);
        QCOMPARE(managedProxyKeyDestination(true, true),
                 ManagedProxyKeyDestination::OverlayInput);
    }

    void managedProxyWindowIsAlsoACaptureKeySource()
    {
        QVERIFY(isManagedProxyInputSource(true, false));
        QVERIFY(isManagedProxyInputSource(false, true));
        QVERIFY(!isManagedProxyInputSource(false, false));
    }

    void overlayEditorIsResolvedFromItsScreenRect()
    {
        const QList<QRect> fields = {QRect(100, 100, 80, 32), QRect(100, 140, 80, 32)};
        QCOMPARE(overlayEditorIndexAt(fields, QPoint(130, 155)), 1);
        QCOMPARE(overlayEditorIndexAt(fields, QPoint(20, 20)), -1);
    }

    void singleClickSelectsTheOverlayEditorValue()
    {
        QVERIFY(shouldSelectOverlayEditorOnPointerPress(true));
        QVERIFY(!shouldSelectOverlayEditorOnPointerPress(false));
    }

    void completedScreenSelectionRestoresCaptureKeyboardFocus()
    {
        QVERIFY(shouldRestoreCaptureKeyboardFocus(true, true, false));
        QVERIFY(!shouldRestoreCaptureKeyboardFocus(false, true, false));
        QVERIFY(!shouldRestoreCaptureKeyboardFocus(true, false, false));
        QVERIFY(!shouldRestoreCaptureKeyboardFocus(true, true, true));
    }

    void managedProxyGrabsKeyboardOnlyForCompletedCapture()
    {
        QVERIFY(shouldGrabCaptureKeyboardFromManagedProxy(true, true, true, false));
        QVERIFY(!shouldGrabCaptureKeyboardFromManagedProxy(false, true, true, false));
        QVERIFY(!shouldGrabCaptureKeyboardFromManagedProxy(true, false, true, false));
        QVERIFY(!shouldGrabCaptureKeyboardFromManagedProxy(true, true, false, false));
        QVERIFY(!shouldGrabCaptureKeyboardFromManagedProxy(true, true, true, true));
    }

    void xwaylandModalIsDetachedFromUnmanagedOverlay()
    {
        QVERIFY(shouldDetachModalFromOverlay(true));
        QVERIFY(!shouldDetachModalFromOverlay(false));
    }

    void crosshairMovementOnlyInvalidatesThinLineRegions()
    {
        const QRect canvas(0, 0, 3840, 1080);
        const QRegion dirty = crosshairUpdateRegion(
            QPoint(200, 300), QPoint(240, 330), canvas);

        QVERIFY(dirty.contains(QPoint(200, 10)));
        QVERIFY(dirty.contains(QPoint(240, 1000)));
        QVERIFY(dirty.contains(QPoint(10, 300)));
        QVERIFY(dirty.contains(QPoint(3800, 330)));
        QVERIFY(!dirty.contains(QPoint(1000, 700)));
        QVERIFY(dirty.boundingRect() == canvas);

        qsizetype dirtyArea = 0;
        for (const QRect &rect : dirty)
            dirtyArea += static_cast<qsizetype>(rect.width()) * rect.height();
        QVERIFY(dirtyArea < static_cast<qsizetype>(canvas.width()) * canvas.height() / 50);
    }

    void selectionMovementOnlyInvalidatesChangedEdgesAndLabels()
    {
        const QRect canvas(0, 0, 3840, 1080);
        const QRect previous(100, 100, 800, 600);
        const QRect current(106, 104, 800, 600);
        const QRegion dirty = selectionUpdateRegion(previous, current, canvas);

        QVERIFY(dirty.contains(previous.topLeft()));
        QVERIFY(dirty.contains(current.bottomRight()));
        QVERIFY(dirty.contains(previous.bottomRight() + QPoint(6, 6)));
        QVERIFY(dirty.contains(previous.topLeft() - QPoint(6, 6)));
        QVERIFY(!dirty.contains(QPoint(500, 400)));

        qsizetype dirtyArea = 0;
        for (const QRect &rect : dirty)
            dirtyArea += static_cast<qsizetype>(rect.width()) * rect.height();
        QVERIFY(dirtyArea < static_cast<qsizetype>(canvas.width()) * canvas.height() / 20);
    }

    void selectionStartInvalidatesTheDismissedCaptureInfoPanel()
    {
        const QRect canvas(0, 0, 1920, 1080);
        const QRect selection(420, 260, 1, 1);
        const QRect infoPanel(640, 24, 640, 132);

        const QRegion dirty = selectionStartUpdateRegion(
            selection, canvas, infoPanel);

        QVERIFY(dirty.contains(infoPanel.topLeft()));
        QVERIFY(dirty.contains(infoPanel.bottomRight()));
        QVERIFY(dirty.contains(selection.topLeft()));
        QVERIFY(!dirty.contains(QPoint(100, 900)));
    }

    void edgeResizeHandlesAdjustOnlyTheirMatchingAxis()
    {
        const QRect selection(100, 200, 80, 60);

        QCOMPARE(selectionResizeHandleAt(selection, QPoint(140, 200)),
                 SelectionResizeHandle::Top);
        QCOMPARE(selectionResizeHandleAt(selection,
                                         QPoint(selection.center().x(), selection.top() + 14)),
                 SelectionResizeHandle::Top);
        QCOMPARE(selectionResizeHandleAt(selection, QPoint(179, 230)),
                 SelectionResizeHandle::Right);
        QCOMPARE(selectionResizeHandleAt(selection, QPoint(140, 259)),
                 SelectionResizeHandle::Bottom);
        QCOMPARE(selectionResizeHandleAt(selection, QPoint(100, 230)),
                 SelectionResizeHandle::Left);

        QCOMPARE(resizedSelectionForHandle(selection, SelectionResizeHandle::Top,
                                           QPoint(140, 180)),
                 QRect(100, 180, 80, 80));
        QCOMPARE(resizedSelectionForHandle(selection, SelectionResizeHandle::Right,
                                           QPoint(205, 230)),
                 QRect(100, 200, 106, 60));
        QCOMPARE(resizedSelectionForHandle(selection, SelectionResizeHandle::Bottom,
                                           QPoint(140, 280)),
                 QRect(100, 200, 80, 81));
        QCOMPARE(resizedSelectionForHandle(selection, SelectionResizeHandle::Left,
                                           QPoint(70, 230)),
                 QRect(70, 200, 110, 60));
    }

    void selectionFrameSharesPixelAlignedHandleCentersAtFractionalScale()
    {
        const QRect selection(20, 10, 22, 20);
        const qreal scale = 1.25;
        const SelectionFrameSegments segments = selectionFrameSegments(selection, scale);

        QCOMPARE(segments.thickness, 0.8);
        QCOMPARE(selectionFrameHandleCenter(selection, SelectionResizeHandle::TopRight, scale).x(),
                 segments.right.x());
        QCOMPARE(selectionFrameHandleCenter(selection, SelectionResizeHandle::BottomRight, scale).x(),
                 segments.right.x());
        QCOMPARE(selectionFrameHandleCenter(selection, SelectionResizeHandle::BottomLeft, scale).y(),
                 segments.bottom.y());
        QCOMPARE(segments.right.x() * scale, qRound(segments.right.x() * scale));
        QCOMPARE(segments.bottom.y() * scale, qRound(segments.bottom.y() * scale));
    }

    void selectionFrameDoesNotPaintBelowAnyHandle()
    {
        const QRect selection(100, 200, 80, 60);
        const QRegion frameClip = selectionFrameClipRegion(selection, QRect(0, 0, 400, 400));

        QVERIFY(!frameClip.contains(selection.topLeft()));
        QVERIFY(!frameClip.contains(QPoint(selection.center().x(), selection.top())));
        QVERIFY(!frameClip.contains(selection.topRight()));
        QVERIFY(!frameClip.contains(QPoint(selection.right(), selection.center().y())));
        QVERIFY(!frameClip.contains(selection.bottomRight()));
        QVERIFY(!frameClip.contains(QPoint(selection.center().x(), selection.bottom())));
        QVERIFY(!frameClip.contains(selection.bottomLeft()));
        QVERIFY(!frameClip.contains(QPoint(selection.left(), selection.center().y())));
    }

    void recordingSelectionSkipsStillImageComposition()
    {
        QVERIFY(shouldComposeCaptureResult(false));
        QVERIFY(!shouldComposeCaptureResult(true));
    }

    void annotationPersistenceLabelsExistInEveryLanguage()
    {
        for (int language = 0; language < TranslationManager::LangCount; ++language) {
            TranslationManager::setLanguage(static_cast<TranslationManager::Language>(language), false);
            QVERIFY(!TranslationManager::rememberLastAnnotationTool().isEmpty());
            QVERIFY(!TranslationManager::rememberLastAnnotationToolHint().isEmpty());
            QVERIFY(!TranslationManager::drawingTools().isEmpty());
            QVERIFY(!TranslationManager::bottomToolbarControls().isEmpty());
        }
        TranslationManager::setLanguage(TranslationManager::Turkish, false);
        QVERIFY(TranslationManager::rememberLastAnnotationTool().contains(QStringLiteral("ı")));
    }

    void auditedTranslationsDoNotFallBackToEnglish()
    {
        const QList<QByteArray> keys = {
            "ocrLanguagePackMissing", "uploadAuthHelpYandex",
            "uploadAuthHelpGoogleDrive", "uploadAuthHelpApiKey",
            "uploadErrorYandexScopeMissing", "uploadErrorGoogleAuthFailed",
            "uploadErrorApiKeyMissing", "toolFontSize", "openFolder",
            "recordingStopShort", "recordingDetails", "ocrAutomatic",
            "captureHintDrag", "captureHintScreen", "captureHintRecording",
            "captureHintCopy", "captureHintSave", "captureHintCancel",
            "captureHintQuickSettings",
            "showCaptureHints", "showCaptureHintsTip", "notificationOpenFolder",
            "gifFpsLabel", "videoFpsLabel"
        };
        TranslationManager::setLanguage(TranslationManager::English, false);
        QHash<QByteArray, QString> english;
        for (const QByteArray &key : keys) english.insert(key, TranslationManager::tr(key.constData()));
        for (int language = TranslationManager::German; language < TranslationManager::LangCount; ++language) {
            TranslationManager::setLanguage(static_cast<TranslationManager::Language>(language), false);
            for (const QByteArray &key : keys)
                QVERIFY2(TranslationManager::tr(key.constData()) != english.value(key), key.constData());
        }
    }
};

QTEST_APPLESS_MAIN(CaptureInteractionPolicyTests)
#include "CaptureInteractionPolicyTests.moc"
