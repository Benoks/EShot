#include "LinuxScreenshotPolicy.h"
#include "LinuxDesktopIntegration.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace LinuxScreenshotPolicy {

bool isKdeWaylandSession(const QString &currentDesktop,
                         const QString &sessionDesktop,
                         const QString &sessionType)
{
    return LinuxDesktopIntegration::detect(currentDesktop, sessionDesktop)
            == LinuxDesktopEnvironment::Kde
        && LinuxDesktopIntegration::isWayland(sessionType);
}

bool isGnomeWaylandSession(const QString &currentDesktop,
                           const QString &sessionDesktop,
                           const QString &sessionType)
{
    return LinuxDesktopIntegration::detect(currentDesktop, sessionDesktop)
            == LinuxDesktopEnvironment::Gnome
        && LinuxDesktopIntegration::isWayland(sessionType);
}

bool useNativeScreenViews(const QString &currentDesktop,
                          const QString &sessionDesktop,
                          const QString &sessionType,
                          const QString &platformName)
{
    return isKdeWaylandSession(currentDesktop, sessionDesktop, sessionType)
        && platformName.contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
}

bool shouldPrepareKWinPermission(const QString &currentDesktop,
                                 const QString &sessionDesktop,
                                 const QString &sessionType,
                                 const QString &appImagePath,
                                 const QString &executablePath)
{
    Q_UNUSED(appImagePath);
    return !executablePath.trimmed().isEmpty()
        && isKdeWaylandSession(currentDesktop, sessionDesktop, sessionType);
}

QString kwinPermissionDesktopEntry(const QString &executablePath)
{
    QString escapedPath = executablePath;
    escapedPath.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escapedPath.replace(QLatin1Char('"'), QStringLiteral("\\\""));

    return QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=EShot KWin Screenshot Integration\n"
        "NoDisplay=true\n"
        "OnlyShowIn=KDE;\n"
        "Exec=\"%1\"\n"
        "X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2\n")
        .arg(escapedPath);
}

bool installKWinPermissionDesktopEntry(const QString &applicationsDirectory,
                                       const QString &executablePath,
                                       QString *desktopPath,
                                       QString *error)
{
    if (error)
        error->clear();

    QDir applications(applicationsDirectory);
    if (!applications.exists() && !applications.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(applicationsDirectory);
        return false;
    }

    const QString path = applications.filePath(
        QStringLiteral("io.github.benoks.EShot.KWinScreenshot.desktop"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    const QByteArray contents = kwinPermissionDesktopEntry(executablePath).toUtf8();
    if (file.write(contents) != contents.size()) {
        if (error)
            *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    if (desktopPath)
        *desktopPath = path;
    return true;
}

QStringList spectacleWorkspaceArguments(const QString &outputPath)
{
    return {QStringLiteral("--fullscreen"),
            QStringLiteral("--background"),
            QStringLiteral("--nonotify"),
            QStringLiteral("--output"),
            outputPath};
}

QVariantMap portalScreenshotOptions(const QString &handleToken,
                                    uint portalVersion,
                                    uint availableTargets)
{
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);
    options.insert(QStringLiteral("interactive"), false);
    options.insert(QStringLiteral("modal"), false);
    constexpr uint ScreenTarget = 1u;
    if (portalVersion >= 3u && (availableTargets & ScreenTarget) != 0u)
        options.insert(QStringLiteral("target"), ScreenTarget);
    return options;
}

bool allowCursorBearingFallback(bool kdeWaylandSession)
{
    return !kdeWaylandSession;
}

bool canPresentCapture(bool captureSucceeded)
{
    return captureSucceeded;
}

}
