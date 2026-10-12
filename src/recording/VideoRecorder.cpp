#include "VideoRecorder.h"
#include "AudioDevices.h"
#include "core/TranslationManager.h"
#include "core/ComponentPaths.h"
#include "core/LinuxPortalScreenCast.h"
#include "PortalRecordingSource.h"
#include "LinuxRecordingSupport.h"
#include "RecordingSegments.h"
#include "RecordingSettingsPolicy.h"
#include "VideoRecordingCompletionPolicy.h"
#include "RecordingFinalizationPolicy.h"
#include "WindowsRecordingSupport.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#endif

namespace {
#ifndef Q_OS_WIN
bool containsDevice(const QStringList &devices, const QString &name)
{
    for (const QString &device : devices) {
        if (device.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QStringList platformAudioDevices()
{
    QStringList devices;
    devices << QStringLiteral("default")
            << QStringLiteral("@DEFAULT_SOURCE@")
            << QStringLiteral("@DEFAULT_SINK@.monitor");
    for (const auto &device : discoverLinuxMicrophoneDevices()) devices << device.second;
    devices.removeDuplicates();
    return devices;
}
#endif

QString ffmpegMissingReason()
{
#ifdef Q_OS_WIN
    return QStringLiteral("ffmpeg.exe not found");
#else
    return QStringLiteral("ffmpeg not found");
#endif
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

void stopRecorderWhenParentExits(QProcess *process)
{
    if (!process)
        return;
    process->setChildProcessModifier([]() {
        // Avoid leaving a CPU-heavy GStreamer/FFmpeg encoder behind if EShot
        // is killed or crashes. SIGINT also lets gst-launch send EOS and close
        // the MP4 container when the parent disappears.
        prctl(PR_SET_PDEATHSIG, SIGINT);
    });
}
#endif

// Hides `.eshot_*` intermediates on Windows (dot files are hidden elsewhere
// already) and unhides a file once it has been renamed to a visible name.
void syncHiddenAttribute(const QString &path)
{
#ifdef Q_OS_WIN
    if (path.isEmpty())
        return;
    const bool hide = QFileInfo(path).fileName().startsWith(QStringLiteral(".eshot_"));
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    const DWORD attributes = GetFileAttributesW(native.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return;
    const DWORD wanted = hide ? (attributes | FILE_ATTRIBUTE_HIDDEN)
                              : (attributes & ~static_cast<DWORD>(FILE_ATTRIBUTE_HIDDEN));
    if (wanted != attributes)
        SetFileAttributesW(native.c_str(), wanted);
#else
    Q_UNUSED(path);
#endif
}

bool renameRecordingFile(const QString &from, const QString &to)
{
    if (!QFile::rename(from, to))
        return false;
    syncHiddenAttribute(to);
    return true;
}

#ifdef Q_OS_WIN
// Every recording FFmpeg process joins this job; the handle is never closed,
// so Windows kills the encoders when EShot exits or crashes instead of
// leaving them running (Linux uses PR_SET_PDEATHSIG for the same purpose).
HANDLE recordingJobObject()
{
    static HANDLE job = []() -> HANDLE {
        HANDLE handle = CreateJobObjectW(nullptr, nullptr);
        if (!handle)
            return nullptr;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
        ZeroMemory(&info, sizeof(info));
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
            CloseHandle(handle);
            return nullptr;
        }
        return handle;
    }();
    return job;
}
#endif

void stopWithRecorder(QProcess *process)
{
#ifdef Q_OS_WIN
    HANDLE job = recordingJobObject();
    if (!job || !process)
        return;
    const qint64 pid = process->processId();
    if (pid <= 0)
        return;
    HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!child)
        return;
    if (!AssignProcessToJobObject(job, child))
        qWarning() << "VideoRecorder: could not tie ffmpeg to the EShot process lifetime" << GetLastError();
    CloseHandle(child);
#else
    Q_UNUSED(process);
#endif
}

#ifdef Q_OS_WIN
// Removes `.eshot_*` intermediates a crashed recording left behind.
void sweepStaleRecordingIntermediates(const QString &directory)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QFileInfoList entries = QDir(directory).entryInfoList(
        {QStringLiteral(".eshot_*")}, QDir::Files | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries) {
        if (isStaleRecordingIntermediate(entry.fileName(), entry.lastModified(), now))
            QFile::remove(entry.absoluteFilePath());
    }
}

void writeSilence(QFile &file, quint64 bytes)
{
    static const QByteArray zeros(64 * 1024, '\0');
    while (bytes > 0) {
        const qint64 chunk = static_cast<qint64>(qMin<quint64>(bytes, static_cast<quint64>(zeros.size())));
        if (file.write(zeros.constData(), chunk) != chunk)
            return;
        bytes -= static_cast<quint64>(chunk);
    }
}

void recordWasapiLoopback(const QString &path, std::atomic_bool *stopFlag,
                          std::atomic_bool *pauseFlag, std::atomic<qint64> *startedEpochMs)
{
    HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE)
        return;

    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioClient *client = nullptr;
    IAudioCaptureClient *capture = nullptr;
    WAVEFORMATEX *format = nullptr;
    QFile file(path);
    // 64-bit: a 32-bit count wrapped after about three hours of float audio.
    quint64 dataBytes = 0;

    auto cleanup = [&]() {
        if (client) client->Stop();
        if (format) CoTaskMemFree(format);
        if (capture) capture->Release();
        if (client) client->Release();
        if (device) device->Release();
        if (enumerator) enumerator->Release();
        if (SUCCEEDED(init)) CoUninitialize();
    };

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator))) ||
        FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) ||
        FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&client))) ||
        FAILED(client->GetMixFormat(&format)) ||
        FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                  10000000, 0, format, nullptr)) ||
        FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&capture))) ||
        !file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cleanup();
        return;
    }

    const QByteArray waveFormat(reinterpret_cast<const char *>(format),
                                static_cast<int>(sizeof(WAVEFORMATEX) + format->cbSize));
    // The header is rewritten with the final size when capture ends; it
    // switches to RF64 past 4 GiB, which FFmpeg reads as well.
    file.write(wavFileHeader(waveFormat, 0));
    syncHiddenAttribute(path);

    if (FAILED(client->Start())) {
        cleanup();
        return;
    }
    startedEpochMs->store(QDateTime::currentMSecsSinceEpoch());

    // Loopback capture delivers no packets while nothing is playing. Pad those
    // gaps with silence based on elapsed time so the WAV stays as long as (and
    // in sync with) the video instead of collapsing silent stretches. Paused
    // time is not recorded by the video either, so it is neither written nor
    // counted towards the expected length.
    const quint64 bytesPerSecond = static_cast<quint64>(format->nSamplesPerSec) * format->nBlockAlign;
    const quint64 gapToleranceBytes = bytesPerSecond / 5;
    const ULONGLONG startTick = GetTickCount64();
    ULONGLONG pausedTicks = 0;
    ULONGLONG pauseStartedTick = 0;
    bool paused = false;

    while (!stopFlag->load()) {
        Sleep(10);
        const bool pauseRequested = pauseFlag->load();
        if (pauseRequested != paused) {
            paused = pauseRequested;
            if (paused)
                pauseStartedTick = GetTickCount64();
            else
                pausedTicks += GetTickCount64() - pauseStartedTick;
        }
        bool gotPacket = false;
        UINT32 packetFrames = 0;
        while (SUCCEEDED(capture->GetNextPacketSize(&packetFrames)) && packetFrames > 0) {
            gotPacket = true;
            BYTE *data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                break;
            const quint32 bytes = frames * format->nBlockAlign;
            // While paused, packets are still drained (and dropped) so the
            // endpoint buffer cannot overflow.
            if (!paused) {
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    writeSilence(file, bytes);
                } else {
                    file.write(reinterpret_cast<const char *>(data), bytes);
                }
                dataBytes += bytes;
            }
            capture->ReleaseBuffer(frames);
        }
        if (!gotPacket && !paused) {
            const ULONGLONG activeTicks = GetTickCount64() - startTick - pausedTicks;
            const quint64 expectedBytes = activeTicks * bytesPerSecond / 1000;
            if (expectedBytes > dataBytes + gapToleranceBytes) {
                // Written in chunks and at most a second per pass: a long
                // stall (e.g. after sleep) must not allocate gigabytes or
                // keep the stop request waiting.
                quint64 missing = qMin<quint64>(expectedBytes - dataBytes, bytesPerSecond);
                missing -= missing % format->nBlockAlign;
                writeSilence(file, missing);
                dataBytes += missing;
            }
        }
    }

    file.seek(0);
    file.write(wavFileHeader(waveFormat, dataBytes));
    file.close();
    cleanup();
}
#endif
}

