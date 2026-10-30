#pragma once

#include <QString>
#include <QStringList>

class QPixmap;
class QWidget;

// Windows Defender's Controlled folder access (CFA) protects Pictures,
// Videos, Documents and similar folders: unknown apps (EShot.exe, and
// ffmpeg.exe separately) are refused write access. EShot's default save
// folders live there, so failures there get a hint on how to allow EShot.
namespace ProtectedFolderHint {

// Pure: whether path is (inside) one of roots. Case-insensitive, both
// separators accepted.
bool pathIsInside(const QString &path, const QStringList &roots);
// Pure: the hint applies on Windows to a protected folder when CFA is known
// to be on or a write there was refused.
bool shouldShowHint(bool windows, bool insideProtectedRoot, bool cfaEnabled, bool writeDenied);

// Folders CFA protects by default for this user (empty off Windows).
QStringList protectedRoots();
// EnableControlledFolderAccess is 1 (block); false when off or unreadable.
bool controlledFolderAccessEnabled();
// A test file in the nearest existing folder of path is refused with
// ERROR_ACCESS_DENIED (Windows only).
bool writeAccessDenied(const QString &path);

// Translated hint for a failed save to path, or empty when CFA is unlikely
// to be the cause.
QString hintForPath(const QString &path);
// Hint for a failed GIF or video recording, based on the configured folder.
QString recordingFailureHint(bool video);

// Shows the save-failure message for a screenshot. When CFA is the likely
// cause it explains how to allow EShot and keeps the image on the clipboard;
// otherwise it shows fallbackText.
void warnScreenshotSaveFailed(QWidget *parent, const QString &title, const QString &targetPath,
                              const QPixmap &image, const QString &fallbackText);

}
