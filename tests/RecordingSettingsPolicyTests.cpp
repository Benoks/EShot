#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>

#include "recording/RecordingSettingsPolicy.h"

class RecordingSettingsPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesDistinctGifAndVideoFpsLimits()
    {
        QCOMPARE(gifRecordingFpsLimit(), 30);
        QCOMPARE(videoRecordingFpsLimit(), 60);
    }

    void preservesExplicitAudioChoices()
    {
        QVERIFY(!initialAudioEnabled(true, false, false, {}, RecordingAudioSource::Desktop));
        QVERIFY(initialAudioEnabled(true, true, false, {}, RecordingAudioSource::Microphone));
    }

    void migratesEveryLegacyAudioMode()
    {
        QVERIFY(!initialAudioEnabled(false, false, true, QStringLiteral("none"), RecordingAudioSource::Desktop));
        QVERIFY(initialAudioEnabled(false, false, true, QStringLiteral("desktop"), RecordingAudioSource::Desktop));
        QVERIFY(!initialAudioEnabled(false, false, true, QStringLiteral("desktop"), RecordingAudioSource::Microphone));
        QVERIFY(initialAudioEnabled(false, false, true, QStringLiteral("microphone"), RecordingAudioSource::Microphone));
        QVERIFY(initialAudioEnabled(false, false, true, QStringLiteral("both"), RecordingAudioSource::Desktop));
        QVERIFY(initialAudioEnabled(false, false, true, QStringLiteral("both"), RecordingAudioSource::Microphone));
    }

    void enablesBothAudioSourcesForNewUsers()
    {
        QVERIFY(initialAudioEnabled(false, false, false, {}, RecordingAudioSource::Desktop));
        QVERIFY(initialAudioEnabled(false, false, false, {}, RecordingAudioSource::Microphone));
    }

    void persistsNewDefaultsAndLegacyMigrationAsExplicitSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);

        QVERIFY(loadRecordingAudioEnabled(settings, RecordingAudioSource::Desktop));
        QVERIFY(loadRecordingAudioEnabled(settings, RecordingAudioSource::Microphone));
        QVERIFY(settings.contains(QStringLiteral("videoDesktopAudioEnabled")));
        QVERIFY(settings.contains(QStringLiteral("videoMicrophoneEnabled")));

        settings.clear();
        settings.setValue(QStringLiteral("videoAudioMode"), QStringLiteral("desktop"));
        QVERIFY(loadRecordingAudioEnabled(settings, RecordingAudioSource::Desktop));
        QVERIFY(!loadRecordingAudioEnabled(settings, RecordingAudioSource::Microphone));
        QCOMPARE(settings.value(QStringLiteral("videoDesktopAudioEnabled")).toBool(), true);
        QCOMPARE(settings.value(QStringLiteral("videoMicrophoneEnabled")).toBool(), false);
    }

    void discoversDevicesOnlyForAnUnresolvedEnabledMicrophone()
    {
        QVERIFY(shouldDiscoverAudioDevices(true, QStringLiteral("default")));
        QVERIFY(shouldDiscoverAudioDevices(true, QString()));
        QVERIFY(!shouldDiscoverAudioDevices(false, QStringLiteral("default")));
        QVERIFY(!shouldDiscoverAudioDevices(true, QStringLiteral("Easy Effects Source")));
    }
};

QTEST_APPLESS_MAIN(RecordingSettingsPolicyTests)
#include "RecordingSettingsPolicyTests.moc"
