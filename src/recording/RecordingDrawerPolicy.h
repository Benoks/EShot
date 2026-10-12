#ifndef RECORDINGDRAWERPOLICY_H
#define RECORDINGDRAWERPOLICY_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QList>
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

// Duration limits offered in the drawer, in seconds (0 = unlimited).
QList<int> recordingDurationChoices();
// Start delays offered in the drawer, in seconds (0 = none).
QList<int> recordingStartDelayChoices();
// Position of `value` in `choices`, or -1 when it is a custom value that the
// drawer shows as an extra "Custom (N s)" entry so it is not lost.
int recordingChoiceIndex(const QList<int> &choices, int value);
// Enter/Return starts the recording while the drawer is open.
bool recordingDrawerStartsOnKey(int key, bool drawerOpen);

#endif
