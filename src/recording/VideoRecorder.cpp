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
#include "WindowsCaptureBackend.h"

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
#include <sys/types.h>
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
    // The first entry is what "default" records. It is left out when pactl
    // lists no microphone at all, so the missing-microphone warning fires;
    // without pactl the default source is used unverified.
    bool discoveryOk = false;
    const QList<QPair<QString, QString>> microphones = discoverLinuxMicrophoneDevices(&discoveryOk);
    const LinuxMicrophoneResolution resolved = resolveLinuxMicrophoneDevice(
        QStringLiteral("default"), microphones, discoveryOk);
    if (resolved.missing)
        return {};
    QStringList devices{resolved.device};
    for (const auto &device : microphones) devices << device.second;
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

// Writes the loopback audio of the active (not paused) time to `path` and
// records where each active span starts and ends in the file, in sample
// frames, so the mux step can line every span up with its video segment.
void recordWasapiLoopback(const QString &path, std::atomic_bool *stopFlag,
                          std::atomic_bool *pauseFlag, std::atomic<qint64> *startedEpochMs,
                          int *sampleRate, QList<QPair<qint64, qint64>> *spans)
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
    const quint64 blockAlign = qMax<quint64>(1, format->nBlockAlign);
    *sampleRate = static_cast<int>(format->nSamplesPerSec);
    qint64 spanStartFrame = 0;

    while (!stopFlag->load()) {
        Sleep(10);
        const bool pauseRequested = pauseFlag->load();
        if (pauseRequested != paused) {
            paused = pauseRequested;
            const qint64 frame = static_cast<qint64>(dataBytes / blockAlign);
            if (paused) {
                pauseStartedTick = GetTickCount64();
                spans->append(qMakePair(spanStartFrame, frame));
            } else {
                pausedTicks += GetTickCount64() - pauseStartedTick;
                spanStartFrame = frame;
            }
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

    if (!paused)
        spans->append(qMakePair(spanStartFrame, static_cast<qint64>(dataBytes / blockAlign)));
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
    // have to wait for the enumeration; likewise the capture backend probe.
    const QString ffmpeg = ffmpegPath();
    refreshDshowAudioDevices(ffmpeg, true);
    requestWindowsCaptureProbe(ffmpeg);
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
    m_desktopAudioDevice = linuxDesktopAudioDevice(m_desktopAudioDevice);
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
    // Kept for every segment of this recording, so they can be joined.
    m_windowsPlan = windowsVideoPlanForRecording(m_ffmpegPath, m_captureRect, false);
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
    m_windowsPlan = WindowsVideoPlan();
    m_segmentRecords.clear();
    m_segmentOutTimeUs = -1;
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
    m_segmentRecords.clear();
    m_audioPaused.store(false);
    if (!startFfmpegSegment(segmentTargetPath(), error))
        return false;

    if (m_systemAudioLoopback) {
        // Loopback capture starts with FFmpeg so no audio is missed; the mux
        // step trims every active span to its video segment (see
        // startSystemAudioMux()). Assigning to a still-joinable std::thread
        // would call std::terminate, so join any previous one first.
        stopSystemAudioCapture();
        m_audioSampleRate = 0;
        m_audioSpans.clear();
        m_audioStop.store(false);
        m_audioThread = std::thread(recordWasapiLoopback, m_audioPath, &m_audioStop, &m_audioPaused,
                                    &m_audioStartEpochMs, &m_audioSampleRate, &m_audioSpans);
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
    if (isFfmpegProgressLine(line)) {
        const qint64 outTimeUs = ffmpegProgressOutTimeUs(line);
        if (outTimeUs >= 0)
            m_segmentOutTimeUs = outTimeUs;
        return;
    }
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

    // Desktop Duplication or the hardware encoder can be unavailable even
    // though the probe passed (remote session, protected content, encoder
    // sessions in use): record this one, and the rest of this run, with GDI.
    // A failing DirectShow input is handled below instead.
    if (m_windowsPlan.source == WindowsCaptureSource::DdaGrab
        && !reason.contains(QStringLiteral("dshow"), Qt::CaseInsensitive)) {
        qWarning() << "VideoRecorder: ddagrab capture failed to start; retrying with gdigrab:" << reason;
        disableWindowsDdaGrabForSession(m_ffmpegPath);
        m_windowsPlan = WindowsVideoPlan();
        QString error;
        if (startWindowsCapture(&error))
            return;
        if (!error.isEmpty())
            reason = error;
    }

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
    options.plan = m_windowsPlan;
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
    m_segmentOutTimeUs = -1;
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
    if (m_usesGStreamer) {
        // gst-launch only turns SIGINT into EOS; SIGTERM would kill it
        // before mp4mux writes the last fragment. A process that is not
        // running yet (or never started) is killed outright.
        const QPointer<QProcess> process(m_process);
        if (m_process->processId() > 0 && m_process->state() == QProcess::Running)
            ::kill(static_cast<pid_t>(m_process->processId()), SIGINT);
        else
            m_process->kill();
        QTimer::singleShot(gstStopGraceMs(activeElapsedMs()), this, [process]() {
            if (process && process->state() != QProcess::NotRunning)
                process->kill();
        });
        return;
    }
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
        // While paused the current segment is already being closed. The
        // system audio ends with the video, not when the encoder has drained.
        m_audioPaused.store(true);
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
    if (!m_recording || m_paused || m_stopping || !m_process || m_starting || m_gstStarting)
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
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_usesGStreamer) {
        resumePortalRecording();
        return;
    }
#endif
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
    // Portal (GStreamer) recordings resume through resumePortalRecording().
    const QString path = recordingSegmentPath(m_segmentDirectory, m_segmentStamp, m_segmentCount);
    return startFfmpegSegment(path, error);
}

void VideoRecorder::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    if (!process || process != m_process)
        return;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_usesGStreamer) {
        handleGstSegmentEnded(exitCode, status, QString());
        return;
    }
