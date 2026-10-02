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
                                          const QString &sessionType,
                                          const QString &captureBackend)
{
    QString escapedPath = executablePath;
    escapedPath.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escapedPath.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    QString command = QStringLiteral("\"") + escapedPath + QStringLiteral("\" --silent");
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        currentDesktop, sessionDesktop);
    if (LinuxDesktopIntegration::useNativeWaylandOverlay(desktop, sessionType, captureBackend)) {
        command.prepend(QStringLiteral(
            "/usr/bin/env QT_QPA_PLATFORM=wayland ESHOT_WAYLAND_XWAYLAND_OVERLAY=0 "
            "ESHOT_CAPTURE_BACKEND=wayland "));
    } else if (LinuxDesktopIntegration::useXWaylandOverlay(desktop, sessionType, captureBackend)) {
        command.prepend(QStringLiteral(
            "/usr/bin/env QT_QPA_PLATFORM=\"xcb;wayland\" ESHOT_WAYLAND_XWAYLAND_OVERLAY=1 "));
    }
    return command;
}