VideoRecorder::VideoRecorder(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    // Fill the DirectShow device cache in the background so start() does not
    // have to wait for the enumeration.
    refreshDshowAudioDevices(ffmpegPath(), true);
#endif
}

VideoRecorder::~VideoRecorder()
{
    discardPreparedSource();
    if (isRecording())
        cancel();
    // cancel() is asynchronous (the encoder exits via the event loop), so make
    // sure the system-audio thread is joined here; the call is idempotent.
    stopSystemAudioCapture();
    cleanupMuxProcess();
    cleanupConcatProcess();
}

void VideoRecorder::start(const QRect &captureRect, int fps, int maxSeconds, int crf,
                          bool desktopAudioEnabled, int desktopVolume,
                          const QString &desktopAudioDevice,
                          bool microphoneEnabled, int microphoneVolume,
                          const QString &microphoneDevice,
                          const QString &outputPath,
                          const QRect &displayRect)
{
    if (isRecording()) {
        emit recordingFailed(QStringLiteral("already recording"));
        return;
    }
    if (captureRect.width() < 8 || captureRect.height() < 8) {
        emit recordingFailed(QStringLiteral("region too small"));
        return;
    }

#ifdef Q_OS_WIN
    m_ffmpegPath = ffmpegPath();
    if (m_ffmpegPath.isEmpty()) {
        emit recordingFailed(ffmpegMissingReason());
        return;
    }
#endif
#ifndef Q_OS_WIN
    const bool discoverAudioDevices = shouldDiscoverAudioDevices(
        microphoneEnabled, microphoneDevice);
    const QStringList audioDevices = discoverAudioDevices
        ? platformAudioDevices() : QStringList();
#endif

    m_warnings.clear();
    m_captureRect = captureRect;
    m_displayRect = displayRect;
    m_fps = qBound(1, fps, videoRecordingFpsLimit());
    m_maxSeconds = qMax(0, maxSeconds);
    m_crf = qBound(18, crf, 32);
    m_desktopAudioEnabled = desktopAudioEnabled;
    m_desktopVolume = qBound(0, desktopVolume, 100);
    m_desktopAudioDevice = desktopAudioDevice.trimmed();
#ifdef Q_OS_WIN
    if (m_desktopAudioDevice.isEmpty() || m_desktopAudioDevice == QStringLiteral("virtual-audio-capturer"))
        m_desktopAudioDevice = QStringLiteral("__wasapi__");
    m_systemAudioLoopback = m_desktopAudioEnabled && m_desktopVolume > 0
        && m_desktopAudioDevice == QStringLiteral("__wasapi__");
#else
    m_systemAudioLoopback = false;
    if (m_desktopAudioDevice.isEmpty() || m_desktopAudioDevice == QStringLiteral("__wasapi__"))
        m_desktopAudioDevice = QStringLiteral("@DEFAULT_SINK@.monitor");
#endif
    m_microphoneEnabled = microphoneEnabled;
    m_microphoneVolume = qBound(0, microphoneVolume, 100);
    m_microphoneDevice = microphoneDevice.trimmed();
#ifdef Q_OS_WIN
    resolveWindowsAudioInputs();
#else
    if (m_microphoneDevice.isEmpty() || m_microphoneDevice == QStringLiteral("default")) {
        m_microphoneDevice = audioDevices.isEmpty() ? QString() : audioDevices.first();
    }
    if (m_microphoneEnabled && discoverAudioDevices
        && (m_microphoneDevice.isEmpty() || !containsDevice(audioDevices, m_microphoneDevice))) {
        m_microphoneEnabled = false;
        if (m_microphoneVolume > 0) {
            m_warnings << TranslationManager::tr("recWarnMicMissing");
            qWarning() << "VideoRecorder: default microphone could not be resolved; recording without it";
        }
    }
#endif
    m_lastElapsedSeconds = -1;
    m_outputPath = outputPath.trimmed().isEmpty() ? makeDefaultOutputPath() : outputPath;
    clearStartupState();
    m_pausedMs = 0;
    m_pauseStartedMs = 0;
    m_recordedMs = 0;
    m_paused = false;
    m_resumePending = false;
    m_stopping = false;
    m_canceling = false;
    m_usesGStreamer = false;
    m_audioPaused.store(false);

    QDir dir(QFileInfo(m_outputPath).absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        emit recordingFailed(QStringLiteral("cannot create output directory"));
        return;
    }
    m_videoOnlyPath.clear();
    m_audioPath.clear();
    m_segmentPaths.clear();
    m_segmentPath.clear();
    m_concatListPath.clear();
    m_segmentCount = 0;
    m_segmentDirectory = dir.absolutePath();
    m_segmentStamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (LinuxPortalScreenCast::isWaylandSession()) {
        startWaylandPortalRecording(captureRect);
        return;
    }
#endif

#ifndef Q_OS_WIN
    m_ffmpegPath = ffmpegPath();
    if (m_ffmpegPath.isEmpty()) {
        emit recordingFailed(ffmpegMissingReason());
        return;
    }
#endif

    if (m_systemAudioLoopback) {
        m_videoOnlyPath = dir.filePath(QStringLiteral(".eshot_video_%1.mp4").arg(m_segmentStamp));
        m_audioPath = dir.filePath(QStringLiteral(".eshot_audio_%1.wav").arg(m_segmentStamp));
    }

    QString error;
#ifdef Q_OS_WIN
    sweepStaleRecordingIntermediates(m_segmentDirectory);
    // recordingStarted() follows once FFmpeg reports progress (see
    // markCaptureStarted()); a failure before that is reported instead.
    if (!startWindowsCapture(&error)) {
        stopSystemAudioCapture();
        cleanupProcess();
        removeRecordingFiles();
        emit recordingFailed(error.isEmpty() ? QStringLiteral("cannot start ffmpeg") : error);
    }
#else
    if (!startFfmpegSegment(segmentTargetPath(), &error)) {
        stopSystemAudioCapture();
        cleanupProcess();
        removeRecordingFiles();
        emit recordingFailed(error.isEmpty() ? QStringLiteral("cannot start ffmpeg") : error);
        return;
    }

    startCountdown();
#endif
}

