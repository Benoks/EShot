#include "ProtectedFolderHint.h"

#include "ComponentPaths.h"
#include "TranslationManager.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPixmap>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ProtectedFolderHint {
namespace {
QString normalizedPath(QString path)
{
    path = QDir::cleanPath(path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/')));
    while (path.size() > 1 && path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path;
}
}

bool pathIsInside(const QString &path, const QStringList &roots)
{
    const QString target = normalizedPath(path);
    if (target.isEmpty())
        return false;
    for (const QString &root : roots) {
        const QString base = normalizedPath(root);
        if (base.isEmpty())
            continue;
        if (target.compare(base, Qt::CaseInsensitive) == 0
            || target.startsWith(base + QLatin1Char('/'), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool shouldShowHint(bool windows, bool insideProtectedRoot, bool cfaEnabled, bool writeDenied)
{
    return windows && insideProtectedRoot && (cfaEnabled || writeDenied);
}

QStringList protectedRoots()
{
#ifdef Q_OS_WIN
    return {
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QDir::home().filePath(QStringLiteral("Favorites")),
    };
#else
    return {};
#endif
}

bool controlledFolderAccessEnabled()
{
#ifdef Q_OS_WIN
    const QStringList keys = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\"
                       "Windows Defender Exploit Guard\\Controlled Folder Access"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Defender\\"
                       "Windows Defender Exploit Guard\\Controlled Folder Access"),
    };
    for (const QString &key : keys) {
        QSettings settings(key, QSettings::NativeFormat);
        const QVariant value = settings.value(QStringLiteral("EnableControlledFolderAccess"));
        if (value.isValid())
            return value.toInt() == 1;
    }
#endif
    return false;
}

bool writeAccessDenied(const QString &path)
{
#ifdef Q_OS_WIN
    QFileInfo info(path);
    QDir dir = info.isDir() ? QDir(info.absoluteFilePath()) : info.absoluteDir();
    while (!dir.exists() && dir.cdUp()) {
    }
    if (!dir.exists())
        return false;
    const QString probe = QDir::toNativeSeparators(dir.filePath(
        QStringLiteral(".eshot_access_probe_%1.tmp").arg(QCoreApplication::applicationPid())));
    const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(probe.utf16()), GENERIC_WRITE, 0,
                                      nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return false;
    }
    return GetLastError() == ERROR_ACCESS_DENIED;
#else
    Q_UNUSED(path);
    return false;
#endif
}

QString hintForPath(const QString &path)
{
#ifdef Q_OS_WIN
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif
    if (!windows || path.trimmed().isEmpty())
        return {};
    const bool inside = pathIsInside(path, protectedRoots());
    if (!inside)
        return {};
    if (!shouldShowHint(windows, inside, controlledFolderAccessEnabled(), writeAccessDenied(path)))
        return {};
    const QFileInfo info(path);
    const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    return TranslationManager::controlledFolderAccessHint(QDir::toNativeSeparators(folder));
}

QString recordingFailureHint(bool video)
{
    QSettings s(QStringLiteral("EShot"), QStringLiteral("EShot"));
    const char *key = video ? "videoSavePath" : "gifSavePath";
    QString dir = s.contains(QLatin1String(key))
        ? s.value(QLatin1String(key)).toString().trimmed()
        : (video ? ComponentPaths::defaultVideoDirectory()
                 : QDir(ComponentPaths::defaultSaveDirectory()).filePath(QStringLiteral("GIFs")));
    if (dir.isEmpty())
        dir = s.value(QStringLiteral("savePath")).toString().trimmed();
    if (dir.isEmpty())
        dir = ComponentPaths::defaultSaveDirectory();
    return hintForPath(dir);
}

void warnScreenshotSaveFailed(QWidget *parent, const QString &title, const QString &targetPath,
                              const QPixmap &image, const QString &fallbackText)
{
    const QString hint = hintForPath(targetPath);
    if (hint.isEmpty()) {
        QMessageBox::warning(parent, title, fallbackText);
        return;
    }
    // Nothing is lost: the capture stays available for pasting.
    if (!image.isNull())
        QGuiApplication::clipboard()->setPixmap(image);
    QMessageBox::warning(parent, title,
                         hint + QStringLiteral("\n\n") + TranslationManager::controlledFolderAccessClipboard());
}

}
