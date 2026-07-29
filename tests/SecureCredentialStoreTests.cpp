#include "core/SecureCredentialStore.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class SecureCredentialStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void migratesLegacyTokenWithoutLeavingPlaintext();
};

void SecureCredentialStoreTests::migratesLegacyTokenWithoutLeavingPlaintext()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    settings.setValue(QStringLiteral("googleDriveToken"), QStringLiteral(" legacy-token "));
    settings.sync();

    const QString token = SecureCredentialStore::migrateLegacyToken(
        settings, QStringLiteral("googleDriveToken"),
        [](const QString &, const QString &) { return false; });

    QCOMPARE(token, QStringLiteral("legacy-token"));
    QVERIFY(!settings.contains(QStringLiteral("googleDriveToken")));
}

QTEST_APPLESS_MAIN(SecureCredentialStoreTests)

#include "SecureCredentialStoreTests.moc"
