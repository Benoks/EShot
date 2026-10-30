#pragma once

#include <QString>

namespace LinuxAutoStartPolicy
{
QString executablePath(const QString &appImagePath, const QString &applicationFilePath);
QString commandLine(const QString &executablePath,
                    const QString &currentDesktop,
                    const QString &sessionDesktop,
                    const QString &sessionType,
                    const QString &captureBackend = QString());

// Any EShot autostart entry counts as enabled unless the desktop marked it
// disabled (Hidden=true or X-GNOME-Autostart-enabled=false), even when it
// starts a different EShot copy: Settings must show it so it can be removed.
bool entryEnabled(const QString &desktopEntryText);
// The program the entry's Exec= line starts, without /usr/bin/env and
// VAR=value prefixes or quotes. Empty when there is no Exec= line.
QString entryExecutable(const QString &desktopEntryText);
// At startup an enabled entry is re-pointed at this copy when its target is
// gone, or when this copy is the integrated one. Starting some other copy
// (for example a download being tried out) leaves a working entry alone.
bool shouldRepointAtStartup(const QString &entryExecutable, bool entryExecutableExists,
                            const QString &currentExecutable,
                            const QString &integratedExecutable);
}
