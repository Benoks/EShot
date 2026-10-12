#include "ScreenRecorder.h"
#include "GifEncoder.h"
#include "core/TranslationManager.h"
#include "core/ComponentPaths.h"
#include "core/LinuxPortalScreenCast.h"
#include "PortalRecordingSource.h"
#include "LinuxRecordingSupport.h"
#include "RecordingSettingsPolicy.h"
#include "RecordingFinalizationPolicy.h"
#include "RecordingSegments.h"
#include "VideoRecordingCompletionPolicy.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QPainter>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QPointer>
#include <QProcessEnvironment>
#include <QSharedPointer>
#include <QStringList>
#include <cstring>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <fcntl.h>
#include <csignal>
#include <sys/prctl.h>
#include <sys/types.h>
#endif

namespace {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
bool configurePipeWireRemote(QProcess *process, int fd)
{
    if (!process || fd < 0)
        return false;

    const int flags = fcntl(fd, F_GETFD);
    if (flags < 0)
        return false;
    if (fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) < 0)
        return false;

    // ponytail: leave the portal PipeWire fd inheritable; add fd allowlisting
    // only if a supported Qt floor gives us it on every Linux target.
    return true;
}
#endif
}

ScreenRecorder::ScreenRecorder(QObject *parent) : QObject(parent)
{
    m_monotonicClock.start();
}

ScreenRecorder::~ScreenRecorder()
{
    discardPreparedSource();
    if (isRecording()) cancel();
    cleanupPortalConversion();
}

void ScreenRecorder::start(const QRect &captureRect, int fps, int maxSeconds, int loopCount,
                           const QString &outputPath, const QRect &displayRect)
{
    if (isRecording()) {
        emit recordingFailed(QStringLiteral("already recording"));
        return;
    }
    if (captureRect.width() < 8 || captureRect.height() < 8) {
        emit recordingFailed(QStringLiteral("region too small"));
        return;
    }
    if (fps < 1) fps = 1;
    if (fps > gifRecordingFpsLimit()) fps = gifRecordingFpsLimit();
    if (maxSeconds < 0) maxSeconds = 0;

    m_captureRect = captureRect;
    m_displayRect = displayRect;
    m_outputSize = boundedOutputSize(captureRect.size());
    m_fps = fps;
    m_maxSeconds = maxSeconds;
    m_frameCount = 0;
    m_lastFrameMs = -1;
    m_outputPath = outputPath;
    m_loopCount = loopCount;
    m_portalVideoPath.clear();
    m_portalSegments.clear();
    m_portalSegmentPath.clear();
    m_portalConcatListPath.clear();
    m_portalPalettePath.clear();
    m_portalSegmentCount = 0;
    m_portalResumePending = false;
    m_recordedMs = 0;
    closePortalSession();
    m_paused = false;
    m_stopping = false;
    m_hasPendingFrame = false;
    m_pendingFrame = QImage();

    if (m_outputPath.isEmpty()) {
        m_outputPath = makeDefaultOutputPath();
    }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (LinuxPortalScreenCast::isWaylandSession()) {
        startWaylandPortalRecording(captureRect);
        return;
    }
#endif

    if (!initCaptureResources()) {
        emit recordingFailed(QStringLiteral("cannot initialize screen capture"));
        return;
    }

    m_encoder = new GifEncoder(this);
    if (!m_encoder->open(m_outputPath, m_outputSize.width(), m_outputSize.height(), loopCount)) {
        QString err = m_encoder->errorString();
        delete m_encoder;
        m_encoder = nullptr;
        releaseCaptureResources();
        emit recordingFailed(err);
        return;
    }

    m_recording = true;
    m_timeline.start(nowMs());
    emit recordingStarted();
    emit remainingTimeChanged(m_maxSeconds > 0 ? m_maxSeconds : -1);
    emit elapsedTimeChanged(0);

    m_frameTimer = new QTimer(this);
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    m_frameTimer->setInterval(qMax(1, 1000 / m_fps));
    connect(m_frameTimer, &QTimer::timeout, this, &ScreenRecorder::captureFrame);
    m_frameTimer->start();

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        const int elapsedSeconds = static_cast<int>(m_timeline.activeElapsedMs(nowMs()) / 1000);
        emit elapsedTimeChanged(elapsedSeconds);
        if (m_maxSeconds <= 0) return;
        int remaining = qMax(0, m_maxSeconds - elapsedSeconds);
        emit remainingTimeChanged(remaining);
        if (remaining == 0)
            finishRecording();
    });
    m_countdownTimer->start();

    QTimer::singleShot(qMin(250, m_frameTimer->interval()), this, &ScreenRecorder::captureFrame);
}

QString ScreenRecorder::makeDefaultOutputPath() const
{
    QSettings s("EShot", "EShot");
    QStringList candidates;
    QString configuredDir = s.contains("gifSavePath")
        ? s.value("gifSavePath").toString().trimmed()
        : QDir(ComponentPaths::defaultSaveDirectory()).filePath(QStringLiteral("GIFs"));
    if (configuredDir.isEmpty())
        configuredDir = s.value("savePath").toString().trimmed();
    if (!configuredDir.isEmpty()) {
        candidates << configuredDir;
    } else {
        candidates << ComponentPaths::defaultSaveDirectory();
    }
    candidates << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
               << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
               << QStandardPaths::writableLocation(QStandardPaths::TempLocation)
               << QDir::homePath();

    const QString fileName = QStringLiteral("EShot_GIF_%1.gif")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    // Names have one-second resolution; never overwrite an earlier GIF.
    const auto fileExists = [](const QString &path) { return QFileInfo::exists(path); };

    for (const QString &candidate : candidates) {
        if (candidate.trimmed().isEmpty()) {
            continue;
        }

        QDir dir(candidate);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            continue;
        }

        const QString probePath = dir.filePath(QStringLiteral(".eshot_write_test.tmp"));
        QFile probe(probePath);
        if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            continue;
        }
        probe.close();
        QFile::remove(probePath);
        return uniqueRecordingOutputPath(dir.filePath(fileName), fileExists);
    }

    return uniqueRecordingOutputPath(QDir(QDir::tempPath()).filePath(fileName), fileExists);
}

