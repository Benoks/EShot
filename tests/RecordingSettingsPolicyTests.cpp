#include <QtTest>

#include <algorithm>
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

    void enablesOnlyDesktopAudioForNewUsers()
    {
        QVERIFY(initialAudioEnabled(false, false, false, {}, RecordingAudioSource::Desktop));
        QVERIFY(!initialAudioEnabled(false, false, false, {}, RecordingAudioSource::Microphone));
    }

    void persistsNewDefaultsAndLegacyMigrationAsExplicitSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);

        QVERIFY(loadRecordingAudioEnabled(settings, RecordingAudioSource::Desktop));
        QVERIFY(!loadRecordingAudioEnabled(settings, RecordingAudioSource::Microphone));
        QVERIFY(settings.contains(QStringLiteral("videoDesktopAudioEnabled")));
        QVERIFY(settings.contains(QStringLiteral("videoMicrophoneEnabled")));

        settings.clear();
        settings.setValue(QStringLiteral("videoAudioMode"), QStringLiteral("desktop"));
        QVERIFY(loadRecordingAudioEnabled(settings, RecordingAudioSource::Desktop));
        QVERIFY(!loadRecordingAudioEnabled(settings, RecordingAudioSource::Microphone));
        QCOMPARE(settings.value(QStringLiteral("videoDesktopAudioEnabled")).toBool(), true);
        QCOMPARE(settings.value(QStringLiteral("videoMicrophoneEnabled")).toBool(), false);
    }

    void gifPresetsMapToSizeAndFps()
    {
        QCOMPARE(gifQualityValues(RecordingQualityPreset::Small).maxSide, 720);
        QCOMPARE(gifQualityValues(RecordingQualityPreset::Small).fps, 10);
        QCOMPARE(gifQualityValues(RecordingQualityPreset::Balanced).maxSide, 1280);
        QCOMPARE(gifQualityValues(RecordingQualityPreset::Balanced).fps, 12);
        QCOMPARE(gifQualityValues(RecordingQualityPreset::High).maxSide, 1920);
        QCOMPARE(gifQualityValues(RecordingQualityPreset::High).fps, 15);
        QCOMPARE(gifQualityValues(RecordingQualityPreset::Balanced).fps, DefaultGifFps);
    }

    void videoPresetsMapToCrfAndFps()
    {
        QCOMPARE(videoQualityValues(RecordingQualityPreset::Small).crf, 30);
        QCOMPARE(videoQualityValues(RecordingQualityPreset::Balanced).crf, 24);
        QCOMPARE(videoQualityValues(RecordingQualityPreset::High).crf, 20);
        for (RecordingQualityPreset preset : selectableRecordingQualityPresets()) {
            QCOMPARE(videoQualityValues(preset).fps, 30);
            const int crf = videoQualityValues(preset).crf;
            QVERIFY(crf >= MinVideoCrf && crf <= MaxVideoCrf);
        }
        // The old defaults are exactly "Balanced".
        QCOMPARE(videoQualityPresetFor(DefaultVideoCrf, DefaultVideoFps), RecordingQualityPreset::Balanced);
    }

    void unmatchedRawValuesAreCustom()
    {
        QCOMPARE(gifQualityPresetFor(1280, 12), RecordingQualityPreset::Balanced);
        QCOMPARE(gifQualityPresetFor(1280, 14), RecordingQualityPreset::Custom);
        QCOMPARE(gifQualityPresetFor(720, 15), RecordingQualityPreset::Custom);
        QCOMPARE(videoQualityPresetFor(20, 30), RecordingQualityPreset::High);
        QCOMPARE(videoQualityPresetFor(20, 60), RecordingQualityPreset::Custom);
        QCOMPARE(videoQualityPresetFor(26, 30), RecordingQualityPreset::Custom);
    }

    void presetKeysRoundTrip()
    {
        for (RecordingQualityPreset preset : selectableRecordingQualityPresets())
            QCOMPARE(recordingQualityPresetFromKey(recordingQualityPresetKey(preset)), preset);
        QCOMPARE(recordingQualityPresetKey(RecordingQualityPreset::Custom), QStringLiteral("custom"));
        QCOMPARE(recordingQualityPresetFromKey(QStringLiteral(" High ")), RecordingQualityPreset::High);
        QCOMPARE(recordingQualityPresetFromKey(QStringLiteral("bogus")), RecordingQualityPreset::Custom);
    }

    void applyingAPresetWritesTheRawKeysTheRecordersRead()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);

        applyGifQualityPreset(settings, RecordingQualityPreset::High);
        QCOMPARE(settings.value("recordingMaxSide").toInt(), 1920);
        QCOMPARE(settings.value("recordingFps").toInt(), 15);
        QCOMPARE(settings.value("gifQualityPreset").toString(), QStringLiteral("high"));
        QCOMPARE(loadGifQualityPreset(settings), RecordingQualityPreset::High);

        applyVideoQualityPreset(settings, RecordingQualityPreset::Small);
        QCOMPARE(settings.value("videoRecordingCrf").toInt(), 30);
        QCOMPARE(settings.value("videoRecordingFps").toInt(), 30);
        QCOMPARE(settings.value("videoQualityPreset").toString(), QStringLiteral("small"));
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Small);

        // Raw values stay authoritative: editing one makes it Custom.
        settings.setValue("videoRecordingFps", 60);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Custom);
        applyVideoQualityPreset(settings, RecordingQualityPreset::Custom);
        QCOMPARE(settings.value("videoRecordingFps").toInt(), 60);
    }

    void freshInstallsStartBalanced()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        QCOMPARE(loadGifQualityPreset(settings), RecordingQualityPreset::Balanced);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Balanced);
    }

    void untouchedLegacyGifDefaultsMoveToBalancedOnce()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        settings.setValue("recordingFps", LegacyDefaultGifFps);
        settings.setValue("recordingMaxSide", 1280);
        QCOMPARE(loadGifQualityPreset(settings), RecordingQualityPreset::Balanced);
        QCOMPARE(settings.value("recordingFps").toInt(), 12);

        // Once a preset was stored, 10 fps at 1280 px is a deliberate choice.
        settings.setValue("recordingFps", LegacyDefaultGifFps);
        QCOMPARE(loadGifQualityPreset(settings), RecordingQualityPreset::Custom);
        QCOMPARE(settings.value("recordingFps").toInt(), LegacyDefaultGifFps);
    }

    void customLegacyGifValuesAreKept()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        settings.setValue("recordingFps", 20);
        settings.setValue("recordingMaxSide", 1920);
        QCOMPARE(loadGifQualityPreset(settings), RecordingQualityPreset::Custom);
        QCOMPARE(settings.value("recordingFps").toInt(), 20);
        QVERIFY(!settings.contains("gifQualityPreset"));
    }

    void untouchedLegacyVideoDefaultsMoveToBalancedOnce()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        // An install from before the presets: CRF 24 with a stored frame rate.
        settings.setValue("videoRecordingCrf", DefaultVideoCrf);
        settings.setValue("videoRecordingFps", 25);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Balanced);
        QCOMPARE(settings.value("videoRecordingFps").toInt(), DefaultVideoFps);
        QCOMPARE(settings.value("videoQualityPreset").toString(), QStringLiteral("balanced"));

        // Once a preset was stored, other values are a deliberate choice.
        settings.setValue("videoRecordingFps", 25);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Custom);
        QCOMPARE(settings.value("videoRecordingFps").toInt(), 25);
    }

    void customLegacyVideoValuesAreKept()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        settings.setValue("videoRecordingCrf", DefaultVideoCrf);
        settings.setValue("videoRecordingFps", 60);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Custom);
        QVERIFY(!settings.contains("videoQualityPreset"));

        settings.setValue("videoRecordingCrf", 20);
        settings.setValue("videoRecordingFps", 24);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Custom);
        QVERIFY(!settings.contains("videoQualityPreset"));
    }

    void chosenCustomStaysCustomEvenWithPresetValues()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        applyVideoQualityPreset(settings, RecordingQualityPreset::Balanced);
        applyVideoQualityPreset(settings, RecordingQualityPreset::Custom);
        QCOMPARE(settings.value("videoRecordingCrf").toInt(), DefaultVideoCrf);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::Custom);
        applyVideoQualityPreset(settings, RecordingQualityPreset::High);
        QCOMPARE(loadVideoQualityPreset(settings), RecordingQualityPreset::High);

        applyGifQualityPreset(settings, RecordingQualityPreset::Small);
        applyGifQualityPreset(settings, RecordingQualityPreset::Custom);
        QCOMPARE(loadGifQualityPreset(settings), RecordingQualityPreset::Custom);
        QCOMPARE(settings.value("recordingMaxSide").toInt(), 720);
    }

    void customFieldChoicesAndQualitySlider()
    {
        const QList<int> sides = gifMaxSideChoices();
        for (RecordingQualityPreset preset : selectableRecordingQualityPresets())
            QVERIFY(sides.contains(gifQualityValues(preset).maxSide));
        QVERIFY(std::is_sorted(sides.cbegin(), sides.cend()));

        // Higher slider = better quality = lower CRF; round trip in range.
        QVERIFY(videoQualitySliderValue(20) > videoQualitySliderValue(30));
        for (int crf = MinVideoCrf; crf <= MaxVideoCrf; ++crf)
            QCOMPARE(videoCrfFromQualitySlider(videoQualitySliderValue(crf)), crf);
        QCOMPARE(videoCrfFromQualitySlider(videoQualitySliderValue(99)), MaxVideoCrf);
        QCOMPARE(videoCrfFromQualitySlider(videoQualitySliderValue(0)), MinVideoCrf);
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
