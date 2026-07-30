#include "RecordingDrawerPolicy.h"

QStringList recordingDrawerFields(RecordingDrawerMode mode)
{
    switch (mode) {
    case RecordingDrawerMode::Gif:
        return {"gifFps", "gifDuration", "gifLoop", "startGif"};
    case RecordingDrawerMode::Video:
        return {"videoFps", "videoQuality", "videoDuration", "desktopAudio", "microphone", "microphoneDevice", "startVideo"};
    case RecordingDrawerMode::None:
        return {};
    }
    return {};
}

QPoint recordingDrawerPosition(const QRect &availableBounds, const QSize &drawerSize)
{
    const int x = availableBounds.x() + qMax(0, (availableBounds.width() - drawerSize.width()) / 2);
    const int y = availableBounds.y() + qMax(0, (availableBounds.height() - drawerSize.height()) / 2);
    return {x, y};
}

OverlayMenuPressAction overlayMenuPressAction(bool recordingOpen,
                                              bool quickSettingsOpen,
                                              bool pressInsideMenu)
{
    if (pressInsideMenu)
        return OverlayMenuPressAction::ConsumeInsideMenu;
    if (recordingOpen)
        return OverlayMenuPressAction::CloseAndConsume;
    if (quickSettingsOpen)
        return OverlayMenuPressAction::CloseAndContinueCapture;
    return OverlayMenuPressAction::ContinueCapture;
}

bool escapeClosesOverlayMenu(bool recordingOpen, bool quickSettingsOpen)
{
    return recordingOpen || quickSettingsOpen;
}
