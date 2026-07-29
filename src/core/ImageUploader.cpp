#include "ImageUploader.h"
#include "SecureCredentialStore.h"
#include "TranslationManager.h"
#include "UploadResponseParser.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QPointer>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {
QString tokenFromQueryLikeString(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1Char('?')) || text.startsWith(QLatin1Char('#')))
        text.remove(0, 1);

    const int tokenIndex = text.indexOf(QStringLiteral("access_token="));
    if (tokenIndex > 0)
        text = text.mid(tokenIndex);

    QUrlQuery query(text);
    return query.queryItemValue(QStringLiteral("access_token"), QUrl::FullyDecoded).trimmed();
}

QString tokenFromJsonLikeString(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || !trimmed.contains(QStringLiteral("access_token")))
        return QString();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        doc = QJsonDocument::fromJson((QStringLiteral("{") + trimmed + QStringLiteral("}")).toUtf8(), &parseError);
    }
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QString token = doc.object().value(QStringLiteral("access_token")).toString().trimmed();
        if (!token.isEmpty())
            return token;
    }

    static const QRegularExpression accessTokenPattern(
        QStringLiteral(R"(["']access_token["']\s*:\s*["']([^"']+)["'])"));
    const QRegularExpressionMatch match = accessTokenPattern.match(trimmed);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString normalizeOAuthTokenInput(const QString &value)
{
    QString text = value.trimmed();
    if (text.isEmpty())
        return text;

    const QString authorizationPrefix = QStringLiteral("Authorization:");
    if (text.startsWith(authorizationPrefix, Qt::CaseInsensitive))
        text = text.mid(authorizationPrefix.size()).trimmed();
    if (text.startsWith(QStringLiteral("OAuth "), Qt::CaseInsensitive))
        text = text.mid(6).trimmed();
    if (text.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive))
        text = text.mid(7).trimmed();

    const QString jsonToken = tokenFromJsonLikeString(text);
    if (!jsonToken.isEmpty())
        return jsonToken;

    QUrl url(text);
    if (url.isValid()) {
        const QString fragmentToken = tokenFromQueryLikeString(url.fragment());
        if (!fragmentToken.isEmpty())
            return fragmentToken;
        const QString queryToken = tokenFromQueryLikeString(url.query());
        if (!queryToken.isEmpty())
            return queryToken;
    }

    const QString inlineToken = tokenFromQueryLikeString(text);
    return inlineToken.isEmpty() ? text : inlineToken;
}

bool hasQueryLikeItem(const QString &text, const QString &name)
{
    QString queryText = text.trimmed();
    if (queryText.startsWith(QLatin1Char('?')) || queryText.startsWith(QLatin1Char('#')))
        queryText.remove(0, 1);

    QUrlQuery query(queryText);
    return query.hasQueryItem(name);
}

bool looksLikeUnsupportedOAuthInput(const QString &value)
{
    const QString text = value.trimmed();
    if (text.isEmpty())
        return false;

    if (hasQueryLikeItem(text, QStringLiteral("access_token")))
        return false;

    QUrl url(text);
    if (url.isValid()) {
        if (hasQueryLikeItem(url.fragment(), QStringLiteral("access_token")) ||
            hasQueryLikeItem(url.query(), QStringLiteral("access_token"))) {
            return false;
        }
        if (hasQueryLikeItem(url.fragment(), QStringLiteral("code")) ||
            hasQueryLikeItem(url.query(), QStringLiteral("code")) ||
            hasQueryLikeItem(url.fragment(), QStringLiteral("client_id")) ||
            hasQueryLikeItem(url.query(), QStringLiteral("client_id")) ||
            hasQueryLikeItem(url.fragment(), QStringLiteral("client_secret")) ||
            hasQueryLikeItem(url.query(), QStringLiteral("client_secret")) ||
            url.toString().contains(QStringLiteral("verification_code"), Qt::CaseInsensitive)) {
            return true;
        }
    }

    return hasQueryLikeItem(text, QStringLiteral("code")) ||
           hasQueryLikeItem(text, QStringLiteral("client_id")) ||
           hasQueryLikeItem(text, QStringLiteral("client_secret")) ||
           text.contains(QStringLiteral("verification_code"), Qt::CaseInsensitive);
}

class FormUploader : public ImageUploader {
public:
    struct Part {
        QString name;
        QByteArray value;
    };

