#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <QObject>
#include <QElapsedTimer>
#include <QProcess>
#include <QRect>
#include <QString>
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
    bool isFinalizing() const { return m_muxProcess != nullptr; }
    QRect captureRect() const { return m_displayRect.isValid() ? m_displayRect : m_captureRect; }
    int maxSeconds() const { return m_maxSeconds; }

    void start(const QRect &captureRect, int fps, int maxSeconds, int crf,
               bool desktopAudioEnabled, int desktopVolume,
               const QString &desktopAudioDevice,
               bool microphoneEnabled, int microphoneVolume,
               const QString &microphoneDevice,
               const QString &outputPath = QString(),
               const QRect &displayRect = QRect());
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

private:
    QString ffmpegPath() const;
    QString makeDefaultOutputPath() const;
    qint64 activeElapsedMs() const;
    bool setProcessSuspended(bool suspended);
    void cleanupProcess();
    void stopSystemAudioCapture();
    bool startSystemAudioMux();
    void cleanupMuxProcess();
    bool startWaylandPortalRecording(const QRect &captureRect);
    QString gstLaunchPath() const;

    QProcess *m_process = nullptr;
    QProcess *m_muxProcess = nullptr;
    QTimer *m_muxTimeout = nullptr;
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
    int m_lastElapsedSeconds = -1;
    bool m_recording = false;
    bool m_paused = false;
    bool m_stopping = false;
    bool m_canceling = false;
    std::atomic_bool m_audioStop { false };
    std::thread m_audioThread;
};

#endif
