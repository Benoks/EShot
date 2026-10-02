#include "LinuxUninstallPolicy.h"

#include <QDir>
#include <QKeyCombination>

namespace {

constexpr auto AppId = "io.github.benoks.EShot";

QString cleanAbsolute(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || !QDir::isAbsolutePath(trimmed))
        return {};
    return QDir::cleanPath(trimmed);
}

QString integratedDirectory(const QString &homePath)
{
    const QString home = cleanAbsolute(homePath);
    return home.isEmpty() || home == QStringLiteral("/")
        ? QString() : QDir(home).filePath(QStringLiteral(".local/opt/EShot"));
}

bool isInside(const QString &path, const QString &directory)
{
    return !directory.isEmpty() && path.startsWith(directory + QLatin1Char('/'));
}

int plainPrint()
{
    return QKeyCombination(Qt::NoModifier, Qt::Key_Print).toCombined();
}

}

QString LinuxUninstallPolicy::integratedAppImagePath(const QString &homePath)
{
    const QString directory = integratedDirectory(homePath);
    return directory.isEmpty() ? QString()
                               : QDir(directory).filePath(QStringLiteral("EShot.AppImage"));
}

LinuxUninstallPolicy::InstallKind LinuxUninstallPolicy::installKind(
    const QString &appImagePath, const QString &applicationFilePath, const QString &homePath)
{
    // The AUR package runs /opt/eshot/EShot.AppImage, so APPIMAGE alone does
    // not mean the user owns the image.
    const QString appImage = cleanAbsolute(appImagePath);
    if (!appImage.isEmpty()) {
        const bool systemImage = appImage.startsWith(QStringLiteral("/opt/"))
            || appImage.startsWith(QStringLiteral("/usr/"));
        return systemImage ? InstallKind::Package : InstallKind::AppImage;
    }

    if (isInside(cleanAbsolute(applicationFilePath), integratedDirectory(homePath)))
        return InstallKind::UserDirectory;
    return InstallKind::Package;
}

LinuxUninstallPolicy::Plan LinuxUninstallPolicy::plan(const QString &appImagePath,
                                                      const QString &applicationFilePath,
                                                      const QString &homePath,
                                                      const QString &dataHome,
                                                      const QString &configHome)
{
    Plan result;
    result.kind = installKind(appImagePath, applicationFilePath, homePath);

    const QString config = cleanAbsolute(configHome);
    const QString data = cleanAbsolute(dataHome);
    const QString appId = QString::fromLatin1(AppId);
    if (!config.isEmpty())
        result.files.append(QDir(config).filePath(QStringLiteral("autostart/%1.desktop").arg(appId)));
    if (!data.isEmpty()) {
        result.files.append(QDir(data).filePath(
            QStringLiteral("applications/%1.KWinScreenshot.desktop").arg(appId)));
    }
    if (result.kind == InstallKind::Package)
        return result;

    // Matches install_desktop_entry in packaging/linux/AppRun and
    // scripts/linux/install-user.sh.
    if (!data.isEmpty()) {
        const QDir dataDir(data);
        result.files.append(dataDir.filePath(QStringLiteral("applications/%1.desktop").arg(appId)));
        for (const QString &name : {appId, appId + QStringLiteral("-v4")}) {
            result.files.append(dataDir.filePath(
                QStringLiteral("icons/hicolor/scalable/apps/%1.svg").arg(name)));
            result.files.append(dataDir.filePath(QStringLiteral("pixmaps/%1.svg").arg(name)));
        }
    }

    const QString directory = integratedDirectory(homePath);
    if (directory.isEmpty())
        return result;
    if (result.kind == InstallKind::UserDirectory) {
        result.ownedDirectory = directory;
    } else {
        const QString appImage = integratedAppImagePath(homePath);
        result.files.append(appImage);
        result.files.append(appImage + QStringLiteral(".new"));
        result.emptyDirectories.append(directory);
    }
    return result;
}

bool LinuxUninstallPolicy::shouldRestoreSpectaclePrint(const QList<int> &eshotShortcuts,
                                                       const QList<int> &spectacleShortcuts,
                                                       const QList<int> &spectacleDefaultShortcuts)
{
    const int print = plainPrint();
    return eshotShortcuts.contains(print)
        && spectacleDefaultShortcuts.contains(print)
        && !spectacleShortcuts.contains(print);
}

QList<int> LinuxUninstallPolicy::spectacleShortcutsWithPrint(const QList<int> &spectacleShortcuts)
{
    QList<int> shortcuts = spectacleShortcuts;
    if (!shortcuts.contains(plainPrint()))
        shortcuts.prepend(plainPrint());
    return shortcuts;
}
