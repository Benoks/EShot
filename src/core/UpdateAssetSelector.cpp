#include "UpdateAssetSelector.h"

#include <QJsonObject>
#include <QRegularExpression>

UpdateAsset selectUpdateAsset(const QJsonArray &assets,
                              UpdatePlatform platform,
                              const QString &architecture)
{
    const QString arch = architecture.trimmed().toLower();
    const bool arm64 = arch.contains(QStringLiteral("arm64"))
        || arch.contains(QStringLiteral("aarch64"));

    if (platform == UpdatePlatform::Linux && arm64)
        return {};

    for (const QJsonValue &value : assets) {
        const QJsonObject object = value.toObject();
        const QString name = object.value(QStringLiteral("name")).toString();
        const QString lower = name.toLower();

        bool matches = false;
        if (platform == UpdatePlatform::Windows) {
            matches = lower.endsWith(QStringLiteral(".exe"))
                && lower.contains(QStringLiteral("setup"))
                && (arm64
                    ? lower.contains(QStringLiteral("arm64"))
                    : lower.contains(QStringLiteral("x64")) && !lower.contains(QStringLiteral("arm64")));
        } else {
            const QRegularExpression appImageName(
                QStringLiteral("^EShot-v[0-9]+\\.[0-9]+\\.[0-9]+-x86_64\\.AppImage$"));
            matches = appImageName.match(name).hasMatch();
        }

        if (!matches)
            continue;

        const QString sha256 = normalizedSha256Digest(
            object.value(QStringLiteral("digest")).toString());
        if (platform == UpdatePlatform::Linux && sha256.isEmpty())
            continue;

        UpdateAsset selected;
        selected.name = name;
        selected.url = object.value(QStringLiteral("browser_download_url")).toString();
        selected.size = static_cast<qint64>(object.value(QStringLiteral("size")).toDouble());
        selected.sha256 = sha256;
        return selected;
    }

    return {};
}
