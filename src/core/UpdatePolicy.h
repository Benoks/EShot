#pragma once

#include <QJsonArray>
#include <QString>

int countNewerStableReleases(const QJsonArray &releases, const QString &currentVersion);
bool shouldSilentlyInstallUpdate(int newerStableReleaseCount, bool selfManagedInstall);