void ScreenRecorder::stop()
{
    if (!m_recording) return;
    if (m_portalRecording) {
        // A second SIGINT (repeated Stop, or the countdown ticking at 0)
        // aborts gst-launch while it is still finalizing the MP4.
        if (m_stopping)
            return;
        m_stopping = true;
        m_portalResumePending = false;
        if (m_countdownTimer)
            m_countdownTimer->stop();
        if (m_process) {
            // While paused the current segment is already being closed.
            requestPortalProcessStop();
            return;
        }
        finishPortalRecording();
        return;
    }
    if (m_paused)
        resume();
    finishRecording();
}

void ScreenRecorder::cancel()
{
    if (!m_recording && !isFinalizing()) return;
    m_recording = false;
    m_portalRecording = false;
    m_paused = false;
    m_stopping = false;
    m_portalResumePending = false;
    if (m_process) { m_process->kill(); m_process->deleteLater(); m_process = nullptr; }
    cleanupPortalConversion();
    if (m_frameTimer)     { m_frameTimer->stop();     m_frameTimer->deleteLater();     m_frameTimer = nullptr; }
    if (m_countdownTimer) { m_countdownTimer->stop(); m_countdownTimer->deleteLater(); m_countdownTimer = nullptr; }
    if (m_encoder)        { delete m_encoder;         m_encoder = nullptr; }
    releaseCaptureResources();
    m_hasPendingFrame = false;
    m_pendingFrame = QImage();
    if (!m_outputPath.isEmpty() && QFile::exists(m_outputPath)) {
        QFile::remove(m_outputPath);
    }
    if (!m_portalVideoPath.isEmpty()) QFile::remove(m_portalVideoPath);
    removePortalIntermediates();
    closePortalSession();
}

void ScreenRecorder::pause()
{
    // Suspending the encoder while it finalizes would corrupt the file.
    if (!m_recording || m_paused || m_stopping)
        return;
    // gst-launch timestamps frames by wall clock, so suspending it would keep
    // the paused time as a frozen frame. Close the current segment instead;
    // resume() starts the next one.
    if (m_portalRecording && (!m_process || m_stopping || m_portalStarting))
        return;
    if (!m_timeline.pause(nowMs()))
        return;
    m_paused = true;
    if (m_portalRecording)
        requestPortalProcessStop();
    else if (m_frameTimer)
        m_frameTimer->stop();
    if (m_countdownTimer)
        m_countdownTimer->stop();
    emit pausedChanged(true);
}

void ScreenRecorder::resume()
{
    if (!m_recording || !m_paused)
        return;
    if (m_portalRecording) {
        if (m_stopping)
            return;
        resumePortalRecording();
        return;
    }
    if (!m_timeline.resume(nowMs()))
        return;
    m_paused = false;
    if (!m_portalRecording && m_frameTimer)
        m_frameTimer->start();
    if (m_countdownTimer)
        m_countdownTimer->start();
    emit pausedChanged(false);
}

void ScreenRecorder::finishRecording()
{
    if (!m_recording) return;
    m_recording = false;
    m_paused = false;
    if (m_frameTimer)     { m_frameTimer->stop();     m_frameTimer->deleteLater();     m_frameTimer = nullptr; }
    if (m_countdownTimer) { m_countdownTimer->stop(); m_countdownTimer->deleteLater(); m_countdownTimer = nullptr; }
    if (!m_hasPendingFrame) {
        QImage frame = grabScreenRegion(m_captureRect);
        if (!frame.isNull()) {
            m_pendingFrame = frame;
            m_hasPendingFrame = true;
            m_lastFrameMs = m_timeline.activeElapsedMs(nowMs());
            m_gifClock.start(m_lastFrameMs);
        }
    }
    const bool hasCapturedFrame = m_frameCount > 0 || m_hasPendingFrame;
    releaseCaptureResources();

    QString savedPath = m_outputPath;
    bool ok = false;
    if (m_encoder) {
        ok = flushPendingFrame(finalFrameDelayCs()) && m_encoder->close();
        QString err = m_encoder->errorString();
        delete m_encoder;
        m_encoder = nullptr;
        m_hasPendingFrame = false;
        m_pendingFrame = QImage();
        if (!ok) {
            if (QFile::exists(savedPath)) QFile::remove(savedPath);
            emit recordingFailed(err);
            return;
        }
    }
    if (!hasCapturedFrame) {
        if (QFile::exists(savedPath))
            QFile::remove(savedPath);
        emit recordingFailed(QStringLiteral("no frames captured"));
        return;
    }
    emit recordingStopped(savedPath);
}

void ScreenRecorder::onPortalProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    if (!process || process != m_process)
        return;
    handlePortalSegmentEnded(exitCode, status, QString());
}

