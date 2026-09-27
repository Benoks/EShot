#include "ComponentPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace ComponentPaths {

QString defaultSaveDirectory()
{
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.trimmed().isEmpty())
        picturesPath = QDir::homePath();
    return QDir(picturesPath).filePath(QStringLiteral("EShot"));
}

QString userComponentsDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("EShot/components"));
}

QStringList componentBaseDirectories()
{
    return {QCoreApplication::applicationDirPath(), userComponentsDirectory()};
}

QString componentInstallDirectory()
{
    // QFileInfo::isWritable() ignores Windows ACLs, so probe with a real file.
    const QString appDir = QCoreApplication::applicationDirPath();
    QTemporaryFile probe(QDir(appDir).filePath(QStringLiteral(".eshot_write_probe_XXXXXX")));
    if (probe.open())
        return appDir;
    return userComponentsDirectory();
}

QString installedComponentDirectory(const QString &component)
{
    for (const QString &base : componentBaseDirectories()) {
        const QString dir = QDir(base).filePath(component);
        if (QFileInfo(dir).isDir())
            return dir;
    }
    return {};
}

QString ffmpegPath()
{
    QStringList candidates;
    for (const QString &base : componentBaseDirectories()) {
        candidates << QDir(base).filePath(QStringLiteral("ffmpeg/ffmpeg.exe"))
                   << QDir(base).filePath(QStringLiteral("ffmpeg.exe"));
#ifndef Q_OS_WIN
        candidates << QDir(base).filePath(QStringLiteral("ffmpeg/ffmpeg"))
                   << QDir(base).filePath(QStringLiteral("ffmpeg"));
#endif
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(appDir).filePath(QStringLiteral("../third_party/ffmpeg/bin/ffmpeg.exe"));
#ifndef Q_OS_WIN
    candidates << QDir(appDir).filePath(QStringLiteral("../third_party/ffmpeg/bin/ffmpeg"));
#endif
    candidates << QDir::current().filePath(QStringLiteral("third_party/ffmpeg/bin/ffmpeg.exe"));
#ifndef Q_OS_WIN
    candidates << QDir::current().filePath(QStringLiteral("third_party/ffmpeg/bin/ffmpeg"));
#endif
    for (const QString &path : candidates) {
        const QFileInfo info(path);
        if (info.exists() && info.isFile())
            return info.absoluteFilePath();
    }
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

}