    FormUploader(Provider provider, const QString &name, const QUrl &url,
                 const QString &fileField, const QList<Part> &parts,
                 QObject *parent = nullptr)
        : ImageUploader(parent)
        , m_provider(provider)
        , m_name(name)
        , m_url(url)
        , m_fileField(fileField)
        , m_parts(parts)
    {}

    ~FormUploader() override { cancel(); }

    Provider provider() const override { return m_provider; }
    QString providerDisplayName() const override { return m_name; }

    void upload() override
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
        for (const Part &part : m_parts) {
            QHttpPart p;
            p.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QStringLiteral("form-data; name=\"%1\"").arg(part.name)));
            p.setBody(part.value);
            m_multipart->append(p);
        }

        QHttpPart filePart;
        filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("image/png")));
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant(QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"")
                                        .arg(m_fileField, QFileInfo(imagePath()).fileName())));
        QFile *file = new QFile(imagePath(), m_multipart);
        if (!file->open(QIODevice::ReadOnly)) {
            delete m_multipart;
            m_multipart = nullptr;
            finishWithError(TranslationManager::uploadErrorCannotReadImage());
            return;
        }
        filePart.setBodyDevice(file);
        m_multipart->append(filePart);

        QNetworkRequest req(m_url);
        req.setRawHeader("User-Agent", "EShot/3.0");
        req.setTransferTimeout(60000);
        m_reply = nam()->post(req, m_multipart);
        m_multipart->setParent(m_reply);

        QPointer<FormUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
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
            if (mp && mp->parent() != r) mp->deleteLater();

            const QString url = QString::fromUtf8(data).trimmed();
            if (err != QNetworkReply::NoError) {
                finishWithError(TranslationManager::uploadErrorNetwork(errStr));
            } else if (code < 200 || code >= 300) {
                finishWithError(TranslationManager::uploadErrorHttp(code) + QStringLiteral(": ") + url.left(120));
            } else if (!url.startsWith(QStringLiteral("https://"))) {
                finishWithError(TranslationManager::uploadErrorUnexpectedResponse(url.left(120)));
            } else {
                finishWithSuccess(url);
            }
        });
    }

    void cancel() override
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
        } else if (m_multipart) {
            m_multipart->deleteLater();
            m_multipart = nullptr;
        }
    }

private:
    Provider m_provider;
    QString m_name;
    QUrl m_url;
    QString m_fileField;
    QList<Part> m_parts;
    QNetworkReply *m_reply = nullptr;
    QHttpMultiPart *m_multipart = nullptr;
};

class SimpleFileUploader : public ImageUploader {
public:
    enum class ResponseMode {
        PlainTextUrl,
        TmpFilesJson,
    };

    SimpleFileUploader(Provider provider, const QString &name, const QUrl &url,
                       const QString &fileField, ResponseMode responseMode,
                       QObject *parent = nullptr)
        : ImageUploader(parent)
        , m_provider(provider)
        , m_name(name)
        , m_url(url)
        , m_fileField(fileField)
        , m_responseMode(responseMode)
    {}

    ~SimpleFileUploader() override { cancel(); }

    Provider provider() const override { return m_provider; }
    QString providerDisplayName() const override { return m_name; }

    void upload() override
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
                           QVariant(QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"")
                                        .arg(m_fileField, QFileInfo(imagePath()).fileName())));
        QFile *file = new QFile(imagePath(), m_multipart);
        if (!file->open(QIODevice::ReadOnly)) {
            delete m_multipart;
            m_multipart = nullptr;
            finishWithError(TranslationManager::uploadErrorCannotReadImage());
            return;
        }
        filePart.setBodyDevice(file);
        m_multipart->append(filePart);

        QNetworkRequest req(m_url);
        req.setRawHeader("User-Agent", "EShot/3.0");
        req.setTransferTimeout(60000);
        m_reply = nam()->post(req, m_multipart);
        m_multipart->setParent(m_reply);

        QPointer<SimpleFileUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const QByteArray data = m_reply->readAll();
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            const QString errStr = m_reply->errorString();
            clearReply();

            if (err != QNetworkReply::NoError) {
                finishWithError(TranslationManager::uploadErrorNetwork(errStr));
                return;
            }
            if (code < 200 || code >= 300) {
                finishWithError(TranslationManager::uploadErrorHttp(code) + QStringLiteral(": ") +
                                QString::fromUtf8(data).simplified().left(120));
                return;
            }

            QString url;
            if (m_responseMode == ResponseMode::PlainTextUrl) {
                url = QString::fromUtf8(data).trimmed();
            } else {
                const QUrl landingUrl = tmpFilesLandingUrl(data);
                if (!landingUrl.isValid()) {
                    finishWithError(TranslationManager::uploadErrorUnexpectedResponse(QString::fromUtf8(data).left(120)));
                    return;
                }
                resolveTmpFilesDirectUrl(landingUrl);
                return;
            }

            if (url.isEmpty() || !url.startsWith(QStringLiteral("https://"))) {
                finishWithError(TranslationManager::uploadErrorUnexpectedResponse(QString::fromUtf8(data).left(120)));
                return;
            }
            finishWithSuccess(url);
        });
    }

    void cancel() override
    {
        clearReply(true);
    }

