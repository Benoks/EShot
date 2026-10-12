#include <QtTest>

#include "recording/AudioDevices.h"

class AudioDevicesTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesDshowAudioDevicesOnly()
    {
        const QString output = QStringLiteral(
            "[dshow @ 000001] \"Integrated Camera\" (video)\n"
            "[dshow @ 000001]   Alternative name \"@device_pnp_camera\"\n"
            "[dshow @ 000001] \"Microphone (Realtek(R) Audio)\" (audio)\n"
            "[dshow @ 000001]   Alternative name \"@device_cm_mic\"\n"
            "[dshow @ 000001] \"Stereo Mix (Realtek(R) Audio)\" (audio)\n"
            "[dshow @ 000001] \"Microphone (Realtek(R) Audio)\" (audio)\n"
            "dummy: Immediate exit requested\n");
        QCOMPARE(parseDshowAudioDevices(output),
                 QStringList({QStringLiteral("Microphone (Realtek(R) Audio)"),
                              QStringLiteral("Stereo Mix (Realtek(R) Audio)")}));
    }

    void emptyOutputHasNoDevices()
    {
        QVERIFY(parseDshowAudioDevices(QString()).isEmpty());
        QVERIFY(parseDshowAudioDeviceList(QString()).isEmpty());
    }

    void keepsAlternativeNamesOfAudioDevices()
    {
        const QString output = QStringLiteral(
            "[dshow @ 000001] \"Integrated Camera\" (video)\r\n"
            "[dshow @ 000001]   Alternative name \"@device_pnp_camera\"\r\n"
            "[dshow @ 000001] \"Mikrofon Dizisi (Realtek(R) Audio)\" (audio)\r\n"
            "[dshow @ 000001]   Alternative name \"@device_cm_{33D9A762}\\wave_{AAAA}\"\r\n"
            "[dshow @ 000001] \"Stereo Mix (Realtek(R) Audio)\" (audio)\r\n"
            "[dshow @ 000001]   Alternative name \"@device_cm_{33D9A762}\\wave_{BBBB}\"\r\n"
            "[in#0 @ 000002] Error opening input: Immediate exit requested\r\n");
        const QList<DshowAudioDevice> devices = parseDshowAudioDeviceList(output);
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices.at(0).name, QStringLiteral("Mikrofon Dizisi (Realtek(R) Audio)"));
        QCOMPARE(devices.at(0).alternativeName, QStringLiteral("@device_cm_{33D9A762}\\wave_{AAAA}"));
        QCOMPARE(devices.at(1).alternativeName, QStringLiteral("@device_cm_{33D9A762}\\wave_{BBBB}"));
        QCOMPARE(dshowAudioInputName(devices.at(0)), devices.at(0).alternativeName);
        QCOMPARE(dshowAudioInputName({QStringLiteral("Mic"), QString()}), QStringLiteral("Mic"));
    }

    void parsesOlderSectionedListing()
    {
        const QString output = QStringLiteral(
            "[dshow @ 01] DirectShow video devices (some may be both video and audio devices)\n"
            "[dshow @ 01]  \"USB Camera\"\n"
            "[dshow @ 01]     Alternative name \"@device_pnp_usb\"\n"
            "[dshow @ 01] DirectShow audio devices\n"
            "[dshow @ 01]  \"Microphone (USB Audio Device)\"\n"
            "[dshow @ 01]     Alternative name \"@device_cm_usbmic\"\n");
        const QList<DshowAudioDevice> devices = parseDshowAudioDeviceList(output);
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.first().name, QStringLiteral("Microphone (USB Audio Device)"));
        QCOMPARE(devices.first().alternativeName, QStringLiteral("@device_cm_usbmic"));
    }

    void splitsMicrophonesFromLoopbackSources()
    {
        const QList<DshowAudioDevice> devices = {
            {QStringLiteral("Microphone (Realtek(R) Audio)"), QStringLiteral("@a")},
            {QStringLiteral("Stereo Mix (Realtek(R) Audio)"), QStringLiteral("@b")},
            {QStringLiteral("Stereo Karışımı (Realtek(R) Audio)"), QStringLiteral("@c")},
            {QStringLiteral("virtual-audio-capturer"), QString()},
        };
        const auto microphones = dshowMicrophoneDevices(devices);
        QCOMPARE(microphones.size(), 1);
        QCOMPARE(microphones.first().first, QStringLiteral("Microphone (Realtek(R) Audio)"));
        QCOMPARE(microphones.first().second, QStringLiteral("Microphone (Realtek(R) Audio)"));
        QCOMPARE(dshowDesktopAudioDevices(devices).size(), 3);
    }

    void resolvesConfiguredMicrophone()
    {
        const QList<DshowAudioDevice> devices = {
            {QStringLiteral("Stereo Mix (Realtek(R) Audio)"), QStringLiteral("@mix")},
            {QStringLiteral("Microphone (Realtek High Defini"), QStringLiteral("@hd")},
            {QStringLiteral("Headset Microphone (Jabra Link 380)"), QStringLiteral("@jabra")},
        };
        // Exact friendly or alternative name, case-insensitively.
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("headset microphone (jabra link 380)")), 2);
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("@hd")), 1);
        // dshow cut the wave name to 31 characters.
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("Microphone (Realtek High Definition Audio)")), 1);
        // Interface name stored by older versions.
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("Jabra Link 380")), 2);
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("Headset Microphone")), 2);
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("Microphone")), 1);
        // Unknown names are not guessed.
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("Unplugged USB Mic")), -1);
    }

    void resolvesDefaultMicrophoneToTheSystemDefault()
    {
        const QList<DshowAudioDevice> devices = {
            {QStringLiteral("Stereo Mix (Realtek(R) Audio)"), QStringLiteral("@mix")},
            {QStringLiteral("Microphone (Realtek(R) Audio)"), QStringLiteral("@mic")},
            {QStringLiteral("Headset Microphone (Jabra)"), QStringLiteral("@jabra")},
        };
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("default"),
                                         QStringLiteral("Headset Microphone (Jabra)")), 2);
        // Unknown default endpoint: first device that is not a loopback.
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("default"), QString()), 1);
        QCOMPARE(resolveDshowAudioDevice(devices, QString(), QStringLiteral("Gone")), 1);
        QCOMPARE(resolveDshowAudioDevice({}, QStringLiteral("default"), QString()), -1);
        // An interface name shared by two devices is ambiguous.
        QCOMPARE(resolveDshowAudioDevice(devices, QStringLiteral("Realtek(R) Audio")), -1);
    }
};

QTEST_APPLESS_MAIN(AudioDevicesTests)
#include "AudioDevicesTests.moc"
