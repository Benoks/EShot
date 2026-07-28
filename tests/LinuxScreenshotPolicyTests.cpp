#include <QtTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "core/LinuxPortalScreenshot.h"
#include "core/LinuxDesktopIntegration.h"
#include "core/LinuxPortalRequest.h"
#include "core/LinuxScreenshotPolicy.h"

class LinuxScreenshotPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void detectsDesktopEnvironment()
    {
        QCOMPARE(LinuxDesktopIntegration::detect(QStringLiteral("KDE"), QString()),
                 LinuxDesktopEnvironment::Kde);
        QCOMPARE(LinuxDesktopIntegration::detect(QStringLiteral("GNOME:GNOME-Classic"), QString()),
                 LinuxDesktopEnvironment::Gnome);
        QCOMPARE(LinuxDesktopIntegration::detect(QString(), QStringLiteral("ubuntu:GNOME")),
                 LinuxDesktopEnvironment::Gnome);
        QCOMPARE(LinuxDesktopIntegration::detect(QStringLiteral("Hyprland"), QString()),
                 LinuxDesktopEnvironment::Other);
    }

    void selectsXWaylandCompatibilityOverlayForSupportedWaylandDesktops()
    {
        QVERIFY(LinuxDesktopIntegration::useXWaylandOverlay(
            LinuxDesktopEnvironment::Kde, QStringLiteral("wayland")));
        QVERIFY(LinuxDesktopIntegration::useXWaylandOverlay(
            LinuxDesktopEnvironment::Gnome, QStringLiteral("Wayland")));
        QVERIFY(!LinuxDesktopIntegration::useXWaylandOverlay(
            LinuxDesktopEnvironment::Other, QStringLiteral("wayland")));
        QVERIFY(!LinuxDesktopIntegration::useXWaylandOverlay(
            LinuxDesktopEnvironment::Gnome, QStringLiteral("x11")));
    }

    void defersGnomeFirstRunHotkeysUntilTheWizardCloses()
    {
        QVERIFY(LinuxDesktopIntegration::deferFirstRunHotkeyRegistration(
            LinuxDesktopEnvironment::Gnome));
        QVERIFY(!LinuxDesktopIntegration::deferFirstRunHotkeyRegistration(
            LinuxDesktopEnvironment::Kde));
        QVERIFY(!LinuxDesktopIntegration::deferFirstRunHotkeyRegistration(
            LinuxDesktopEnvironment::Other));
    }

    void usesGnomeSettingsWhenTheShortcutPortalCannotBeUsed()
    {
        QVERIFY(LinuxDesktopIntegration::useGnomeShortcutFallback(
            LinuxDesktopEnvironment::Gnome, false));
        QVERIFY(!LinuxDesktopIntegration::useGnomeShortcutFallback(
            LinuxDesktopEnvironment::Gnome, true));
        QVERIFY(!LinuxDesktopIntegration::useGnomeShortcutFallback(
            LinuxDesktopEnvironment::Kde, false));
    }

    void showsControlCenterOnlyForTraylessInteractiveGnome()
    {
        QVERIFY(LinuxDesktopIntegration::shouldShowControlCenter(
            LinuxDesktopEnvironment::Gnome, false, false));
        QVERIFY(!LinuxDesktopIntegration::shouldShowControlCenter(
            LinuxDesktopEnvironment::Gnome, true, false));
        QVERIFY(!LinuxDesktopIntegration::shouldShowControlCenter(
            LinuxDesktopEnvironment::Gnome, false, true));
        QVERIFY(!LinuxDesktopIntegration::shouldShowControlCenter(
            LinuxDesktopEnvironment::Kde, false, false));
    }

    void predictsPortalRequestPathBeforeCallingPortal()
    {
        QCOMPARE(LinuxPortalRequest::requestPath(
                     QStringLiteral(":1.245"), QStringLiteral("eshot_capture_1")),
                 QStringLiteral("/org/freedesktop/portal/desktop/request/1_245/eshot_capture_1"));
        QVERIFY(LinuxPortalRequest::requestPath(QString(), QStringLiteral("token")).isEmpty());
        QVERIFY(LinuxPortalRequest::requestPath(QStringLiteral(":1.2"), QString()).isEmpty());
    }

    void detectsKdeWaylandSession()
    {
        QVERIFY(LinuxScreenshotPolicy::isKdeWaylandSession(
            QStringLiteral("KDE"), QString(), QStringLiteral("wayland")));
        QVERIFY(LinuxScreenshotPolicy::isKdeWaylandSession(
            QStringLiteral("KDE:GNOME"), QString(), QStringLiteral("Wayland")));
        QVERIFY(LinuxScreenshotPolicy::isKdeWaylandSession(
            QString(), QStringLiteral("plasma"), QStringLiteral("wayland")));
        QVERIFY(!LinuxScreenshotPolicy::isKdeWaylandSession(
            QStringLiteral("GNOME"), QStringLiteral("KDE"), QStringLiteral("wayland")));
        QVERIFY(!LinuxScreenshotPolicy::isKdeWaylandSession(
            QStringLiteral("KDE"), QString(), QStringLiteral("x11")));
    }

    void detectsGnomeWaylandSessionWithoutTreatingItAsKde()
    {
        QVERIFY(LinuxScreenshotPolicy::isGnomeWaylandSession(
            QStringLiteral("GNOME"), QString(), QStringLiteral("wayland")));
        QVERIFY(LinuxScreenshotPolicy::isGnomeWaylandSession(
            QString(), QStringLiteral("ubuntu:GNOME"), QStringLiteral("Wayland")));
        QVERIFY(!LinuxScreenshotPolicy::isGnomeWaylandSession(
            QStringLiteral("KDE"), QStringLiteral("GNOME"), QStringLiteral("wayland")));
        QVERIFY(!LinuxScreenshotPolicy::isGnomeWaylandSession(
            QStringLiteral("GNOME"), QString(), QStringLiteral("x11")));
    }

    void preparesKWinPermissionForAnyKdeWaylandExecutable()
    {
        const QString executable = QStringLiteral("/tmp/.mount_EShot.test/usr/bin/EShot");

        QVERIFY(LinuxScreenshotPolicy::shouldPrepareKWinPermission(
            QStringLiteral("KDE"), QString(), QStringLiteral("wayland"),
            QStringLiteral("/home/user/EShot.AppImage"), executable));
        QVERIFY(!LinuxScreenshotPolicy::shouldPrepareKWinPermission(
            QStringLiteral("GNOME"), QString(), QStringLiteral("wayland"),
            QStringLiteral("/home/user/EShot.AppImage"), executable));
        QVERIFY(!LinuxScreenshotPolicy::shouldPrepareKWinPermission(
            QStringLiteral("KDE"), QString(), QStringLiteral("x11"),
            QStringLiteral("/home/user/EShot.AppImage"), executable));
        QVERIFY(LinuxScreenshotPolicy::shouldPrepareKWinPermission(
            QStringLiteral("KDE"), QString(), QStringLiteral("wayland"),
            QString(), executable));
    }

    void buildsHiddenKWinPermissionDesktopEntry()
    {
        const QString entry = LinuxScreenshotPolicy::kwinPermissionDesktopEntry(
            QStringLiteral("/tmp/.mount_EShot.test/usr/bin/EShot"));

        QVERIFY(entry.contains(QStringLiteral("NoDisplay=true\n")));
        QVERIFY(entry.contains(QStringLiteral(
            "Exec=\"/tmp/.mount_EShot.test/usr/bin/EShot\"\n")));
        QVERIFY(entry.contains(QStringLiteral(
            "X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2\n")));
    }

    void installsKWinPermissionDesktopEntryAtomically()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        QString desktopPath;
        QString error;
        QVERIFY(LinuxScreenshotPolicy::installKWinPermissionDesktopEntry(
            directory.path(), QStringLiteral("/tmp/.mount_EShot.test/usr/bin/EShot"),
            &desktopPath, &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(desktopPath, directory.filePath(
            QStringLiteral("io.github.benoks.EShot.KWinScreenshot.desktop")));

        QFile desktopFile(desktopPath);
        QVERIFY(desktopFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(QString::fromUtf8(desktopFile.readAll()),
                 LinuxScreenshotPolicy::kwinPermissionDesktopEntry(
                     QStringLiteral("/tmp/.mount_EShot.test/usr/bin/EShot")));
    }

    void buildsCursorFreeSpectacleArguments()
    {
        const QStringList arguments = LinuxScreenshotPolicy::spectacleWorkspaceArguments(
            QStringLiteral("/tmp/EShot capture.png"));

        QCOMPARE(arguments,
                 QStringList({QStringLiteral("--fullscreen"),
                              QStringLiteral("--background"),
                              QStringLiteral("--nonotify"),
                              QStringLiteral("--output"),
                              QStringLiteral("/tmp/EShot capture.png")}));
        QVERIFY(!arguments.contains(QStringLiteral("--pointer")));
    }

    void buildsStandardsCompliantPortalScreenshotOptions()
    {
        const QVariantMap modern = LinuxScreenshotPolicy::portalScreenshotOptions(
            QStringLiteral("eshot_token"), 3u, 1u | 2u | 4u);
        QCOMPARE(modern.value(QStringLiteral("handle_token")).toString(),
                 QStringLiteral("eshot_token"));
        QCOMPARE(modern.value(QStringLiteral("interactive")).toBool(), false);
        QCOMPARE(modern.value(QStringLiteral("modal")).toBool(), false);
        QCOMPARE(modern.value(QStringLiteral("target")).toUInt(), 1u);
        QVERIFY(!modern.contains(QStringLiteral("include-cursor")));
        QVERIFY(!modern.contains(QStringLiteral("include_cursor")));

        const QVariantMap legacy = LinuxScreenshotPolicy::portalScreenshotOptions(
            QStringLiteral("eshot_token"), 2u, 0u);
        QVERIFY(!legacy.contains(QStringLiteral("target")));
    }

    void rejectsCursorBearingFallbackOnKdeWayland()
    {
        QVERIFY(!LinuxScreenshotPolicy::allowCursorBearingFallback(true));
        QVERIFY(LinuxScreenshotPolicy::allowCursorBearingFallback(false));
    }

    void presentsOverlayOnlyForUsableCapture()
    {
        QVERIFY(LinuxScreenshotPolicy::canPresentCapture(true));
        QVERIFY(!LinuxScreenshotPolicy::canPresentCapture(false));
    }

    void loadsAndCleansUpSpectacleWorkspaceCapture()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString sourcePath = directory.filePath(QStringLiteral("source.png"));
        QImage source(13, 7, QImage::Format_ARGB32_Premultiplied);
        source.fill(QColor(12, 34, 56));
        QVERIFY(source.save(sourcePath));

        const QString executablePath = directory.filePath(QStringLiteral("spectacle"));
        QFile executable(executablePath);
        QVERIFY(executable.open(QIODevice::WriteOnly | QIODevice::Text));
        executable.write(
            "#!/bin/sh\n"
            "output=\n"
            "while [ \"$#\" -gt 0 ]; do\n"
            "  if [ \"$1\" = \"--output\" ]; then shift; output=$1; fi\n"
            "  shift\n"
            "done\n"
            "/usr/bin/cp \"$ESHOT_TEST_SPECTACLE_IMAGE\" \"$output\"\n");
        executable.close();
        QVERIFY(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner));

        const QByteArray oldPath = qgetenv("PATH");
        qputenv("PATH", directory.path().toLocal8Bit());
        qputenv("ESHOT_TEST_SPECTACLE_IMAGE", sourcePath.toLocal8Bit());
        const QPixmap capture = LinuxPortalScreenshot::grabSpectacleWorkspace(nullptr, 5000);
        qputenv("PATH", oldPath);
        qunsetenv("ESHOT_TEST_SPECTACLE_IMAGE");

        QVERIFY(!capture.isNull());
        QCOMPARE(capture.size(), QSize(13, 7));
        const QStringList leftovers = QDir(directory.path()).entryList(
            {QStringLiteral("eshot-spectacle-*.png")}, QDir::Files);
        QVERIFY(leftovers.isEmpty());
    }
};

QTEST_MAIN(LinuxScreenshotPolicyTests)
#include "LinuxScreenshotPolicyTests.moc"
