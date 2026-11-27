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

QPoint recordingBarPosition(const QRect &selection, const QSize &barSize, const QRect &bounds, int gap)
{
    // Centred under the selection like the capture toolbar; above it when
    // there is no room below, inside its bottom edge when neither fits.
    int x = selection.center().x() - barSize.width() / 2;
    x = qBound(bounds.left(), x, qMax(bounds.left(), bounds.right() + 1 - barSize.width()));
    int y = selection.bottom() + 1 + gap;
    if (y + barSize.height() > bounds.bottom() + 1)
        y = selection.top() - gap - barSize.height();
    if (y < bounds.top())
        y = selection.bottom() + 1 - gap - barSize.height();
    y = qBound(bounds.top(), y, qMax(bounds.top(), bounds.bottom() + 1 - barSize.height()));
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

bool recordingMicrophoneDeviceMissing(const QString &savedDevice,
                                      const QList<QPair<QString, QString>> &devices)
{
    const QString device = savedDevice.trimmed();
    if (device.isEmpty()
        || device.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0
        || device == QStringLiteral("@DEFAULT_SOURCE@"))
        return false;
    for (const auto &entry : devices) {
        if (entry.second.compare(device, Qt::CaseInsensitive) == 0)
            return false;
    }
    return true;
}
