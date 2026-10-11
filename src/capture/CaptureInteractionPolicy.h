#pragma once

#include <QRect>
#include <QRegion>
#include <QSize>
#include <QString>
#include <QList>

enum class SelectionResizeHandle {
    None,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    Move,
    NewSelection
};

struct SelectionFrameSegments {
    QRectF top;
    QRectF right;
    QRectF bottom;
    QRectF left;
    qreal thickness = 1.0;
};

// Keyboard fine-tuning of a finished selection: arrows move it, Ctrl+arrows
// move its right/bottom edge. Shift makes the step 10 px instead of 1 px.
enum class SelectionNudgeMode {
    Move,
    Resize
};

struct SelectionNudge {
    bool valid = false;
    SelectionNudgeMode mode = SelectionNudgeMode::Move;
    QPoint delta;
};

enum class ManagedProxyKeyDestination {
    TextEditor,
    OverlayInput,
    CaptureOverlay
};

bool shouldReleaseToolForResize(bool handleHit, int currentTool, int noneTool);
int initialAnnotationTool(bool rememberLastTool, int storedTool, int noneTool);
bool shouldShowCaptureHints(bool enabled, bool selecting, bool selectionComplete,
                            bool eyedropperActive);
QRect captureHintRect(const QRect &monitorRect, const QSize &preferredSize);
QRect selectionUiMonitorRect(const QRect &selection, const QList<QRect> &monitors,
                            const QRect &fallback);
int quickSettingsTabHeight(int textWidth, int availableHeight);
ManagedProxyKeyDestination managedProxyKeyDestination(bool textEditorVisible,
                                                      bool overlayInputActive);
bool isManagedProxyInputSource(bool proxyWidget, bool proxyWindow);
int overlayEditorIndexAt(const QList<QRect> &editorRects, const QPoint &globalPosition);
bool shouldSelectOverlayEditorOnPointerPress(bool editorUnderPointer);
bool shouldRestoreCaptureKeyboardFocus(bool overlayVisible, bool selectionComplete,
                                       bool textEditorVisible);
bool shouldGrabCaptureKeyboardFromManagedProxy(bool managedProxyAvailable,
                                               bool overlayVisible, bool selectionComplete,
                                               bool textEditorVisible);
bool shouldDetachModalFromOverlay(bool xwaylandOverlay);
bool shouldComposeCaptureResult(bool recordingMode);
bool shouldUsePartialOverlayUpdates(qreal devicePixelRatio);
bool shouldRepaintCrosshairOnMove(const QString &crosshairStyle);
QRegion crosshairUpdateRegion(const QPoint &previousPosition,
                              const QPoint &currentPosition,
                              const QRect &canvasRect);
QRegion selectionUpdateRegion(const QRect &previousSelection,
                              const QRect &currentSelection,
                              const QRect &canvasRect);
QRegion selectionStartUpdateRegion(const QRect &currentSelection,
                                   const QRect &canvasRect,
                                   const QRect &dismissedUiRect);
SelectionResizeHandle selectionResizeHandleAt(const QRect &selection,
                                              const QPoint &position,
                                              int hitRadius = 10);
QRect resizedSelectionForHandle(const QRect &selection,
                                SelectionResizeHandle handle,
                                const QPoint &position);
SelectionFrameSegments selectionFrameSegments(const QRect &selection, qreal devicePixelRatio);
QPointF selectionFrameHandleCenter(const QRect &selection, SelectionResizeHandle handle,
                                   qreal devicePixelRatio);
QRegion selectionFrameClipRegion(const QRect &selection, const QRect &canvasRect);
SelectionNudge selectionNudgeForKey(int key, Qt::KeyboardModifiers modifiers);
QRect nudgedSelection(const QRect &selection, SelectionNudgeMode mode,
                      const QPoint &delta, const QRect &bounds);
