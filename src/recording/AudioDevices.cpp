#include "AudioDevices.h"
#include "LinuxRecordingSupport.h"

#include <QCoreApplication>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QThreadPool>

#include <chrono>
#include <condition_variable>
#include <mutex>

#ifdef Q_OS_WIN
#include <windows.h>
#ifdef __MINGW32__
#include <initguid.h>
#endif
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#endif

namespace {
// DirectShow lists wave devices by their WAVEINCAPS name, which is cut to
// MAXPNAMELEN - 1 characters on many systems.
constexpr int DshowTruncatedNameLength = 31;

int uniqueCandidate(const QList<int> &candidates)
{
    return candidates.size() == 1 ? candidates.first() : -1;
}

int findDshowDevice(const QList<DshowAudioDevice> &devices, const QString &name)
{
    if (name.isEmpty())
        return -1;
    for (int i = 0; i < devices.size(); ++i) {
        const DshowAudioDevice &device = devices.at(i);
        if (device.name.compare(name, Qt::CaseInsensitive) == 0
            || (!device.alternativeName.isEmpty()
                && device.alternativeName.compare(name, Qt::CaseInsensitive) == 0))
            return i;
    }

    QList<int> truncated;
    for (int i = 0; i < devices.size(); ++i) {
        const QString &listed = devices.at(i).name;
        if (listed.size() >= DshowTruncatedNameLength - 1 && name.size() > listed.size()
            && name.startsWith(listed, Qt::CaseInsensitive))
            truncated.append(i);
    }
    if (const int index = uniqueCandidate(truncated); index >= 0)
        return index;

    // Older versions stored the endpoint's interface name ("Realtek(R) Audio")
    // or its description ("Microphone"); DirectShow shows "Description (Interface)".
    QList<int> partial;
    for (int i = 0; i < devices.size(); ++i) {
        const QString &listed = devices.at(i).name;
        if (listed.endsWith(QStringLiteral("(%1)").arg(name), Qt::CaseInsensitive)
            || listed.startsWith(name + QStringLiteral(" ("), Qt::CaseInsensitive))
            partial.append(i);
    }
    return uniqueCandidate(partial);
}

#ifdef Q_OS_WIN
struct PendingDshowRequest {
    QString ffmpegPath;
    QPointer<QObject> context;
    std::function<void(const QList<DshowAudioDevice> &)> callback;
};

struct DshowDeviceCache {
    std::mutex mutex;
    std::condition_variable finished;
    QHash<QString, QList<DshowAudioDevice>> devices;
    QList<DshowAudioDevice> lastDevices;
    QSet<QString> inFlight;
    QList<PendingDshowRequest> pending;
};

DshowDeviceCache &dshowDeviceCache()
{
    static DshowDeviceCache cache;
    return cache;
}

QList<DshowAudioDevice> enumerateDshowAudioDevices(const QString &ffmpegPath)
{
    // Runs on a worker thread: DirectShow enumeration can take seconds while
    // drivers wake up, which used to freeze the UI at recording start.
    QProcess process;
    process.setProgram(ffmpegPath);
    process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-list_devices"), QStringLiteral("true"),
                          QStringLiteral("-f"), QStringLiteral("dshow"), QStringLiteral("-i"), QStringLiteral("dummy")});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(8000)) {
        process.kill();
        process.waitForFinished(1000);
    }
    // FFmpeg prints device names as UTF-8.
    return parseDshowAudioDeviceList(QString::fromUtf8(process.readAll()));
}

void deliverDshowAudioDevices(const QString &ffmpegPath)
{
    DshowDeviceCache &cache = dshowDeviceCache();
    QList<PendingDshowRequest> ready;
    QList<DshowAudioDevice> devices;
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        devices = cache.devices.value(ffmpegPath);
        for (int i = cache.pending.size() - 1; i >= 0; --i) {
            if (cache.pending.at(i).ffmpegPath == ffmpegPath)
                ready.prepend(cache.pending.takeAt(i));
        }
    }
    for (const PendingDshowRequest &request : std::as_const(ready)) {
        if (request.context && request.callback)
            request.callback(devices);
    }
}
#endif
}

QString defaultDesktopAudioDevice()
{
#ifdef Q_OS_WIN
    return QStringLiteral("__wasapi__");
#else
    return QStringLiteral("@DEFAULT_MONITOR@");
#endif
}