void ScreenRecorder::handlePortalSegmentEnded(int exitCode, QProcess::ExitStatus status,
                                              const QString &launchError)
{
    QProcess *process = m_process;
    if (!process)
        return;
    m_portalProcessLog += process->readAll();
    const QString stderrText = launchError.isEmpty()
        ? gstFailureReason(QString::fromLocal8Bit(m_portalProcessLog)) : launchError;
    const qint64 runMs = m_portalSegmentClock.isValid() ? m_portalSegmentClock.elapsed() : -1;
    const bool reachedPlaying = m_portalSegmentPlaying;
    const QString segment = m_portalSegmentPath;
    disconnect(process, nullptr, this, nullptr);
    process->deleteLater();
    m_process = nullptr;
    m_portalSegmentPath.clear();
    m_portalRemoteFd.reset();
    m_portalSegmentClock.invalidate();
    m_portalProcessLog.clear();
    m_portalSegmentPlaying = false;

    const bool firstSegment = m_portalStarting;
    const bool resumeSegment = m_portalResuming;
    // gst-launch may report the SIGINT sent by stop() or pause() as its exit
    // status once the MP4 is finalized; the conversion validates it either way.
    const bool expectedStop = m_stopping || (m_paused && !resumeSegment);
    const bool succeeded = videoRecordingProcessSucceeded(true, expectedStop, status, exitCode,
                                                          QFileInfo(segment).size());
    const bool startupFailure = !expectedStop
        && (!reachedPlaying || recordingProcessFailedAtStartup(runMs, expectedStop));
    const RecordingSegmentDisposition disposition = recordingSegmentDisposition(
        succeeded, startupFailure, mp4FileLooksPlayable(segment));
    switch (disposition) {
    case RecordingSegmentDisposition::Keep:
        m_portalSegments.append(segment);
        break;
    case RecordingSegmentDisposition::Salvage:
        // A crashed or killed encoder still leaves the fragments written so
        // far; convert those instead of losing the recording.
        m_portalSegments.append(segment);
        qWarning() << "ScreenRecorder: kept the playable part of an interrupted segment" << segment << stderrText;
        break;
    case RecordingSegmentDisposition::Drop:
        QFile::remove(segment);
        qWarning() << "ScreenRecorder: dropped unusable recording segment" << segment << stderrText;
        break;
    }

    if (resumeSegment) {
        m_portalResuming = false;
        if (!expectedStop)
            qWarning() << "ScreenRecorder: cannot resume portal recording:" << stderrText;
        m_stopping = true;
        finishPortalRecording();
        return;
    }

    if (m_paused && !m_stopping) {
        if (m_portalResumePending) {
            m_portalResumePending = false;
            resume();
        }
        return;
    }

    if (m_portalSegments.isEmpty()) {
        m_recording = false;
        m_portalRecording = false;
        m_paused = false;
        m_stopping = false;
        closePortalSession();
        if (m_countdownTimer) { m_countdownTimer->stop(); m_countdownTimer->deleteLater(); m_countdownTimer = nullptr; }
        if (QFileInfo::exists(m_outputPath))
            QFile::remove(m_outputPath);
        removePortalIntermediates();
        QString reason = stderrText;
        if (reason.isEmpty()) {
            reason = firstSegment ? QStringLiteral("gstreamer pipeline exited during startup")
                                  : QStringLiteral("gstreamer exited with code %1").arg(exitCode);
        }
        emit recordingFailed(reason);
        return;
    }
    finishPortalRecording();
}

// Called once the last portal segment is closed: converts the recorded
// segments to the GIF.
void ScreenRecorder::finishPortalRecording()
{
    m_recordedMs = m_timeline.activeElapsedMs(nowMs());
    m_recording = false;
    m_portalRecording = false;
    m_paused = false;
    m_stopping = false;
    m_portalResumePending = false;
    closePortalSession();
    if (m_countdownTimer) { m_countdownTimer->stop(); m_countdownTimer->deleteLater(); m_countdownTimer = nullptr; }

    if (m_portalSegments.isEmpty()) {
        removePortalIntermediates();
        emit recordingFailed(QStringLiteral("no frames captured"));
        return;
    }
    if (!startPortalVideoToGifConversion())
        failPortalConversion(QStringLiteral("ffmpeg not found"));
}

void ScreenRecorder::onPortalConversionFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_conversionProcess)
        return;

    const QString reason = QString::fromLocal8Bit(
        m_conversionProcess->readAllStandardError()).trimmed();
    const bool timedOut = m_conversionProcess->property("eshotTimedOut").toBool();
    const bool normalExit = !timedOut && status == QProcess::NormalExit;
    const ConversionStage stage = m_conversionStage;
    cleanupPortalConversion();

    QString failure;
    if (timedOut)
        failure = TranslationManager::tr("gifConversionTimedOut");
    else if (!reason.isEmpty())
        failure = reason;
    else
        failure = QStringLiteral("failed to convert portal video to GIF");

    switch (stage) {
    case ConversionStage::Concat:
        if (!normalExit || exitCode != 0 || QFileInfo(m_portalVideoPath).size() <= 0) {
            failPortalConversion(failure);
            return;
        }
        for (const QString &segment : std::as_const(m_portalSegments))
            QFile::remove(segment);
        m_portalSegments = {m_portalVideoPath};
        QFile::remove(m_portalConcatListPath);
        m_portalConcatListPath.clear();
        startPortalConversionStage(ConversionStage::Palette,
                                   portalGifPaletteArguments(m_portalVideoPath, m_portalPalettePath, m_fps,
                                                             m_portalGifScale),
                                   portalGifPassTimeoutMs(m_recordedMs));
        return;
    case ConversionStage::Palette:
        if (!normalExit || exitCode != 0 || QFileInfo(m_portalPalettePath).size() <= 0) {
            failPortalConversion(failure);
            return;
        }
        startPortalConversionStage(ConversionStage::Encode,
                                   portalGifEncodeArguments(m_portalVideoPath, m_portalPalettePath,
                                                            m_fps, m_loopCount, m_outputPath,
                                                            m_portalGifScale),
                                   portalGifPassTimeoutMs(m_recordedMs));
        return;
    case ConversionStage::Encode:
        break;
    }

    if (!portalGifConversionSucceeded(normalExit, exitCode, QFileInfo(m_outputPath).size())) {
        failPortalConversion(failure);
        return;
    }
    const QString output = m_outputPath;
    QFile::remove(m_portalVideoPath);
    m_portalVideoPath.clear();
    m_portalSegments.clear();
    removePortalIntermediates();
    emit recordingStopped(output);
}

