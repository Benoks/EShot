#ifndef UPDATEASSETSELECTOR_H
#define UPDATEASSETSELECTOR_H

#include "Sha256Digest.h"

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

UpdateAsset selectUpdateAsset(const QJsonArray &assets,
                              UpdatePlatform platform,
                              const QString &architecture);

#endif
