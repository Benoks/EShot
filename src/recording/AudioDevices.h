#ifndef AUDIODEVICES_H
#define AUDIODEVICES_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include <functional>

class QObject;

// An audio device as listed by `ffmpeg -list_devices true -f dshow -i dummy`.
// The alternative name (`@device_cm_...`) is unique and ASCII, so it is what
// gets passed to `-f dshow -i audio=...`; the friendly name is shown and stored.
struct DshowAudioDevice {
    QString name;
    QString alternativeName;
};

// Setting value for the platform's default desktop (loopback) audio source.
QString defaultDesktopAudioDevice();
// Microphones as (display name, device id) pairs. On Windows these are the
// DirectShow capture devices from the device cache (empty until it is warm);
// the id is the friendly name DirectShow accepts.
QList<QPair<QString, QString>> microphoneAudioDevices();

// Audio device names in the output of the dshow device listing.
QStringList parseDshowAudioDevices(const QString &listDevicesOutput);
// Audio devices (with their alternative names) in the dshow device listing;
// understands both the "(audio)" suffix and the older section headers.
QList<DshowAudioDevice> parseDshowAudioDeviceList(const QString &listDevicesOutput);
// DirectShow devices that capture what is played back (Stereo Mix, virtual
// audio capturer, ...), which belong in the desktop list, not the mic list.
bool isLoopbackAudioDeviceName(const QString &name);
// Microphone entries (display name, setting value) of a dshow device list.
QList<QPair<QString, QString>> dshowMicrophoneDevices(const QList<DshowAudioDevice> &devices);
// Desktop (loopback) entries of a dshow device list, by friendly name.
QStringList dshowDesktopAudioDevices(const QList<DshowAudioDevice> &devices);
// Index of the device a configured setting refers to, or -1. "default" (or an
// empty value) picks `defaultCaptureName` (the Windows default capture
// endpoint), falling back to the first non-loopback device. Matches friendly
// and alternative names, dshow's 31-character name truncation and the bare
// interface/description names older versions stored.
int resolveDshowAudioDevice(const QList<DshowAudioDevice> &devices, const QString &configured,
                            const QString &defaultCaptureName = QString());
// Name to pass after `audio=` for a resolved device.
QString dshowAudioInputName(const DshowAudioDevice &device);

// DirectShow enumeration runs ffmpeg and can take seconds, so it is cached and
// refreshed on a worker thread. These do nothing useful outside Windows.
// Starts a background refresh unless one is running (or, with onlyIfMissing,
// unless a list is already cached).
void refreshDshowAudioDevices(const QString &ffmpegPath, bool onlyIfMissing = false);
// Calls `callback` on the GUI thread with the device list: right away when a
// list is cached (and a refresh is started), otherwise once enumeration
// finishes. Dropped when `context` is destroyed first.
void requestDshowAudioDevices(const QString &ffmpegPath, QObject *context,
                              const std::function<void(const QList<DshowAudioDevice> &)> &callback);
// The cached list; when there is none yet, waits up to `waitMs` for the
// enumeration to finish.
QList<DshowAudioDevice> cachedDshowAudioDevices(const QString &ffmpegPath, int waitMs);
// Friendly name of the Windows default capture (console) endpoint.
QString windowsDefaultCaptureDeviceName();

#endif