// Ends a failed portal conversion without losing the recording: the source
// video (or its paused parts) is kept and its location reported.
void ScreenRecorder::failPortalConversion(const QString &reason)
{
    cleanupPortalConversion();
    QFile::remove(m_outputPath);
    if (!m_portalConcatListPath.isEmpty()) {
        QFile::remove(m_portalConcatListPath);
        m_portalConcatListPath.clear();
    }
    if (!m_portalPalettePath.isEmpty()) {
        QFile::remove(m_portalPalettePath);
        m_portalPalettePath.clear();
    }

    QStringList kept;
    if (m_portalSegments.size() == 1) {
        const QString source = m_portalSegments.first();
        const QString target = keptPortalVideoPath(m_outputPath);
        if (!QFileInfo::exists(target) && QFile::rename(source, target))
            kept << QDir::toNativeSeparators(target);
        else
            kept << QDir::toNativeSeparators(source);
    } else {
        // Unjoined parts of a paused recording; the portal video path holds
        // at most a partial join unless it is still the first part.
        if (!m_portalSegments.contains(m_portalVideoPath))
            QFile::remove(m_portalVideoPath);
        int partNumber = 1;
        for (const QString &segment : std::as_const(m_portalSegments)) {
            const QString part = recordingPartPath(m_outputPath, partNumber++);
            if (!QFileInfo::exists(part) && QFile::rename(segment, part))
                kept << QDir::toNativeSeparators(part);
            else
                kept << QDir::toNativeSeparators(segment);
        }
    }
    m_portalSegments.clear();
    m_portalVideoPath.clear();
    // First line is the reason (localized in main), second where the video is.
    emit recordingFailed(reason + QStringLiteral("\n")
                         + TranslationManager::tr("recVideoKeptAt").arg(kept.join(QStringLiteral(", "))));
}

void ScreenRecorder::removePortalIntermediates()
{
    QStringList paths = {m_portalSegmentPath, m_portalConcatListPath, m_portalPalettePath};
    paths << m_portalSegments;
    for (const QString &path : std::as_const(paths)) {
        if (!path.isEmpty())
            QFile::remove(path);
    }
    m_portalSegments.clear();
    m_portalSegmentPath.clear();
    m_portalConcatListPath.clear();
    m_portalPalettePath.clear();
}

void ScreenRecorder::captureFrame()
{
    if (!m_recording || m_paused || !m_encoder) return;

    QImage frame = grabScreenRegion(m_captureRect);
    if (frame.isNull()) {
        qWarning() << "ScreenRecorder: grabScreenRegion returned null";
        return;
    }

    // Derive GIF frame delays from the real elapsed time (monotonic,
    // pause-aware) instead of accumulating the nominal 1000/fps interval,
    // whose truncation makes playback speed drift over long recordings.
    // A pending frame is shown until the next distinct frame is captured.
    const qint64 frameMs = m_timeline.activeElapsedMs(nowMs());
    m_lastFrameMs = frameMs;

    if (!m_hasPendingFrame) {
        m_pendingFrame = frame;
        m_hasPendingFrame = true;
        m_gifClock.start(frameMs);
    } else if (framesEqual(m_pendingFrame, frame) && m_gifClock.pendingCs(frameMs) < 65000) {
        // Identical frame: the pending frame's delay keeps growing until a
        // different frame arrives.
    } else {
        if (!flushPendingFrame(m_gifClock.takeDelayCs(frameMs))) {
            QString err = m_encoder->errorString();
            cancel();
            emit recordingFailed(err);
            return;
        }
        m_pendingFrame = frame;
        m_hasPendingFrame = true;
    }
    ++m_frameCount;
    emit frameCaptured(m_frameCount);

    if (m_maxSeconds > 0) {
        const int elapsedSeconds = static_cast<int>(m_timeline.activeElapsedMs(nowMs()) / 1000);
        int remaining = qMax(0, m_maxSeconds - elapsedSeconds);
        emit remainingTimeChanged(remaining);
    }
}

bool ScreenRecorder::flushPendingFrame(int delayCs)
{
    if (!m_hasPendingFrame || !m_encoder) return true;
    const bool ok = m_encoder->addFrame(m_pendingFrame, qMax(1, delayCs));
    if (ok) {
        m_hasPendingFrame = false;
        m_pendingFrame = QImage();
    }
    return ok;
}

int ScreenRecorder::finalFrameDelayCs()
{
    // The last frame has no successor to end it: show it until the recording
    // stopped, but at least for one frame interval.
    const qint64 endMs = qMax(m_timeline.activeElapsedMs(nowMs()),
                              m_lastFrameMs + qMax(1, 1000 / m_fps));
    return m_gifClock.takeDelayCs(endMs);
}