QList<QPair<QString, QString>> microphoneAudioDevices()
{
#ifdef Q_OS_WIN
    DshowDeviceCache &cache = dshowDeviceCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    return dshowMicrophoneDevices(cache.lastDevices);
#else
    return discoverLinuxMicrophoneDevices();
#endif
}

QList<DshowAudioDevice> parseDshowAudioDeviceList(const QString &listDevicesOutput)
{
    static const QRegularExpression alternativeRe(QStringLiteral("Alternative name\\s+\"([^\"]+)\""));
    static const QRegularExpression deviceRe(QStringLiteral("\"([^\"]+)\"\\s*(?:\\(([^)]*)\\))?\\s*$"));

    enum class Section { Unknown, Video, Audio };
    Section section = Section::Unknown;
    QList<DshowAudioDevice> devices;
    bool lastWasNewAudioDevice = false;
    for (const QString &rawLine : listDevicesOutput.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.contains(QStringLiteral("DirectShow video devices"), Qt::CaseInsensitive)) {
            section = Section::Video;
            lastWasNewAudioDevice = false;
            continue;
        }
        if (line.contains(QStringLiteral("DirectShow audio devices"), Qt::CaseInsensitive)) {
            section = Section::Audio;
            lastWasNewAudioDevice = false;
            continue;
        }
        const QRegularExpressionMatch alternative = alternativeRe.match(line);
        if (alternative.hasMatch()) {
            if (lastWasNewAudioDevice && devices.last().alternativeName.isEmpty())
                devices.last().alternativeName = alternative.captured(1).trimmed();
            lastWasNewAudioDevice = false;
            continue;
        }
        const QRegularExpressionMatch device = deviceRe.match(line);
        if (!device.hasMatch()) {
            lastWasNewAudioDevice = false;
            continue;
        }
        const QString type = device.captured(2).trimmed().toLower();
        const bool isAudio = type.isEmpty() ? section == Section::Audio
                                            : type.contains(QStringLiteral("audio"));
        lastWasNewAudioDevice = false;
        if (!isAudio)
            continue;
        const QString name = device.captured(1).trimmed();
        if (name.isEmpty())
            continue;
        bool duplicate = false;
        for (const DshowAudioDevice &known : std::as_const(devices)) {
            if (known.name.compare(name, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        devices.append({name, QString()});
        lastWasNewAudioDevice = true;
    }
    return devices;
}

QStringList parseDshowAudioDevices(const QString &listDevicesOutput)
{
    QStringList names;
    for (const DshowAudioDevice &device : parseDshowAudioDeviceList(listDevicesOutput))
        names.append(device.name);
    return names;
}

bool isLoopbackAudioDeviceName(const QString &name)
{
    static const QStringList markers = {
        QStringLiteral("virtual-audio-capturer"),
        QStringLiteral("stereo mix"),
        QStringLiteral("stereomix"),
        QStringLiteral("what u hear"),
        QStringLiteral("loopback"),
        QStringLiteral("wave out"),
        QStringLiteral("stereo karışımı"),
        QStringLiteral("mixage stéréo"),
        QStringLiteral("mezcla estéreo"),
        QStringLiteral("ステレオ ミキサー"),
        QStringLiteral("立体声混音"),
        QStringLiteral("стерео микшер"),
    };
    const QString lower = name.toLower();
    for (const QString &marker : markers) {
        if (lower.contains(marker))
            return true;
    }
    return false;
}

QList<QPair<QString, QString>> dshowMicrophoneDevices(const QList<DshowAudioDevice> &devices)
{
    QList<QPair<QString, QString>> microphones;
    for (const DshowAudioDevice &device : devices) {
        if (!isLoopbackAudioDeviceName(device.name))
            microphones.append(qMakePair(device.name, device.name));
    }
    return microphones;
}

QStringList dshowDesktopAudioDevices(const QList<DshowAudioDevice> &devices)
{
    QStringList desktop;
    for (const DshowAudioDevice &device : devices) {
        if (isLoopbackAudioDeviceName(device.name))
            desktop.append(device.name);
    }
    return desktop;
}

int resolveDshowAudioDevice(const QList<DshowAudioDevice> &devices, const QString &configured,
                            const QString &defaultCaptureName)
{
    const QString wanted = configured.trimmed();
    const bool useDefault = wanted.isEmpty()
        || wanted.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0;
    if (!useDefault)
        return findDshowDevice(devices, wanted);

    const int index = findDshowDevice(devices, defaultCaptureName.trimmed());
    if (index >= 0)
        return index;
    for (int i = 0; i < devices.size(); ++i) {
        if (!isLoopbackAudioDeviceName(devices.at(i).name))
            return i;
    }
    return -1;
}

QString dshowAudioInputName(const DshowAudioDevice &device)
{
    return device.alternativeName.isEmpty() ? device.name : device.alternativeName;
}

void refreshDshowAudioDevices(const QString &ffmpegPath, bool onlyIfMissing)
{
#ifdef Q_OS_WIN
    if (ffmpegPath.isEmpty())
        return;
    DshowDeviceCache &cache = dshowDeviceCache();
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        if (cache.inFlight.contains(ffmpegPath))
            return;
        if (onlyIfMissing && cache.devices.contains(ffmpegPath))
            return;
        cache.inFlight.insert(ffmpegPath);
    }
    QThreadPool::globalInstance()->start([ffmpegPath]() {
        const QList<DshowAudioDevice> found = enumerateDshowAudioDevices(ffmpegPath);
        DshowDeviceCache &cache = dshowDeviceCache();
        {
            std::lock_guard<std::mutex> lock(cache.mutex);
            // An empty run (driver still waking up) keeps the last good list.
            if (!found.isEmpty() || !cache.devices.contains(ffmpegPath))
                cache.devices.insert(ffmpegPath, found);
            if (!found.isEmpty())
                cache.lastDevices = found;
            cache.inFlight.remove(ffmpegPath);
        }
        cache.finished.notify_all();
        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [ffmpegPath]() {
                deliverDshowAudioDevices(ffmpegPath);
            }, Qt::QueuedConnection);
        }
    });