void VideoRecorder::clearStartupState()
{
    m_starting = false;
    m_stopAfterStart = false;
    m_startupRetried = false;
    if (m_startupTimer) {
        m_startupTimer->stop();
        m_startupTimer->deleteLater();
        m_startupTimer = nullptr;
    }
    m_ffmpegOutputBuffer.clear();
    m_ffmpegLogTail.clear();
    m_videoStartEpochMs = -1;
    m_audioStartEpochMs.store(-1);
}

void VideoRecorder::resolveWindowsAudioInputs()
{
#ifdef Q_OS_WIN
    m_desktopDshowInput.clear();
    m_microphoneDshowInput.clear();
    const bool wantMicrophone = m_microphoneEnabled && m_microphoneVolume > 0;
    const bool wantDesktopDshow = m_desktopAudioEnabled && m_desktopVolume > 0 && !m_systemAudioLoopback;
    if (!wantMicrophone && !wantDesktopDshow)
        return;

    // Normally warm (filled in the background); only a cold cache waits, and
    // only briefly. A device that cannot be resolved is left out with a
    // warning: a wrong dshow name would make FFmpeg fail the whole recording.
    const QList<DshowAudioDevice> devices = cachedDshowAudioDevices(m_ffmpegPath, 2500);
    if (wantMicrophone) {
        const bool useDefault = m_microphoneDevice.isEmpty()
            || m_microphoneDevice.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0;
        const int index = resolveDshowAudioDevice(
            devices, m_microphoneDevice, useDefault ? windowsDefaultCaptureDeviceName() : QString());
        if (index >= 0) {
            m_microphoneDshowInput = dshowAudioInputName(devices.at(index));
        } else {
            m_microphoneEnabled = false;
            m_warnings << TranslationManager::tr("recWarnMicMissing");
            qWarning() << "VideoRecorder: microphone" << m_microphoneDevice
                       << "is not a DirectShow device; recording without it";
        }
    }
    if (wantDesktopDshow) {
        const int index = resolveDshowAudioDevice(devices, m_desktopAudioDevice);
        if (index >= 0) {
            m_desktopDshowInput = dshowAudioInputName(devices.at(index));
        } else {
            m_desktopAudioEnabled = false;
            m_warnings << TranslationManager::tr("recWarnDesktopAudioMissing");
            qWarning() << "VideoRecorder: desktop audio source" << m_desktopAudioDevice
                       << "is not a DirectShow device; recording without it";
        }
    }
#endif
}

bool VideoRecorder::startWindowsCapture(QString *error)
{
#ifdef Q_OS_WIN
    m_starting = false;
    m_ffmpegOutputBuffer.clear();
    m_ffmpegLogTail.clear();
    m_videoStartEpochMs = -1;
    m_audioStartEpochMs.store(-1);
    m_segmentCount = 0;
    if (!startFfmpegSegment(segmentTargetPath(), error))
        return false;

    if (m_systemAudioLoopback) {
        // Loopback capture starts with FFmpeg so no audio is missed; the mux
        // step trims it to the first video frame using gdigrab's wall-clock
        // start time. Assigning to a still-joinable std::thread would call
        // std::terminate, so join any previous one.
        stopSystemAudioCapture();
        m_audioStop.store(false);
        m_audioThread = std::thread(recordWasapiLoopback, m_audioPath, &m_audioStop, &m_audioPaused,
                                    &m_audioStartEpochMs);
    }

    m_starting = true;
    if (m_startupTimer) {
        m_startupTimer->stop();
        m_startupTimer->deleteLater();
    }
    m_startupTimer = new QTimer(this);
    m_startupTimer->setSingleShot(true);
    m_startupTimer->setInterval(8000);
    connect(m_startupTimer, &QTimer::timeout, this, [this]() {
        // Still running but silent (e.g. an FFmpeg without progress output):
        // treat it as started rather than waiting forever.
        if (m_starting && m_process) {
            qWarning() << "VideoRecorder: ffmpeg reported no progress yet; assuming capture is running";
            markCaptureStarted();
        }
    });
    m_startupTimer->start();
    return true;
#else
    Q_UNUSED(error);
    return false;
#endif
}

void VideoRecorder::readFfmpegOutput(QProcess *process)
{
    if (!process)
        return;
    const QByteArray chunk = process->readAll();
    if (process != m_process)
        return;
    m_ffmpegOutputBuffer += chunk;
    qsizetype newline = -1;
    while ((newline = m_ffmpegOutputBuffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_ffmpegOutputBuffer.left(newline)).trimmed();
        m_ffmpegOutputBuffer.remove(0, newline + 1);
        handleFfmpegLine(line);
    }
    if (m_ffmpegOutputBuffer.size() > 64 * 1024)
        m_ffmpegOutputBuffer.clear();
}

void VideoRecorder::handleFfmpegLine(const QString &line)
{
    if (line.isEmpty())
        return;
    if (isFfmpegProgressBlockEnd(line)) {
        if (m_starting)
            markCaptureStarted();
        return;
    }
    if (isFfmpegProgressLine(line))
        return;
    // The first input dump is gdigrab's (input #0).
    if (m_starting && m_videoStartEpochMs < 0) {
        const qint64 start = ffmpegInputStartEpochMs(line);
        if (start > 0)
            m_videoStartEpochMs = start;
    }
    m_ffmpegLogTail.append(line);
    while (m_ffmpegLogTail.size() > 40)
        m_ffmpegLogTail.removeFirst();
}

void VideoRecorder::markCaptureStarted()
{
    if (!m_starting)
        return;
    m_starting = false;
    if (m_startupTimer) {
        m_startupTimer->stop();
        m_startupTimer->deleteLater();
        m_startupTimer = nullptr;
    }
    syncHiddenAttribute(m_segmentPath);
    startCountdown();
    if (m_stopAfterStart) {
        m_stopAfterStart = false;
        stop();
    }
}

