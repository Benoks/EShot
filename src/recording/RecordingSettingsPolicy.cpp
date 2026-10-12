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
    // New users record desktop audio only: an always-on microphone would
    // capture room noise and private conversations without being asked for.
    if (!hasLegacyMode)
        return source == RecordingAudioSource::Desktop;

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

namespace {
const char *const GifPresetKey = "gifQualityPreset";
const char *const VideoPresetKey = "videoQualityPreset";
}

QList<RecordingQualityPreset> selectableRecordingQualityPresets()
{
    return {RecordingQualityPreset::Small, RecordingQualityPreset::Balanced,
            RecordingQualityPreset::High};
}

GifQualityValues gifQualityValues(RecordingQualityPreset preset)
{
    switch (preset) {
    case RecordingQualityPreset::Small:
        return {720, 10};
    case RecordingQualityPreset::High:
        return {1920, 15};
    case RecordingQualityPreset::Balanced:
    case RecordingQualityPreset::Custom:
        break;
    }
    return {1280, 12};
}

VideoQualityValues videoQualityValues(RecordingQualityPreset preset)
{
    switch (preset) {
    case RecordingQualityPreset::Small:
        return {30, 30};
    case RecordingQualityPreset::High:
        return {20, 30};
    case RecordingQualityPreset::Balanced:
    case RecordingQualityPreset::Custom:
        break;
    }
    return {24, 30};
}

RecordingQualityPreset gifQualityPresetFor(int maxSide, int fps)
{
    for (RecordingQualityPreset preset : selectableRecordingQualityPresets()) {
        const GifQualityValues values = gifQualityValues(preset);
        if (values.maxSide == maxSide && values.fps == fps)
            return preset;
    }
    return RecordingQualityPreset::Custom;
}

RecordingQualityPreset videoQualityPresetFor(int crf, int fps)
{
    for (RecordingQualityPreset preset : selectableRecordingQualityPresets()) {
        const VideoQualityValues values = videoQualityValues(preset);
        if (values.crf == crf && values.fps == fps)
            return preset;
    }
    return RecordingQualityPreset::Custom;
}

QString recordingQualityPresetKey(RecordingQualityPreset preset)
{
    switch (preset) {
    case RecordingQualityPreset::Small:
        return QStringLiteral("small");
    case RecordingQualityPreset::Balanced:
        return QStringLiteral("balanced");
    case RecordingQualityPreset::High:
        return QStringLiteral("high");
    case RecordingQualityPreset::Custom:
        break;
    }
    return QStringLiteral("custom");
}

RecordingQualityPreset recordingQualityPresetFromKey(const QString &key)
{
    const QString normalized = key.trimmed().toLower();
    for (RecordingQualityPreset preset : selectableRecordingQualityPresets()) {
        if (normalized == recordingQualityPresetKey(preset))
            return preset;
    }
    return RecordingQualityPreset::Custom;
}

namespace {
bool storedPresetIsCustom(QSettings &settings, const char *key)
{
    return settings.contains(QLatin1String(key))
        && recordingQualityPresetFromKey(settings.value(QLatin1String(key)).toString())
               == RecordingQualityPreset::Custom;
}
}

RecordingQualityPreset loadGifQualityPreset(QSettings &settings)
{
    if (storedPresetIsCustom(settings, GifPresetKey))
        return RecordingQualityPreset::Custom;
    const int maxSide = settings.value(QStringLiteral("recordingMaxSide"), DefaultGifMaxSide).toInt();
    int fps = settings.value(QStringLiteral("recordingFps"), DefaultGifFps).toInt();
    if (!settings.contains(QLatin1String(GifPresetKey)) && settings.contains(QStringLiteral("recordingFps"))
        && fps == LegacyDefaultGifFps && maxSide == DefaultGifMaxSide) {
        applyGifQualityPreset(settings, RecordingQualityPreset::Balanced);
        return RecordingQualityPreset::Balanced;
    }
    return gifQualityPresetFor(maxSide, fps);
}

RecordingQualityPreset loadVideoQualityPreset(QSettings &settings)
{
    if (storedPresetIsCustom(settings, VideoPresetKey))
        return RecordingQualityPreset::Custom;
    const int crf = settings.value(QStringLiteral("videoRecordingCrf"), DefaultVideoCrf).toInt();
    const int fps = settings.value(QStringLiteral("videoRecordingFps"), DefaultVideoFps).toInt();
    if (!settings.contains(QLatin1String(VideoPresetKey))
        && (settings.contains(QStringLiteral("videoRecordingCrf"))
            || settings.contains(QStringLiteral("videoRecordingFps")))
        && crf == DefaultVideoCrf && fps > 0 && fps <= DefaultVideoFps) {
        applyVideoQualityPreset(settings, RecordingQualityPreset::Balanced);
        return RecordingQualityPreset::Balanced;
    }
    return videoQualityPresetFor(crf, fps);
}

QList<int> gifMaxSideChoices()
{
    return {480, 640, 720, 960, 1280, 1600, 1920};
}

int videoQualitySliderValue(int crf)
{
    return MinVideoCrf + MaxVideoCrf - qBound(MinVideoCrf, crf, MaxVideoCrf);
}

int videoCrfFromQualitySlider(int sliderValue)
{
    return MinVideoCrf + MaxVideoCrf - qBound(MinVideoCrf, sliderValue, MaxVideoCrf);
}

void applyGifQualityPreset(QSettings &settings, RecordingQualityPreset preset)
{
    settings.setValue(QLatin1String(GifPresetKey), recordingQualityPresetKey(preset));
    if (preset == RecordingQualityPreset::Custom)
        return;
    const GifQualityValues values = gifQualityValues(preset);
    settings.setValue(QStringLiteral("recordingMaxSide"), values.maxSide);
    settings.setValue(QStringLiteral("recordingFps"), values.fps);
}

void applyVideoQualityPreset(QSettings &settings, RecordingQualityPreset preset)
{
    settings.setValue(QLatin1String(VideoPresetKey), recordingQualityPresetKey(preset));
    if (preset == RecordingQualityPreset::Custom)
        return;
    const VideoQualityValues values = videoQualityValues(preset);
    settings.setValue(QStringLiteral("videoRecordingCrf"), values.crf);
    settings.setValue(QStringLiteral("videoRecordingFps"), values.fps);
}
