#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#include <QObject>
#include <QTimer>
#include <QRect>
#include <QImage>
#include <QProcess>
#include <QSize>
#include <QString>
#include <QDateTime>
#include <QElapsedTimer>

#include "RecordingTimeline.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class GifEncoder;

class ScreenRecorder : public QObject {
    Q_OBJECT

public:
    explicit ScreenRecorder(QObject *parent = nullptr);
    ~ScreenRecorder();

    bool isRecording() const { return m_recording || isFinalizing(); }
    bool isPaused() const { return m_paused; }
    bool isFinalizing() const { return m_conversionProcess != nullptr; }
    QRect captureRect() const { return m_displayRect.isValid() ? m_displayRect : m_captureRect; }
    int frameCount() const { return m_frameCount; }
    int maxSeconds() const { return m_maxSeconds; }

    void start(const QRect &captureRect, int fps, int maxSeconds, int loopCount,
               const QString &outputPath, const QRect &displayRect = QRect());
    void stop();
    void cancel();
    void pause();
    void resume();

signals:
    void recordingStarted();
    void recordingStopped(const QString &outputPath);
    void recordingFailed(const QString &reason);
    void frameCaptured(int frameNumber);
    void remainingTimeChanged(int seconds);
    void elapsedTimeChanged(int seconds);
    void pausedChanged(bool paused);
    void timeLimitReached();

private slots:
    void captureFrame();
    void onPortalProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onPortalConversionFinished(int exitCode, QProcess::ExitStatus status);

private:
    void finishRecording();
    bool flushPendingFrame();
    bool framesEqual(const QImage &a, const QImage &b) const;
    QString makeDefaultOutputPath() const;
    QImage grabScreenRegion(const QRect &rect);
    QSize boundedOutputSize(const QSize &sourceSize) const;
    bool initCaptureResources();
    void releaseCaptureResources();
    bool startWaylandPortalRecording(const QRect &captureRect);
    QString gstLaunchPath() const;
    QString ffmpegPath() const;
    bool startPortalVideoToGifConversion();
    void cleanupPortalConversion();
    void closePortalSession();
    bool setPortalProcessSuspended(bool suspended);
    qint64 nowMs() const;

    GifEncoder *m_encoder = nullptr;
    QProcess *m_process = nullptr;
    QProcess *m_conversionProcess = nullptr;
    QTimer *m_conversionTimeout = nullptr;
    QTimer *m_frameTimer = nullptr;
    QTimer *m_countdownTimer = nullptr;
    QRect m_captureRect;
    QRect m_displayRect;
    QSize m_outputSize;
    int m_fps = 10;
    int m_maxSeconds = 0;
    int m_frameCount = 0;
    int m_delayCs = 10;
    bool m_recording = false;
    bool m_portalRecording = false;
    bool m_paused = false;
    bool m_hasPendingFrame = false;
    QImage m_pendingFrame;
    int m_pendingDelayCs = 0;
    qint64 m_lastFrameMs = -1;
    QString m_outputPath;
    QString m_portalVideoPath;
    int m_loopCount = 0;
    QString m_portalSessionHandle;
    RecordingTimeline m_timeline;
    QElapsedTimer m_monotonicClock;

#ifdef Q_OS_WIN
    HDC m_screenDC = nullptr;
    HDC m_memDC = nullptr;
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_oldBitmap = nullptr;
    void *m_bits = nullptr;
#endif
};

#endif
