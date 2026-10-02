#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Decides what "Remove EShot from this system" touches on Linux. Everything
// here is pure so the file list can be tested against a temporary HOME.
namespace LinuxUninstallPolicy
{
enum class InstallKind {
    // An AppImage the user downloaded or EShot integrated into ~/.local/opt.
    AppImage,
    // A per-user build copied into ~/.local/opt/EShot by install-user.sh.
    UserDirectory,
    // A system package (AUR, /opt, /usr) or the portable archive. Its files
    // belong to the package manager or the user, so only per-user
    // integration is removed.
    Package,
};

struct Plan {
    InstallKind kind = InstallKind::Package;
    // Removed when present.
    QStringList files;
    // Removed only when empty after the files above are gone.
    QStringList emptyDirectories;
    // Removed recursively; only ever the install-user.sh prefix.
    QString ownedDirectory;
};

QString integratedAppImagePath(const QString &homePath);
InstallKind installKind(const QString &appImagePath,
                        const QString &applicationFilePath,
                        const QString &homePath);
Plan plan(const QString &appImagePath,
          const QString &applicationFilePath,
          const QString &homePath,
          const QString &dataHome,
          const QString &configHome);

// KDE: the setup wizard moves plain Print from Spectacle to EShot. Give it
// back only when EShot held it and Spectacle's default still includes it.
bool shouldRestoreSpectaclePrint(const QList<int> &eshotShortcuts,
                                 const QList<int> &spectacleShortcuts,
                                 const QList<int> &spectacleDefaultShortcuts);
QList<int> spectacleShortcutsWithPrint(const QList<int> &spectacleShortcuts);
}
