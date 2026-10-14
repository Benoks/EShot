#pragma once

#include <QJsonArray>
#include <QString>

int countNewerStableReleases(const QJsonArray &releases, const QString &currentVersion);
bool shouldSilentlyInstallUpdate(int newerStableReleaseCount, bool selfManagedInstall);

// A .portable file opts out of automatic updates without disabling manual
// checks. For AppImages, also inspect the original image's directory.
bool shouldCheckForUpdates(bool manual, const QString &appDirectory,
                           const QString &appImagePath = {});

// How a Linux build receives updates. Only an AppImage the user can write
// can replace itself; the AUR package (eshot-bin) installs a root-owned
// AppImage in /opt/eshot, and .deb or archive builds have no AppImage.
enum class LinuxUpdateChannel {
    SelfUpdate,
    Aur,
    PackageManager
};

LinuxUpdateChannel linuxUpdateChannel(const QString &appImagePath,
                                      bool appImageIsFile, bool appImageIsWritable);
