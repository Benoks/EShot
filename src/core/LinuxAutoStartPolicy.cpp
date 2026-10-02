#include "LinuxAutoStartPolicy.h"
#include "LinuxDesktopIntegration.h"

#include <QFileInfo>

QString LinuxAutoStartPolicy::executablePath(const QString &appImagePath,
                                             const QString &applicationFilePath)
{
    const QFileInfo appImage(appImagePath);
    if (!appImagePath.isEmpty() && appImage.isFile())
        return appImage.absoluteFilePath();

    return applicationFilePath;
}

QString LinuxAutoStartPolicy::commandLine(const QString &executablePath,
                                          const QString &currentDesktop,
                                          const QString &sessionDesktop,
                                          const QString &sessionType)
{
    QString escapedPath = executablePath;
    escapedPath.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escapedPath.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    QString command = QStringLiteral("\"") + escapedPath + QStringLiteral("\" --silent");
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        currentDesktop, sessionDesktop);
    if (LinuxDesktopIntegration::useNativeWaylandOverlay(desktop, sessionType)) {
        command.prepend(QStringLiteral(
            "/usr/bin/env QT_QPA_PLATFORM=wayland ESHOT_WAYLAND_XWAYLAND_OVERLAY=0 "));
    } else if (LinuxDesktopIntegration::useXWaylandOverlay(desktop, sessionType)) {
        command.prepend(QStringLiteral(
            "/usr/bin/env QT_QPA_PLATFORM=\"xcb;wayland\" ESHOT_WAYLAND_XWAYLAND_OVERLAY=1 "));
    }
    return command;
}
