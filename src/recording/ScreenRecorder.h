#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#include "core/LinuxPortalScreenCast.h"

#include <QObject>
#include <QTimer>
#include <QRect>
#include <QImage>
#include <QProcess>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QElapsedTimer>

#include "LinuxRecordingSupport.h"
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
    void frameCaptured(int frameNumber);
    void remainingTimeChanged(int seconds);
    void elapsedTimeChanged(int seconds);
    void pausedChanged(bool paused);

private slots:
    void captureFrame();
    void onPortalProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onPortalConversionFinished(int exitCode, QProcess::ExitStatus status);

private:
    enum class ConversionStage { Concat, Palette, Encode };

    void finishRecording();
    bool flushPendingFrame(int delayCs);
    int finalFrameDelayCs();
    bool framesEqual(const QImage &a, const QImage &b) const;
    QString makeDefaultOutputPath() const;
    QImage grabScreenRegion(const QRect &rect);
    QSize boundedOutputSize(const QSize &sourceSize) const;
    bool initCaptureResources();
    void releaseCaptureResources();
    bool startWaylandPortalRecording(const QRect &captureRect);
    QString gstLaunchPath() const;
    void discardPreparedSource();
    QString ffmpegPath() const;
    bool startPortalVideoToGifConversion();
    void cleanupPortalConversion();
    void closePortalSession();
    QStringList portalCaptureArguments(int pipewireFd, const QString &outputPath) const;
    bool startPortalSegment(int pipewireFd, const QString &outputPath, QString *error);
    bool startNextPortalSegment(QString *error);
    void requestPortalProcessStop();
    void finishPortalRecording();
    void startPortalConversionStage(ConversionStage stage, const QStringList &arguments, int timeoutMs);
    void failPortalConversion(const QString &reason);
    void removePortalIntermediates();
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
    bool m_recording = false;
    bool m_portalRecording = false;
    bool m_paused = false;
    bool m_stopping = false;
    bool m_hasPendingFrame = false;
    QImage m_pendingFrame;
    GifFrameClock m_gifClock;
    qint64 m_lastFrameMs = -1;
    QString m_outputPath;
    QString m_portalVideoPath;
    // Wayland portal recordings write one file per active span (see
    // RecordingSegments.h); m_portalSegmentPath is the one being written.
    QStringList m_portalSegments;
    QString m_portalSegmentPath;
    QString m_portalSegmentStamp;
    QString m_portalConcatListPath;
    QString m_portalPalettePath;
    QString m_gstPath;
    QString m_portalSourcePath;
    PortalCropGeometry m_portalCrop;
    QString m_conversionFfmpeg;
    ConversionStage m_conversionStage = ConversionStage::Encode;
    int m_portalSegmentCount = 0;
    bool m_portalResumePending = false;
    qint64 m_recordedMs = 0;
    int m_loopCount = 0;
    QString m_portalSessionHandle;
    LinuxPortalScreenCast::Stream m_preparedStream;
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
