#include "ScreenRecorder.h"
#include "GifEncoder.h"
#include "core/LinuxPortalScreenCast.h"
#include "LinuxRecordingSupport.h"
#include "RecordingSettingsPolicy.h"
#include "RecordingFinalizationPolicy.h"

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
#include <QStringList>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <fcntl.h>
#endif

namespace {
QString defaultSaveDirectory()
{
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.trimmed().isEmpty())
        picturesPath = QDir::homePath();
    return QDir(picturesPath).filePath(QStringLiteral("EShot"));
}

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
    m_delayCs = qMax(1, qRound(100.0 / m_fps));
    m_lastFrameMs = -1;
    m_outputPath = outputPath;
    m_loopCount = loopCount;
    m_portalVideoPath.clear();
    closePortalSession();
    m_paused = false;
    m_hasPendingFrame = false;
    m_pendingFrame = QImage();
    m_pendingDelayCs = 0;

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
        if (remaining == 0) {
            emit timeLimitReached();
            finishRecording();
        }
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
        : QDir(defaultSaveDirectory()).filePath(QStringLiteral("GIFs"));
    if (configuredDir.isEmpty())
        configuredDir = s.value("savePath").toString().trimmed();
    if (!configuredDir.isEmpty()) {
        candidates << configuredDir;
    } else {
        candidates << defaultSaveDirectory();
    }
    candidates << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
               << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
               << QStandardPaths::writableLocation(QStandardPaths::TempLocation)
               << QDir::homePath();

    const QString fileName = QStringLiteral("EShot_GIF_%1.gif")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

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
        return dir.filePath(fileName);
    }

    return QDir(QDir::tempPath()).filePath(fileName);
}

void ScreenRecorder::stop()
{
    if (!m_recording) return;
    if (m_paused)
        resume();
    if (m_portalRecording && m_process) {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (m_process->processId() > 0)
            QProcess::execute(QStringLiteral("kill"),
                              {QStringLiteral("-INT"), QString::number(m_process->processId())});
#else
        m_process->write("q\n");
#endif
        // Capture the process pointer so a fast stop()->start() cannot make
        // these deferred timers tear down the NEW recording's gst process.
        QProcess *process = m_process;
        QTimer::singleShot(2500, this, [this, process]() {
            if (m_process == process && m_recording)
                m_process->terminate();
        });
        QTimer::singleShot(5000, this, [this, process]() {
            if (m_process == process && m_recording)
                m_process->kill();
        });
        return;
    }
    finishRecording();
}

void ScreenRecorder::cancel()
{
    if (!m_recording && !isFinalizing()) return;
    m_recording = false;
    m_portalRecording = false;
    m_paused = false;
    if (m_process) { m_process->kill(); m_process->deleteLater(); m_process = nullptr; }
    cleanupPortalConversion();
    if (m_frameTimer)     { m_frameTimer->stop();     m_frameTimer->deleteLater();     m_frameTimer = nullptr; }
    if (m_countdownTimer) { m_countdownTimer->stop(); m_countdownTimer->deleteLater(); m_countdownTimer = nullptr; }
    if (m_encoder)        { delete m_encoder;         m_encoder = nullptr; }
    releaseCaptureResources();
    m_hasPendingFrame = false;
    m_pendingFrame = QImage();
    m_pendingDelayCs = 0;
    if (!m_outputPath.isEmpty() && QFile::exists(m_outputPath)) {
        QFile::remove(m_outputPath);
    }
    if (!m_portalVideoPath.isEmpty()) QFile::remove(m_portalVideoPath);
    closePortalSession();
}

void ScreenRecorder::pause()
{
    if (!m_recording || m_paused)
        return;
    if (m_portalRecording && !setPortalProcessSuspended(true))
        return;
    if (!m_timeline.pause(nowMs()))
        return;
    m_paused = true;
    if (!m_portalRecording && m_frameTimer)
        m_frameTimer->stop();
    if (m_countdownTimer)
        m_countdownTimer->stop();
    emit pausedChanged(true);
}