bool ScreenRecorder::framesEqual(const QImage &a, const QImage &b) const
{
    if (a.size() != b.size() || a.format() != b.format()) return false;
    if (a.isNull() || b.isNull()) return false;
    const qsizetype bytes = static_cast<qsizetype>(a.bytesPerLine()) * a.height();
    return bytes == static_cast<qsizetype>(b.bytesPerLine()) * b.height()
        && std::memcmp(a.constBits(), b.constBits(), static_cast<size_t>(bytes)) == 0;
}

bool ScreenRecorder::prepareSource(const QRect &captureRect, const QRect &displayRect)
{
    discardPreparedSource();
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (isRecording() || !LinuxPortalScreenCast::isWaylandSession())
        return true;
    const QString missing = portalPreflightError();
    if (!missing.isEmpty()) {
        emit recordingFailed(missing);
        return false;
    }
    QString error;
    m_preparedStream = selectPortalRecordingStream(captureRect, displayRect, &error);
    if (!m_preparedStream.isValid()) {
        emit recordingFailed(error);
        return false;
    }
#else
    Q_UNUSED(captureRect);
    Q_UNUSED(displayRect);
#endif
    return true;
}

void ScreenRecorder::discardPreparedSource()
{
    if (!m_preparedStream.sessionHandle.isEmpty())
        LinuxPortalScreenCast::closeSession(m_preparedStream.sessionHandle);
    m_preparedStream = {};
}

QString ScreenRecorder::portalPreflightError()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString gst = gstLaunchPath();
    if (gst.isEmpty())
        return QStringLiteral("gstreamer not found");
    // The portal recording is converted to GIF with ffmpeg afterwards; check
    // now instead of discarding the finished recording.
    if (ffmpegPath().isEmpty())
        return QStringLiteral("ffmpeg not found");
    bool catalogOk = false;
    const QStringList elements = cachedGstElements(gstInspectPathFor(gst), &catalogOk);
    if (!catalogOk) {
        // gst-inspect-1.0 could not run; let gst-launch report problems.
        m_gstH264Encoder = QStringLiteral("x264enc");
        return {};
    }
    // Paused parts are joined with ffmpeg, so splitmuxsrc is not needed.
    const QStringList missing = missingPortalRecordingElements(elements, false, false, false);
    if (!missing.isEmpty()) {
        invalidateGstElementCache();
        return gstMissingElementsReason(missing);
    }
    m_gstH264Encoder = preferredGstH264Encoder(elements);
    return {};
#else
    return {};
#endif
}

bool ScreenRecorder::startWaylandPortalRecording(const QRect &captureRect)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // Report missing tools before the source picker opens.
    QString error = portalPreflightError();
    if (!error.isEmpty()) {
        discardPreparedSource();
        emit recordingFailed(error);
        return false;
    }
    m_gstPath = gstLaunchPath();

    // A stream picked by prepareSource() already covers the region.
    LinuxPortalScreenCast::Stream stream;
    if (m_preparedStream.isValid()) {
        stream = std::exchange(m_preparedStream, {});
    } else {
        stream = selectPortalRecordingStream(captureRect, m_displayRect, &error);
        if (!stream.isValid()) {
            emit recordingFailed(error);
            return false;
        }
    }

    // Record the region at full size; the GIF conversion scales it down
    // with lanczos once, instead of a lower quality scale per frame here.
    const PortalCropGeometry crop = portalCropGeometry(
        captureRect, m_displayRect, stream.position, stream.size, captureRect.size());
    if (!crop.valid) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        emit recordingFailed(QStringLiteral("Wayland recording source does not contain the selected region"));
        return false;
    }
    const QSharedPointer<int> remoteFd = stream.pipewireFd;
    if (!remoteFd || *remoteFd < 0) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        emit recordingFailed(QStringLiteral("Wayland PipeWire remote could not be opened"));
        return false;
    }
    m_portalSessionHandle = stream.sessionHandle;
    m_portalSourcePath = pipeWireSourcePath(stream.nodeId, stream.pipewireSerial);
    m_portalCrop = crop;
    const QSize gifSize = boundedOutputSize(crop.cropSize);
    m_portalGifScale = gifSize != crop.cropSize ? gifSize : QSize();
    m_timeline = RecordingTimeline();
    m_portalVideoPath = m_outputPath + QStringLiteral(".portal.mp4");
    m_portalSegmentStamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    QFile::remove(m_portalVideoPath);

    m_recording = true;
    m_portalRecording = true;
    m_portalStarting = true;
    m_portalResuming = false;

    // Stopping the share from the desktop ends the recording and keeps it.
    m_sessionWatcher = new LinuxPortalSessionWatcher(m_portalSessionHandle, this);
    connect(m_sessionWatcher, &LinuxPortalSessionWatcher::closed,
            this, &ScreenRecorder::onPortalSessionClosed);

    // The selection overlay or the start countdown has just been hidden;
    // give the compositor a moment so they do not end up in the first frame.
    const int generation = m_portalGeneration;
    QTimer::singleShot(250, this, [this, generation, remoteFd]() {
        if (generation != m_portalGeneration || !m_recording || m_stopping)
            return;
        QString startError;
        if (!startPortalSegment(remoteFd, m_portalVideoPath, &startError)) {
            m_recording = false;
            m_portalRecording = false;
            closePortalSession();
            emit recordingFailed(startError.isEmpty() ? QStringLiteral("cannot start gstreamer") : startError);
        }
    });
    // recordingStarted() follows once the pipeline is running.
    return true;
