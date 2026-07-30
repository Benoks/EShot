#ifndef RECORDINGDRAWERPOLICY_H
#define RECORDINGDRAWERPOLICY_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStringList>

enum class RecordingDrawerMode {
    None,
    Gif,
    Video
};

enum class OverlayMenuPressAction {
    ContinueCapture,
    CloseAndContinueCapture,
    CloseAndConsume,
    ConsumeInsideMenu
};

QStringList recordingDrawerFields(RecordingDrawerMode mode);
QPoint recordingDrawerPosition(const QRect &availableBounds, const QSize &drawerSize);
OverlayMenuPressAction overlayMenuPressAction(bool recordingOpen,
                                              bool quickSettingsOpen,
                                              bool pressInsideMenu);
bool escapeClosesOverlayMenu(bool recordingOpen, bool quickSettingsOpen);

#endif
