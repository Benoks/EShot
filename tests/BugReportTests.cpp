#include <QtTest>

#include "core/BugReport.h"

class BugReportTests : public QObject
{
    Q_OBJECT

private slots:
    void fillsEnvironmentIntoANewIssue()
    {
        BugReportInfo info;
        info.eshotVersion = QStringLiteral("4.8.0");
        info.operatingSystem = QStringLiteral("CachyOS");
        info.architecture = QStringLiteral("x86_64");
        info.qtVersion = QStringLiteral("6.8.2");
        info.platform = QStringLiteral("xcb");
        info.desktop = QStringLiteral("KDE");
        info.sessionType = QStringLiteral("wayland");
        info.installKind = QStringLiteral("AppImage");

        const QUrl url = bugReportUrl(info);
        QCOMPARE(url.host(), QStringLiteral("github.com"));
        QCOMPARE(url.path(), QStringLiteral("/Benoks/EShot/issues/new"));
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(QStringLiteral("labels")), QStringLiteral("bug"));
        const QString body = query.queryItemValue(QStringLiteral("body"), QUrl::FullyDecoded);
        QVERIFY(body.contains(QStringLiteral("- EShot: 4.8.0")));
        QVERIFY(body.contains(QStringLiteral("- OS: CachyOS (x86_64)")));
        QVERIFY(body.contains(QStringLiteral("- Desktop: KDE (wayland)")));
        QVERIFY(body.contains(QStringLiteral("- Install: AppImage")));
        QVERIFY(body.contains(QStringLiteral("- Qt: 6.8.2 (xcb)")));
        QVERIFY(body.contains(QStringLiteral("**What happened?**")));
    }

    void leavesOutUnknownValues()
    {
        BugReportInfo info;
        info.eshotVersion = QStringLiteral("4.8.0");
        info.architecture = QStringLiteral("arm64");
        const QUrlQuery query(bugReportUrl(info));
        const QString body = query.queryItemValue(QStringLiteral("body"), QUrl::FullyDecoded);
        QVERIFY(body.contains(QStringLiteral("- OS: arm64")));
        QVERIFY(!body.contains(QStringLiteral("- Desktop")));
        QVERIFY(!body.contains(QStringLiteral("- Install")));
    }
};

QTEST_GUILESS_MAIN(BugReportTests)
#include "BugReportTests.moc"
