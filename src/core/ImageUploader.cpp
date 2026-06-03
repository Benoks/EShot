#include "ImageUploader.h"
#include <QNetworkAccessManager>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>

ImageUploader::ImageUploader(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
}

ImageUploader::~ImageUploader()
{
    cleanupTempFile();
}

void ImageUploader::setImage(const QPixmap &pixmap)
{
    cleanupTempFile();
    if (pixmap.isNull()) return;

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    pixmap.save(&buf, "PNG");

    QString path = QDir::temp().filePath(
        QStringLiteral("eshot_upload_%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(bytes);
        f.close();
        m_imagePath = path;
        m_ownsTempFile = true;
    } else {
        qWarning() << "ImageUploader: cannot write temp file:" << path;
        m_imagePath.clear();
        m_ownsTempFile = false;
    }
}

void ImageUploader::setImagePath(const QString &path)
{
    cleanupTempFile();
    m_imagePath = path;
    m_ownsTempFile = false;
}

bool ImageUploader::hasImage() const
{
    return !m_imagePath.isEmpty() && QFile::exists(m_imagePath);
}

void ImageUploader::finishWithError(const QString &reason)
{
    cleanupTempFile();
    emit failed(reason);
}

void ImageUploader::finishWithSuccess(const QString &url, const QString &deleteUrl)
{
    cleanupTempFile();
    emit succeeded(url, deleteUrl);
}

void ImageUploader::cleanupTempFile()
{
    if (m_ownsTempFile && !m_imagePath.isEmpty() && QFile::exists(m_imagePath)) {
        QFile::remove(m_imagePath);
    }
    m_imagePath.clear();
    m_ownsTempFile = false;
}

ImageUploader *ImageUploader::create(Provider p, QObject *parent)
{
    switch (p) {
    case Provider::Catbox: {
        extern ImageUploader *createCatboxUploader(QObject *parent);
        return createCatboxUploader(parent);
    }
    case Provider::Uguu: {
        extern ImageUploader *createUguuUploader(QObject *parent);
        return createUguuUploader(parent);
    }
    }
    return nullptr;
}