void ScreenRecorder::resume()
{
    if (!m_recording || !m_paused)
        return;
    if (m_portalRecording && !setPortalProcessSuspended(false))
        return;
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
            m_pendingDelayCs = m_delayCs;
            m_hasPendingFrame = true;
        }
    }
    const bool hasCapturedFrame = m_frameCount > 0 || m_hasPendingFrame;
    releaseCaptureResources();

    QString savedPath = m_outputPath;
    bool ok = false;
    if (m_encoder) {
        ok = flushPendingFrame() && m_encoder->close();
        QString err = m_encoder->errorString();
        delete m_encoder;
        m_encoder = nullptr;
        m_hasPendingFrame = false;
        m_pendingFrame = QImage();
        m_pendingDelayCs = 0;
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
    if (!m_portalRecording && !m_process)
        return;
    const QString stderrText = m_process ? QString::fromLocal8Bit(m_process->readAll()).trimmed() : QString();
    const QString output = m_outputPath;

    m_recording = false;
    m_portalRecording = false;
    m_paused = false;
    closePortalSession();
    if (m_countdownTimer) { m_countdownTimer->stop(); m_countdownTimer->deleteLater(); m_countdownTimer = nullptr; }
    if (m_process) { m_process->deleteLater(); m_process = nullptr; }

    if (status == QProcess::NormalExit && exitCode == 0
        && QFileInfo::exists(m_portalVideoPath)
        && startPortalVideoToGifConversion()) {
        return;
    }

    if (QFileInfo::exists(output))
        QFile::remove(output);
    if (!m_portalVideoPath.isEmpty()) QFile::remove(m_portalVideoPath);
    emit recordingFailed(stderrText.isEmpty() ? QStringLiteral("gstreamer exited with code %1").arg(exitCode) : stderrText);
}

