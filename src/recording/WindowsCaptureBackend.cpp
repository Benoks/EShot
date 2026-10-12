#include "WindowsCaptureBackend.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QThreadPool>

#include <mutex>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi.h>
#endif

namespace {
#ifdef Q_OS_WIN
struct CaptureProbeCache {
    std::mutex mutex;
    QHash<QString, WindowsCaptureCapabilities> results;
    QSet<QString> inFlight;
    QSet<QString> ddaDisabled;
};

CaptureProbeCache &captureProbeCache()
{
    static CaptureProbeCache cache;
    return cache;
}

QString probeSettingsKey(const QString &ffmpegPath)
{
    const QFileInfo info(ffmpegPath);
    return windowsCaptureProbeCacheKey(info.absoluteFilePath(),
                                       info.lastModified().toMSecsSinceEpoch(), info.size());
}

// Runs on the probe worker thread.
bool runFfmpeg(const QString &ffmpegPath, const QStringList &arguments, int timeoutMs, QString *output)
{
    QProcess process;
    process.setProgram(ffmpegPath);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(5000))
        return false;
    // Nothing reads stdin; a probe must never wait for "q".
    process.closeWriteChannel();
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        return false;
    }
    const QByteArray bytes = process.readAll();
    if (output)
        *output = QString::fromUtf8(bytes);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void probeCaptureBackends(const QString &ffmpegPath, const QString &settingsKey)
{
    WindowsCaptureCapabilities capabilities;
    capabilities.ready = true;
    // A result that may be temporary (FFmpeg did not run, Desktop
    // Duplication unavailable right now: secure desktop, remote session) is
    // only kept for this run, so the next start probes again.
    bool persist = true;

    QString filters;
    QString encoders;
    if (!runFfmpeg(ffmpegPath, {QStringLiteral("-hide_banner"), QStringLiteral("-filters")}, 15000, &filters)
        || !runFfmpeg(ffmpegPath, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")}, 15000, &encoders)) {
        persist = false;
    } else if (ffmpegListHasFilter(filters, QStringLiteral("ddagrab"))) {
        const QList<WindowsVideoEncoder> candidates = windowsDdaEncoderCandidates(ffmpegListedEncoders(encoders));
        for (WindowsVideoEncoder encoder : candidates) {
            QString log;
            if (runFfmpeg(ffmpegPath, windowsDdaEncoderProbeArguments(encoder), 20000, &log)) {
                capabilities.ddaEncoders.append(encoder);
            } else {
                qInfo() << "[RecordingProbe] ddagrab with" << ffmpegVideoEncoderName(encoder)
                        << "is not usable:" << log.trimmed().right(300);
            }
        }
        if (capabilities.ddaEncoders.isEmpty())
            persist = false;
    }
    qInfo() << "[RecordingProbe] ffmpeg capture backends:"
            << serializeWindowsCaptureCapabilities(capabilities) << (persist ? "" : "(not cached)");

    CaptureProbeCache &cache = captureProbeCache();
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        cache.results.insert(ffmpegPath, capabilities);
        cache.inFlight.remove(ffmpegPath);
    }
    if (persist) {
        // QSettings is reentrant; this instance belongs to the worker thread.
        QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
        settings.setValue(settingsKey, serializeWindowsCaptureCapabilities(capabilities));
    }
}
#endif
}

void requestWindowsCaptureProbe(const QString &ffmpegPath)
{
#ifdef Q_OS_WIN
    if (ffmpegPath.isEmpty() || !QFileInfo::exists(ffmpegPath))
        return;
    CaptureProbeCache &cache = captureProbeCache();
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        if (cache.results.contains(ffmpegPath) || cache.inFlight.contains(ffmpegPath))
            return;
    }
    const QString settingsKey = probeSettingsKey(ffmpegPath);
    const WindowsCaptureCapabilities stored = parseWindowsCaptureCapabilities(
        QSettings(QStringLiteral("EShot"), QStringLiteral("EShot")).value(settingsKey).toString());
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        if (cache.results.contains(ffmpegPath) || cache.inFlight.contains(ffmpegPath))
            return;
        if (stored.ready) {
            cache.results.insert(ffmpegPath, stored);
            return;
        }
        cache.inFlight.insert(ffmpegPath);
    }
    QThreadPool::globalInstance()->start([ffmpegPath, settingsKey]() {
        probeCaptureBackends(ffmpegPath, settingsKey);
    });
#else
    Q_UNUSED(ffmpegPath);
#endif
}