void VideoRecorder::handleWindowsStartupFailure(QString reason, const QString &segment)
{
#ifdef Q_OS_WIN
    m_starting = false;
    if (m_startupTimer) {
        m_startupTimer->stop();
        m_startupTimer->deleteLater();
        m_startupTimer = nullptr;
    }
    stopSystemAudioCapture();
    QFile::remove(segment);
    if (!m_audioPath.isEmpty())
        QFile::remove(m_audioPath);

    // A dshow device that is listed but cannot be opened (in use, unplugged,
    // driver error) fails FFmpeg as a whole. Record without it instead.
    const bool microphoneInput = m_microphoneEnabled && !m_microphoneDshowInput.isEmpty();
    const bool desktopInput = m_desktopAudioEnabled && !m_systemAudioLoopback && !m_desktopDshowInput.isEmpty();
    if (!m_startupRetried && (microphoneInput || desktopInput)) {
        m_startupRetried = true;
        qWarning() << "VideoRecorder: ffmpeg failed to start with DirectShow audio; retrying without it:" << reason;
        if (microphoneInput) {
            m_microphoneEnabled = false;
            m_microphoneDshowInput.clear();
        }
        if (desktopInput) {
            m_desktopAudioEnabled = false;
            m_desktopDshowInput.clear();
        }
        m_warnings << TranslationManager::tr("recWarnAudioInputFailed");
        QString error;
        if (startWindowsCapture(&error))
            return;
        if (!error.isEmpty())
            reason = error;
    }

    removeRecordingFiles();
    cleanupProcess();
    emit recordingFailed(reason.isEmpty() ? QStringLiteral("ffmpeg exited during startup") : reason);
#else
    Q_UNUSED(reason);
    Q_UNUSED(segment);
#endif
}

QString VideoRecorder::segmentTargetPath() const
{
    return m_systemAudioLoopback ? m_videoOnlyPath : m_outputPath;
}

QStringList VideoRecorder::ffmpegCaptureArguments(const QString &outputPath) const
{
#ifdef Q_OS_WIN
    WindowsCaptureOptions options;
    options.captureRect = m_captureRect;
    options.fps = m_fps;
    options.crf = m_crf;
    if (m_desktopAudioEnabled && !m_systemAudioLoopback) {
        options.desktopDshowInput = m_desktopDshowInput;
        options.desktopVolume = m_desktopVolume;
    }
    if (m_microphoneEnabled) {
        options.microphoneDshowInput = m_microphoneDshowInput;
        options.microphoneVolume = m_microphoneVolume;
    }
    return windowsFfmpegCaptureArguments(options, outputPath);
#else
    const QRect &captureRect = m_captureRect;
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-hide_banner")
         << QStringLiteral("-loglevel") << QStringLiteral("error");
    QString display = QProcessEnvironment::systemEnvironment().value(QStringLiteral("DISPLAY"));
    if (display.trimmed().isEmpty())
        display = QStringLiteral(":0.0");
    args << QStringLiteral("-f") << QStringLiteral("x11grab")
         << QStringLiteral("-draw_mouse") << QStringLiteral("1")
         << QStringLiteral("-framerate") << QString::number(m_fps)
         << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(captureRect.width()).arg(captureRect.height())
         << QStringLiteral("-i") << QStringLiteral("%1+%2,%3").arg(display).arg(captureRect.x()).arg(captureRect.y());

    QList<QPair<int, int>> audioInputs;
    int nextInputIndex = 1;
    if (m_desktopAudioEnabled && !m_systemAudioLoopback && m_desktopVolume > 0) {
        args << QStringLiteral("-f") << QStringLiteral("pulse")
             << QStringLiteral("-i") << m_desktopAudioDevice;
        audioInputs.append(qMakePair(nextInputIndex++, m_desktopVolume));
    }
    if (m_microphoneEnabled && m_microphoneVolume > 0) {
        args << QStringLiteral("-f") << QStringLiteral("pulse")
             << QStringLiteral("-i") << m_microphoneDevice;
        audioInputs.append(qMakePair(nextInputIndex++, m_microphoneVolume));
    }

    args
         << QStringLiteral("-vf") << QStringLiteral("pad=ceil(iw/2)*2:ceil(ih/2)*2")
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("veryfast")
         << QStringLiteral("-crf") << QString::number(m_crf)
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    args << ffmpegAudioMixArguments(audioInputs);
    args << outputPath;
    return args;
#endif
}

bool VideoRecorder::startFfmpegSegment(const QString &outputPath, QString *error)
{
    QProcess *process = new QProcess(this);
    process->setProgram(m_ffmpegPath);
    process->setArguments(ffmpegCaptureArguments(outputPath));
    process->setProcessChannelMode(QProcess::MergedChannels);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    stopRecorderWhenParentExits(process);
#endif
#ifdef Q_OS_WIN
    // Windows refuses to overwrite a hidden file in place.
    QFile::remove(outputPath);
#endif
    process->start();
    if (!process->waitForStarted(3000)) {
        if (error)
            *error = process->errorString();
        process->kill();
        process->deleteLater();
        QFile::remove(outputPath);
        return false;
    }
#ifdef Q_OS_WIN
    stopWithRecorder(process);
    // Progress lines arrive every half second; read them as they come so the
    // output buffer stays small and startup is detected.
    m_ffmpegOutputBuffer.clear();
    m_ffmpegLogTail.clear();
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        readFfmpegOutput(process);
    });
#endif
    connect(process, &QProcess::finished, this, &VideoRecorder::onProcessFinished);
    m_process = process;
    m_segmentPath = outputPath;
    ++m_segmentCount;
    return true;
}

// Marks the recording as started and drives the elapsed/remaining time
// signals, stopping automatically once the time limit is reached.
void VideoRecorder::startCountdown()
{
    m_recording = true;
    m_elapsed.start();
    emit recordingStarted();
    emit remainingTimeChanged(m_maxSeconds > 0 ? m_maxSeconds : -1);
    emit elapsedTimeChanged(0);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(500);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        if (!m_recording || m_paused)
            return;
        const int elapsed = static_cast<int>(activeElapsedMs() / 1000);
        if (elapsed != m_lastElapsedSeconds) {
            m_lastElapsedSeconds = elapsed;
            emit elapsedTimeChanged(elapsed);
        }
        if (m_maxSeconds > 0) {
            const int remaining = qMax(0, m_maxSeconds - elapsed);
            emit remainingTimeChanged(remaining);
            if (remaining == 0)
                stop();
        }
    });
    m_countdownTimer->start();
}

// Asks the running encoder to finalize its file; used for both stop and
// pause, since a pause closes the current segment.
void VideoRecorder::requestEncoderStop()
{
    // A second SIGINT aborts gst-launch mid-finalize, so signal each process
    // only once.
    if (!m_process || m_process->property("eshotStopRequested").toBool())
        return;
    m_process->setProperty("eshotStopRequested", true);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_usesGStreamer && m_process->processId() > 0)
        QProcess::execute(QStringLiteral("kill"),
                          {QStringLiteral("-INT"), QString::number(m_process->processId())});
    else
        m_process->write("q\n");
