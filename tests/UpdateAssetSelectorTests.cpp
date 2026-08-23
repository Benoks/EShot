#include <QtTest>

#include "core/UpdateAssetSelector.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>

namespace {
QJsonObject asset(const QString &name, const QString &digest = QString())
{
    QJsonObject value{
        {QStringLiteral("name"), name},
        {QStringLiteral("browser_download_url"), QStringLiteral("https://example.test/") + name},
        {QStringLiteral("size"), 123456}
    };
    if (!digest.isEmpty())
        value.insert(QStringLiteral("digest"), digest);
    return value;
}

QJsonArray releaseAssets()
{
    return {
        asset(QStringLiteral("EShot_Setup_v3.2.0_x64.exe"),
              QStringLiteral("sha256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")),
        asset(QStringLiteral("EShot_Setup_v3.2.0_arm64.exe")),
        asset(QStringLiteral("EShot-v3.2.0-x86_64.AppImage"),
              QStringLiteral("sha256:BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB")),
        asset(QStringLiteral("EShot-v3.2.0-aarch64.AppImage"),
              QStringLiteral("sha256:DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD")),
        asset(QStringLiteral("EShot-v3.2.0-linux-x64.tar.gz"))
    };
}
}

class UpdateAssetSelectorTests : public QObject {
    Q_OBJECT

private slots:
    void selectsWindowsX64Installer()
    {
        const UpdateAsset selected = selectUpdateAsset(
            releaseAssets(), UpdatePlatform::Windows, QStringLiteral("x86_64"));
        QCOMPARE(selected.name, QStringLiteral("EShot_Setup_v3.2.0_x64.exe"));
        QCOMPARE(selected.size, 123456);
        QCOMPARE(selected.sha256,
                 QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    }

    void selectsWindowsArm64Installer()
    {
        const UpdateAsset selected = selectUpdateAsset(
            releaseAssets(), UpdatePlatform::Windows, QStringLiteral("arm64"));
        QCOMPARE(selected.name, QStringLiteral("EShot_Setup_v3.2.0_arm64.exe"));
    }

    void selectsLinuxX86_64AppImage()
    {
        const UpdateAsset selected = selectUpdateAsset(
            releaseAssets(), UpdatePlatform::Linux, QStringLiteral("amd64"));
        QCOMPARE(selected.name, QStringLiteral("EShot-v3.2.0-x86_64.AppImage"));
        QCOMPARE(selected.sha256,
                 QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
    }

    void requiresDigestForLinuxAppImage()
    {
        const QJsonArray assets{
            asset(QStringLiteral("EShot-v3.2.0-x86_64.AppImage")),
            asset(QStringLiteral("EShot-v3.2.1-x86_64.AppImage"),
                  QStringLiteral("sha256:CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"))
        };

        const UpdateAsset selected = selectUpdateAsset(
            assets, UpdatePlatform::Linux, QStringLiteral("x86_64"));
        QCOMPARE(selected.name, QStringLiteral("EShot-v3.2.1-x86_64.AppImage"));
        QCOMPARE(selected.sha256,
                 QStringLiteral("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"));
    }

    void rejectsLinuxAppImageWithoutDigest()
    {
        const QJsonArray assets{asset(QStringLiteral("EShot-v3.2.0-x86_64.AppImage"))};
        QVERIFY(!selectUpdateAsset(
                     assets, UpdatePlatform::Linux, QStringLiteral("x86_64")).isValid());
    }

    void rejectsNonContractLinuxAssetName()
    {
        const QJsonArray assets{
            asset(QStringLiteral("EShot-nightly-x86_64.AppImage"),
                  QStringLiteral("sha256:EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE"))
        };
        QVERIFY(!selectUpdateAsset(
                     assets, UpdatePlatform::Linux, QStringLiteral("x86_64")).isValid());
    }

    void rejectsUnsupportedLinuxArm64AppImage()
    {
        const UpdateAsset selected = selectUpdateAsset(
            releaseAssets(), UpdatePlatform::Linux, QStringLiteral("aarch64"));
        QVERIFY(!selected.isValid());
    }

    void rejectsAssetsForAnotherArchitecture()
    {
        const QJsonArray assets{asset(QStringLiteral("EShot-v3.2.0-aarch64.AppImage"))};
        QVERIFY(!selectUpdateAsset(assets, UpdatePlatform::Linux, QStringLiteral("x86_64")).isValid());
    }

    void normalizesOnlyValidSha256Digests()
    {
        QCOMPARE(normalizedSha256Digest(
                     QStringLiteral("sha256:ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789")),
                 QStringLiteral("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
        QVERIFY(normalizedSha256Digest(QStringLiteral("sha256:not-a-digest")).isEmpty());
        QVERIFY(normalizedSha256Digest(QStringLiteral("md5:abcdef")).isEmpty());
    }

    void verifiesDownloadedFileDigest()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        QCOMPARE(file.write("hello"), 5);
        file.flush();

        QVERIFY(fileMatchesSha256(
            file.fileName(),
            QStringLiteral("2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824")));
        QVERIFY(!fileMatchesSha256(
            file.fileName(),
            QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
        QVERIFY(!fileMatchesSha256(file.fileName(), QString()));
    }

    void acceptsMissingOptionalDigestButRejectsMissingRequiredDigest()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        QCOMPARE(file.write("hello"), 5);
        file.flush();

        QVERIFY(downloadedAssetDigestIsValid(file.fileName(), QString(), false));
        QVERIFY(!downloadedAssetDigestIsValid(file.fileName(), QString(), true));
        QVERIFY(downloadedAssetDigestIsValid(
            file.fileName(),
            QStringLiteral("2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"),
            true));
    }
};

QTEST_APPLESS_MAIN(UpdateAssetSelectorTests)
#include "UpdateAssetSelectorTests.moc"
