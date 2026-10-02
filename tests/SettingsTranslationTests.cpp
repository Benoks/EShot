#include <QtTest/QTest>

#include "core/TranslationManager.h"

class SettingsTranslationTests : public QObject
{
    Q_OBJECT

private slots:
    void issue15SettingsTextExistsInEveryLanguage();
    void v416TextExistsInEveryLanguage();
    void hotkeyFailureTextExistsInEveryLanguage();
    void linuxRemovalTextExistsInEveryLanguage();
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
        QStringLiteral("packageChecksumUnavailable"),
        QStringLiteral("packageChecksumMismatch"),
        QStringLiteral("ocrLanguagePacks"),
        QStringLiteral("ocrPackagesHint"),
        QStringLiteral("downloadEssentials"),
        QStringLiteral("deleteSelected"),
        QStringLiteral("ocrPackageInstallHint"),
        QStringLiteral("ocrEngineMissingHint"),
        QStringLiteral("ocrTranslate"),
        QStringLiteral("ocrTranslateBrowserError"),
        QStringLiteral("formatLabel"),
        QStringLiteral("rememberSettingsWindowSize"),
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

void SettingsTranslationTests::hotkeyFailureTextExistsInEveryLanguage()
{
    const QStringList keys = {
        QStringLiteral("hotkeyNotActiveTitle"),
        QStringLiteral("hotkeyNotActiveBody"),
        QStringLiteral("hotkeyCaptureFallback"),
        QStringLiteral("hotkeyNoneActive"),
        QStringLiteral("hotkeyConflictWith"),
        QStringLiteral("hotkeyConflictSave"),
        QStringLiteral("trayWelcomeBodyNoHotkey"),
    };

    for (int language = 0; language < TranslationManager::LangCount; ++language) {
        TranslationManager::setLanguage(static_cast<TranslationManager::Language>(language), false);
        for (const QString &key : keys) {
            const QString translated = TranslationManager::tr(key.toUtf8().constData());
            QVERIFY2(translated != key,
                     qPrintable(QStringLiteral("Missing translation for %1 in language %2")
                                    .arg(key).arg(language)));
        }
        // Placeholders must survive translation or arg() would drop the key.
        QVERIFY(TranslationManager::hotkeyNotActiveBody().contains(QStringLiteral("%1")));
        QVERIFY(TranslationManager::hotkeyNotActiveBody().contains(QStringLiteral("%2")));
        QVERIFY(TranslationManager::hotkeyCaptureFallback().contains(QStringLiteral("%1")));
        QVERIFY(TranslationManager::hotkeyConflictWith().contains(QStringLiteral("%1")));
    }
}

void SettingsTranslationTests::linuxRemovalTextExistsInEveryLanguage()
{
    const QStringList keys = {
        QStringLiteral("removeFromSystemTitle"),
        QStringLiteral("removeFromSystem"),
        QStringLiteral("removeFromSystemConfirm"),
        QStringLiteral("removeFromSystemConfirmPackage"),
        QStringLiteral("removeFromSystemDone"),
        QStringLiteral("removeFromSystemDonePackage"),
        QStringLiteral("removeFromSystemErrors"),
    };

    for (int language = 0; language < TranslationManager::LangCount; ++language) {
        TranslationManager::setLanguage(static_cast<TranslationManager::Language>(language), false);
        for (const QString &key : keys) {
            const QString translated = TranslationManager::tr(key.toUtf8().constData());
            QVERIFY2(!translated.isEmpty() && translated != key,
                     qPrintable(QStringLiteral("Missing translation for %1 in language %2")
                                    .arg(key).arg(language)));
        }
    }
}

QTEST_APPLESS_MAIN(SettingsTranslationTests)

#include "SettingsTranslationTests.moc"