void ScreenRecorder::onPortalConversionFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_conversionProcess)
        return;

    const QString reason = QString::fromLocal8Bit(
        m_conversionProcess->readAllStandardError()).trimmed();
    const bool timedOut = m_conversionProcess->property("eshotTimedOut").toBool();
    const bool success = !timedOut
        && portalGifConversionSucceeded(
            status == QProcess::NormalExit, exitCode, QFileInfo(m_outputPath).size());
    const QString output = m_outputPath;
    cleanupPortalConversion();

    if (success) {
        QFile::remove(m_portalVideoPath);
        m_portalVideoPath.clear();
        emit recordingStopped(output);
        return;
    }

    QFile::remove(output);
    QFile::remove(m_portalVideoPath);
    m_portalVideoPath.clear();
    emit recordingFailed(timedOut
        ? QStringLiteral("GIF conversion timed out")
        : (reason.isEmpty() ? QStringLiteral("failed to convert portal video to GIF") : reason));
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
    const qint64 frameMs = m_timeline.activeElapsedMs(nowMs());
    if (m_lastFrameMs >= 0 && m_hasPendingFrame)
        m_pendingDelayCs += static_cast<int>(qMax<qint64>(0, (frameMs - m_lastFrameMs + 5) / 10));
    m_lastFrameMs = frameMs;

    if (!m_hasPendingFrame) {
        m_pendingFrame = frame;
        m_pendingDelayCs = 0;
        m_hasPendingFrame = true;
    } else if (framesEqual(m_pendingFrame, frame) && m_pendingDelayCs < 65000) {
        // Identical frame: its delay was already extended by the measured
        // delta above.
    } else {
        if (!flushPendingFrame()) {
            QString err = m_encoder->errorString();
            cancel();
            emit recordingFailed(err);
            return;
        }
        m_pendingFrame = frame;
        m_pendingDelayCs = 0;
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

bool ScreenRecorder::flushPendingFrame()
{
    if (!m_hasPendingFrame || !m_encoder) return true;
    const bool ok = m_encoder->addFrame(m_pendingFrame, qMax(1, m_pendingDelayCs));
    if (ok) {
        m_hasPendingFrame = false;
        m_pendingFrame = QImage();
        m_pendingDelayCs = 0;
    }
    return ok;
}

bool ScreenRecorder::framesEqual(const QImage &a, const QImage &b) const
{
    if (a.size() != b.size() || a.format() != b.format()) return false;
    if (a.isNull() || b.isNull()) return false;
    const qsizetype bytes = static_cast<qsizetype>(a.bytesPerLine()) * a.height();
    return bytes == static_cast<qsizetype>(b.bytesPerLine()) * b.height()
        && std::memcmp(a.constBits(), b.constBits(), static_cast<size_t>(bytes)) == 0;
}

bool ScreenRecorder::startWaylandPortalRecording(const QRect &captureRect)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString gst = gstLaunchPath();
    if (gst.isEmpty()) {
        emit recordingFailed(QStringLiteral("gstreamer not found"));
        return false;
    }
    if (!LinuxPortalScreenCast::isAvailable()) {
        emit recordingFailed(QStringLiteral("Wayland ScreenCast portal is not available"));
        return false;
    }

    QString persistenceId;
    if (m_displayRect.isValid()) {
        if (QScreen *screen = QGuiApplication::screenAt(m_displayRect.center()))
            persistenceId = screen->name();
    }
    LinuxPortalScreenCast::Stream stream = LinuxPortalScreenCast::selectStream(
        nullptr, 120000, persistenceId);
    if (!stream.isValid()) {
        emit recordingFailed(QStringLiteral("Wayland screen recording permission was not granted"));
        return false;
    }

    PortalCropGeometry crop = portalCropGeometry(
        captureRect, m_displayRect, stream.position, stream.size, m_outputSize);
    if (!crop.valid && stream.usedRestoreToken) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        LinuxPortalScreenCast::clearRestoreToken(persistenceId);
        stream = LinuxPortalScreenCast::selectStream(nullptr, 120000, persistenceId);
        if (stream.isValid()) {
            crop = portalCropGeometry(
                captureRect, m_displayRect, stream.position, stream.size, m_outputSize);
        }
    }
    if (!stream.isValid() || !crop.valid) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        emit recordingFailed(QStringLiteral("Wayland recording source does not contain the selected region"));
        return false;
    }
    m_portalSessionHandle = stream.sessionHandle;

    const QString sourcePath = pipeWireSourcePath(stream.nodeId, stream.pipewireSerial);
    const int pipewireFd = stream.remoteFd();
    m_portalVideoPath = m_outputPath + QStringLiteral(".portal.mp4");
    QFile::remove(m_portalVideoPath);

    QStringList args;
    args << QStringLiteral("-e")
         << QStringLiteral("pipewiresrc")
         << QStringLiteral("fd=%1").arg(pipewireFd)
         << sourcePath
         << QStringLiteral("do-timestamp=true")
         << QStringLiteral("!")
         << QStringLiteral("queue")
         << QStringLiteral("!")
         << QStringLiteral("videoconvert")
         << QStringLiteral("!")
         << QStringLiteral("videocrop")
         << QStringLiteral("left=%1").arg(crop.left)
         << QStringLiteral("right=%1").arg(crop.right)
         << QStringLiteral("top=%1").arg(crop.top)
         << QStringLiteral("bottom=%1").arg(crop.bottom)
         << QStringLiteral("!")
         << QStringLiteral("videoscale")
         << QStringLiteral("!")
         << QStringLiteral("videorate")
         << QStringLiteral("!")
         << QStringLiteral("video/x-raw,width=%1,height=%2,framerate=%3/1")
                .arg(crop.outputSize.width())
                .arg(crop.outputSize.height())
                .arg(m_fps)
         << QStringLiteral("!")
         << QStringLiteral("x264enc")
         << QStringLiteral("speed-preset=veryfast")
         << QStringLiteral("tune=zerolatency")
         << QStringLiteral("!")
         << QStringLiteral("h264parse")
         << QStringLiteral("!")
         << QStringLiteral("mp4mux")
         << QStringLiteral("!")
         << QStringLiteral("filesink")
         << QStringLiteral("location=%1").arg(m_portalVideoPath);

    m_process = new QProcess(this);
    m_process->setProgram(gst);
    m_process->setArguments(args);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    if (!configurePipeWireRemote(m_process, pipewireFd)) {
        if (m_process) { m_process->deleteLater(); m_process = nullptr; }
        closePortalSession();
        emit recordingFailed(QStringLiteral("Wayland PipeWire remote could not be opened"));
        return false;
    }
    m_process->start();
    if (!m_process->waitForStarted(3000)) {
        const QString reason = m_process->errorString();
        if (m_process) { m_process->deleteLater(); m_process = nullptr; }
        closePortalSession();
        emit recordingFailed(reason.isEmpty() ? QStringLiteral("cannot start gstreamer") : reason);
        return false;
    }
    if (m_process->waitForFinished(700)) {
        const QString reason = QString::fromLocal8Bit(m_process->readAll()).trimmed();
        m_process->deleteLater();
        m_process = nullptr;
        QFile::remove(m_portalVideoPath);
        closePortalSession();
        emit recordingFailed(reason.isEmpty() ? QStringLiteral("gstreamer pipeline exited during startup") : reason);
        return false;
    }

    connect(m_process, &QProcess::finished, this, &ScreenRecorder::onPortalProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_recording) return;
        const QString reason = m_process ? m_process->errorString() : QStringLiteral("gstreamer process error");
        m_recording = false;
        m_portalRecording = false;
        m_paused = false;
        if (m_process) { m_process->deleteLater(); m_process = nullptr; }
        QFile::remove(m_portalVideoPath);
        closePortalSession();
        emit recordingFailed(reason);
    });

    m_recording = true;
    m_portalRecording = true;
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
    return true;
