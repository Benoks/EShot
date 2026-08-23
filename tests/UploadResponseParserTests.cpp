#include <QtTest/QTest>

#include "core/UploadResponseParser.h"

class UploadResponseParserTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesTmpFilesLandingUrl();
    void parsesCurrentTmpFilesDirectUrl();
    void trimsTrailingPunctuationFromDirectUrl();
};

void UploadResponseParserTests::parsesTmpFilesLandingUrl()
{
    const QByteArray response = R"({"status":"success","data":{"url":"https://tmpfiles.org/abc123/image.png"}})";
    QCOMPARE(tmpFilesLandingUrl(response), QUrl(QStringLiteral("https://tmpfiles.org/abc123/image.png")));
}

void UploadResponseParserTests::parsesCurrentTmpFilesDirectUrl()
{
    const QByteArray html = R"(
        <img src="https://tmpfiles.org/dl/1784410563.acf749b9bd45c45d/abc123/image.png">
        <a href="https://tmpfiles.org/dl/1784410563.acf749b9bd45c45d/abc123/image.png">Download</a>
    )";
    QCOMPARE(tmpFilesDirectUrl(html),
             QUrl(QStringLiteral("https://tmpfiles.org/dl/1784410563.acf749b9bd45c45d/abc123/image.png")));
}

void UploadResponseParserTests::trimsTrailingPunctuationFromDirectUrl()
{
    const QByteArray html = R"(
        <a href="https://tmpfiles.org/dl/1784410563.acf749b9bd45c45d/abc123/image.png.">Download</a>
    )";
    QCOMPARE(tmpFilesDirectUrl(html),
             QUrl(QStringLiteral("https://tmpfiles.org/dl/1784410563.acf749b9bd45c45d/abc123/image.png")));
}

QTEST_APPLESS_MAIN(UploadResponseParserTests)

#include "UploadResponseParserTests.moc"