#else
    Q_UNUSED(captureRect);
    return false;
#endif
}

void ScreenRecorder::startPortalCountdown()
{
    m_timeline.start(nowMs());
    emit recordingStarted();
    emit remainingTimeChanged(m_maxSeconds > 0 ? m_maxSeconds : -1);
    emit elapsedTimeChanged(0);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        const int elapsedSeconds = static_cast<int>(m_timeline.activeElapsedMs(nowMs()) / 1000);
        emit elapsedTimeChanged(elapsedSeconds);
        if (m_maxSeconds <= 0)
            return;
        const int remaining = qMax(0, m_maxSeconds - elapsedSeconds);
        emit remainingTimeChanged(remaining);
        if (remaining == 0)
            stop();
    });
    m_countdownTimer->start();
}

QStringList ScreenRecorder::portalCaptureArguments(int pipewireFd, const QString &outputPath) const
{
    PortalVideoPipelineOptions options;
    options.pipewireFd = pipewireFd;
    options.sourcePath = m_portalSourcePath;
    options.crop = m_portalCrop;
    options.fps = m_fps;
    options.h264Encoder = m_gstH264Encoder;
    // The GIF is built from this file: keep it (near) lossless so the
    // palette pass sees the real colours instead of encoder artefacts.
    options.losslessIntermediate = true;
    return portalRecordingPipelineArguments(options, outputPath);
}

bool ScreenRecorder::startPortalSegment(const QSharedPointer<int> &pipewireFd, const QString &outputPath,
                                        QString *error)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!pipewireFd || *pipewireFd < 0) {
        if (error)
            *error = QStringLiteral("Wayland PipeWire remote could not be opened");
        return false;
    }
    QProcess *process = new QProcess(this);
    process->setProgram(m_gstPath);
    process->setArguments(portalCaptureArguments(*pipewireFd, outputPath));
    process->setProcessChannelMode(QProcess::MergedChannels);
    // English output: the PLAYING marker and ERROR lines are parsed.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANGUAGE"), QStringLiteral("en"));
    process->setProcessEnvironment(env);
    // Do not leave gst-launch encoding if EShot crashes or is killed.
    process->setChildProcessModifier([]() { prctl(PR_SET_PDEATHSIG, SIGINT); });
    if (!configurePipeWireRemote(process, *pipewireFd)) {
        process->deleteLater();
        if (error)
            *error = QStringLiteral("Wayland PipeWire remote could not be opened");
        return false;
    }

    m_process = process;
    m_portalSegmentPath = outputPath;
    m_portalRemoteFd = pipewireFd;
    m_portalProcessLog.clear();
    m_portalSegmentPlaying = false;
    m_portalSegmentClock.start();
    ++m_portalSegmentCount;

    connect(process, &QProcess::readyRead, this, [this, process]() {
        if (process != m_process)
            return;
        m_portalProcessLog += process->readAll();
        if (!m_portalSegmentPlaying
            && gstPipelineReachedPlaying(QString::fromLocal8Bit(m_portalProcessLog)))
            onPortalSegmentPlaying();
        if (m_portalProcessLog.size() > 64 * 1024)
            m_portalProcessLog = m_portalProcessLog.right(32 * 1024);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError processError) {
        if (processError == QProcess::FailedToStart && process == m_process)
            handlePortalSegmentEnded(-1, QProcess::CrashExit, process->errorString());
    });
    // Crashes (including the SIGINT exit after stop()/pause()) are handled
    // in onPortalProcessFinished().
    connect(process, &QProcess::finished, this, &ScreenRecorder::onPortalProcessFinished);
    process->start();

    // Without the PLAYING line (unexpected output) count a pipeline that is
    // still alive after a few seconds as running.
    const QPointer<QProcess> guard(process);
    QTimer::singleShot(3000, this, [this, guard]() {
        if (guard && guard == m_process && !m_portalSegmentPlaying
            && guard->state() == QProcess::Running)
            onPortalSegmentPlaying();
    });
    return true;
#else
    Q_UNUSED(pipewireFd)
    Q_UNUSED(outputPath)
    Q_UNUSED(error)
    return false;
#endif
}

void ScreenRecorder::onPortalSegmentPlaying()
{
    if (m_portalSegmentPlaying)
        return;
    m_portalSegmentPlaying = true;
    m_portalRemoteFd.reset();
    if (m_portalStarting) {
        // The elapsed clock starts with the first captured frame.
        m_portalStarting = false;
        startPortalCountdown();
        return;
    }
    if (m_portalResuming) {
        // The pause ends when the new segment runs, so the elapsed time
        // matches the recorded video.
        m_portalResuming = false;
        if (!m_timeline.resume(nowMs()))
            return;
        m_paused = false;
        if (m_countdownTimer)
            m_countdownTimer->start();
        emit pausedChanged(false);
    }
}

void ScreenRecorder::resumePortalRecording()
{
    if (m_portalResuming)
        return;
    if (m_process) {
        // The previous segment is still being finalized.
        m_portalResumePending = true;
        return;
    }
    m_portalResuming = true;
    const int generation = m_portalGeneration;
    const QString path = recordingSegmentPath(QFileInfo(m_outputPath).absolutePath(),
                                              m_portalSegmentStamp, m_portalSegmentCount);
    LinuxPortalScreenCast::openPipeWireRemoteAsync(
        m_portalSessionHandle, this, [this, generation, path](QSharedPointer<int> fd) {
        if (generation != m_portalGeneration || !m_portalResuming || m_stopping || !m_recording)
            return;
        QString error;
        if (startPortalSegment(fd, path, &error))
            return;
        qWarning() << "ScreenRecorder: cannot resume portal recording:" << error;
        m_portalResuming = false;
        m_stopping = true;
        finishPortalRecording();
    });
}

