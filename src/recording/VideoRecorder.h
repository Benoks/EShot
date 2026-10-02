#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include "core/LinuxPortalScreenCast.h"
#include "LinuxRecordingSupport.h"

#include <QObject>
#include <QElapsedTimer>
#include <QProcess>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <thread>

class VideoRecorder : public QObject {
    Q_OBJECT

public:
    explicit VideoRecorder(QObject *parent = nullptr);
    ~VideoRecorder() override;

    bool isRecording() const { return m_recording || isFinalizing(); }
    bool isPaused() const { return m_paused; }
    bool isFinalizing() const { return m_muxProcess != nullptr || m_concatProcess != nullptr; }
    QRect captureRect() const { return m_displayRect.isValid() ? m_displayRect : m_captureRect; }
    int maxSeconds() const { return m_maxSeconds; }
    // Problems that did not stop the recording but changed what was saved
    // (e.g. a dropped audio source); valid once recordingStopped() is emitted.
    QStringList warnings() const { return m_warnings; }

    void start(const QRect &captureRect, int fps, int maxSeconds, int crf,
               bool desktopAudioEnabled, int desktopVolume,
               const QString &desktopAudioDevice,
               bool microphoneEnabled, int microphoneVolume,
               const QString &microphoneDevice,
               const QString &outputPath = QString(),
               const QRect &displayRect = QRect());
    // Opens the Wayland portal source picker before start() so a start delay
    // runs after the user picked the source; start() then reuses that stream.
    // Other platforms have nothing to prepare. On failure recordingFailed()
    // has been emitted.
    bool prepareSource(const QRect &captureRect, const QRect &displayRect = QRect());
    void stop();
    void cancel();
    void pause();
    void resume();

signals:
    void recordingStarted();
    void recordingStopped(const QString &outputPath);
    void recordingFailed(const QString &reason);
    void remainingTimeChanged(int seconds);
    void pausedChanged(bool paused);
    void elapsedTimeChanged(int seconds);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onMuxFinished(int exitCode, QProcess::ExitStatus status);
    void onConcatFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString ffmpegPath() const;
    QString makeDefaultOutputPath() const;
    qint64 activeElapsedMs() const;
    void startCountdown();
    QString segmentTargetPath() const;
    QStringList ffmpegCaptureArguments(const QString &outputPath) const;
    QStringList gstCaptureArguments(int pipewireFd, const QString &outputPath) const;
    bool startFfmpegSegment(const QString &outputPath, QString *error);
    bool startGstSegment(int pipewireFd, const QString &outputPath, QString *error);
    bool startNextSegment(QString *error);
    void requestEncoderStop();
    void finishRecording();
    void finishVideo();
    bool startSegmentConcat();
    void keepSegmentsAfterFailedConcat(const QString &reason);
    void cleanupConcatProcess();
    void cleanupProcess();
    void stopSystemAudioCapture();
    void removeRecordingFiles();
    bool startSystemAudioMux();
    void useVideoWithoutSystemAudio(const QString &warning);
    void cleanupMuxProcess();
    bool startWaylandPortalRecording(const QRect &captureRect);
    QString gstLaunchPath() const;
    void discardPreparedSource();

    QProcess *m_process = nullptr;
    QProcess *m_muxProcess = nullptr;
    QTimer *m_muxTimeout = nullptr;
    QProcess *m_concatProcess = nullptr;
    QTimer *m_concatTimeout = nullptr;
    QTimer *m_countdownTimer = nullptr;
    QRect m_captureRect;
    QRect m_displayRect;
    QString m_outputPath;
    QString m_videoOnlyPath;
    QString m_audioPath;
    QString m_ffmpegPath;
    bool m_usesGStreamer = false;
    QElapsedTimer m_elapsed;
    qint64 m_pausedMs = 0;
    qint64 m_pauseStartedMs = 0;
    qint64 m_recordedMs = 0;
    int m_fps = 30;
    int m_maxSeconds = 0;
    int m_crf = 24;
    bool m_desktopAudioEnabled = false;
    int m_desktopVolume = 80;
    QString m_desktopAudioDevice;
    bool m_systemAudioLoopback = false;
    bool m_microphoneEnabled = false;
    int m_microphoneVolume = 80;
    QString m_microphoneDevice;
    QString m_portalSessionHandle;
    LinuxPortalScreenCast::Stream m_preparedStream;
    QString m_gstPath;
    QString m_portalSourcePath;
    PortalCropGeometry m_portalCrop;
    QString m_gstAacEncoder;
    bool m_gstHasAudio = false;
    // One file per active span; m_segmentPath is the one being written.
    QStringList m_segmentPaths;
    QString m_segmentPath;
    QString m_segmentDirectory;
    QString m_segmentStamp;
    QString m_concatListPath;
    int m_segmentCount = 0;
    QStringList m_warnings;
    int m_lastElapsedSeconds = -1;
    bool m_recording = false;
    bool m_paused = false;
    bool m_resumePending = false;
    bool m_stopping = false;
    bool m_canceling = false;
    std::atomic_bool m_audioStop { false };
    std::atomic_bool m_audioPaused { false };
    std::thread m_audioThread;
};

#endif
