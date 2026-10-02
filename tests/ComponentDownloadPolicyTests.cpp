#include <QtTest>

#include "core/ComponentDownloadPolicy.h"
#include "core/Sha256Digest.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>

namespace {
QJsonObject asset(const QString &name, const QString &digest = QString())
{
    QJsonObject value{
        {QStringLiteral("name"), name},
        {QStringLiteral("browser_download_url"), QStringLiteral("https://example.test/") + name},
        {QStringLiteral("size"), 4096}
    };
    if (!digest.isEmpty())
        value.insert(QStringLiteral("digest"), digest);
    return value;
}

QJsonArray releaseAssets()
{
    return {
        asset(QStringLiteral("EShot-v4.5.1-arm64-portable.zip"),
              QStringLiteral("sha256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")),
        asset(QStringLiteral("EShot-v4.5.1-linux-x64.tar.gz"),
              QStringLiteral("sha256:CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC")),
        asset(QStringLiteral("EShot-v4.5.1-x64-portable.zip"),
              QStringLiteral("sha256:BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB")),
        asset(QStringLiteral("EShot_Setup_v4.5.1_x64.exe"),
              QStringLiteral("sha256:DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD")),
    };
}

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
}

class ComponentDownloadPolicyTests : public QObject {
    Q_OBJECT

private slots:
    void selectsX64PortableZipWithDigest()
    {
        const UpdateAsset selected = selectPortableComponentAsset(releaseAssets(), QStringLiteral("x86_64"));
        QCOMPARE(selected.name, QStringLiteral("EShot-v4.5.1-x64-portable.zip"));
        QCOMPARE(selected.size, 4096);
        QCOMPARE(selected.sha256,
                 QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
    }

    void selectsArm64PortableZip()
    {
        const UpdateAsset selected = selectPortableComponentAsset(releaseAssets(), QStringLiteral("arm64"));
        QCOMPARE(selected.name, QStringLiteral("EShot-v4.5.1-arm64-portable.zip"));
        QCOMPARE(selected.sha256,
                 QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    }

    void reportsMissingOrMalformedDigestAsEmpty()
    {
        const QJsonArray assets{
            asset(QStringLiteral("EShot-v4.5.1-x64-portable.zip")),
            asset(QStringLiteral("EShot-v4.5.1-arm64-portable.zip"), QStringLiteral("sha256:not-hex")),
        };
        const UpdateAsset x64 = selectPortableComponentAsset(assets, QStringLiteral("x86_64"));
        QVERIFY(x64.isValid());
        QVERIFY(x64.sha256.isEmpty());
        const UpdateAsset arm = selectPortableComponentAsset(assets, QStringLiteral("arm64"));
        QVERIFY(arm.isValid());
        QVERIFY(arm.sha256.isEmpty());
    }

    void returnsInvalidAssetWhenNoPortableZipMatches()
    {
        const QJsonArray assets{asset(QStringLiteral("EShot-v4.5.1-arm64-portable.zip"))};
        QVERIFY(!selectPortableComponentAsset(assets, QStringLiteral("x86_64")).isValid());
        QVERIFY(!selectPortableComponentAsset(QJsonArray(), QStringLiteral("arm64")).isValid());
    }

    void pinsEveryOfferedOcrLanguageToAFixedCommit()
    {
        const QStringList codes{
            QStringLiteral("eng"), QStringLiteral("tur"), QStringLiteral("rus"), QStringLiteral("deu"),
            QStringLiteral("fra"), QStringLiteral("spa"), QStringLiteral("ita"), QStringLiteral("por"),
            QStringLiteral("pol"), QStringLiteral("nld"), QStringLiteral("jpn"), QStringLiteral("kor"),
            QStringLiteral("chi_sim"),
        };
        const QRegularExpression hexPattern(QStringLiteral("^[0-9a-f]{64}$"));
        const QRegularExpression urlPattern(QStringLiteral(
            "^https://raw\\.githubusercontent\\.com/tesseract-ocr/tessdata_fast/[0-9a-f]{40}/[a-z_]+\\.traineddata$"));
        for (const QString &code : codes) {
            QVERIFY2(hexPattern.match(pinnedTessdataSha256(code)).hasMatch(), qPrintable(code));
            const QString url = tessdataDownloadUrl(code);
            QVERIFY2(urlPattern.match(url).hasMatch(), qPrintable(url));
            QVERIFY(url.endsWith(QStringLiteral("/") + code + QStringLiteral(".traineddata")));
        }
    }

    void refusesUnpinnedOcrLanguages()
    {
        QVERIFY(pinnedTessdataSha256(QStringLiteral("xyz")).isEmpty());
        QVERIFY(tessdataDownloadUrl(QStringLiteral("xyz")).isEmpty());
        QVERIFY(tessdataDownloadUrl(QStringLiteral("../eng")).isEmpty());
        QVERIFY(tessdataDownloadUrl(QString()).isEmpty());
    }

    void verifiesInMemoryData()
    {
        const QByteArray data("traineddata payload");
        const QString digest = sha256Hex(data);
        QVERIFY(dataMatchesSha256(data, digest));
        QVERIFY(dataMatchesSha256(data, digest.toUpper()));
        QVERIFY(!dataMatchesSha256(data + "x", digest));
        QVERIFY(!dataMatchesSha256(data, QString()));
        QVERIFY(!dataMatchesSha256(data, QStringLiteral("abc")));
    }

    void rejectsMalformedExpectedDigestForFiles()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("component");
        file.flush();
        const QString digest = sha256Hex(QByteArray("component"));
        QVERIFY(fileMatchesSha256(file.fileName(), digest));
        // 64 characters but not hex must not be accepted.
        QVERIFY(!fileMatchesSha256(file.fileName(), QString(64, QLatin1Char('z'))));
        QVERIFY(!downloadedAssetDigestIsValid(file.fileName(), QString(), true));
    }
};

QTEST_APPLESS_MAIN(ComponentDownloadPolicyTests)

#include "ComponentDownloadPolicyTests.moc"