#endif

#ifdef Q_OS_WIN
    // No video is captured any more, so no system audio either (for a pause
    // or stop this already happened when it was requested).
    m_audioPaused.store(true);
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
    m_segmentRecords.append(SegmentRecord{ok, ok && m_segmentOutTimeUs >= 0 ? m_segmentOutTimeUs / 1000 : -1});
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
    // Every started segment has one active audio span (the audio thread has
    // been joined). If they do not pair up, only the start is aligned.
    if (!m_segmentRecords.isEmpty() && m_audioSpans.size() == m_segmentRecords.size()
        && m_audioSampleRate > 0) {
        QList<SystemAudioSpan> spans;
        for (int i = 0; i < m_segmentRecords.size(); ++i) {
            if (!m_segmentRecords.at(i).kept)
                continue;
            SystemAudioSpan span;
            span.startFrame = m_audioSpans.at(i).first;
            span.endFrame = m_audioSpans.at(i).second;
            span.videoDurationMs = m_segmentRecords.at(i).durationMs;
            spans.append(span);
        }
        const qint64 firstLeadMs = m_segmentRecords.first().kept ? audioLeadMs : 0;
        m_muxProcess->setArguments(systemAudioSpanMuxArguments(m_videoOnlyPath, m_audioPath, m_desktopVolume,
                                                               videoHasAudio, spans, m_audioSampleRate,
                                                               firstLeadMs, m_outputPath));
    } else {
        qWarning() << "VideoRecorder: system audio spans" << m_audioSpans.size()
                   << "do not match the video segments" << m_segmentRecords.size()
                   << "; aligning the start only";
        m_muxProcess->setArguments(systemAudioMuxArguments(m_videoOnlyPath, m_audioPath, m_desktopVolume,
                                                           videoHasAudio, audioLeadMs, m_outputPath));
    }
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
    // Audio settings are only known in start(); check the video elements
    // here so a missing encoder is reported before the picker opens.
    const QString missing = portalPreflightError(false);
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

void VideoRecorder::discardPreparedSource()
{
    if (!m_preparedStream.sessionHandle.isEmpty())
        LinuxPortalScreenCast::closeSession(m_preparedStream.sessionHandle);
    m_preparedStream = {};
}

