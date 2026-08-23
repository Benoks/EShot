#include "UguuUploader.h"
#include "TranslationManager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {
QString sanitizedFileName(const QString &filePath)
{
    QString name = QFileInfo(filePath).fileName();
    QString sanitized;
    sanitized.reserve(name.size());
    for (const QChar &c : name) {
        const ushort u = c.unicode();
        if (u >= 0x20 && u != 0x7f && c != QLatin1Char('"'))
            sanitized.append(c);
    }
    return sanitized;
}

QString responsePreview(const QByteArray &data)
{
    QString text = QString::fromUtf8(data).trimmed();
    text.replace('\n', ' ');
    text.replace('\r', ' ');
    return text.left(180);
}

QString networkErrorMessage(const QString &errorString, int statusCode, const QByteArray &data)
{
    QString message = errorString.trimmed();
    const QString body = responsePreview(data);
    if (message.isEmpty())
        message = TranslationManager::uploadErrorServerRejected();
    if (statusCode > 0)
        message += QStringLiteral(" (HTTP %1)").arg(statusCode);
    if (!body.isEmpty())
        message += QStringLiteral(": ") + body;
    return message;
}
}

UguuUploader::UguuUploader(QObject *parent) : ImageUploader(parent) {}

UguuUploader::~UguuUploader()
{
    cancel();
}

QString UguuUploader::providerDisplayName() const
{
    return QStringLiteral("Uguu.se");
}

void UguuUploader::upload()
{
    if (m_reply) {
        emit failed(TranslationManager::uploadErrorInProgress());
        return;
    }
    if (!hasImage()) {
        finishWithError(TranslationManager::uploadErrorImageMissing());
        return;
    }

    emit uploading();

    m_multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("image/png")));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"files[]\"; filename=\"%1\"")
                                    .arg(sanitizedFileName(imagePath()))));
    QFile *file = new QFile(imagePath(), m_multipart);
    if (!file->open(QIODevice::ReadOnly)) {
        delete m_multipart;
        m_multipart = nullptr;
        finishWithError(TranslationManager::uploadErrorCannotReadImage());
        return;
    }
    filePart.setBodyDevice(file);
    m_multipart->append(filePart);

    QNetworkRequest req(QUrl(QStringLiteral("https://uguu.se/upload?output=json")));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "EShot/3.0");
    req.setTransferTimeout(60000);

    m_reply = nam()->post(req, m_multipart);
    m_multipart->setParent(m_reply);

    QPointer<UguuUploader> self(this);
    connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
        if (!self) return;
        QByteArray data = m_reply->readAll();
        int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QNetworkReply::NetworkError err = m_reply->error();
        QString errStr = m_reply->errorString();

        QNetworkReply *r = m_reply;
        QHttpMultiPart *mp = m_multipart;
        m_reply = nullptr;
        m_multipart = nullptr;
        r->deleteLater();
        if (mp) mp->deleteLater();

        if (err != QNetworkReply::NoError) {
            finishWithError(TranslationManager::uploadErrorNetwork(networkErrorMessage(errStr, code, data)));
            return;
        }
        if (code < 200 || code >= 300) {
            const QString body = responsePreview(data);
            finishWithError(body.isEmpty()
                ? TranslationManager::uploadErrorHttp(code)
                : TranslationManager::uploadErrorHttp(code) + QStringLiteral(": ") + body);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(data);
        QString url;
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QJsonArray files = obj.value(QStringLiteral("files")).toArray();
            if (!files.isEmpty())
                url = files.first().toObject().value(QStringLiteral("url")).toString();
        }

        if (url.isEmpty() || !url.startsWith(QStringLiteral("https://"))) {
            QString text = QString::fromUtf8(data).trimmed();
            finishWithError(TranslationManager::uploadErrorUnexpectedResponse(text.left(120)));
            return;
        }
        finishWithSuccess(url);
    });
}

void UguuUploader::cancel()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        QHttpMultiPart *multipart = m_multipart;
        m_reply = nullptr;
        m_multipart = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
        if (multipart && multipart->parent() != reply)
            multipart->deleteLater();
        return;
    }
    if (m_multipart) {
        m_multipart->deleteLater();
        m_multipart = nullptr;
    }
}

ImageUploader *createUguuUploader(QObject *parent)
{
    return new UguuUploader(parent);
}
