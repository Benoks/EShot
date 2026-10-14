#include <QtTest>

#include "core/UpdatePolicy.h"

namespace {
QJsonObject release(const QString &tag, bool prerelease = false, bool draft = false)
{
    return {{QStringLiteral("tag_name"), tag},
            {QStringLiteral("prerelease"), prerelease},
            {QStringLiteral("draft"), draft}};
}
}

class UpdatePolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void onlyWritableAppImagesUpdateThemselves()
    {
        QCOMPARE(linuxUpdateChannel(QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage"),
                                    true, true),
                 LinuxUpdateChannel::SelfUpdate);
        QCOMPARE(linuxUpdateChannel(QStringLiteral("/opt/eshot/EShot.AppImage"), true, false),
                 LinuxUpdateChannel::Aur);
        QCOMPARE(linuxUpdateChannel(QStringLiteral("/opt/eshot/EShot.AppImage"), true, true),
                 LinuxUpdateChannel::Aur);
        QCOMPARE(linuxUpdateChannel(QString(), false, false),
                 LinuxUpdateChannel::PackageManager);
        QCOMPARE(linuxUpdateChannel(QStringLiteral("/usr/local/bin/EShot.AppImage"), true, false),
                 LinuxUpdateChannel::PackageManager);
    }

    void countsOnlyNewerStableReleases()
    {
        const QJsonArray releases{
            release(QStringLiteral("v4.2.1")),
            release(QStringLiteral("v4.2.0")),
            release(QStringLiteral("v4.2.0-rc1"), true),
            release(QStringLiteral("v4.1.7"), false, true),
            release(QStringLiteral("v4.1.6"))};

        QCOMPARE(countNewerStableReleases(releases, QStringLiteral("4.1.6")), 2);
        QCOMPARE(countNewerStableReleases(releases, QStringLiteral("4.2.0")), 1);
    }

    void comparesMultiDigitVersionsNumerically()
    {
        // As text, "4.10.0" would sort before "4.9.9".
        const QJsonArray releases{
            release(QStringLiteral("v4.10.1")),
            release(QStringLiteral("v4.10.0")),
            release(QStringLiteral("v4.9.9"))};

        QCOMPARE(countNewerStableReleases(releases, QStringLiteral("4.9.9")), 2);
        QCOMPARE(countNewerStableReleases(releases, QStringLiteral("4.10.0")), 1);
        QCOMPARE(countNewerStableReleases(releases, QStringLiteral("4.10.1")), 0);
    }

    void requiresTwoMissedReleasesAndAManagedInstall()
    {
        QVERIFY(!shouldSilentlyInstallUpdate(1, true));
        QVERIFY(!shouldSilentlyInstallUpdate(2, false));
        QVERIFY(shouldSilentlyInstallUpdate(2, true));
    }

    void portableMarkerDisablesOnlyAutomaticChecks()
    {
        QTemporaryDir app;
        QVERIFY(app.isValid());
        QVERIFY(shouldCheckForUpdates(false, app.path()));
        QFile marker(app.filePath(QStringLiteral(".portable")));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
        QVERIFY(!shouldCheckForUpdates(false, app.path()));
        QVERIFY(shouldCheckForUpdates(true, app.path()));
        QVERIFY(marker.remove());
        QVERIFY(shouldCheckForUpdates(false, app.path()));
    }

    void directoryNamedPortableIsNotAMarkerFile()
    {
        QTemporaryDir app;
        QVERIFY(app.isValid());
        QVERIFY(QDir(app.path()).mkdir(QStringLiteral(".portable")));
        QVERIFY(shouldCheckForUpdates(false, app.path()));
    }

    void appImageMarkerLivesBesideTheOriginalAppImage()
    {
        QTemporaryDir mountedApp;
        QTemporaryDir downloads;
        QVERIFY(mountedApp.isValid());
        QVERIFY(downloads.isValid());
        const QString image = downloads.filePath(QStringLiteral("EShot.AppImage"));
        QVERIFY(shouldCheckForUpdates(false, mountedApp.path(), image));
        QFile marker(downloads.filePath(QStringLiteral(".portable")));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
        QVERIFY(!shouldCheckForUpdates(false, mountedApp.path(), image));
        QVERIFY(shouldCheckForUpdates(true, mountedApp.path(), image));
        // A marker belonging to another installation must not affect this one.
        QVERIFY(shouldCheckForUpdates(false, mountedApp.path()));
    }
};

QTEST_APPLESS_MAIN(UpdatePolicyTests)
#include "UpdatePolicyTests.moc"
