#include "ComponentDownloadPolicy.h"

#include <QJsonObject>

namespace {
// tesseract-ocr/tessdata_fast "main" as of 2024-08-01. Pinning the commit
// keeps the URL immutable, so the hashes below cannot drift under us.
constexpr const char *kTessdataCommit = "87416418657359cb625c412a48b6e1d6d41c29bd";

struct TessdataPin {
    const char *code;
    const char *sha256;
};

// SHA-256 of <code>.traineddata at kTessdataCommit. Keep in sync with the
// language list in SettingsDialog (ocrPackageDefs).
constexpr TessdataPin kTessdataPins[] = {
    {"eng", "7d4322bd2a7749724879683fc3912cb542f19906c83bcc1a52132556427170b2"},
    {"tur", "7393381111e1152420fc4092cb44eef4237580d21b92bf30d7d221aad192c6b7"},
    {"rus", "e16e5e036cce1d9ec2b00063cf8b54472625b9e14d893a169e2b0dedeb4df225"},
    {"deu", "19d219bbb6672c869d20a9636c6816a81eb9a71796cb93ebe0cb1530e2cdb22d"},
    {"fra", "ced037562e8c80c13122dece28dd477d399af80911a28791a66a63ac1e3445ca"},
    {"spa", "6f2e04d02774a18f01bed44b1111f2cd7f3ba7ac9dc4373cd3f898a40ea6b464"},
    {"ita", "b8f89e1e785118dac4d51ae042c029a64edb5c3ee42ef73027a6d412748d8827"},
    {"por", "c4932b937207a9514b7514d518b931a99938c02a28a5a5a553f8599ed58b7deb"},
    {"pol", "c4476cdbc0e33d898d32345122b7be1cbf85ace15f920f06c7714756e1ef79b2"},
    {"nld", "ced0e5e046a84c908a6aa7accbef9a232c4a5d9a8276691b81c6ee64d02963f6"},
    {"jpn", "1f5de9236d2e85f5fdf4b3c500f2d4926f8d9449f28f5394472d9e8d83b91b4d"},
    {"kor", "6b85e11d9bbf07863b97b3523b1b112844c43e713df8b66418a081fd1060b3b2"},
    {"chi_sim", "a5fcb6f0db1e1d6d8522f39db4e848f05984669172e584e8d76b6b3141e1f730"},
};
}

UpdateAsset selectPortableComponentAsset(const QJsonArray &assets, const QString &architecture)
{
    const bool arm64 = architecture.trimmed().toLower().contains(QStringLiteral("arm"));
    for (const QJsonValue &value : assets) {
        const QJsonObject object = value.toObject();
        const QString name = object.value(QStringLiteral("name")).toString();
        const QString lower = name.toLower();
        if (!lower.endsWith(QStringLiteral(".zip")) || !lower.contains(QStringLiteral("portable")))
            continue;
        const bool archMatches = arm64
            ? lower.contains(QStringLiteral("arm64"))
            : lower.contains(QStringLiteral("x64")) && !lower.contains(QStringLiteral("arm64"));
        if (!archMatches)
            continue;

        UpdateAsset selected;
        selected.name = name;
        selected.url = object.value(QStringLiteral("browser_download_url")).toString();
        selected.size = static_cast<qint64>(object.value(QStringLiteral("size")).toDouble());
        selected.sha256 = normalizedSha256Digest(object.value(QStringLiteral("digest")).toString());
        return selected;
    }
    return {};
}

QString pinnedTessdataSha256(const QString &code)
{
    for (const TessdataPin &pin : kTessdataPins) {
        if (code == QLatin1String(pin.code))
            return QString::fromLatin1(pin.sha256);
    }
    return {};
}

QString tessdataDownloadUrl(const QString &code)
{
    if (pinnedTessdataSha256(code).isEmpty())
        return {};
    return QStringLiteral("https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/%1/%2.traineddata")
        .arg(QLatin1String(kTessdataCommit), code);
}
