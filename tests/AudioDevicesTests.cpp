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
    }
};

QTEST_APPLESS_MAIN(AudioDevicesTests)
#include "AudioDevicesTests.moc"
