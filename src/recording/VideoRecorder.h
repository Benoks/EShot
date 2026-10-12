#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include "core/LinuxPortalScreenCast.h"
#include "LinuxRecordingSupport.h"
#include "RecordingControlLayout.h"
#include "WindowsRecordingSupport.h"

#include <QObject>
#include <QElapsedTimer>
#include <QList>
#include <QPair>
#include <QProcess>
#include <QRect>
#include <QSharedPointer>
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

    // Also true while the encoder is starting up (before recordingStarted()).
    bool isRecording() const { return m_recording || m_starting || isFinalizing(); }
    bool isPaused() const { return m_paused; }
    bool isFinalizing() const { return m_muxProcess != nullptr || m_concatProcess != nullptr; }
    QRect captureRect() const { return m_displayRect.isValid() ? m_displayRect : m_captureRect; }
    int maxSeconds() const { return m_maxSeconds; }
    // Problems that did not stop the recording but changed what was saved
    // (e.g. a dropped audio source); valid once recordingStopped() is emitted.
    QStringList warnings() const { return m_warnings; }
    // How the running recording captures the screen (for the indicator's
    // capture exclusion); valid once recordingStarted() is emitted.
    RecordingCaptureBackend captureBackend() const;

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
    void onPortalSessionClosed();

private:
    QString ffmpegPath() const;
    QString makeDefaultOutputPath() const;
    qint64 activeElapsedMs() const;
    void startCountdown();
    QString segmentTargetPath() const;
    QStringList ffmpegCaptureArguments(const QString &outputPath) const;
    QStringList gstCaptureArguments(int pipewireFd, const QString &outputPath) const;
    bool startFfmpegSegment(const QString &outputPath, QString *error);
    bool startGstSegment(const QSharedPointer<int> &pipewireFd, const QString &outputPath,
                         QString *error);
    void onGstSegmentPlaying();
    void handleGstSegmentEnded(int exitCode, QProcess::ExitStatus status, const QString &launchError);
    void resumePortalRecording();
    void restartPortalWithSoftwareEncoder();
    QString portalPreflightError(bool includeAudio);
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
    // Windows backend: resolves the dshow inputs, starts the first segment
    // and waits (asynchronously) for FFmpeg to report progress.
    void resolveWindowsAudioInputs();
    bool startWindowsCapture(QString *error);
    void readFfmpegOutput(QProcess *process);
    void handleFfmpegLine(const QString &line);
    void markCaptureStarted();
    void handleWindowsStartupFailure(QString reason, const QString &segment);
    void clearStartupState();

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
    // `audio=` names passed to dshow (Windows); empty when not recorded.
    QString m_desktopDshowInput;
    QString m_microphoneDshowInput;
    bool m_starting = false;
    bool m_stopAfterStart = false;
    bool m_startupRetried = false;
    // Windows: capture source and encoder of this recording (all segments).
    WindowsVideoPlan m_windowsPlan;
    // Windows: last out_time_us of the running segment, and per started
    // segment whether it was kept and how long it is (-1: unknown). Used
    // to line the system audio up with each segment.
    qint64 m_segmentOutTimeUs = -1;
    struct SegmentRecord {
        bool kept = false;
        qint64 durationMs = -1;
    };
    QList<SegmentRecord> m_segmentRecords;
    // Written by the WASAPI thread, read after it was joined: sample rate
    // and the active spans of the WAV in sample frames.
    int m_audioSampleRate = 0;
    QList<QPair<qint64, qint64>> m_audioSpans;
    QTimer *m_startupTimer = nullptr;
    QByteArray m_ffmpegOutputBuffer;
    QStringList m_ffmpegLogTail;
    qint64 m_videoStartEpochMs = -1;
    std::atomic<qint64> m_audioStartEpochMs { -1 };
    QString m_portalSessionHandle;
    LinuxPortalScreenCast::Stream m_preparedStream;
    QString m_gstPath;
    QString m_portalSourcePath;
    PortalCropGeometry m_portalCrop;
    QString m_gstAacEncoder;
    // Encoder of this recording (kept for every pause/resume segment so the
    // segments join) and the software fallback found by the preflight.
    QString m_gstH264Encoder = QStringLiteral("x264enc");
    QString m_gstSoftwareH264Encoder = QStringLiteral("x264enc");
    bool m_gstEncoderRetried = false;
    bool m_gstHasAudio = false;
    // Portal segment state: launch-to-exit clock, the output read so far
    // and whether gst-launch reported PLAYING.
    QElapsedTimer m_segmentRunClock;
    QByteArray m_processLog;
    QSharedPointer<int> m_segmentRemoteFd;
    bool m_segmentPlaying = false;
    // First segment launched but not running yet / resumed segment likewise.
    bool m_gstStarting = false;
    bool m_resuming = false;
    int m_startGeneration = 0;
    LinuxPortalSessionWatcher *m_sessionWatcher = nullptr;
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
