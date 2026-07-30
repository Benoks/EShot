#include "RecordingSettingsPolicy.h"

#include <QSettings>

int gifRecordingFpsLimit()
{
    return 30;
}

int videoRecordingFpsLimit()
{
    return 60;
}

bool initialAudioEnabled(bool hasExplicitSetting,
                         bool explicitEnabled,
                         bool hasLegacyMode,
                         const QString &legacyMode,
                         RecordingAudioSource source)
{
    if (hasExplicitSetting)
        return explicitEnabled;
    if (!hasLegacyMode)
        return true;

    const QString normalizedMode = legacyMode.trimmed().toLower();
    if (normalizedMode == QStringLiteral("both"))
        return true;
    if (source == RecordingAudioSource::Desktop)
        return normalizedMode == QStringLiteral("desktop");
    return normalizedMode == QStringLiteral("microphone");
}

bool loadRecordingAudioEnabled(QSettings &settings, RecordingAudioSource source)
{
    const QString key = source == RecordingAudioSource::Desktop
        ? QStringLiteral("videoDesktopAudioEnabled")
        : QStringLiteral("videoMicrophoneEnabled");
    const bool hasExplicitSetting = settings.contains(key);
    const bool enabled = initialAudioEnabled(
        hasExplicitSetting,
        settings.value(key, false).toBool(),
        settings.contains(QStringLiteral("videoAudioMode")),
        settings.value(QStringLiteral("videoAudioMode")).toString(),
        source);
    if (!hasExplicitSetting)
        settings.setValue(key, enabled);
    return enabled;
}

bool shouldDiscoverAudioDevices(bool microphoneEnabled,
                                const QString &microphoneDevice)
{
    if (!microphoneEnabled)
        return false;
    const QString normalized = microphoneDevice.trimmed();
    return normalized.isEmpty()
        || normalized.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0;
}
