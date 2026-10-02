#include "AudioDevices.h"
#include "LinuxRecordingSupport.h"

#include <QHash>
#include <QProcess>
#include <QRegularExpression>

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
#ifdef Q_OS_WIN
void appendDeviceProperty(IPropertyStore *store, const PROPERTYKEY &key, QStringList &devices)
{
    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(store->GetValue(key, &value)) && value.vt == VT_LPWSTR && value.pwszVal) {
        const QString name = QString::fromWCharArray(value.pwszVal).trimmed();
        if (!name.isEmpty() && !devices.contains(name))
            devices.append(name);
    }
    PropVariantClear(&value);
}
#endif
}

QStringList windowsAudioInputDevices()
{
    QStringList devices;
#ifdef Q_OS_WIN
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninit = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE)
        return devices;

    IMMDeviceEnumerator *enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void **>(&enumerator));
    if (SUCCEEDED(hr) && enumerator) {
        IMMDeviceCollection *collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
        if (SUCCEEDED(hr) && collection) {
            UINT count = 0;
            collection->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice *device = nullptr;
                if (FAILED(collection->Item(i, &device)) || !device)
                    continue;
                IPropertyStore *store = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && store) {
                    appendDeviceProperty(store, PKEY_DeviceInterface_FriendlyName, devices);
                    appendDeviceProperty(store, PKEY_Device_FriendlyName, devices);
                    appendDeviceProperty(store, PKEY_Device_DeviceDesc, devices);
                    store->Release();
                }
                device->Release();
            }
            collection->Release();
        }
        enumerator->Release();
    }
    if (shouldUninit)
        CoUninitialize();
#endif
    return devices;
}

QString defaultDesktopAudioDevice()
{
#ifdef Q_OS_WIN
    return QStringLiteral("__wasapi__");
#else
    return QStringLiteral("@DEFAULT_SINK@.monitor");
#endif
}

QList<QPair<QString, QString>> microphoneAudioDevices()
{
#ifdef Q_OS_WIN
    QList<QPair<QString, QString>> devices;
    for (const QString &name : windowsAudioInputDevices()) devices.append(qMakePair(name, name));
    return devices;
#else
    return discoverLinuxMicrophoneDevices();
#endif
}

QStringList parseDshowAudioDevices(const QString &listDevicesOutput)
{
    QStringList devices;
    QRegularExpression re(QStringLiteral("\"([^\"]+)\"\\s*\\(audio\\)"));
    auto it = re.globalMatch(listDevicesOutput);
    while (it.hasNext()) {
        const QString name = it.next().captured(1).trimmed();
        if (!name.isEmpty() && !devices.contains(name))
            devices.append(name);
    }
    return devices;
}

QStringList dshowAudioDevices(const QString &ffmpegPath)
{
    // DirectShow enumeration can take seconds while drivers wake up; a short
    // limit returned no devices and silently dropped the default microphone.
    // Keep the last successful list for runs that still come back empty.
    static QHash<QString, QStringList> lastDevices;

    QProcess process;
    process.setProgram(ffmpegPath);
    process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-list_devices"), QStringLiteral("true"),
                          QStringLiteral("-f"), QStringLiteral("dshow"), QStringLiteral("-i"), QStringLiteral("dummy")});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(6000)) {
        process.kill();
        process.waitForFinished(1000);
    }

    const QStringList devices = parseDshowAudioDevices(QString::fromLocal8Bit(process.readAll()));
    if (devices.isEmpty())
        return lastDevices.value(ffmpegPath);
    lastDevices.insert(ffmpegPath, devices);
    return devices;
}
