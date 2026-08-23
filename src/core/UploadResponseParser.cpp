#include "UploadResponseParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
bool isTmpFilesHttpsUrl(const QUrl &url)
{
    return url.scheme() == QStringLiteral("https") &&
           url.host().compare(QStringLiteral("tmpfiles.org"), Qt::CaseInsensitive) == 0;
}
}

QUrl tmpFilesLandingUrl(const QByteArray &response)
{
    const QUrl url(QJsonDocument::fromJson(response)
                       .object()
                       .value(QStringLiteral("data"))
                       .toObject()
                       .value(QStringLiteral("url"))
                       .toString());
    return isTmpFilesHttpsUrl(url) ? url : QUrl();
}

QUrl tmpFilesDirectUrl(const QByteArray &html)
{
    static const QRegularExpression directUrlPattern(
        QStringLiteral(R"(https://tmpfiles\.org/dl/[^\s\"'<>]+)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression trailingPunctuationPattern(
        QStringLiteral(R"([.,;:)]+$)"));
    QString page = QString::fromUtf8(html);
    page.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    const QRegularExpressionMatch match = directUrlPattern.match(page);
    if (!match.hasMatch())
        return {};
    QString captured = match.captured();
    captured.remove(trailingPunctuationPattern);
    const QUrl url(captured);
    return isTmpFilesHttpsUrl(url) ? url : QUrl();
}