#else
    m_process->write("q\n");
#endif
    // Track the process itself so these deferred timers can never tear down
    // the encoder of a later segment or recording.
    const QPointer<QProcess> process(m_process);
#ifdef Q_OS_WIN
    // terminate() only posts WM_CLOSE, which console FFmpeg ignores, so the
    // encoder gets a long grace period to drain after "q". If it still has
    // to be killed, the fragmented segment keeps everything up to its last
    // fragment.
    QTimer::singleShot(windowsEncoderStopGraceMs(m_captureRect.size(), m_fps), this, [process]() {
        if (process && process->state() != QProcess::NotRunning) {
            qWarning() << "VideoRecorder: ffmpeg did not finish after stop; killing it";
            process->kill();
        }
    });
#else
    QTimer::singleShot(2500, this, [process]() {
        if (process && process->state() != QProcess::NotRunning)
            process->terminate();
    });
    QTimer::singleShot(5000, this, [process]() {
        if (process && process->state() != QProcess::NotRunning)
            process->kill();
    });
#endif
}

void VideoRecorder::stop()
{
    // Stop only once: the countdown keeps firing at 0 and users may press Stop
    // repeatedly.
    if (m_starting) {
        // Nothing has been reported as started yet; stop right after it is.
        m_stopAfterStart = true;
        return;
    }
    if (!m_recording || m_stopping)
        return;
    m_stopping = true;
    m_resumePending = false;
    if (m_countdownTimer)
        m_countdownTimer->stop();
    if (m_process) {
        // While paused the current segment is already being closed.
        requestEncoderStop();
        return;
    }
    finishRecording();
}

void VideoRecorder::cancel()
{
    if (!m_recording && !m_starting && !isFinalizing())
        return;
    if (isFinalizing()) {
        cleanupMuxProcess();
        cleanupConcatProcess();
        removeRecordingFiles();
        return;
    }
    m_canceling = true;
    if (m_process) {
        m_process->kill();
    } else {
        stopSystemAudioCapture();
        removeRecordingFiles();
        cleanupProcess();
    }
}

void VideoRecorder::pause()
{
    // Pausing while the encoder finalizes would corrupt the file.
    if (!m_recording || m_paused || m_stopping || !m_process)
        return;
    // The encoder timestamps frames by wall clock, so suspending it would
    // leave the paused time in the file as a frozen frame. Close the segment
    // instead; resume() starts the next one.
    m_pauseStartedMs = m_elapsed.elapsed();
    m_paused = true;
    m_audioPaused.store(true);
    requestEncoderStop();
    emit pausedChanged(true);
}

void VideoRecorder::resume()
{
    if (!m_recording || !m_paused || m_stopping)
        return;
    if (m_process) {
        // The previous segment is still being finalized.
        m_resumePending = true;
        return;
    }
    QString error;
    if (!startNextSegment(&error)) {
        m_warnings << TranslationManager::tr("recWarnResumeFailed")
                          .arg(error.isEmpty() ? QStringLiteral("encoder did not start") : error);
        m_stopping = true;
        finishRecording();
        return;
    }
    m_pausedMs += qMax<qint64>(0, m_elapsed.elapsed() - m_pauseStartedMs);
    m_pauseStartedMs = 0;
    m_paused = false;
    m_audioPaused.store(false);
    emit pausedChanged(false);
}

bool VideoRecorder::startNextSegment(QString *error)
{
    const QString path = recordingSegmentPath(m_segmentDirectory, m_segmentStamp, m_segmentCount);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_usesGStreamer) {
        const QSharedPointer<int> fd = LinuxPortalScreenCast::openPipeWireRemote(m_portalSessionHandle);
        if (!fd || *fd < 0) {
            if (error)
                *error = QStringLiteral("Wayland PipeWire remote could not be opened");
            return false;
        }
        return startGstSegment(*fd, path, error);
    }
#endif
    return startFfmpegSegment(path, error);
}

void VideoRecorder::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    if (!process || process != m_process)
        return;

#ifdef Q_OS_WIN
    readFfmpegOutput(process);
    if (!m_ffmpegOutputBuffer.isEmpty()) {
        handleFfmpegLine(QString::fromUtf8(m_ffmpegOutputBuffer).trimmed());
        m_ffmpegOutputBuffer.clear();
    }
    const QString stderrText = ffmpegFailureReason(m_ffmpegLogTail);
    const bool fragmentedOutput = true;
#else
    const QString stderrText = gstFailureReason(QString::fromLocal8Bit(process->readAll()));
    const bool fragmentedOutput = false;
#endif
    const QString segment = m_segmentPath;
    m_process->deleteLater();
    m_process = nullptr;
    m_segmentPath.clear();

    if (m_canceling) {
        stopSystemAudioCapture();
        removeRecordingFiles();
        cleanupProcess();
        return;
    }

    if (m_starting) {
        // FFmpeg exited before it reported any progress.
        handleWindowsStartupFailure(stderrText, segment);
        return;
    }

    const bool expectedStop = m_stopping || m_paused
        || (m_maxSeconds > 0 && activeElapsedMs() / 1000 >= m_maxSeconds);
    const bool ok = videoRecordingProcessSucceeded(m_usesGStreamer, expectedStop,
                                                   status, exitCode, QFileInfo(segment).size(),
                                                   fragmentedOutput && !m_usesGStreamer);
    if (ok) {
        m_segmentPaths.append(segment);
        syncHiddenAttribute(segment);
        if (fragmentedOutput && !expectedStop && !(status == QProcess::NormalExit && exitCode == 0)) {
            // Kept, but cut short by an encoder failure, not a stop request.
            m_warnings << TranslationManager::tr("recWarnEndNotSaved");
            qWarning() << "VideoRecorder: encoder ended unexpectedly; keeping what was written" << stderrText;
        }
    } else {
        QFile::remove(segment);
        qWarning() << "VideoRecorder: dropped unusable recording segment" << segment << stderrText;
    }

    if (m_paused && !m_stopping) {
        if (m_resumePending) {
            m_resumePending = false;
            resume();
        }
        return;
    }

    if (m_segmentPaths.isEmpty()) {
        stopSystemAudioCapture();
        removeRecordingFiles();
        cleanupProcess();
        const QString processName = m_usesGStreamer ? QStringLiteral("gstreamer") : QStringLiteral("ffmpeg");
        emit recordingFailed(stderrText.isEmpty() ? QStringLiteral("%1 exited with code %2").arg(processName).arg(exitCode) : stderrText);
        return;
    }
    if (!ok)
        m_warnings << TranslationManager::tr("recWarnEndNotSaved");
    finishRecording();
}

