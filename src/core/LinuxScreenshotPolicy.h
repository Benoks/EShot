#ifndef LINUXSCREENSHOTPOLICY_H
#define LINUXSCREENSHOTPOLICY_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace LinuxScreenshotPolicy {

bool isKdeWaylandSession(const QString &currentDesktop,
                         const QString &sessionDesktop,
                         const QString &sessionType);
bool isGnomeWaylandSession(const QString &currentDesktop,
                           const QString &sessionDesktop,
                            const QString &sessionType);
bool useNativeScreenViews(const QString &currentDesktop,
                          const QString &sessionDesktop,
                          const QString &sessionType,
                          const QString &platformName);
bool shouldPrepareKWinPermission(const QString &currentDesktop,
                                 const QString &sessionDesktop,
                                 const QString &sessionType,
                                 const QString &appImagePath,
                                 const QString &executablePath);
QString kwinPermissionDesktopEntry(const QString &executablePath);
bool installKWinPermissionDesktopEntry(const QString &applicationsDirectory,
                                       const QString &executablePath,
                                       QString *desktopPath = nullptr,
                                       QString *error = nullptr);
QStringList spectacleWorkspaceArguments(const QString &outputPath);
QVariantMap portalScreenshotOptions(const QString &handleToken,
                                    uint portalVersion,
                                    uint availableTargets);
bool allowCursorBearingFallback(bool kdeWaylandSession);
bool canPresentCapture(bool captureSucceeded);

}

#endif
