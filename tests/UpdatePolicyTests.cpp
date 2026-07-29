#include <QtTest>

#include "core/UpdatePolicy.h"

namespace {
QJsonObject release(const QString &tag, bool prerelease = false, bool draft = false)
{
    return {{QStringLiteral("tag_name"), tag},
            {QStringLiteral("prerelease"), prerelease},
            {QStringLiteral("draft"), draft}};
}
}

class UpdatePolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void countsOnlyNewerStableReleases()
    {
        const QJsonArray releases{
            release(QStringLiteral("v4.2.1")),
            release(QStringLiteral("v4.2.0")),
            release(QStringLiteral("v4.2.0-rc1"), true),
            release(QStringLiteral("v4.1.7"), false, true),
            release(QStringLiteral("v4.1.6"))};

        QCOMPARE(countNewerStableReleases(releases, QStringLiteral("4.1.6")), 2);
        QCOMPARE(countNewerStableReleases(releases, QStringLiteral("4.2.0")), 1);
    }

    void requiresTwoMissedReleasesAndAManagedInstall()
    {
        QVERIFY(!shouldSilentlyInstallUpdate(1, true));
        QVERIFY(!shouldSilentlyInstallUpdate(2, false));
        QVERIFY(shouldSilentlyInstallUpdate(2, true));
    }
};

QTEST_APPLESS_MAIN(UpdatePolicyTests)
#include "UpdatePolicyTests.moc"
