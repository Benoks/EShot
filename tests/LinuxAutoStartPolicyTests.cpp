#include <QtTest>

#include "core/LinuxAutoStartPolicy.h"

#include <QFile>
#include <QTemporaryDir>

class LinuxAutoStartPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void prefersExistingOuterAppImage();
    void fallsBackWhenAppImageIsEmpty();
    void fallsBackWhenAppImageDoesNotExist();
    void usesXWaylandForKdeAndGnomeWithOptInNativeKde();
};

void LinuxAutoStartPolicyTests::prefersExistingOuterAppImage()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString appImagePath = tempDir.filePath(QStringLiteral("EShot.AppImage"));
    QFile appImage(appImagePath);
    QVERIFY(appImage.open(QIODevice::WriteOnly));
    appImage.close();

    QCOMPARE(LinuxAutoStartPolicy::executablePath(
                 appImagePath, QStringLiteral("/tmp/.mount_EShot/usr/bin/EShot")),
             appImagePath);
}

void LinuxAutoStartPolicyTests::fallsBackWhenAppImageIsEmpty()
{
    const QString executable = QStringLiteral("/opt/eshot/bin/EShot");
    QCOMPARE(LinuxAutoStartPolicy::executablePath(QString(), executable), executable);
}

void LinuxAutoStartPolicyTests::fallsBackWhenAppImageDoesNotExist()
{
    const QString executable = QStringLiteral("/opt/eshot/bin/EShot");
    QCOMPARE(LinuxAutoStartPolicy::executablePath(
                 QStringLiteral("/missing/EShot.AppImage"), executable),
             executable);
}

void LinuxAutoStartPolicyTests::usesXWaylandForKdeAndGnomeWithOptInNativeKde()
{
    const QString executable = QStringLiteral("/opt/EShot AppImage");
    const QString kde = LinuxAutoStartPolicy::commandLine(
        executable, QStringLiteral("KDE"), QString(), QStringLiteral("wayland"));
    const QString gnome = LinuxAutoStartPolicy::commandLine(
        executable, QStringLiteral("GNOME"), QString(), QStringLiteral("wayland"));
    const QString gnomeX11 = LinuxAutoStartPolicy::commandLine(
        executable, QStringLiteral("GNOME"), QString(), QStringLiteral("x11"));

    QVERIFY(kde.startsWith(QStringLiteral(
        "/usr/bin/env QT_QPA_PLATFORM=\"xcb;wayland\" ESHOT_WAYLAND_XWAYLAND_OVERLAY=1 ")));
    QVERIFY(LinuxAutoStartPolicy::commandLine(
                executable, QStringLiteral("KDE"), QString(), QStringLiteral("wayland"),
                QStringLiteral("wayland"))
                .startsWith(QStringLiteral(
                    "/usr/bin/env QT_QPA_PLATFORM=wayland ESHOT_WAYLAND_XWAYLAND_OVERLAY=0 "
                    "ESHOT_CAPTURE_BACKEND=wayland ")));
    QVERIFY(gnome.startsWith(QStringLiteral(
        "/usr/bin/env QT_QPA_PLATFORM=\"xcb;wayland\" ESHOT_WAYLAND_XWAYLAND_OVERLAY=1 ")));
    QCOMPARE(gnomeX11, QStringLiteral("\"/opt/EShot AppImage\" --silent"));
    QCOMPARE(LinuxAutoStartPolicy::commandLine(
                 executable, QStringLiteral("KDE"), QString(), QStringLiteral("x11")),
             gnomeX11);
    QCOMPARE(LinuxAutoStartPolicy::commandLine(
                 executable, QStringLiteral("sway"), QString(), QStringLiteral("wayland")),
             gnomeX11);
    QVERIFY(LinuxAutoStartPolicy::commandLine(
                executable, QString(), QStringLiteral("plasma"), QStringLiteral("Wayland"))
                .startsWith(QStringLiteral("/usr/bin/env QT_QPA_PLATFORM=\"xcb;wayland\" ")));
}

QTEST_MAIN(LinuxAutoStartPolicyTests)
#include "LinuxAutoStartPolicyTests.moc"
