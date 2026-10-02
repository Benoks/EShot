#ifndef SHA256DIGEST_H
#define SHA256DIGEST_H

#include <QByteArray>
#include <QString>

// Shared SHA-256 helpers for everything EShot downloads and then installs or
// executes (app updates, Windows components, OCR language data).

// Accepts GitHub's "sha256:<hex>" asset digest form; returns lowercase hex or
// an empty string when the value is missing or malformed.
QString normalizedSha256Digest(const QString &digest);
bool fileMatchesSha256(const QString &path, const QString &expectedSha256);
bool dataMatchesSha256(const QByteArray &data, const QString &expectedSha256);
// Linux AppImages always require the release digest. GitHub's Windows release
// assets may not provide one, so a missing digest is accepted there.
bool downloadedAssetDigestIsValid(const QString &path, const QString &expectedSha256,
                                  bool digestRequired);

#endif
