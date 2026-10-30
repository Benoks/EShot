#include "BugReport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSysInfo>

BugReportInfo currentBugReportInfo()
{
    BugReportInfo info;
    info.eshotVersion = QCoreApplication::applicationVersion();
    info.operatingSystem = QSysInfo::prettyProductName();
    info.architecture = QSysInfo::currentCpuArchitecture();
    info.qtVersion = QString::fromLatin1(qVersion());
    info.platform = QGuiApplication::platformName();
#ifdef Q_OS_WIN
    const QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    info.installKind = QFileInfo::exists(QDir(appDir).filePath(QStringLiteral(".portable")))
        ? QStringLiteral("portable") : QStringLiteral("installer");
#else
    info.desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    info.sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
    const QString appPath = QCoreApplication::applicationFilePath();
    if (!qEnvironmentVariable("APPIMAGE").isEmpty())
        info.installKind = QStringLiteral("AppImage");
    else if (appPath.startsWith(QStringLiteral("/opt/eshot/")))
        info.installKind = QStringLiteral("AUR package");
    else if (appPath.startsWith(QStringLiteral("/usr/")))
        info.installKind = QStringLiteral("system package");
    else
        info.installKind = QStringLiteral("archive or source build");
#endif
    return info;
}

QUrl bugReportUrl(const BugReportInfo &info)
{
    QStringList environment;
    const auto add = [&environment](const QString &name, const QString &value) {
        if (!value.trimmed().isEmpty())
            environment << QStringLiteral("- %1: %2").arg(name, value.trimmed());
    };
    add(QStringLiteral("EShot"), info.eshotVersion);
    add(QStringLiteral("OS"), info.operatingSystem.isEmpty()
            ? info.architecture
            : info.operatingSystem + QStringLiteral(" (") + info.architecture + QLatin1Char(')'));
    add(QStringLiteral("Desktop"), info.sessionType.isEmpty()
            ? info.desktop
            : info.desktop + QStringLiteral(" (") + info.sessionType + QLatin1Char(')'));
    add(QStringLiteral("Install"), info.installKind);
    add(QStringLiteral("Qt"), info.platform.isEmpty()
            ? info.qtVersion
            : info.qtVersion + QStringLiteral(" (") + info.platform + QLatin1Char(')'));

    const QString body = QStringLiteral(
        "**What happened?**\n\n\n"
        "**Steps to reproduce**\n1. \n\n"
        "**What did you expect?**\n\n\n"
        "**Environment**\n%1\n").arg(environment.join(QLatin1Char('\n')));

    QUrl url(QStringLiteral("https://github.com/Benoks/EShot/issues/new"));
    url.setQuery(QStringLiteral("labels=bug&body=")
                     + QString::fromLatin1(QUrl::toPercentEncoding(body)),
                 QUrl::StrictMode);
    return url;
}
