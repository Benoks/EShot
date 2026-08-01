#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStringList>

struct PortalCropGeometry {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    QSize outputSize;
    bool valid = false;
};

QString pipeWireSourcePath(uint nodeId, quint64 pipewireSerial = 0);
PortalCropGeometry portalCropGeometry(const QRect &captureRect,
                                      const QRect &displayRect,
                                      const QPoint &streamPosition,
                                      const QSize &streamDisplaySize,
                                      const QSize &requestedOutputSize);
QSize evenRecordingSize(const QSize &size);
QString preferredGstAacEncoder(const QStringList &availableElements);
QString discoverGstAacEncoder();
QStringList waylandRecordingAudioArguments(bool desktopEnabled, int desktopVolume,
                                           const QString &desktopDevice,
                                           bool microphoneEnabled, int microphoneVolume,
                                           const QString &microphoneDevice,
                                           const QString &aacEncoder);
QList<QPair<QString, QString>> linuxMicrophoneDevices(const QString &pactlSources);
QList<QPair<QString, QString>> discoverLinuxMicrophoneDevices();
