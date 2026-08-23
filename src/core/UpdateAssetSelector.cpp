#include "UpdateAssetSelector.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonObject>
#include <QRegularExpression>

QString normalizedSha256Digest(const QString &digest)
{
    QString value = digest.trimmed().toLower();
    if (!value.startsWith(QStringLiteral("sha256:")))
        return QString();
    value = value.mid(7);
    static const QRegularExpression sha256Pattern(QStringLiteral("^[0-9a-f]{64}$"));
    return sha256Pattern.match(value).hasMatch() ? value : QString();
}

bool fileMatchesSha256(const QString &path, const QString &expectedSha256)
{
    if (expectedSha256.size() != 64)
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return false;
    return QString::fromLatin1(hash.result().toHex()) == expectedSha256.toLower();
}

bool downloadedAssetDigestIsValid(const QString &path, const QString &expectedSha256,
                                  bool digestRequired)
{
    if (expectedSha256.isEmpty())
        return !digestRequired;
    return fileMatchesSha256(path, expectedSha256);
}

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