// Called once the last segment is closed: joins the segments into the video
// file and continues with the system-audio mux where needed.
void VideoRecorder::finishRecording()
{
    m_recordedMs = activeElapsedMs();
    stopSystemAudioCapture();
    cleanupProcess();

    if (m_segmentPaths.isEmpty()) {
        removeRecordingFiles();
        emit recordingFailed(QStringLiteral("no video was recorded"));
        return;
    }
#ifdef Q_OS_WIN
    // Segments are fragmented MP4s: rewrite even a single one as a regular
    // MP4 with +faststart. With system audio the mux step does that instead.
    const bool remuxSingleSegment = !m_systemAudioLoopback;
#else
    const bool remuxSingleSegment = false;
#endif
    if (m_segmentPaths.size() > 1 || remuxSingleSegment) {
        if (!startSegmentConcat())
            keepSegmentsAfterFailedConcat(QStringLiteral("cannot start the join step"));
        return;
    }

    const QString target = segmentTargetPath();
    const QString segment = m_segmentPaths.takeFirst();
    if (segment != target) {
        QFile::remove(target);
        if (!renameRecordingFile(segment, target)) {
            m_segmentPaths.append(segment);
            keepSegmentsAfterFailedConcat(QStringLiteral("cannot rename the recording"));
            return;
        }
    }
    finishVideo();
}

bool VideoRecorder::startSegmentConcat()
{
    cleanupConcatProcess();
    const QString target = segmentTargetPath();
    // The first segment is written straight to the target; move it aside so
    // all inputs share the segment naming and the target can be rewritten.
    if (m_segmentPaths.first() == target) {
        const QString firstSegment = recordingSegmentPath(m_segmentDirectory, m_segmentStamp, 0);
        QFile::remove(firstSegment);
        if (!renameRecordingFile(target, firstSegment))
            return false;
        m_segmentPaths[0] = firstSegment;
    }
    QFile::remove(target);

    QString program;
    QStringList args;
    if (m_usesGStreamer) {
        // Wayland recordings only require GStreamer; FFmpeg may be missing.
        program = m_gstPath;
        args = gstConcatArguments(recordingSegmentPattern(m_segmentDirectory, m_segmentStamp),
                                  target, m_gstHasAudio);
    } else {
        m_concatListPath = QDir(m_segmentDirectory).filePath(
            QStringLiteral(".eshot_segments_%1.txt").arg(m_segmentStamp));
        QFile list(m_concatListPath);
        if (!list.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        list.write(ffmpegConcatList(m_segmentPaths));
        list.close();
        syncHiddenAttribute(m_concatListPath);
        program = m_ffmpegPath;
        args = ffmpegConcatArguments(m_concatListPath, target);
    }

    m_concatProcess = new QProcess(this);
    m_concatProcess->setProgram(program);
    m_concatProcess->setArguments(args);
    m_concatProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_concatProcess, &QProcess::finished, this, &VideoRecorder::onConcatFinished);
    connect(m_concatProcess, &QProcess::started, this, [process = m_concatProcess]() {
        stopWithRecorder(process);
    });
    connect(m_concatProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_concatProcess)
            return;
        const QString reason = m_concatProcess->errorString();
        cleanupConcatProcess();
        keepSegmentsAfterFailedConcat(reason);
    });
    m_concatTimeout = new QTimer(this);
    m_concatTimeout->setSingleShot(true);
    m_concatTimeout->setInterval(segmentConcatTimeoutMs(m_recordedMs));
    connect(m_concatTimeout, &QTimer::timeout, this, [this]() {
        if (!m_concatProcess)
            return;
        m_concatProcess->setProperty("eshotTimedOut", true);
        m_concatProcess->kill();
    });
    m_concatTimeout->start();
    m_concatProcess->start();
    return true;
}

void VideoRecorder::onConcatFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_concatProcess)
        return;
    const bool timedOut = m_concatProcess->property("eshotTimedOut").toBool();
    const QString log = QString::fromLocal8Bit(m_concatProcess->readAll()).trimmed();
    const bool ok = !timedOut && status == QProcess::NormalExit && exitCode == 0
        && QFileInfo(segmentTargetPath()).size() > 0;
    cleanupConcatProcess();

    if (!ok) {
        keepSegmentsAfterFailedConcat(timedOut
            ? QStringLiteral("timed out")
            : (log.isEmpty() ? QStringLiteral("exit code %1").arg(exitCode) : log));
        return;
    }
    for (const QString &segment : std::as_const(m_segmentPaths))
        QFile::remove(segment);
    m_segmentPaths.clear();
    if (!m_concatListPath.isEmpty()) {
        QFile::remove(m_concatListPath);
        m_concatListPath.clear();
    }
    syncHiddenAttribute(segmentTargetPath());
    finishVideo();
}

void VideoRecorder::keepSegmentsAfterFailedConcat(const QString &reason)
{
    // A single segment only needed a remux; the unremuxed file is still a
    // playable (fragmented) MP4, so use it as the result.
    if (m_segmentPaths.size() == 1) {
        const QString segment = m_segmentPaths.first();
        const QString target = segmentTargetPath();
        if (segment != target)
            QFile::remove(target);
        if (segment == target || renameRecordingFile(segment, target)) {
            qWarning() << "VideoRecorder: kept the recording without the final remux:" << reason;
            m_segmentPaths.clear();
            if (!m_concatListPath.isEmpty()) {
                QFile::remove(m_concatListPath);
                m_concatListPath.clear();
            }
            finishVideo();
            return;
        }
    }
    // Never throw away recorded footage: keep each part next to the output.
    if (!m_segmentPaths.contains(segmentTargetPath()))
        QFile::remove(segmentTargetPath());
    if (!m_concatListPath.isEmpty()) {
        QFile::remove(m_concatListPath);
        m_concatListPath.clear();
    }
    if (!m_audioPath.isEmpty())
        QFile::remove(m_audioPath);
    QStringList kept;
    int partNumber = 1;
    for (const QString &segment : std::as_const(m_segmentPaths)) {
        const QString part = recordingPartPath(m_outputPath, partNumber++);
        if (!QFileInfo::exists(part) && renameRecordingFile(segment, part))
            kept << QDir::toNativeSeparators(part);
        else
            kept << QDir::toNativeSeparators(segment);
    }
    m_segmentPaths.clear();
    emit recordingFailed(TranslationManager::tr("recPartsNotJoined").arg(reason) + QStringLiteral("\n")
                         + TranslationManager::tr("recVideoKeptAt").arg(kept.join(QStringLiteral(", "))));
}

void VideoRecorder::cleanupConcatProcess()
{
    if (m_concatTimeout) {
        m_concatTimeout->stop();
        m_concatTimeout->deleteLater();
        m_concatTimeout = nullptr;
    }
    if (m_concatProcess) {
        if (m_concatProcess->state() != QProcess::NotRunning)
            m_concatProcess->kill();
        m_concatProcess->deleteLater();
        m_concatProcess = nullptr;
    }
}