void ScreenRecorder::requestPortalProcessStop()
{
    // A second SIGINT aborts gst-launch mid-finalize; signal each process once.
    if (!m_process || m_process->property("eshotStopRequested").toBool())
        return;
    m_process->setProperty("eshotStopRequested", true);
    const QPointer<QProcess> process(m_process);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // gst-launch only turns SIGINT into EOS; SIGTERM would kill it before
    // mp4mux writes the last fragment.
    if (m_process->processId() > 0 && m_process->state() == QProcess::Running)
        ::kill(static_cast<pid_t>(m_process->processId()), SIGINT);
    else
        m_process->kill();
#else
    m_process->write("q\n");
#endif
    // Track the process itself so the deferred kill can never tear down the
    // gst process of a later segment or recording.
    QTimer::singleShot(gstStopGraceMs(m_timeline.activeElapsedMs(nowMs())), this, [process]() {
        if (process && process->state() != QProcess::NotRunning)
            process->kill();
    });
}

void ScreenRecorder::onPortalSessionClosed()
{
    if (!m_recording || !m_portalRecording || m_stopping)
        return;
    qInfo() << "ScreenRecorder: screen sharing was stopped from the desktop; saving the recording";
    stop();
}

qint64 ScreenRecorder::nowMs() const
{
    // Monotonic clock: wall-clock time (QDateTime) jumps on NTP/DST changes and
    // would corrupt the recording timeline. VideoRecorder already works this way.
    return m_monotonicClock.elapsed();
}

QString ScreenRecorder::gstLaunchPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("gstreamer/gst-launch-1.0")),
        QDir(appDir).filePath(QStringLiteral("gst-launch-1.0"))
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return QFileInfo(path).absoluteFilePath();
    }
    return QStandardPaths::findExecutable(QStringLiteral("gst-launch-1.0"));
}

QString ScreenRecorder::ffmpegPath() const
{
    return ComponentPaths::ffmpegPath();
}

void ScreenRecorder::closePortalSession()
{
    // Invalidates pending deferred starts and resume callbacks.
    ++m_portalGeneration;
    m_portalStarting = false;
    m_portalResuming = false;
    if (m_sessionWatcher) {
        // Our own Close below must not look like the user stopping the share.
        m_sessionWatcher->disconnect(this);
        m_sessionWatcher->deleteLater();
        m_sessionWatcher = nullptr;
    }
    if (m_portalSessionHandle.isEmpty()) return;
    LinuxPortalScreenCast::closeSession(m_portalSessionHandle);
    m_portalSessionHandle.clear();
}

bool ScreenRecorder::startPortalVideoToGifConversion()
{
    m_conversionFfmpeg = ffmpegPath();
    if (m_conversionFfmpeg.isEmpty() || m_portalSegments.isEmpty()) return false;
    cleanupPortalConversion();
    QFile::remove(m_outputPath);
    const QDir dir = QFileInfo(m_outputPath).absoluteDir();
    m_portalPalettePath = dir.filePath(QStringLiteral(".eshot_palette_%1.png").arg(m_portalSegmentStamp));

    if (m_portalSegments.size() == 1) {
        if (m_portalSegments.first() != m_portalVideoPath) {
            QFile::remove(m_portalVideoPath);
            if (!QFile::rename(m_portalSegments.first(), m_portalVideoPath))
                return false;
            m_portalSegments = {m_portalVideoPath};
        }
        startPortalConversionStage(ConversionStage::Palette,
                                   portalGifPaletteArguments(m_portalVideoPath, m_portalPalettePath, m_fps,
                                                             m_portalGifScale),
                                   portalGifPassTimeoutMs(m_recordedMs));
        return true;
    }

    // A paused recording: join the segments (stream copy) into the portal
    // video first, so a failed conversion still leaves one playable file.
    if (m_portalSegments.first() == m_portalVideoPath) {
        const QString firstSegment = recordingSegmentPath(dir.absolutePath(), m_portalSegmentStamp, 0);
        QFile::remove(firstSegment);
        if (!QFile::rename(m_portalVideoPath, firstSegment))
            return false;
        m_portalSegments[0] = firstSegment;
    }
    m_portalConcatListPath = dir.filePath(QStringLiteral(".eshot_segments_%1.txt").arg(m_portalSegmentStamp));
    QFile list(m_portalConcatListPath);
    if (!list.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    list.write(ffmpegConcatList(m_portalSegments));
    list.close();
    startPortalConversionStage(ConversionStage::Concat,
                               ffmpegConcatArguments(m_portalConcatListPath, m_portalVideoPath),
                               segmentConcatTimeoutMs(m_recordedMs));
    return true;
}

void ScreenRecorder::startPortalConversionStage(ConversionStage stage, const QStringList &arguments,
                                                int timeoutMs)
{
    cleanupPortalConversion();
    m_conversionStage = stage;
    m_conversionProcess = new QProcess(this);
    m_conversionProcess->setProgram(m_conversionFfmpeg);
    m_conversionProcess->setProcessChannelMode(QProcess::SeparateChannels);
    m_conversionProcess->setArguments(arguments);
    connect(m_conversionProcess, &QProcess::finished,
            this, &ScreenRecorder::onPortalConversionFinished);
    connect(m_conversionProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_conversionProcess)
            return;
        failPortalConversion(m_conversionProcess->errorString());
    });

    m_conversionTimeout = new QTimer(this);
    m_conversionTimeout->setSingleShot(true);
    m_conversionTimeout->setInterval(timeoutMs);
    connect(m_conversionTimeout, &QTimer::timeout, this, [this]() {
        if (!m_conversionProcess)
            return;
        m_conversionProcess->setProperty("eshotTimedOut", true);
        m_conversionProcess->kill();
    });
    m_conversionTimeout->start();
    m_conversionProcess->start();
}

