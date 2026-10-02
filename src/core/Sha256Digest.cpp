#include "Sha256Digest.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>

namespace {
bool isSha256Hex(const QString &value)
{
    static const QRegularExpression sha256Pattern(QStringLiteral("^[0-9a-f]{64}$"));
    return sha256Pattern.match(value).hasMatch();
}
}

QString normalizedSha256Digest(const QString &digest)
{
    QString value = digest.trimmed().toLower();
    if (!value.startsWith(QStringLiteral("sha256:")))
        return QString();
    value = value.mid(7);
    return isSha256Hex(value) ? value : QString();
}

bool fileMatchesSha256(const QString &path, const QString &expectedSha256)
{
    const QString expected = expectedSha256.toLower();
    if (!isSha256Hex(expected))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return false;
    return QString::fromLatin1(hash.result().toHex()) == expected;
}

bool dataMatchesSha256(const QByteArray &data, const QString &expectedSha256)
{
    const QString expected = expectedSha256.toLower();
    if (!isSha256Hex(expected))
        return false;
    const QByteArray actual = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(actual) == expected;
}

bool downloadedAssetDigestIsValid(const QString &path, const QString &expectedSha256,
                                  bool digestRequired)
{
    if (expectedSha256.isEmpty())
        return !digestRequired;
    return fileMatchesSha256(path, expectedSha256);
}