// The video file is complete; add the separately captured system audio if
// there is any and report the result.
void VideoRecorder::finishVideo()
{
    const QString output = m_outputPath;
    if (m_systemAudioLoopback) {
        if (!QFileInfo::exists(m_videoOnlyPath)) {
            QFile::remove(m_audioPath);
            emit recordingFailed(QStringLiteral("failed to mux system audio"));
            return;
        }
        if (!QFileInfo::exists(m_audioPath) || QFileInfo(m_audioPath).size() < 128) {
            useVideoWithoutSystemAudio(TranslationManager::tr("recWarnSystemAudioCapture"));
            return;
        }
        if (startSystemAudioMux())
            return;
        useVideoWithoutSystemAudio(TranslationManager::tr("recWarnSystemAudioAdd"));
        return;
    }

    emit recordingStopped(output);
}

void VideoRecorder::useVideoWithoutSystemAudio(const QString &warning)
{
    QFile::remove(m_outputPath);
    const bool fallbackOk = renameRecordingFile(m_videoOnlyPath, m_outputPath);
    QFile::remove(m_audioPath);
    if (!fallbackOk) {
        emit recordingFailed(QStringLiteral("failed to mux system audio"));
        return;
    }
    m_warnings << warning;
    qWarning() << "VideoRecorder:" << warning;
    emit recordingStopped(m_outputPath);
}

void VideoRecorder::stopSystemAudioCapture()
{
    m_audioStop.store(true);
    if (m_audioThread.joinable())
        m_audioThread.join();
}

void VideoRecorder::removeRecordingFiles()
{
    // Output plus the hidden intermediates (pause segments, the concat list
    // and the loopback .eshot_video_*.mp4 / .eshot_audio_*.wav); call after
    // the audio thread has been joined.
    QStringList paths = {m_outputPath, m_videoOnlyPath, m_audioPath, m_segmentPath, m_concatListPath};
    paths << m_segmentPaths;
    for (const QString &path : std::as_const(paths)) {
        if (!path.isEmpty())
            QFile::remove(path);
    }
    m_segmentPaths.clear();
    m_segmentPath.clear();
    m_concatListPath.clear();
}

bool VideoRecorder::startSystemAudioMux()
{
    if (m_videoOnlyPath.isEmpty() || m_audioPath.isEmpty())
        return false;
    if (!QFileInfo::exists(m_videoOnlyPath))
        return false;
    cleanupMuxProcess();
    QFile::remove(m_outputPath);
    m_muxProcess = new QProcess(this);
    m_muxProcess->setProgram(m_ffmpegPath);
    const bool videoHasAudio = m_microphoneEnabled && m_microphoneVolume > 0;
    const qint64 audioLeadMs = systemAudioLeadMs(m_audioStartEpochMs.load(), m_videoStartEpochMs);
    m_muxProcess->setArguments(systemAudioMuxArguments(m_videoOnlyPath, m_audioPath, m_desktopVolume,
                                                       videoHasAudio, audioLeadMs, m_outputPath));
    m_muxProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_muxProcess, &QProcess::finished, this, &VideoRecorder::onMuxFinished);
    connect(m_muxProcess, &QProcess::started, this, [process = m_muxProcess]() {
        stopWithRecorder(process);
    });
    connect(m_muxProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_muxProcess)
            return;
        cleanupMuxProcess();
        useVideoWithoutSystemAudio(TranslationManager::tr("recWarnSystemAudioAdd"));
    });
    m_muxTimeout = new QTimer(this);
    m_muxTimeout->setSingleShot(true);
    m_muxTimeout->setInterval(videoMuxTimeoutMs(m_recordedMs));
    connect(m_muxTimeout, &QTimer::timeout, this, [this]() {
        if (!m_muxProcess)
            return;
        m_muxProcess->setProperty("eshotTimedOut", true);
        m_muxProcess->kill();
    });
    m_muxTimeout->start();
    m_muxProcess->start();
    return true;
}

void VideoRecorder::onMuxFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_muxProcess)
        return;
    const bool timedOut = m_muxProcess->property("eshotTimedOut").toBool();
    const VideoMuxCompletionAction action = videoMuxCompletionAction(
        status == QProcess::NormalExit && !timedOut,
        exitCode, QFileInfo(m_outputPath).size());
    cleanupMuxProcess();

    if (action == VideoMuxCompletionAction::UseMuxedOutput) {
        QFile::remove(m_videoOnlyPath);
        QFile::remove(m_audioPath);
        emit recordingStopped(m_outputPath);
        return;
    }
    useVideoWithoutSystemAudio(timedOut
        ? TranslationManager::tr("recWarnSystemAudioTimeout")
        : TranslationManager::tr("recWarnSystemAudioAdd"));
}

void VideoRecorder::cleanupMuxProcess()
{
    if (m_muxTimeout) {
        m_muxTimeout->stop();
        m_muxTimeout->deleteLater();
        m_muxTimeout = nullptr;
    }
    if (m_muxProcess) {
        if (m_muxProcess->state() != QProcess::NotRunning)
            m_muxProcess->kill();
        m_muxProcess->deleteLater();
        m_muxProcess = nullptr;
    }
}

bool VideoRecorder::prepareSource(const QRect &captureRect, const QRect &displayRect)
{
    discardPreparedSource();
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (isRecording() || !LinuxPortalScreenCast::isWaylandSession())
        return true;
    if (gstLaunchPath().isEmpty()) {
        emit recordingFailed(QStringLiteral("gstreamer not found"));
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

void VideoRecorder::discardPreparedSource()
{
    if (!m_preparedStream.sessionHandle.isEmpty())
        LinuxPortalScreenCast::closeSession(m_preparedStream.sessionHandle);
    m_preparedStream = {};
}

bool VideoRecorder::startWaylandPortalRecording(const QRect &captureRect)
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
    // A stream picked by prepareSource() already covers the region.
    LinuxPortalScreenCast::Stream stream = m_preparedStream.isValid()
        ? std::exchange(m_preparedStream, {})
        : LinuxPortalScreenCast::selectStream(nullptr, 120000, persistenceId);
    if (!stream.isValid()) {
        emit recordingFailed(QStringLiteral("Wayland screen recording permission was not granted"));
        return false;
    }

    PortalCropGeometry crop = portalCropGeometry(
        captureRect, m_displayRect, stream.position, stream.size, captureRect.size());
    if (!crop.valid && stream.usedRestoreToken) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        LinuxPortalScreenCast::clearRestoreToken(persistenceId);
        stream = LinuxPortalScreenCast::selectStream(nullptr, 120000, persistenceId);
        if (stream.isValid()) {
            crop = portalCropGeometry(
                captureRect, m_displayRect, stream.position, stream.size, captureRect.size());
        }
    }
    if (!stream.isValid() || !crop.valid) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        emit recordingFailed(QStringLiteral("Wayland recording source does not contain the selected region"));
        return false;
    }
    m_portalSessionHandle = stream.sessionHandle;
    m_gstPath = gst;
    m_portalSourcePath = pipeWireSourcePath(stream.nodeId, stream.pipewireSerial);
    m_portalCrop = crop;

    const bool wantDesktopAudio = m_desktopAudioEnabled && m_desktopVolume > 0 && !m_desktopAudioDevice.isEmpty();
    const bool wantMicrophoneAudio = m_microphoneEnabled && m_microphoneVolume > 0 && !m_microphoneDevice.isEmpty();
    m_gstHasAudio = wantDesktopAudio || wantMicrophoneAudio;
    m_gstAacEncoder = m_gstHasAudio ? discoverGstAacEncoder() : QString();
    if (m_gstHasAudio && m_gstAacEncoder.isEmpty()) {
        cleanupProcess();
        emit recordingFailed(QStringLiteral("No GStreamer AAC encoder is installed"));
        return false;
    }

    m_usesGStreamer = true;
    QString error;
    if (!startGstSegment(stream.remoteFd(), m_outputPath, &error)) {
        cleanupProcess();
        emit recordingFailed(error.isEmpty() ? QStringLiteral("cannot start gstreamer") : error);
        return false;
    }

    startCountdown();
    return true;