void ScreenRecorder::cleanupPortalConversion()
{
    if (m_conversionTimeout) {
        m_conversionTimeout->stop();
        m_conversionTimeout->deleteLater();
        m_conversionTimeout = nullptr;
    }
    if (m_conversionProcess) {
        if (m_conversionProcess->state() != QProcess::NotRunning)
            m_conversionProcess->kill();
        m_conversionProcess->deleteLater();
        m_conversionProcess = nullptr;
    }
}

bool ScreenRecorder::initCaptureResources()
{
#ifdef Q_OS_WIN
    releaseCaptureResources();
    if (m_captureRect.width() <= 0 || m_captureRect.height() <= 0) return false;

    m_screenDC = GetDC(nullptr);
    if (!m_screenDC) return false;
    m_memDC = CreateCompatibleDC(m_screenDC);
    if (!m_memDC) {
        releaseCaptureResources();
        return false;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = m_captureRect.width();
    bi.bmiHeader.biHeight = -m_captureRect.height();
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    m_bits = nullptr;
    m_bitmap = CreateDIBSection(m_screenDC, &bi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
    if (!m_bitmap) {
        releaseCaptureResources();
        return false;
    }
    m_oldBitmap = SelectObject(m_memDC, m_bitmap);
    return m_oldBitmap != nullptr;
#else
    return true;
#endif
}

void ScreenRecorder::releaseCaptureResources()
{
#ifdef Q_OS_WIN
    if (m_memDC && m_oldBitmap) {
        SelectObject(m_memDC, m_oldBitmap);
        m_oldBitmap = nullptr;
    }
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
    }
    m_bits = nullptr;
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    if (m_screenDC) {
        ReleaseDC(nullptr, m_screenDC);
        m_screenDC = nullptr;
    }
#endif
}

QImage ScreenRecorder::grabScreenRegion(const QRect &rect)
{
#ifndef Q_OS_WIN
    // rect is in physical (snapshot) pixels, but QScreen::grabWindow() and
    // screenAt() take logical coordinates. Prefer the logical display rect
    // from the overlay; otherwise scale by the DPR of the screen containing it.
    QRect logicalRect = m_displayRect;
    QScreen *screen = nullptr;
    if (logicalRect.isValid()) {
        screen = QGuiApplication::screenAt(logicalRect.center());
    } else {
        for (QScreen *candidate : QGuiApplication::screens()) {
            const QRect sg = candidate->geometry();
            const qreal dpr = candidate->devicePixelRatio();
            const QRectF physical(sg.x() * dpr, sg.y() * dpr, sg.width() * dpr, sg.height() * dpr);
            if (physical.contains(QRectF(rect).center())) {
                screen = candidate;
                break;
            }
        }
        if (!screen) screen = QGuiApplication::primaryScreen();
        const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
        logicalRect = QRectF(rect.x() / dpr, rect.y() / dpr,
                             rect.width() / dpr, rect.height() / dpr).toAlignedRect();
    }
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect sg = screen->geometry();
        QPixmap pix = screen->grabWindow(0,
                                         logicalRect.x() - sg.x(),
                                         logicalRect.y() - sg.y(),
                                         logicalRect.width(),
                                         logicalRect.height());
        if (!pix.isNull()) {
            QImage img = pix.toImage().convertToFormat(QImage::Format_RGB32);
            if (m_outputSize.isValid() && img.size() != m_outputSize)
                img = img.scaled(m_outputSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            return img;
        }
    }
#endif

#ifdef Q_OS_WIN
    int sx = rect.x();
    int sy = rect.y();
    int sw = rect.width();
    int sh = rect.height();
    if (sw <= 0 || sh <= 0) return QImage();

    if (!m_screenDC || !m_memDC || !m_bitmap || !m_bits) return QImage();
    BOOL ok = BitBlt(m_memDC, 0, 0, sw, sh, m_screenDC, sx, sy, SRCCOPY | CAPTUREBLT);
    if (!ok) return QImage();

    QImage img(static_cast<uchar *>(m_bits), sw, sh, sw * 4, QImage::Format_RGB32);
    if (m_outputSize.isValid() && m_outputSize != QSize(sw, sh))
        return img.scaled(m_outputSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    return img.copy();
#else
    return QImage();
#endif
}

QSize ScreenRecorder::boundedOutputSize(const QSize &sourceSize) const
{
    if (sourceSize.isEmpty()) return sourceSize;

    QSettings s("EShot", "EShot");
    const int maxSide = qBound(320, s.value("recordingMaxSide", 1280).toInt(), 3840);
    if (sourceSize.width() <= maxSide && sourceSize.height() <= maxSide)
        return sourceSize;

    QSize output = sourceSize;
    output.scale(maxSide, maxSide, Qt::KeepAspectRatio);
    output.setWidth(qMax(8, output.width()));
    output.setHeight(qMax(8, output.height()));
    return output;
}
