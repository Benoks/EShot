#pragma once

#include <QRect>
#include <QRegion>
#include <QSize>

bool shouldReleaseToolForResize(bool handleHit, int currentTool, int noneTool);
int initialAnnotationTool(bool rememberLastTool, int storedTool, int noneTool);
bool shouldShowCaptureHints(bool enabled, bool selecting, bool selectionComplete,
                            bool eyedropperActive);
QRect captureHintRect(const QRect &monitorRect, const QSize &preferredSize);
int quickSettingsTabHeight(int textWidth, int availableHeight);
bool shouldForwardCaptureKeyFromManagedProxy(bool textEditorVisible);
bool shouldDetachModalFromOverlay(bool xwaylandOverlay);
bool shouldComposeCaptureResult(bool recordingMode);
QRegion crosshairUpdateRegion(const QPoint &previousPosition,
                              const QPoint &currentPosition,
                              const QRect &canvasRect);
QRegion selectionUpdateRegion(const QRect &previousSelection,
                              const QRect &currentSelection,
                              const QRect &canvasRect);