#else
    Q_UNUSED(captureRect);
    return false;
#endif
}

QStringList VideoRecorder::gstCaptureArguments(int pipewireFd, const QString &outputPath) const
{
    const PortalCropGeometry &crop = m_portalCrop;
    QStringList args;
    args << QStringLiteral("-e")
         << QStringLiteral("mp4mux")
         << QStringLiteral("name=mux")
         << QStringLiteral("!")
         << QStringLiteral("filesink")
         << QStringLiteral("location=%1").arg(outputPath)
         << QStringLiteral("pipewiresrc")
         << QStringLiteral("fd=%1").arg(pipewireFd)
         << m_portalSourcePath
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
         << QStringLiteral("pass=qual")
         << QStringLiteral("quantizer=%1").arg(qBound(18, m_crf, 32))
         << QStringLiteral("!")
         << QStringLiteral("h264parse")
         << QStringLiteral("!")
         << QStringLiteral("queue")
         << QStringLiteral("!")
         << QStringLiteral("mux.");

    const bool wantDesktopAudio = m_desktopAudioEnabled && m_desktopVolume > 0 && !m_desktopAudioDevice.isEmpty();
    const bool wantMicrophoneAudio = m_microphoneEnabled && m_microphoneVolume > 0 && !m_microphoneDevice.isEmpty();
    args << waylandRecordingAudioArguments(
        wantDesktopAudio, m_desktopVolume, m_desktopAudioDevice,
        wantMicrophoneAudio, m_microphoneVolume, m_microphoneDevice,
        m_gstAacEncoder);
    return args;
}

bool VideoRecorder::startGstSegment(int pipewireFd, const QString &outputPath, QString *error)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QProcess *process = new QProcess(this);
    process->setProgram(m_gstPath);
    process->setArguments(gstCaptureArguments(pipewireFd, outputPath));
    process->setProcessChannelMode(QProcess::MergedChannels);
    stopRecorderWhenParentExits(process);
    if (!configurePipeWireRemote(process, pipewireFd)) {
        process->deleteLater();
        if (error)
            *error = QStringLiteral("Wayland PipeWire remote could not be opened");
        return false;
    }
    process->start();
    if (!process->waitForStarted(3000)) {
        if (error)
            *error = process->errorString();
        process->kill();
        process->deleteLater();
        return false;
    }
    if (process->waitForFinished(700)) {
        if (error) {
            const QString reason = QString::fromLocal8Bit(process->readAll()).trimmed();
            *error = reason.isEmpty() ? QStringLiteral("gstreamer pipeline exited during startup") : reason;
        }
        process->deleteLater();
        QFile::remove(outputPath);
        return false;
    }
    // gst-launch may exit through the SIGINT sent by stop() or pause();
    // onProcessFinished() judges the file either way.
    connect(process, &QProcess::finished, this, &VideoRecorder::onProcessFinished);
    m_process = process;
    m_segmentPath = outputPath;
    ++m_segmentCount;
    return true;
#else
    Q_UNUSED(pipewireFd);
    Q_UNUSED(outputPath);
    Q_UNUSED(error);
    return false;
#endif
}

QString VideoRecorder::ffmpegPath() const
{
    return ComponentPaths::ffmpegPath();
}

QString VideoRecorder::gstLaunchPath() const
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

QString VideoRecorder::makeDefaultOutputPath() const
{
    const auto fileExists = [](const QString &path) { return QFileInfo::exists(path); };
    QSettings s("EShot", "EShot");
    QStringList candidates;
    QString configuredDir = s.contains("videoSavePath")
        ? s.value("videoSavePath").toString().trimmed()
        : QDir(ComponentPaths::defaultSaveDirectory()).filePath(QStringLiteral("Videos"));
    if (configuredDir.isEmpty())
        configuredDir = s.value("savePath").toString().trimmed();
    candidates << (configuredDir.isEmpty() ? ComponentPaths::defaultSaveDirectory() : configuredDir)
               << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
               << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
               << QStandardPaths::writableLocation(QStandardPaths::TempLocation)
               << QDir::homePath();

    const QString fileName = QStringLiteral("EShot_Video_%1.mp4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));

    for (const QString &candidate : candidates) {
        if (candidate.trimmed().isEmpty())
            continue;
        QDir dir(candidate);
        if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
            continue;
        const QString probePath = dir.filePath(QStringLiteral(".eshot_write_test.tmp"));
        QFile probe(probePath);
        if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate))
            continue;
        probe.close();
        QFile::remove(probePath);
        return uniqueRecordingOutputPath(dir.filePath(fileName), fileExists);
    }
    return uniqueRecordingOutputPath(QDir(QDir::tempPath()).filePath(fileName), fileExists);
}

qint64 VideoRecorder::activeElapsedMs() const
{
    if (!m_elapsed.isValid())
        return 0;
    qint64 paused = m_pausedMs;
    if (m_paused)
        paused += qMax<qint64>(0, m_elapsed.elapsed() - m_pauseStartedMs);
    return qMax<qint64>(0, m_elapsed.elapsed() - paused);
}

void VideoRecorder::cleanupProcess()
{
    if (!m_portalSessionHandle.isEmpty()) {
        LinuxPortalScreenCast::closeSession(m_portalSessionHandle);
        m_portalSessionHandle.clear();
    }
    m_recording = false;
    m_paused = false;
    m_resumePending = false;
    m_stopping = false;
    m_canceling = false;
    m_starting = false;
    m_stopAfterStart = false;
    if (m_startupTimer) {
        m_startupTimer->stop();
        m_startupTimer->deleteLater();
        m_startupTimer = nullptr;
    }
    m_audioPaused.store(false);
    if (m_countdownTimer) {
        m_countdownTimer->stop();
        m_countdownTimer->deleteLater();
        m_countdownTimer = nullptr;
    }
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}