#else
    Q_UNUSED(ffmpegPath);
    Q_UNUSED(onlyIfMissing);
#endif
}

void requestDshowAudioDevices(const QString &ffmpegPath, QObject *context,
                              const std::function<void(const QList<DshowAudioDevice> &)> &callback)
{
#ifdef Q_OS_WIN
    if (ffmpegPath.isEmpty()) {
        if (callback)
            callback({});
        return;
    }
    DshowDeviceCache &cache = dshowDeviceCache();
    bool cached = false;
    QList<DshowAudioDevice> devices;
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        cached = cache.devices.contains(ffmpegPath);
        devices = cache.devices.value(ffmpegPath);
        // Also deliver the refreshed list, which may contain a device that
        // was plugged in since the cache was filled.
        cache.pending.append(PendingDshowRequest{ffmpegPath, QPointer<QObject>(context), callback});
    }
    if (cached && callback)
        callback(devices);
    refreshDshowAudioDevices(ffmpegPath);
#else
    Q_UNUSED(ffmpegPath);
    Q_UNUSED(context);
    if (callback)
        callback({});
#endif
}

QList<DshowAudioDevice> cachedDshowAudioDevices(const QString &ffmpegPath, int waitMs)
{
#ifdef Q_OS_WIN
    if (ffmpegPath.isEmpty())
        return {};
    refreshDshowAudioDevices(ffmpegPath, true);
    DshowDeviceCache &cache = dshowDeviceCache();
    std::unique_lock<std::mutex> lock(cache.mutex);
    cache.finished.wait_for(lock, std::chrono::milliseconds(qMax(0, waitMs)), [&cache, &ffmpegPath]() {
        return cache.devices.contains(ffmpegPath);
    });
    return cache.devices.value(ffmpegPath);
#else
    Q_UNUSED(ffmpegPath);
    Q_UNUSED(waitMs);
    return {};
#endif
}

QString windowsDefaultCaptureDeviceName()
{
    QString name;
#ifdef Q_OS_WIN
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninit = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE)
        return name;

    IMMDeviceEnumerator *enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void **>(&enumerator));
    if (SUCCEEDED(hr) && enumerator) {
        IMMDevice *device = nullptr;
        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device)) && device) {
            IPropertyStore *store = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && store) {
                PROPVARIANT value;
                PropVariantInit(&value);
                if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value))
                    && value.vt == VT_LPWSTR && value.pwszVal)
                    name = QString::fromWCharArray(value.pwszVal).trimmed();
                PropVariantClear(&value);
                store->Release();
            }
            device->Release();
        }
        enumerator->Release();
    }
    if (shouldUninit)
        CoUninitialize();
#endif
    return name;
}
