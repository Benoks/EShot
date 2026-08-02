#include "CaptureInteractionPolicy.h"

bool shouldReleaseToolForResize(bool handleHit, int currentTool, int noneTool)
{
    return handleHit && currentTool != noneTool;
}

int initialAnnotationTool(bool rememberLastTool, int storedTool, int noneTool)
{
    return rememberLastTool ? storedTool : noneTool;
}

bool shouldShowCaptureHints(bool enabled, bool selecting, bool selectionComplete,
                            bool eyedropperActive)
{
    return enabled && !selecting && !selectionComplete && !eyedropperActive;
}

QRect captureHintRect(const QRect &monitorRect, const QSize &preferredSize)
{
    if (!monitorRect.isValid() || preferredSize.isEmpty())
        return {};

    constexpr int HorizontalMargin = 16;
    constexpr int TopMargin = 24;
    const int width = qMin(preferredSize.width(),
                           qMax(0, monitorRect.width() - HorizontalMargin * 2));
    const int height = qMin(preferredSize.height(), monitorRect.height());
    const int x = monitorRect.left() + (monitorRect.width() - width) / 2;
    const int y = monitorRect.top() + qMin(TopMargin, qMax(0, monitorRect.height() - height));
    return QRect(x, y, width, height);
}

int quickSettingsTabHeight(int textWidth, int availableHeight)
{
    constexpr int MinimumHeight = 118;
    constexpr int TextPadding = 32;
    constexpr int ScreenMargins = 64;
    const int preferredHeight = qMax(MinimumHeight, qMax(0, textWidth) + TextPadding);
    const int maximumHeight = qMax(28, availableHeight - ScreenMargins);
    return qMin(preferredHeight, maximumHeight);
}

bool shouldForwardCaptureKeyFromManagedProxy(bool textEditorVisible)
{
    return !textEditorVisible;
}

bool shouldDetachModalFromOverlay(bool xwaylandOverlay)
{
    return xwaylandOverlay;
}

bool shouldComposeCaptureResult(bool recordingMode)
{
    return !recordingMode;
}

QRegion crosshairUpdateRegion(const QPoint &previousPosition,
                              const QPoint &currentPosition,
                              const QRect &canvasRect)
{
    if (!canvasRect.isValid())
        return {};

    constexpr int CrosshairRepaintMargin = 2;
    const int lineExtent = CrosshairRepaintMargin * 2 + 1;
    QRegion dirty;
    const QPoint positions[] = {previousPosition, currentPosition};
    for (const QPoint &position : positions) {
        dirty += QRect(position.x() - CrosshairRepaintMargin,
                       canvasRect.top(), lineExtent, canvasRect.height())
                     .intersected(canvasRect);
        dirty += QRect(canvasRect.left(),
                       position.y() - CrosshairRepaintMargin,
                       canvasRect.width(), lineExtent)
                     .intersected(canvasRect);
    }
    return dirty;
}

QRegion selectionUpdateRegion(const QRect &previousSelection,
                              const QRect &currentSelection,
                              const QRect &canvasRect)
{
    if (!canvasRect.isValid())
        return {};

    auto clippedSelection = [&canvasRect](const QRect &selection) {
        return selection.normalized().intersected(canvasRect);
    };
    auto outline = [](const QRect &rect) {
        if (!rect.isValid())
            return QRegion();
        // The selection frame is 2 px wide and its corner handles extend
        // 5 px beyond the rectangle. Keep an extra pixel for rasterization.
        constexpr int BorderMargin = 7;
        const QRect outer = rect.adjusted(-BorderMargin, -BorderMargin,
                                          BorderMargin, BorderMargin);
        const QRect inner = rect.adjusted(BorderMargin, BorderMargin,
                                          -BorderMargin, -BorderMargin);
        return QRegion(outer) - QRegion(inner);
    };
    auto sizeLabelArea = [&canvasRect](const QRect &rect) {
        if (!rect.isValid())
            return QRect();
        constexpr int LabelWidth = 200;
        constexpr int LabelHeight = 52;
        const int labelY = rect.top() - LabelHeight >= canvasRect.top()
            ? rect.top() - LabelHeight : rect.top();
        return QRect(rect.left() - 4, labelY, LabelWidth, LabelHeight)
            .intersected(canvasRect);
    };

    const QRect previous = clippedSelection(previousSelection);
    const QRect current = clippedSelection(currentSelection);
    QRegion dirty = QRegion(previous).xored(QRegion(current));
    dirty += outline(previous).intersected(canvasRect);
    dirty += outline(current).intersected(canvasRect);
    dirty += sizeLabelArea(previous);
    dirty += sizeLabelArea(current);
    return dirty;
}

QRegion selectionStartUpdateRegion(const QRect &currentSelection,
                                   const QRect &canvasRect,
                                   const QRect &dismissedUiRect)
{
    QRegion dirty = selectionUpdateRegion(QRect(), currentSelection, canvasRect);
    dirty += dismissedUiRect.intersected(canvasRect);
    return dirty;
}

SelectionResizeHandle selectionResizeHandleAt(const QRect &selection,
                                              const QPoint &position,
                                              int hitRadius)
{
    const QRect rect = selection.normalized();
    if (!rect.isValid())
        return SelectionResizeHandle::NewSelection;

    const auto isNear = [position](const QPoint &handle, int radius) {
        return QLineF(position, handle).length() <= radius;
    };

    if (isNear(rect.topLeft(), hitRadius))
        return SelectionResizeHandle::TopLeft;
    if (isNear(rect.topRight(), hitRadius))
        return SelectionResizeHandle::TopRight;
    if (isNear(rect.bottomRight(), hitRadius))
        return SelectionResizeHandle::BottomRight;
    if (isNear(rect.bottomLeft(), hitRadius))
        return SelectionResizeHandle::BottomLeft;
    constexpr int EdgeHandleExtraHitRadius = 4;
    const int edgeHitRadius = hitRadius + EdgeHandleExtraHitRadius;
    if (isNear(QPoint(rect.center().x(), rect.top()), edgeHitRadius))
        return SelectionResizeHandle::Top;
    if (isNear(QPoint(rect.right(), rect.center().y()), edgeHitRadius))
        return SelectionResizeHandle::Right;
    if (isNear(QPoint(rect.center().x(), rect.bottom()), edgeHitRadius))
        return SelectionResizeHandle::Bottom;
    if (isNear(QPoint(rect.left(), rect.center().y()), edgeHitRadius))
        return SelectionResizeHandle::Left;
    if (rect.contains(position))
        return SelectionResizeHandle::Move;
    return SelectionResizeHandle::NewSelection;
}

QRect resizedSelectionForHandle(const QRect &selection,
                                SelectionResizeHandle handle,
                                const QPoint &position)
{
    QRect result = selection.normalized();
    switch (handle) {
    case SelectionResizeHandle::Top:
        result.setTop(position.y());
        break;
    case SelectionResizeHandle::Right:
        result.setRight(position.x());
        break;
    case SelectionResizeHandle::Bottom:
        result.setBottom(position.y());
        break;
    case SelectionResizeHandle::Left:
        result.setLeft(position.x());
        break;
    default:
        break;
    }
    return result.normalized();
}

QPen selectionFramePen()
{
    QPen pen(QColor(0, 120, 215), 2);
    pen.setCosmetic(true);
    return pen;
}
