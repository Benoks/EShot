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
}
