#ifndef COMPONENTDOWNLOADPOLICY_H
#define COMPONENTDOWNLOADPOLICY_H

#include "UpdateAssetSelector.h"

#include <QJsonArray>
#include <QString>

// Download sources and expected SHA-256 values for the optional components
// the Settings "Packages" tab installs (FFmpeg / Tesseract from the EShot
// portable release ZIP, OCR traineddata from tesseract-ocr/tessdata_fast).

// Picks the "*-portable.zip" asset matching the CPU architecture. The
// returned sha256 comes from GitHub's asset "digest" field and is empty when
// the release does not publish one; callers must then refuse to install.
UpdateAsset selectPortableComponentAsset(const QJsonArray &assets, const QString &architecture);

// Traineddata is fetched from a fixed tessdata_fast commit so the pinned
// hashes below stay valid. Both return an empty string for unknown codes.
QString tessdataDownloadUrl(const QString &code);
QString pinnedTessdataSha256(const QString &code);

#endif