private:
    void resolveTmpFilesDirectUrl(const QUrl &landingUrl)
    {
        QNetworkRequest req(landingUrl);
        req.setRawHeader("User-Agent", "EShot/3.0");
        req.setTransferTimeout(60000);
        m_reply = nam()->get(req);

        QPointer<SimpleFileUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const QByteArray data = m_reply->readAll();
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            const QString errStr = m_reply->errorString();
            clearReply();

            if (err != QNetworkReply::NoError) {
                finishWithError(TranslationManager::uploadErrorNetwork(errStr));
                return;
            }
            if (code < 200 || code >= 300) {
                finishWithError(TranslationManager::uploadErrorHttp(code));
                return;
            }

            const QUrl directUrl = tmpFilesDirectUrl(data);
            if (!directUrl.isValid()) {
                finishWithError(TranslationManager::uploadErrorUnexpectedResponse(
                    QString::fromUtf8(data).simplified().left(120)));
                return;
            }
            finishWithSuccess(directUrl.toString());
        });
    }

    void clearReply(bool abort = false)
    {
        QNetworkReply *reply = m_reply;
        QHttpMultiPart *multipart = m_multipart;
        m_reply = nullptr;
        m_multipart = nullptr;
        if (reply) {
            disconnect(reply, nullptr, this, nullptr);
            if (abort)
                reply->abort();
            reply->deleteLater();
        }
        if (multipart && multipart->parent() != reply)
            multipart->deleteLater();
    }

    Provider m_provider;
    QString m_name;
    QUrl m_url;
    QString m_fileField;
    ResponseMode m_responseMode;
    QNetworkReply *m_reply = nullptr;
    QHttpMultiPart *m_multipart = nullptr;
};

class CheveretoUploader : public ImageUploader {
public:
    CheveretoUploader(Provider provider, const QString &name, const QUrl &url,
                      const QString &settingsKey, QObject *parent = nullptr)
        : ImageUploader(parent)
        , m_provider(provider)
        , m_name(name)
        , m_url(url)
        , m_settingsKey(settingsKey)
    {
        QSettings s("EShot", "EShot");
        m_apiKey = s.value(m_settingsKey).toString().trimmed();
    }

    ~CheveretoUploader() override { cancel(); }

    Provider provider() const override { return m_provider; }
    QString providerDisplayName() const override { return m_name; }
    bool needsAuth() const override { return true; }
    QString authValue() const override { return m_apiKey; }
    QString authPlaceholder() const override { return TranslationManager::uploadApiKeyPlaceholder(m_name); }
    void setAuthValue(const QString &value) override
    {
        m_apiKey = value.trimmed();
        QSettings s("EShot", "EShot");
        s.setValue(m_settingsKey, m_apiKey);
    }

    void upload() override
    {
        if (m_reply) {
            emit failed(TranslationManager::uploadErrorInProgress());
            return;
        }
        if (!hasImage()) {
            finishWithError(TranslationManager::uploadErrorImageMissing());
            return;
        }
        if (m_apiKey.isEmpty()) {
            finishWithError(TranslationManager::uploadErrorApiKeyMissing(m_name));
            return;
        }

        emit uploading();
        m_multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        QHttpPart keyPart;
        keyPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant(QStringLiteral("form-data; name=\"key\"")));
        keyPart.setBody(m_apiKey.toUtf8());
        m_multipart->append(keyPart);

