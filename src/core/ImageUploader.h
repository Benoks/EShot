#ifndef IMAGEUPLOADER_H
#define IMAGEUPLOADER_H

#include <QObject>
#include <QString>
#include <QPixmap>

class QNetworkAccessManager;

class ImageUploader : public QObject {
    Q_OBJECT

public:
    enum class Provider {
        Catbox,
        Uguu,
        Litterbox,
        YandexDisk,
        GoogleDrive,
        TmpFiles,
        TempSh,
        Allwebs,
        RadikalCloud,
    };

    struct Result {
        bool ok = false;
        QString url;
        QString deleteUrl;
        QString error;
    };

    explicit ImageUploader(QObject *parent = nullptr);
    ~ImageUploader() override;

    virtual Provider provider() const = 0;
    virtual QString providerDisplayName() const = 0;
    virtual bool needsAuth() const { return false; }
    virtual QString authValue() const { return QString(); }
    virtual QString authPlaceholder() const { return QString(); }
    // Returns false when the credential could not be stored for later runs.
    virtual bool setAuthValue(const QString &) { return true; }

    void setImage(const QPixmap &pixmap);
    void setImagePath(const QString &path);
    bool hasImage() const;

    virtual void upload() = 0;
    virtual void cancel() = 0;

    static ImageUploader *create(Provider p, QObject *parent = nullptr);

signals:
    void uploading();
    void succeeded(const QString &url, const QString &deleteUrl);
    void failed(const QString &reason);

protected:
    QNetworkAccessManager *nam() const { return m_nam; }
    QString imagePath() const { return m_imagePath; }

    void finishWithError(const QString &reason);
    void finishWithSuccess(const QString &url, const QString &deleteUrl = QString());
    void cleanupTempFile();

private:
    QNetworkAccessManager *m_nam = nullptr;
    QString m_imagePath;
    bool m_ownsTempFile = false;
};

#endif