QString VideoRecorder::portalPreflightError(bool includeAudio)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString gst = gstLaunchPath();
    if (gst.isEmpty())
        return QStringLiteral("gstreamer not found");

    const bool wantDesktopAudio = m_desktopAudioEnabled && m_desktopVolume > 0 && !m_desktopAudioDevice.isEmpty();
    const bool wantMicrophoneAudio = m_microphoneEnabled && m_microphoneVolume > 0 && !m_microphoneDevice.isEmpty();
    const bool withAudio = includeAudio && (wantDesktopAudio || wantMicrophoneAudio);

    bool catalogOk = false;
    const QStringList elements = cachedGstElements(gstInspectPathFor(gst), &catalogOk);
    if (!catalogOk) {
        // gst-inspect-1.0 could not run: keep the previous behaviour and let
        // gst-launch report problems, but an AAC encoder cannot be guessed.
        m_gstH264Encoder = QStringLiteral("x264enc");
        m_gstAacEncoder.clear();
        return withAudio ? QStringLiteral("No GStreamer AAC encoder is installed") : QString();
    }
    const QStringList missing = missingPortalRecordingElements(
        elements, withAudio, wantDesktopAudio && wantMicrophoneAudio, true);
    if (!missing.isEmpty()) {
        // Check again next time; the user may install the plugins meanwhile.
        invalidateGstElementCache();
        return gstMissingElementsReason(missing);
    }
    m_gstH264Encoder = preferredGstH264Encoder(elements);
    m_gstAacEncoder = withAudio ? preferredGstAacEncoder(elements) : QString();
    return {};
#else
    Q_UNUSED(includeAudio);
    return {};
#endif
}