WindowsCaptureCapabilities cachedWindowsCaptureCapabilities(const QString &ffmpegPath)
{
#ifdef Q_OS_WIN
    if (ffmpegPath.isEmpty())
        return {};
    CaptureProbeCache &cache = captureProbeCache();
    bool known = false;
    WindowsCaptureCapabilities capabilities;
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        known = cache.results.contains(ffmpegPath);
        capabilities = cache.results.value(ffmpegPath);
        if (cache.ddaDisabled.contains(ffmpegPath))
            capabilities.ddaEncoders.clear();
    }
    if (!known) {
        // Reads a stored result right away, or starts the probe for later.
        requestWindowsCaptureProbe(ffmpegPath);
        std::lock_guard<std::mutex> lock(cache.mutex);
        capabilities = cache.results.value(ffmpegPath);
        if (cache.ddaDisabled.contains(ffmpegPath))
            capabilities.ddaEncoders.clear();
    }
    return capabilities;
#else
    Q_UNUSED(ffmpegPath);
    return {};
#endif
}

void disableWindowsDdaGrabForSession(const QString &ffmpegPath)
{
#ifdef Q_OS_WIN
    CaptureProbeCache &cache = captureProbeCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    cache.ddaDisabled.insert(ffmpegPath);
#else
    Q_UNUSED(ffmpegPath);
#endif
}

QList<DxgiOutputInfo> enumerateDxgiOutputs()
{
    QList<DxgiOutputInfo> outputs;
#ifdef Q_OS_WIN
    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory))) || !factory)
        return outputs;
    for (UINT adapterIndex = 0;; ++adapterIndex) {
        IDXGIAdapter1 *adapter = nullptr;
        if (FAILED(factory->EnumAdapters1(adapterIndex, &adapter)) || !adapter)
            break;
        for (UINT outputIndex = 0;; ++outputIndex) {
            IDXGIOutput *output = nullptr;
            if (FAILED(adapter->EnumOutputs(outputIndex, &output)) || !output)
                break;
            DXGI_OUTPUT_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            if (SUCCEEDED(output->GetDesc(&desc)) && desc.AttachedToDesktop) {
                const RECT &r = desc.DesktopCoordinates;
                DxgiOutputInfo info;
                info.adapterIndex = static_cast<int>(adapterIndex);
                info.outputIndex = static_cast<int>(outputIndex);
                info.desktopRect = QRect(static_cast<int>(r.left), static_cast<int>(r.top),
                                         static_cast<int>(r.right - r.left),
                                         static_cast<int>(r.bottom - r.top));
                info.rotated = desc.Rotation == DXGI_MODE_ROTATION_ROTATE90
                    || desc.Rotation == DXGI_MODE_ROTATION_ROTATE180
                    || desc.Rotation == DXGI_MODE_ROTATION_ROTATE270;
                outputs.append(info);
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
#endif
    return outputs;
}

WindowsVideoPlan windowsVideoPlanForRecording(const QString &ffmpegPath, const QRect &captureRect,
                                              bool losslessIntermediate)
{
    WindowsVideoPlan plan;
#ifdef Q_OS_WIN
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    WindowsVideoPlanRequest request;
    request.allowDdaGrab = settings.value(QLatin1String(RecordingWindowsCaptureBackendKey),
                                          QStringLiteral("auto")).toString().trimmed().toLower()
        != QStringLiteral("gdigrab");
    request.allowHardwareEncoder = settings.value(QLatin1String(RecordingHardwareEncodingKey), true).toBool();
    request.losslessIntermediate = losslessIntermediate;

    const WindowsCaptureCapabilities capabilities = cachedWindowsCaptureCapabilities(ffmpegPath);
    if (request.allowDdaGrab && capabilities.ready && capabilities.ddaGrab())
        plan = chooseWindowsVideoPlan(captureRect, capabilities, enumerateDxgiOutputs(), request);
    qInfo() << "[Recording] capture plan for" << captureRect << ":" << describeWindowsVideoPlan(plan)
            << (capabilities.ready ? "" : "(probe not finished)");
#else
    Q_UNUSED(ffmpegPath);
    Q_UNUSED(captureRect);
    Q_UNUSED(losslessIntermediate);
#endif
    return plan;
}

RecordingCaptureBackend recordingCaptureBackendForPlan(const WindowsVideoPlan &plan)
{
    return plan.source == WindowsCaptureSource::DdaGrab ? RecordingCaptureBackend::WindowsDdaGrab
                                                        : RecordingCaptureBackend::WindowsGdiGrab;
}

void tieRecordingProcessToApplication(QProcess *process)
{
#ifdef Q_OS_WIN
    // The handle is never closed: Windows closes it when EShot ends, which
    // kills every process in the job.
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
    if (!job || !process)
        return;
    const qint64 pid = process->processId();
    if (pid <= 0)
        return;
    HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!child)
        return;
    if (!AssignProcessToJobObject(job, child))
        qWarning() << "[Recording] could not tie ffmpeg to the EShot process lifetime" << GetLastError();
    CloseHandle(child);
#else
    Q_UNUSED(process);
#endif
}

void syncRecordingFileHiddenAttribute(const QString &path)
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
