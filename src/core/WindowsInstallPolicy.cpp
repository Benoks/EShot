#include "WindowsInstallPolicy.h"

#include <QDir>

namespace {
QString normalizedInstallDirectory(QString path)
{
    if (path.trimmed().isEmpty())
        return {};
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(path).toCaseFolded();
}
}

WindowsInstallMode windowsInstallMode(const QString &appDirectory,
                                      const QString &currentUserInstallDirectory,
                                      const QString &allUsersInstallDirectory)
{
    const QString app = normalizedInstallDirectory(appDirectory);
    if (app.isEmpty())
        return WindowsInstallMode::Portable;
    if (app == normalizedInstallDirectory(currentUserInstallDirectory))
        return WindowsInstallMode::CurrentUser;
    if (app == normalizedInstallDirectory(allUsersInstallDirectory))
        return WindowsInstallMode::AllUsers;
    return WindowsInstallMode::Portable;
}

QString windowsInstallerModeArgument(WindowsInstallMode mode)
{
    return mode == WindowsInstallMode::AllUsers
        ? QStringLiteral("/ALLUSERS")
        : QStringLiteral("/CURRENTUSER");
}
