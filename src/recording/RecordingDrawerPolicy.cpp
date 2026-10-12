#include "RecordingDrawerPolicy.h"

#include <Qt>

QStringList recordingDrawerFields(RecordingDrawerMode mode)
{
    switch (mode) {
    case RecordingDrawerMode::Gif:
        return {"mode", "quality", "duration", "startDelay", "advancedSettings", "hotkeys", "start"};
    case RecordingDrawerMode::Video:
        return {"mode", "quality", "duration", "startDelay", "systemAudio", "microphone",
                "microphoneDevice", "advancedSettings", "hotkeys", "start"};
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

QList<int> recordingDurationChoices()
{
    return {0, 15, 30, 60, 300};
}

QList<int> recordingStartDelayChoices()
{
    return {0, 3, 5, 10};
}

int recordingChoiceIndex(const QList<int> &choices, int value)
{
    return choices.indexOf(value);
}

bool recordingDrawerStartsOnKey(int key, bool drawerOpen)
{
    return drawerOpen && (key == Qt::Key_Return || key == Qt::Key_Enter);
}

QString recordingDrawerModeKey(RecordingDrawerMode mode)
{
    switch (mode) {
    case RecordingDrawerMode::Gif:
        return QStringLiteral("gif");
    case RecordingDrawerMode::Video:
        return QStringLiteral("video");
    case RecordingDrawerMode::None:
        break;
    }
    return QString();
}

RecordingDrawerMode recordingDrawerModeFromKey(const QString &key)
{
    return key.trimmed().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0
        ? RecordingDrawerMode::Gif
        : RecordingDrawerMode::Video;
}

QStringList toolbarControlsWithSingleRecordButton(const QStringList &controls)
{
    const QString gif = QStringLiteral("Gif");
    const QString record = QStringLiteral("Video");
    QStringList result;
    const bool hasRecord = controls.contains(record);
    for (const QString &control : controls) {
        if (control == gif) {
            if (!hasRecord && !result.contains(record))
                result.append(record);
            continue;
        }
        if (!result.contains(control))
            result.append(control);
    }
    return result;
}