        QHttpPart formatPart;
        formatPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                             QVariant(QStringLiteral("form-data; name=\"format\"")));
        formatPart.setBody("json");
        m_multipart->append(formatPart);

        QHttpPart filePart;
        filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("image/png")));
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant(QStringLiteral("form-data; name=\"source\"; filename=\"%1\"")
                                        .arg(QFileInfo(imagePath()).fileName())));
        QFile *file = new QFile(imagePath(), m_multipart);
        if (!file->open(QIODevice::ReadOnly)) {
            delete m_multipart;
            m_multipart = nullptr;
            finishWithError(TranslationManager::uploadErrorCannotReadImage());
            return;
        }
        filePart.setBodyDevice(file);
        m_multipart->append(filePart);

        QNetworkRequest req(m_url);
        req.setRawHeader("Accept", "application/json");
        req.setRawHeader("User-Agent", "EShot/3.0");
        req.setTransferTimeout(60000);
        m_reply = nam()->post(req, m_multipart);
        m_multipart->setParent(m_reply);

        QPointer<CheveretoUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const QByteArray data = m_reply->readAll();
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            const QString errStr = m_reply->errorString();
            clearReply();

            if (err != QNetworkReply::NoError) {
                finishWithError(TranslationManager::uploadErrorNetwork(errStr));
                return;
            }
            if (code < 200 || code >= 300) {
                finishWithError(TranslationManager::uploadErrorHttp(code) + QStringLiteral(": ") +
                                QString::fromUtf8(data).simplified().left(120));
                return;
            }

            const QJsonObject image = QJsonDocument::fromJson(data)
                .object()
                .value(QStringLiteral("image"))
                .toObject();
            QString url = image.value(QStringLiteral("url")).toString();
            if (url.isEmpty())
                url = image.value(QStringLiteral("display_url")).toString();
            if (url.isEmpty())
                url = image.value(QStringLiteral("url_viewer")).toString();

            if (url.isEmpty() || !url.startsWith(QStringLiteral("http"))) {
                finishWithError(TranslationManager::uploadErrorUnexpectedResponse(QString::fromUtf8(data).left(120)));
                return;
            }
            finishWithSuccess(url);
        });
    }

    void cancel() override
    {
        clearReply(true);
    }

private:
    void clearReply(bool abort = false)
    {
        QNetworkReply *reply = m_reply;
        QHttpMultiPart *multipart = m_multipart;
        m_reply = nullptr;
        m_multipart = nullptr;
        if (reply) {
            disconnect(reply, nullptr, this, nullptr);
            if (abort)
                reply->abort();
            reply->deleteLater();
        }
        if (multipart && multipart->parent() != reply)
            multipart->deleteLater();
    }

    Provider m_provider;
    QString m_name;
    QUrl m_url;
    QString m_settingsKey;
    QString m_apiKey;
    QNetworkReply *m_reply = nullptr;
    QHttpMultiPart *m_multipart = nullptr;
};

class YandexDiskUploader : public ImageUploader {
public:
    explicit YandexDiskUploader(QObject *parent = nullptr)
        : ImageUploader(parent)
    {
        m_token = normalizeOAuthTokenInput(
            SecureCredentialStore::read(QStringLiteral("yandexDiskToken")));
        if (m_token.isEmpty()) {
            QSettings settings("EShot", "EShot");
            m_token = normalizeOAuthTokenInput(SecureCredentialStore::migrateLegacyToken(
                settings, QStringLiteral("yandexDiskToken"), [](const QString &key, const QString &token) {
                    const QString normalized = normalizeOAuthTokenInput(token);
                    return normalized.isEmpty() || SecureCredentialStore::write(key, normalized);
                }));
        }
    }

    ~YandexDiskUploader() override { cancel(); }

    Provider provider() const override { return Provider::YandexDisk; }
    QString providerDisplayName() const override { return QStringLiteral("Yandex Disk"); }
    bool needsAuth() const override { return true; }
    QString authValue() const override { return m_token; }
    QString authPlaceholder() const override { return TranslationManager::yandexAuthPlaceholder(); }
    void setAuthValue(const QString &value) override
    {
        m_token = normalizeOAuthTokenInput(value);
        SecureCredentialStore::write(QStringLiteral("yandexDiskToken"), m_token);
    }

