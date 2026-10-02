#pragma once

#include "LinuxUninstallPolicy.h"

#include <QStringList>

// Removes EShot's desktop integration for the current user: restores the
// Print Screen shortcut, then deletes the files from LinuxUninstallPolicy.
// Used by Settings and by `EShot --uninstall`.
namespace LinuxUninstaller
{
struct Report {
    LinuxUninstallPolicy::InstallKind kind = LinuxUninstallPolicy::InstallKind::Package;
    QStringList removed;
    QStringList errors;
};

LinuxUninstallPolicy::InstallKind currentInstallKind();
Report run();
}
