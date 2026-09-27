#ifndef AUDIODEVICES_H
#define AUDIODEVICES_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// Active WASAPI capture endpoints by friendly name (empty outside Windows).
QStringList windowsAudioInputDevices();
// Setting value for the platform's default desktop (loopback) audio source.
QString defaultDesktopAudioDevice();
// Microphones as (display name, device id) pairs.
QList<QPair<QString, QString>> microphoneAudioDevices();
// Audio devices reported by `ffmpeg -list_devices true -f dshow -i dummy`.
QStringList dshowAudioDevices(const QString &ffmpegPath);

#endif
