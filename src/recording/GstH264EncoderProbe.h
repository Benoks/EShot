#pragma once

#include "LinuxRecordingSupport.h"

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>

// Finds out in the background which hardware H.264 encoders really work by
// running a tiny gst-launch encode per candidate (see
// gstH264EncoderProbeArguments()). Results are kept in QSettings
// ("recordingH264Probe/*") for the current GStreamer installation and GPU
// drivers, so the probe only runs again after one of them changed.
//
// Nothing here blocks: until the probe has finished, workingHardwareEncoders()
// returns what is known so far (usually nothing), and recordings use the
// software encoder.
class GstH264EncoderProbe : public QObject {
    Q_OBJECT
public:
    static GstH264EncoderProbe *instance();

    // Loads the cached results for this gst-launch and probes the remaining
    // candidates asynchronously. Cheap and idempotent.
    void ensureStarted(const QString &gstLaunchPath);
    // Working hardware encoders in preference order. *finished is false
    // while a probe is still running.
    QStringList workingHardwareEncoders(bool *finished = nullptr) const;
    // A real recording could not start with this encoder: drop it and probe
    // the next candidate.
    void markFailed(const QString &encoder);

    // QSettings key (bool, default true): false forces the software encoder.
    static QString hardwareEncodingSettingsKey();
    static bool hardwareEncodingEnabled();

private:
    explicit GstH264EncoderProbe(QObject *parent = nullptr);
    QString currentSignature(const QString &gstLaunchPath) const;
    void loadState();
    void saveState() const;
    void probeNext();
    void finishProbe(const QString &probedEncoder, bool worked, const QString &detail);

    QString m_gstLaunchPath;
    GstH264ProbeState m_state;
    QPointer<QProcess> m_process;
    QString m_probingEncoder;
    bool m_loaded = false;
};
