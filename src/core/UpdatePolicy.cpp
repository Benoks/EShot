#include "UpdatePolicy.h"

#include <QJsonObject>
#include <QSet>
#include <QVersionNumber>

namespace {
QVersionNumber versionFromTag(QString tag)
{
    if (tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        tag.remove(0, 1);
    return QVersionNumber::fromString(tag.trimmed());
}
}

int countNewerStableReleases(const QJsonArray &releases, const QString &currentVersion)
{
    const QVersionNumber current = versionFromTag(currentVersion);
    if (current.isNull())
        return 0;

    QSet<QString> versions;
    for (const QJsonValue &value : releases) {
        const QJsonObject release = value.toObject();
        if (release.value(QStringLiteral("draft")).toBool()
            || release.value(QStringLiteral("prerelease")).toBool()) {
            continue;
        }
        const QVersionNumber candidate = versionFromTag(
            release.value(QStringLiteral("tag_name")).toString());
        if (!candidate.isNull() && QVersionNumber::compare(candidate, current) > 0)
            versions.insert(candidate.toString());
    }
    return versions.size();
}

bool shouldSilentlyInstallUpdate(int newerStableReleaseCount, bool selfManagedInstall)
{
    return selfManagedInstall && newerStableReleaseCount >= 2;
}