#else
    Q_UNUSED(captureRect);
    return false;
#endif
}

bool ScreenRecorder::setPortalProcessSuspended(bool suspended)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!m_process)
        return false;
    return m_process->processId() > 0
        && QProcess::execute(QStringLiteral("kill"),
                             {suspended ? QStringLiteral("-STOP") : QStringLiteral("-CONT"),
                              QString::number(m_process->processId())}) == 0;
#else
    Q_UNUSED(suspended)
    return false;
#endif
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
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

void ScreenRecorder::closePortalSession()
{
    if (m_portalSessionHandle.isEmpty()) return;
    LinuxPortalScreenCast::closeSession(m_portalSessionHandle);
    m_portalSessionHandle.clear();
}

bool ScreenRecorder::startPortalVideoToGifConversion()
{
    const QString ffmpeg = ffmpegPath();
    if (ffmpeg.isEmpty() || !QFileInfo::exists(m_portalVideoPath)) return false;
    cleanupPortalConversion();
    QFile::remove(m_outputPath);
    m_conversionProcess = new QProcess(this);
    m_conversionProcess->setProgram(ffmpeg);
    m_conversionProcess->setProcessChannelMode(QProcess::SeparateChannels);
    const QString filter = QStringLiteral(
        "fps=%1,split[s0][s1];[s0]palettegen=max_colors=256[p];[s1][p]paletteuse=dither=sierra2_4a")
        .arg(m_fps);
    m_conversionProcess->setArguments({
        QStringLiteral("-y"), QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), m_portalVideoPath,
        QStringLiteral("-vf"), filter,
        QStringLiteral("-loop"), QString::number(m_loopCount),
        m_outputPath
    });
    connect(m_conversionProcess, &QProcess::finished,
            this, &ScreenRecorder::onPortalConversionFinished);
    connect(m_conversionProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_conversionProcess)
            return;
        const QString reason = m_conversionProcess->errorString();
        cleanupPortalConversion();
        QFile::remove(m_outputPath);
        QFile::remove(m_portalVideoPath);
        m_portalVideoPath.clear();
        emit recordingFailed(reason);
    });

    m_conversionTimeout = new QTimer(this);
    m_conversionTimeout->setSingleShot(true);
    m_conversionTimeout->setInterval(60000);
    connect(m_conversionTimeout, &QTimer::timeout, this, [this]() {
        if (!m_conversionProcess)
            return;
        m_conversionProcess->setProperty("eshotTimedOut", true);
        m_conversionProcess->kill();
    });
    m_conversionTimeout->start();
    m_conversionProcess->start();
    return true;
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
    QScreen *screen = QGuiApplication::screenAt(rect.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect sg = screen->geometry();
        QPixmap pix = screen->grabWindow(0,
                                         rect.x() - sg.x(),
                                         rect.y() - sg.y(),
                                         rect.width(),
                                         rect.height());
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