    void upload() override
    {
        if (m_reply) {
            emit failed(TranslationManager::uploadErrorInProgress());
            return;
        }
        if (!hasImage()) {
            finishWithError(TranslationManager::uploadErrorImageMissing());
            return;
        }
        if (m_token.trimmed().isEmpty()) {
            finishWithError(TranslationManager::uploadErrorYandexTokenMissing());
            return;
        }
        if (looksLikeUnsupportedOAuthInput(m_token)) {
            finishWithError(TranslationManager::uploadErrorYandexUnsupportedToken());
            return;
        }

        emit uploading();
        m_remotePath = QStringLiteral("disk:/EShot/%1").arg(QFileInfo(imagePath()).fileName());
        createFolder();
    }

    void cancel() override
    {
        if (!m_reply)
            return;
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

private:
    QString yandexError(const QString &step, int code, const QByteArray &data) const
    {
        if (code == 401) {
            return TranslationManager::uploadErrorYandexAuthFailed();
        }
        if (code == 403) {
            return TranslationManager::uploadErrorYandexScopeMissing();
        }

        QString message = TranslationManager::uploadErrorYandexStep(step, code);
        const QString body = QString::fromUtf8(data).simplified().left(160);
        if (!body.isEmpty())
            message += QStringLiteral(": ") + body;
        return message;
    }

    QNetworkRequest request(const QUrl &url) const
    {
        QNetworkRequest req(url);
        req.setRawHeader("Authorization", "OAuth " + m_token.toUtf8());
        req.setRawHeader("Accept", "application/json");
        req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json")));
        req.setRawHeader("User-Agent", "EShot/3.0");
        req.setTransferTimeout(60000);
        return req;
    }

    QUrl apiUrl(const QString &path, const QUrlQuery &query = QUrlQuery()) const
    {
        QUrl url(QStringLiteral("https://cloud-api.yandex.net/v1/disk/%1").arg(path));
        url.setQuery(query);
        return url;
    }

    void createFolder()
    {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("path"), QStringLiteral("disk:/EShot"));
        m_reply = nam()->put(request(apiUrl(QStringLiteral("resources"), query)), QByteArray());
        QPointer<YandexDiskUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const QByteArray data = m_reply->readAll();
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            clearReply();
            if (err != QNetworkReply::NoError && code != 409) {
                finishWithError(yandexError(QStringLiteral("folder"), code, data));
                return;
            }
            requestUploadUrl();
        });
    }

    void requestUploadUrl()
    {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("path"), m_remotePath);
        query.addQueryItem(QStringLiteral("overwrite"), QStringLiteral("true"));
        m_reply = nam()->get(request(apiUrl(QStringLiteral("resources/upload"), query)));
        QPointer<YandexDiskUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const QByteArray data = m_reply->readAll();
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            clearReply();
            if (err != QNetworkReply::NoError) {
                finishWithError(yandexError(QStringLiteral("upload URL"), code, data));
                return;
            }
            const QUrl href(QJsonDocument::fromJson(data).object().value(QStringLiteral("href")).toString());
            if (!href.isValid()) {
                finishWithError(TranslationManager::uploadErrorYandexUploadUrlMissing());
                return;
            }
            putFile(href);
        });
    }

    void putFile(const QUrl &href)
    {
        QFile *file = new QFile(imagePath(), this);
        if (!file->open(QIODevice::ReadOnly)) {
            file->deleteLater();
            finishWithError(TranslationManager::uploadErrorCannotReadImage());
            return;
        }
        QNetworkRequest req(href);
        req.setTransferTimeout(60000);
        m_reply = nam()->put(req, file);
        file->setParent(m_reply);
        QPointer<YandexDiskUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            clearReply();
            if (err != QNetworkReply::NoError || code < 200 || code >= 300) {
                finishWithError(TranslationManager::uploadErrorYandexStep(QStringLiteral("upload"), code));
                return;
            }
            publish();
        });
    }

    void publish()
    {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("path"), m_remotePath);
        m_reply = nam()->put(request(apiUrl(QStringLiteral("resources/publish"), query)), QByteArray());
        QPointer<YandexDiskUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            const QByteArray data = m_reply->readAll();
            clearReply();
            if (err != QNetworkReply::NoError && code != 409) {
                finishWithError(yandexError(QStringLiteral("publish"), code, data));
                return;
            }
            readPublicUrl();
        });
    }

    void readPublicUrl()
    {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("path"), m_remotePath);
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("public_url"));
        m_reply = nam()->get(request(apiUrl(QStringLiteral("resources"), query)));
        QPointer<YandexDiskUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const QByteArray data = m_reply->readAll();
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            clearReply();
            if (err != QNetworkReply::NoError) {
                finishWithError(yandexError(QStringLiteral("link"), code, data));
                return;
            }
            const QString url = QJsonDocument::fromJson(data).object().value(QStringLiteral("public_url")).toString();
            if (url.isEmpty()) {
                finishWithError(TranslationManager::uploadErrorYandexPublicLinkMissing());
                return;
            }
            finishWithSuccess(url);
        });
    }

    void clearReply()
    {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (reply)
            reply->deleteLater();
    }

    QString m_token;
    QString m_remotePath;
    QNetworkReply *m_reply = nullptr;
};

