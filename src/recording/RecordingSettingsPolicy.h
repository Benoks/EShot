#pragma once

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
