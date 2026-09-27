#pragma once

#include <QString>
#include <QStringList>

// Locations shared by the capture overlay, recorders, OCR and Settings.
namespace ComponentPaths {

// Default root for screenshots, GIFs and videos (~/Pictures/EShot).
QString defaultSaveDirectory();

// Per-user folder for optional components (FFmpeg, Tesseract) downloaded when
// the install folder is not writable, e.g. an all-users install in Program Files.
QString userComponentsDirectory();

// Folders that may hold an optional component, in lookup order.
QStringList componentBaseDirectories();

// Where a new component download should be installed.
QString componentInstallDirectory();

// First existing <base>/<component> folder, or an empty string.
QString installedComponentDirectory(const QString &component);

// Bundled, downloaded or system FFmpeg executable.
QString ffmpegPath();

}
