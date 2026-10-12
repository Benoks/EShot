#pragma once

#include <QList>
#include <QString>

class QSettings;

enum class RecordingAudioSource
{
    Desktop,
    Microphone
};

int gifRecordingFpsLimit();
int videoRecordingFpsLimit();

bool initialAudioEnabled(bool hasExplicitSetting,
                         bool explicitEnabled,
                         bool hasLegacyMode,
                         const QString &legacyMode,
                         RecordingAudioSource source);

bool loadRecordingAudioEnabled(QSettings &settings, RecordingAudioSource source);
bool shouldDiscoverAudioDevices(bool microphoneEnabled,
                                const QString &microphoneDevice);

// Defaults of the raw recording keys the recorders read.
constexpr int DefaultGifFps = 12;            // "recordingFps"
constexpr int LegacyDefaultGifFps = 10;      // default before quality presets
constexpr int DefaultGifMaxSide = 1280;      // "recordingMaxSide"
constexpr int DefaultGifMaxSeconds = 30;     // "recordingMaxSeconds"
constexpr int DefaultVideoFps = 30;          // "videoRecordingFps"
constexpr int DefaultVideoCrf = 24;          // "videoRecordingCrf"
constexpr int DefaultVideoMaxSeconds = 0;    // "videoRecordingMaxSeconds"
constexpr int MinVideoCrf = 18;
constexpr int MaxVideoCrf = 32;

// Quality presets offered in the recording drawer and in Settings. Choosing
// one writes the raw keys above; Custom means the raw values match no preset.
enum class RecordingQualityPreset
{
    Small,
    Balanced,
    High,
    Custom
};

struct GifQualityValues
{
    int maxSide = DefaultGifMaxSide;
    int fps = DefaultGifFps;
};

struct VideoQualityValues
{
    int crf = DefaultVideoCrf;
    int fps = DefaultVideoFps;
};

// Selectable presets in display order (Custom is never offered for picking).
QList<RecordingQualityPreset> selectableRecordingQualityPresets();
// Raw values of a preset; Custom yields the Balanced values.
GifQualityValues gifQualityValues(RecordingQualityPreset preset);
VideoQualityValues videoQualityValues(RecordingQualityPreset preset);
// Preset whose values are exactly these, otherwise Custom.
RecordingQualityPreset gifQualityPresetFor(int maxSide, int fps);
RecordingQualityPreset videoQualityPresetFor(int crf, int fps);
// Stable setting values ("small", "balanced", "high", "custom").
QString recordingQualityPresetKey(RecordingQualityPreset preset);
RecordingQualityPreset recordingQualityPresetFromKey(const QString &key);

// Current presets derived from the raw keys (which stay authoritative). The
// GIF variant moves untouched pre-preset defaults (1280 px at 10 fps) to
// Balanced once, so existing installs do not start out as "Custom".
RecordingQualityPreset loadGifQualityPreset(QSettings &settings);
RecordingQualityPreset loadVideoQualityPreset(QSettings &settings);
// Write a preset's raw values plus the preset key; Custom only stores the key.
void applyGifQualityPreset(QSettings &settings, RecordingQualityPreset preset);
void applyVideoQualityPreset(QSettings &settings, RecordingQualityPreset preset);