bool VideoRecorder::startWaylandPortalRecording(const QRect &captureRect)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const bool wantDesktopAudio = m_desktopAudioEnabled && m_desktopVolume > 0 && !m_desktopAudioDevice.isEmpty();
    const bool wantMicrophoneAudio = m_microphoneEnabled && m_microphoneVolume > 0 && !m_microphoneDevice.isEmpty();
    m_gstHasAudio = wantDesktopAudio || wantMicrophoneAudio;

    // Report missing GStreamer elements before the source picker opens.
    QString error = portalPreflightError(true);
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
    m_usesGStreamer = true;
    m_recording = true;
    m_gstStarting = true;
    m_resuming = false;

    // Stopping the share from the desktop ends the recording and keeps it.
    m_sessionWatcher = new LinuxPortalSessionWatcher(m_portalSessionHandle, this);
    connect(m_sessionWatcher, &LinuxPortalSessionWatcher::closed,
            this, &VideoRecorder::onPortalSessionClosed);

    // The selection overlay or the start countdown has just been hidden;
    // give the compositor a moment so they do not end up in the first frame.
    const int generation = m_startGeneration;
    QTimer::singleShot(250, this, [this, generation, remoteFd]() {
        if (generation != m_startGeneration || !m_recording || m_canceling || m_stopping)
            return;
        QString startError;
        if (!startGstSegment(remoteFd, m_outputPath, &startError)) {
            cleanupProcess();
            removeRecordingFiles();
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

QStringList VideoRecorder::gstCaptureArguments(int pipewireFd, const QString &outputPath) const
{
    PortalVideoPipelineOptions options;
    options.pipewireFd = pipewireFd;
    options.sourcePath = m_portalSourcePath;
    options.crop = m_portalCrop;
    options.fps = m_fps;
    options.crf = m_crf;
    options.h264Encoder = m_gstH264Encoder;

    const bool wantDesktopAudio = m_desktopAudioEnabled && m_desktopVolume > 0 && !m_desktopAudioDevice.isEmpty();
    const bool wantMicrophoneAudio = m_microphoneEnabled && m_microphoneVolume > 0 && !m_microphoneDevice.isEmpty();
    return portalRecordingPipelineArguments(
        options, outputPath,
        waylandRecordingAudioArguments(
            wantDesktopAudio, m_desktopVolume, m_desktopAudioDevice,
            wantMicrophoneAudio, m_microphoneVolume, m_microphoneDevice,
            m_gstAacEncoder));
}

bool VideoRecorder::startGstSegment(const QSharedPointer<int> &pipewireFd, const QString &outputPath,
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
    process->setArguments(gstCaptureArguments(*pipewireFd, outputPath));
    process->setProcessChannelMode(QProcess::MergedChannels);
    // English output: the PLAYING marker and ERROR lines are parsed.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANGUAGE"), QStringLiteral("en"));
    process->setProcessEnvironment(env);
    stopRecorderWhenParentExits(process);
    if (!configurePipeWireRemote(process, *pipewireFd)) {
        process->deleteLater();
        if (error)
            *error = QStringLiteral("Wayland PipeWire remote could not be opened");
        return false;
    }

    m_process = process;
    m_segmentPath = outputPath;
    m_segmentRemoteFd = pipewireFd;
    m_processLog.clear();
    m_segmentPlaying = false;
    m_segmentRunClock.start();
    ++m_segmentCount;

    connect(process, &QProcess::readyRead, this, [this, process]() {
        if (process != m_process)
            return;
        m_processLog += process->readAll();
        if (!m_segmentPlaying && gstPipelineReachedPlaying(QString::fromLocal8Bit(m_processLog)))
            onGstSegmentPlaying();
        if (m_processLog.size() > 64 * 1024)
            m_processLog = m_processLog.right(32 * 1024);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError processError) {
        if (processError == QProcess::FailedToStart && process == m_process)
            handleGstSegmentEnded(-1, QProcess::CrashExit, process->errorString());
    });
    // gst-launch may exit through the SIGINT sent by stop() or pause();
    // handleGstSegmentEnded() judges the file either way.
    connect(process, &QProcess::finished, this, &VideoRecorder::onProcessFinished);
    process->start();

    // Without the PLAYING line (unexpected output) count a pipeline that is
    // still alive after a few seconds as running.
    const QPointer<QProcess> guard(process);
    QTimer::singleShot(3000, this, [this, guard]() {
        if (guard && guard == m_process && !m_segmentPlaying
            && guard->state() == QProcess::Running)
            onGstSegmentPlaying();
    });
    return true;
#else
    Q_UNUSED(pipewireFd);
    Q_UNUSED(outputPath);
    Q_UNUSED(error);
    return false;
#endif
}

void VideoRecorder::onGstSegmentPlaying()
{
    if (m_segmentPlaying)
        return;
    m_segmentPlaying = true;
    // The child inherited the PipeWire remote; our copy is no longer needed.
    m_segmentRemoteFd.reset();
    if (m_gstStarting) {
        // The elapsed clock starts with the first captured frame, not with
        // the portal dialog or the process launch.
        m_gstStarting = false;
        startCountdown();
        return;
    }
    if (m_resuming) {
        m_resuming = false;
        m_pausedMs += qMax<qint64>(0, m_elapsed.elapsed() - m_pauseStartedMs);
        m_pauseStartedMs = 0;
        m_paused = false;
        m_audioPaused.store(false);
        emit pausedChanged(false);
    }
}

void VideoRecorder::handleGstSegmentEnded(int exitCode, QProcess::ExitStatus status,
                                          const QString &launchError)
{
    QProcess *process = m_process;
    if (!process)
        return;
    m_processLog += process->readAll();
    const QString failureText = launchError.isEmpty()
        ? gstFailureReason(QString::fromLocal8Bit(m_processLog)) : launchError;
    const qint64 runMs = m_segmentRunClock.isValid() ? m_segmentRunClock.elapsed() : -1;
    const bool reachedPlaying = m_segmentPlaying;
    const QString segment = m_segmentPath;
    disconnect(process, nullptr, this, nullptr);
    process->deleteLater();
    m_process = nullptr;
    m_segmentPath.clear();
    m_segmentRemoteFd.reset();
    m_segmentRunClock.invalidate();
    m_processLog.clear();
    m_segmentPlaying = false;

    if (m_canceling) {
        stopSystemAudioCapture();
        QFile::remove(segment);
        removeRecordingFiles();
        cleanupProcess();
        return;
    }

    const bool firstSegment = m_gstStarting;
    const bool resumeSegment = m_resuming;
    const bool expectedStop = m_stopping || (m_paused && !resumeSegment)
        || (m_maxSeconds > 0 && activeElapsedMs() / 1000 >= m_maxSeconds);
    const bool succeeded = videoRecordingProcessSucceeded(true, expectedStop, status, exitCode,
                                                          QFileInfo(segment).size());
    const bool startupFailure = !expectedStop
        && (!reachedPlaying || recordingProcessFailedAtStartup(runMs, expectedStop));
    const RecordingSegmentDisposition disposition = recordingSegmentDisposition(
        succeeded, startupFailure, mp4FileLooksPlayable(segment));
    auto warnEndNotSaved = [this]() {
        const QString warning = TranslationManager::tr("recWarnEndNotSaved");
        if (!m_warnings.contains(warning))
            m_warnings << warning;
    };
    switch (disposition) {
    case RecordingSegmentDisposition::Keep:
        m_segmentPaths.append(segment);
        break;
    case RecordingSegmentDisposition::Salvage:
        // A crashed or killed encoder still leaves the fragments written so
        // far; keep them instead of losing the whole span.
        m_segmentPaths.append(segment);
        warnEndNotSaved();
        qWarning() << "VideoRecorder: kept the playable part of an interrupted segment" << segment << failureText;
        break;
    case RecordingSegmentDisposition::Drop:
        QFile::remove(segment);
        qWarning() << "VideoRecorder: dropped unusable recording segment" << segment << failureText;
        break;
    }

    if (firstSegment && m_segmentPaths.isEmpty()) {
        stopSystemAudioCapture();
        removeRecordingFiles();
        cleanupProcess();
        emit recordingFailed(failureText.isEmpty()
                                 ? QStringLiteral("gstreamer pipeline exited during startup")
                                 : failureText);
        return;
    }

    if (resumeSegment) {
        m_resuming = false;
        if (!expectedStop && disposition == RecordingSegmentDisposition::Drop) {
            m_warnings << TranslationManager::tr("recWarnResumeFailed")
                              .arg(failureText.isEmpty() ? QStringLiteral("encoder did not start") : failureText);
        }
        m_stopping = true;
        finishRecording();
        return;
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
        emit recordingFailed(failureText.isEmpty()
                                 ? QStringLiteral("gstreamer exited with code %1").arg(exitCode)
                                 : failureText);
        return;
    }
    if (disposition == RecordingSegmentDisposition::Drop)
        warnEndNotSaved();
    finishRecording();
}

void VideoRecorder::resumePortalRecording()
{
    if (m_resuming)
        return;
    if (m_process) {
        // The previous segment is still being finalized.
        m_resumePending = true;
        return;
    }
    m_resuming = true;
    // The pause ends when the new segment runs (see onGstSegmentPlaying()),
    // so the elapsed time matches the saved video.
    const int generation = m_startGeneration;
    const QString path = recordingSegmentPath(m_segmentDirectory, m_segmentStamp, m_segmentCount);
    LinuxPortalScreenCast::openPipeWireRemoteAsync(
        m_portalSessionHandle, this, [this, generation, path](QSharedPointer<int> fd) {
        if (generation != m_startGeneration || !m_resuming || m_stopping || !m_recording)
            return;
        QString error;
        if (startGstSegment(fd, path, &error))
            return;
        m_resuming = false;
        m_warnings << TranslationManager::tr("recWarnResumeFailed")
                          .arg(error.isEmpty() ? QStringLiteral("encoder did not start") : error);
        m_stopping = true;
        finishRecording();
    });
}

void VideoRecorder::onPortalSessionClosed()
{
    if (!m_recording || m_stopping)
        return;
    qInfo() << "VideoRecorder: screen sharing was stopped from the desktop; saving the recording";
    m_warnings << TranslationManager::tr("recWarnSharingStopped");
    stop();
}

RecordingCaptureBackend VideoRecorder::captureBackend() const
{
#ifdef Q_OS_WIN
    return recordingCaptureBackendForPlan(m_windowsPlan);
#else
    return defaultRecordingCaptureBackend();
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
    // Invalidates pending deferred starts and resume callbacks.
    ++m_startGeneration;
    m_gstStarting = false;
    m_resuming = false;
    if (m_sessionWatcher) {
        // Our own Close below must not look like the user stopping the share.
        m_sessionWatcher->disconnect(this);
        m_sessionWatcher->deleteLater();
        m_sessionWatcher = nullptr;
    }
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
