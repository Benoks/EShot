#ifndef UPDATEASSETSELECTOR_H
#define UPDATEASSETSELECTOR_H

#include <QJsonArray>
#include <QString>

enum class UpdatePlatform {
    Windows,
    Linux
};

struct UpdateAsset {
    QString name;
    QString url;
    QString sha256;
    qint64 size = 0;

    bool isValid() const { return !name.isEmpty() && !url.isEmpty(); }
};

QString normalizedSha256Digest(const QString &digest);
bool fileMatchesSha256(const QString &path, const QString &expectedSha256);
// Linux AppImages always require the release digest. GitHub's Windows release
// assets may not provide one, so a missing digest is accepted there.
bool downloadedAssetDigestIsValid(const QString &path, const QString &expectedSha256,
                                  bool digestRequired);
UpdateAsset selectUpdateAsset(const QJsonArray &assets,
                              UpdatePlatform platform,
                              const QString &architecture);

#endif