class GoogleDriveUploader : public ImageUploader {
public:
    explicit GoogleDriveUploader(QObject *parent = nullptr)
        : ImageUploader(parent)
    {
        m_token = normalizeOAuthTokenInput(
            SecureCredentialStore::read(QStringLiteral("googleDriveToken")));
        if (m_token.isEmpty()) {
            QSettings settings("EShot", "EShot");
            m_token = normalizeOAuthTokenInput(SecureCredentialStore::migrateLegacyToken(
                settings, QStringLiteral("googleDriveToken"), [](const QString &key, const QString &token) {
                    const QString normalized = normalizeOAuthTokenInput(token);
                    return normalized.isEmpty() || SecureCredentialStore::write(key, normalized);
                }));
        }
    }

    ~GoogleDriveUploader() override { cancel(); }

    Provider provider() const override { return Provider::GoogleDrive; }
    QString providerDisplayName() const override { return QStringLiteral("Google Drive"); }
    bool needsAuth() const override { return true; }
    QString authValue() const override { return m_token; }
    QString authPlaceholder() const override { return TranslationManager::googleDriveAuthPlaceholder(); }
    void setAuthValue(const QString &value) override
    {
        m_token = normalizeOAuthTokenInput(value);
        SecureCredentialStore::write(QStringLiteral("googleDriveToken"), m_token);
    }

    void upload() override
    {
        if (m_reply) {
            emit failed(TranslationManager::uploadErrorInProgress());
            return;
        }
        if (!hasImage()) {
            finishWithError(TranslationManager::uploadErrorImageMissing());
            return;
        }
        if (m_token.trimmed().isEmpty()) {
            finishWithError(TranslationManager::uploadErrorGoogleTokenMissing());
            return;
        }

        emit uploading();
        createFile();
    }

    void cancel() override
    {
        QNetworkReply *reply = m_reply;
        if (m_reply) {
            m_reply = nullptr;
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
        if (m_multipart) {
            if (!reply || m_multipart->parent() != reply)
                m_multipart->deleteLater();
            m_multipart = nullptr;
        }
    }

private:
    QNetworkRequest request(const QUrl &url, const QByteArray &contentType = QByteArray()) const
    {
        QNetworkRequest req(url);
        req.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
        req.setRawHeader("User-Agent", "EShot/3.0");
        if (!contentType.isEmpty())
            req.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(contentType));
        req.setTransferTimeout(60000);
        return req;
    }

