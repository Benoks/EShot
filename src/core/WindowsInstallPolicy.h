#pragma once

#include <QString>

enum class WindowsInstallMode {
    Portable,
    CurrentUser,
    AllUsers
};

WindowsInstallMode windowsInstallMode(const QString &appDirectory,
                                      const QString &currentUserInstallDirectory,
                                      const QString &allUsersInstallDirectory);
QString windowsInstallerModeArgument(WindowsInstallMode mode);
