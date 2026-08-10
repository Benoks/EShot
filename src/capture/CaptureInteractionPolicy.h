#pragma once

#include <QRect>
#include <QRegion>
#include <QSize>

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

bool shouldReleaseToolForResize(bool handleHit, int currentTool, int noneTool);
int initialAnnotationTool(bool rememberLastTool, int storedTool, int noneTool);
bool shouldShowCaptureHints(bool enabled, bool selecting, bool selectionComplete,
                            bool eyedropperActive);
QRect captureHintRect(const QRect &monitorRect, const QSize &preferredSize);
int quickSettingsTabHeight(int textWidth, int availableHeight);
bool shouldForwardCaptureKeyFromManagedProxy(bool textEditorVisible);
bool shouldRestoreCaptureKeyboardFocus(bool overlayVisible, bool selectionComplete,
                                       bool textEditorVisible);
bool shouldGrabCaptureKeyboardFromManagedProxy(bool managedProxyAvailable,
                                               bool overlayVisible, bool selectionComplete,
                                               bool textEditorVisible);
bool shouldDetachModalFromOverlay(bool xwaylandOverlay);
bool shouldComposeCaptureResult(bool recordingMode);
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
