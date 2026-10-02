#include "LinuxUninstaller.h"

#include "LinuxGnomeShortcutInstaller.h"
#include "LinuxKGlobalAccelShortcuts.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyCombination>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr int FirstEshotAction = 1;
constexpr int LastEshotAction = 7;

void restoreKdePrintScreen(LinuxUninstaller::Report &report)
{
    QDBusInterface accel(QStringLiteral("org.kde.kglobalaccel"),
                         QStringLiteral("/kglobalaccel"),
                         QStringLiteral("org.kde.KGlobalAccel"),
                         QDBusConnection::sessionBus());
    if (!accel.isValid())
        return;

    // Collect EShot's keys before unregistering, then drop every EShot action
    // so KDE's shortcut settings no longer list an application that is gone.
    QList<int> eshotKeys;
    for (int id = FirstEshotAction; id <= LastEshotAction; ++id) {
        const QStringList actionId = LinuxKGlobalAccelShortcuts::actionId(id);
        const QDBusReply<QList<int>> keys = accel.call(QStringLiteral("shortcut"), actionId);
        if (keys.isValid())
            eshotKeys += keys.value();
        accel.call(QStringLiteral("unregister"), actionId.at(0), actionId.at(1));
    }

    const QStringList spectacleLaunchId = {
        QStringLiteral("org.kde.spectacle.desktop"),
        QStringLiteral("_launch"),
        QStringLiteral("Spectacle"),
        QStringLiteral("Launch Spectacle")
    };
    const QDBusReply<QList<int>> current = accel.call(QStringLiteral("shortcut"), spectacleLaunchId);
    const QDBusReply<QList<int>> defaults = accel.call(QStringLiteral("defaultShortcut"),
                                                       spectacleLaunchId);
    if (!current.isValid() || !defaults.isValid()
        || !LinuxUninstallPolicy::shouldRestoreSpectaclePrint(eshotKeys, current.value(),
                                                               defaults.value())) {
        return;
    }

    const int print = QKeyCombination(Qt::NoModifier, Qt::Key_Print).toCombined();
    const QDBusReply<bool> available = accel.call(QStringLiteral("isGlobalShortcutAvailable"),
                                                  print, spectacleLaunchId.at(0));
    if (available.isValid() && !available.value()) {
        report.errors.append(QStringLiteral(
            "Print Screen is used by another KDE shortcut; Spectacle was left unchanged."));
        return;
    }
    const QDBusReply<void> restored = accel.call(
        QStringLiteral("setForeignShortcut"), spectacleLaunchId,
        QVariant::fromValue(LinuxUninstallPolicy::spectacleShortcutsWithPrint(current.value())));
    if (!restored.isValid())
        report.errors.append(QStringLiteral("KDE did not restore Spectacle's Print Screen shortcut."));
}

void removeFile(const QString &path, LinuxUninstaller::Report &report)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return;
    if (QFile::remove(path))
        report.removed.append(path);
    else
        report.errors.append(QStringLiteral("Could not remove %1").arg(path));
}

void refreshDesktopCaches(const QString &dataHome)
{
    // Best effort, like AppRun's install_desktop_entry: menus and KWin's
    // service cache drop the removed entries without a re-login.
    const auto start = [](const QString &program, const QStringList &arguments) {
        if (!QStandardPaths::findExecutable(program).isEmpty())
            QProcess::startDetached(program, arguments);
    };
    start(QStringLiteral("update-desktop-database"),
          {QDir(dataHome).filePath(QStringLiteral("applications"))});
    start(QStringLiteral("gtk-update-icon-cache"),
          {QStringLiteral("-f"), QStringLiteral("-t"),
           QDir(dataHome).filePath(QStringLiteral("icons/hicolor"))});
    start(QStringLiteral("kbuildsycoca6"), {QStringLiteral("--noincremental")});
}

}

LinuxUninstallPolicy::InstallKind LinuxUninstaller::currentInstallKind()
{
    return LinuxUninstallPolicy::installKind(qEnvironmentVariable("APPIMAGE"),
                                             QCoreApplication::applicationFilePath(),
                                             QDir::homePath());
}

LinuxUninstaller::Report LinuxUninstaller::run()
{
    Report report;

    const auto gnome = LinuxGnomeShortcutInstaller::restoreDesktopScreenshotShortcut();
    if (!gnome.success)
        report.errors.append(QStringLiteral("GNOME shortcut: %1").arg(gnome.error));
    restoreKdePrintScreen(report);

    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const LinuxUninstallPolicy::Plan plan = LinuxUninstallPolicy::plan(
        qEnvironmentVariable("APPIMAGE"), QCoreApplication::applicationFilePath(),
        QDir::homePath(), dataHome,
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    report.kind = plan.kind;

    for (const QString &path : plan.files)
        removeFile(path, report);
    for (const QString &directory : plan.emptyDirectories) {
        if (QDir(directory).exists() && QDir().rmdir(directory))
            report.removed.append(directory);
    }
    if (!plan.ownedDirectory.isEmpty() && QDir(plan.ownedDirectory).exists()) {
        if (QDir(plan.ownedDirectory).removeRecursively())
            report.removed.append(plan.ownedDirectory);
        else
            report.errors.append(QStringLiteral("Could not remove %1").arg(plan.ownedDirectory));
    }

    // Shortcuts were handed back above; a later reinstall should offer the
    // Linux setup (and Print Screen assignment) again. Other settings stay.
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    settings.remove(QStringLiteral("linuxSetupCompleted"));
    settings.sync();

    refreshDesktopCaches(dataHome);
    return report;
}
