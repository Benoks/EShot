#include <QtTest/QTest>

#include "core/TranslationManager.h"

class SettingsTranslationTests : public QObject
{
    Q_OBJECT

private slots:
    void issue15SettingsTextExistsInEveryLanguage();
    void v416TextExistsInEveryLanguage();
};

void SettingsTranslationTests::issue15SettingsTextExistsInEveryLanguage()
{
    const QStringList keys = {
        QStringLiteral("tabPackages"),
        QStringLiteral("mediaFolders"),
        QStringLiteral("screenshotsLabel"),
        QStringLiteral("videosLabel"),
        QStringLiteral("defaultFolderHint"),
        QStringLiteral("componentStatus"),
        QStringLiteral("packageInstalled"),
        QStringLiteral("packageMissing"),
        QStringLiteral("packageMissingDownloadable"),
        QStringLiteral("packageDelete"),
        QStringLiteral("packageDownload"),
        QStringLiteral("packageDownloading"),
        QStringLiteral("ocrLanguagePacks"),
        QStringLiteral("ocrPackagesHint"),
        QStringLiteral("downloadEssentials"),
        QStringLiteral("deleteSelected"),
        QStringLiteral("ocrPackageInstallHint"),
        QStringLiteral("ocrEngineMissingHint"),
        QStringLiteral("ocrTranslate"),
        QStringLiteral("ocrTranslateBrowserError"),
        QStringLiteral("formatLabel"),
        QStringLiteral("instantCopyAfterSelection"),
        QStringLiteral("gifSizeSmallest"),
        QStringLiteral("gifSizeBalanced"),
        QStringLiteral("gifSizeBest"),
        QStringLiteral("gifSizePresetLabel"),
        QStringLiteral("recordingStartDelayLabel"),
        QStringLiteral("desktopVolumeLabel"),
        QStringLiteral("microphoneVolumeLabel"),
        QStringLiteral("defaultAudioDevice"),
        QStringLiteral("visualSearchPrivacy"),
        QStringLiteral("visualSearchFailed"),
        QStringLiteral("visualSearchTempCreateError"),
        QStringLiteral("visualSearchTempPrepareError"),
        QStringLiteral("visualSearchUploaderCreateError"),
        QStringLiteral("visualSearchUploadUnavailable"),
    };

    for (int language = 0; language < TranslationManager::LangCount; ++language) {
        TranslationManager::setLanguage(static_cast<TranslationManager::Language>(language), false);
        for (const QString &key : keys) {
            const QString translated = TranslationManager::tr(key.toUtf8().constData());
            QVERIFY2(translated != key,
                     qPrintable(QStringLiteral("Missing translation for %1 in language %2")
                                    .arg(key).arg(language)));
        }
    }
}

void SettingsTranslationTests::v416TextExistsInEveryLanguage()
{
    const QStringList keys = {
        QStringLiteral("trayCancelRecording"),
        QStringLiteral("actionLock"),
        QStringLiteral("quickGifRecording"),
        QStringLiteral("videoRecordingTitle"),
        QStringLiteral("gifFpsLabel"),
        QStringLiteral("videoFpsLabel"),
        QStringLiteral("quickMaxSeconds"),
        QStringLiteral("recordingUnlimited"),
        QStringLiteral("recordingLoop"),
        QStringLiteral("recordingLoopInfinite"),
        QStringLiteral("videoQualityCrf"),
        QStringLiteral("audioDesktop"),
        QStringLiteral("audioMicrophone"),
        QStringLiteral("audioMicrophoneDevice"),
        QStringLiteral("defaultAudioDevice"),
        QStringLiteral("recordingStart"),
        QStringLiteral("recordingCancel"),
    };

    for (int language = 0; language < TranslationManager::LangCount; ++language) {
        TranslationManager::setLanguage(static_cast<TranslationManager::Language>(language), false);
        for (const QString &key : keys) {
            const QString translated = TranslationManager::tr(key.toUtf8().constData());
            QVERIFY2(translated != key,
                     qPrintable(QStringLiteral("Missing translation for %1 in language %2")
                                    .arg(key).arg(language)));
        }
    }
}

QTEST_APPLESS_MAIN(SettingsTranslationTests)

#include "SettingsTranslationTests.moc"
