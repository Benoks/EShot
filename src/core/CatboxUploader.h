#ifndef CATBOXXUPLOADER_H
#define CATBOXXUPLOADER_H

#include "ImageUploader.h"

class QNetworkReply;
class QHttpMultiPart;

class CatboxUploader : public ImageUploader {
    Q_OBJECT

public:
    explicit CatboxUploader(QObject *parent = nullptr);
    ~CatboxUploader() override;

    Provider provider() const override { return Provider::Catbox; }
    QString providerDisplayName() const override;
    bool needsAuth() const override { return true; }
    QString authValue() const override { return m_userHash; }
    QString authPlaceholder() const override;
    bool setAuthValue(const QString &value) override { return setUserHash(value); }

    bool setUserHash(const QString &hash);
    QString userHash() const { return m_userHash; }

    void upload() override;
    void cancel() override;

private:
    QString m_userHash;
    QNetworkReply *m_reply = nullptr;
    QHttpMultiPart *m_multipart = nullptr;
};

ImageUploader *createCatboxUploader(QObject *parent = nullptr);

#endif