    void createFile()
    {
        QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("multipart"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("id"));
        url.setQuery(query);

        m_multipart = new QHttpMultiPart(QHttpMultiPart::RelatedType);

        QJsonObject metadata;
        metadata.insert(QStringLiteral("name"), QFileInfo(imagePath()).fileName());
        metadata.insert(QStringLiteral("mimeType"), QStringLiteral("image/png"));

        QHttpPart metadataPart;
        metadataPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/json; charset=UTF-8")));
        metadataPart.setBody(QJsonDocument(metadata).toJson(QJsonDocument::Compact));
        m_multipart->append(metadataPart);

        QHttpPart filePart;
        filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("image/png")));
        QFile *file = new QFile(imagePath(), m_multipart);
        if (!file->open(QIODevice::ReadOnly)) {
            delete m_multipart;
            m_multipart = nullptr;
            finishWithError(TranslationManager::uploadErrorCannotReadImage());
            return;
        }
        filePart.setBodyDevice(file);
        m_multipart->append(filePart);

        m_reply = nam()->post(request(url), m_multipart);
        m_multipart->setParent(m_reply);

        QPointer<GoogleDriveUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const QByteArray data = m_reply->readAll();
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            QNetworkReply *reply = m_reply;
            QHttpMultiPart *mp = m_multipart;
            m_multipart = nullptr;
            clearReply();
            if (mp && mp->parent() != reply) mp->deleteLater();
            if (err != QNetworkReply::NoError || code < 200 || code >= 300) {
                if (code == 401) {
                    finishWithError(TranslationManager::uploadErrorGoogleAuthFailed());
                    return;
                }
                finishWithError(TranslationManager::uploadErrorHttp(code));
                return;
            }
            const QJsonObject obj = QJsonDocument::fromJson(data).object();
            m_fileId = obj.value(QStringLiteral("id")).toString();
            if (m_fileId.isEmpty()) {
                finishWithError(TranslationManager::uploadErrorGoogleFileIdMissing());
                return;
            }
            publish();
        });
    }

    void publish()
    {
        QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1/permissions")
            .arg(QString::fromLatin1(QUrl::toPercentEncoding(m_fileId))));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("id"));
        url.setQuery(query);

        QJsonObject permission;
        permission.insert(QStringLiteral("type"), QStringLiteral("anyone"));
        permission.insert(QStringLiteral("role"), QStringLiteral("reader"));

        m_reply = nam()->post(
            request(url, "application/json"),
            QJsonDocument(permission).toJson(QJsonDocument::Compact));

        QPointer<GoogleDriveUploader> self(this);
        QObject::connect(m_reply, &QNetworkReply::finished, this, [this, self]() {
            if (!self) return;
            const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QNetworkReply::NetworkError err = m_reply->error();
            clearReply();
            if (err != QNetworkReply::NoError || code < 200 || code >= 300) {
                if (code == 401) {
                    finishWithError(TranslationManager::uploadErrorGoogleAuthFailed());
                    return;
                }
                finishWithError(TranslationManager::uploadErrorHttp(code));
                return;
            }
            finishWithSuccess(QStringLiteral("https://drive.google.com/file/d/%1/view").arg(m_fileId));
        });
    }

    void clearReply()
    {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (reply)
            reply->deleteLater();
    }

    QString m_token;
    QString m_fileId;
    QNetworkReply *m_reply = nullptr;
    QHttpMultiPart *m_multipart = nullptr;
};
}

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
    case Provider::Litterbox:
        return new FormUploader(
            Provider::Litterbox,
            QStringLiteral("Litterbox (24 hours)"),
            QUrl(QStringLiteral("https://litterbox.catbox.moe/resources/internals/api.php")),
            QStringLiteral("fileToUpload"),
            {{QStringLiteral("reqtype"), "fileupload"}, {QStringLiteral("time"), "24h"}},
            parent);
    case Provider::YandexDisk:
        return new YandexDiskUploader(parent);
    case Provider::GoogleDrive:
        return new GoogleDriveUploader(parent);
    case Provider::TmpFiles:
        return new SimpleFileUploader(
            Provider::TmpFiles,
            QStringLiteral("TmpFiles.org"),
            QUrl(QStringLiteral("https://tmpfiles.org/api/v1/upload")),
            QStringLiteral("file"),
            SimpleFileUploader::ResponseMode::TmpFilesJson,
            parent);
    case Provider::TempSh:
        return new SimpleFileUploader(
            Provider::TempSh,
            QStringLiteral("temp.sh"),
            QUrl(QStringLiteral("https://temp.sh/upload")),
            QStringLiteral("file"),
            SimpleFileUploader::ResponseMode::PlainTextUrl,
            parent);
    case Provider::Allwebs:
        return new CheveretoUploader(
            Provider::Allwebs,
            QStringLiteral("Allwebs"),
            QUrl(QStringLiteral("https://allwebs.ru/api/1/upload")),
            QStringLiteral("allwebsApiKey"),
            parent);
    case Provider::RadikalCloud:
        return new CheveretoUploader(
            Provider::RadikalCloud,
            QStringLiteral("Radikal Cloud"),
            QUrl(QStringLiteral("https://radikal.cloud/api/1/upload")),
            QStringLiteral("radikalCloudApiKey"),
            parent);
    }
    return nullptr;
}
