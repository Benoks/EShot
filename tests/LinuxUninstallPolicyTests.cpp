#include <QtTest>

#include "core/LinuxUninstallPolicy.h"

#include <QKeyCombination>

using LinuxUninstallPolicy::InstallKind;

class LinuxUninstallPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void classifiesInstallKinds();
    void packageInstallsOnlyRemoveUserIntegration();
    void appImageInstallsRemoveIntegratedCopyAndDesktopFiles();
    void userDirectoryInstallsRemoveTheirPrefix();
    void refusesRelativeOrRootHome();
    void restoresSpectaclePrintOnlyWhenEshotTookIt();
};

namespace {
const QString Home = QStringLiteral("/home/user");
const QString Data = QStringLiteral("/home/user/.local/share");
const QString Config = QStringLiteral("/home/user/.config");

int plainPrint()
{
    return QKeyCombination(Qt::NoModifier, Qt::Key_Print).toCombined();
}
}

void LinuxUninstallPolicyTests::classifiesInstallKinds()
{
    const QString mounted = QStringLiteral("/tmp/.mount_EShotX/usr/bin/EShot");
    QCOMPARE(LinuxUninstallPolicy::installKind(
                 QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage"), mounted, Home),
             InstallKind::AppImage);
    QCOMPARE(LinuxUninstallPolicy::installKind(
                 QStringLiteral("/home/user/Downloads/EShot-v4.5.1-x86_64.AppImage"), mounted, Home),
             InstallKind::AppImage);
    // The AUR package runs its AppImage from /opt.
    QCOMPARE(LinuxUninstallPolicy::installKind(
                 QStringLiteral("/opt/eshot/EShot.AppImage"), mounted, Home),
             InstallKind::Package);
    QCOMPARE(LinuxUninstallPolicy::installKind(
                 QString(), QStringLiteral("/usr/bin/EShot"), Home),
             InstallKind::Package);
    QCOMPARE(LinuxUninstallPolicy::installKind(
                 QString(), QStringLiteral("/home/user/.local/opt/EShot/bin/EShot"), Home),
             InstallKind::UserDirectory);
    // A sibling folder that merely shares the prefix is not EShot's.
    QCOMPARE(LinuxUninstallPolicy::installKind(
                 QString(), QStringLiteral("/home/user/.local/opt/EShotOld/bin/EShot"), Home),
             InstallKind::Package);
    // Portable archive extracted anywhere else.
    QCOMPARE(LinuxUninstallPolicy::installKind(
                 QString(), QStringLiteral("/home/user/EShot/bin/EShot"), Home),
             InstallKind::Package);
}

void LinuxUninstallPolicyTests::packageInstallsOnlyRemoveUserIntegration()
{
    const auto plan = LinuxUninstallPolicy::plan(
        QStringLiteral("/opt/eshot/EShot.AppImage"),
        QStringLiteral("/tmp/.mount_EShotX/usr/bin/EShot"), Home, Data, Config);
    QCOMPARE(plan.kind, InstallKind::Package);
    QCOMPARE(plan.files, QStringList({
        QStringLiteral("/home/user/.config/autostart/io.github.benoks.EShot.desktop"),
        QStringLiteral("/home/user/.local/share/applications/io.github.benoks.EShot.KWinScreenshot.desktop"),
    }));
    QVERIFY(plan.emptyDirectories.isEmpty());
    QVERIFY(plan.ownedDirectory.isEmpty());
}

void LinuxUninstallPolicyTests::appImageInstallsRemoveIntegratedCopyAndDesktopFiles()
{
    const auto plan = LinuxUninstallPolicy::plan(
        QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage"),
        QStringLiteral("/tmp/.mount_EShotX/usr/bin/EShot"),
        QStringLiteral("/home/user/"), QStringLiteral("/data"), QStringLiteral("/cfg"));
    QCOMPARE(plan.kind, InstallKind::AppImage);
    QCOMPARE(plan.files, QStringList({
        QStringLiteral("/cfg/autostart/io.github.benoks.EShot.desktop"),
        QStringLiteral("/data/applications/io.github.benoks.EShot.KWinScreenshot.desktop"),
        QStringLiteral("/data/applications/io.github.benoks.EShot.desktop"),
        QStringLiteral("/data/icons/hicolor/scalable/apps/io.github.benoks.EShot.svg"),
        QStringLiteral("/data/pixmaps/io.github.benoks.EShot.svg"),
        QStringLiteral("/data/icons/hicolor/scalable/apps/io.github.benoks.EShot-v4.svg"),
        QStringLiteral("/data/pixmaps/io.github.benoks.EShot-v4.svg"),
        QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage"),
        QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage.new"),
    }));
    QCOMPARE(plan.emptyDirectories,
             QStringList({QStringLiteral("/home/user/.local/opt/EShot")}));
    QVERIFY(plan.ownedDirectory.isEmpty());
}

void LinuxUninstallPolicyTests::userDirectoryInstallsRemoveTheirPrefix()
{
    const auto plan = LinuxUninstallPolicy::plan(
        QString(), QStringLiteral("/home/user/.local/opt/EShot/bin/EShot"), Home, Data, Config);
    QCOMPARE(plan.kind, InstallKind::UserDirectory);
    QVERIFY(plan.files.contains(
        QStringLiteral("/home/user/.local/share/applications/io.github.benoks.EShot.desktop")));
    QVERIFY(!plan.files.contains(
        QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage")));
    QCOMPARE(plan.ownedDirectory, QStringLiteral("/home/user/.local/opt/EShot"));
}

void LinuxUninstallPolicyTests::refusesRelativeOrRootHome()
{
    for (const QString &home : {QString(), QStringLiteral("relative"), QStringLiteral("/")}) {
        const auto plan = LinuxUninstallPolicy::plan(
            QString(), QStringLiteral("/.local/opt/EShot/bin/EShot"), home,
            QStringLiteral("relative/share"), QString());
        QCOMPARE(plan.kind, InstallKind::Package);
        QVERIFY(plan.files.isEmpty());
        QVERIFY(plan.ownedDirectory.isEmpty());
        QVERIFY(LinuxUninstallPolicy::integratedAppImagePath(home).isEmpty());
    }
}

void LinuxUninstallPolicyTests::restoresSpectaclePrintOnlyWhenEshotTookIt()
{
    const int print = plainPrint();
    const int metaPrint = QKeyCombination(Qt::MetaModifier, Qt::Key_Print).toCombined();
    QVERIFY(LinuxUninstallPolicy::shouldRestoreSpectaclePrint({print}, {metaPrint}, {print}));
    // EShot used another key: Spectacle's assignment is the user's choice.
    QVERIFY(!LinuxUninstallPolicy::shouldRestoreSpectaclePrint({metaPrint}, {}, {print}));
    // Spectacle already has it, or never had Print by default.
    QVERIFY(!LinuxUninstallPolicy::shouldRestoreSpectaclePrint({print}, {print}, {print}));
    QVERIFY(!LinuxUninstallPolicy::shouldRestoreSpectaclePrint({print}, {}, {}));

    QCOMPARE(LinuxUninstallPolicy::spectacleShortcutsWithPrint({metaPrint}),
             QList<int>({print, metaPrint}));
    QCOMPARE(LinuxUninstallPolicy::spectacleShortcutsWithPrint({print}), QList<int>({print}));
}

QTEST_APPLESS_MAIN(LinuxUninstallPolicyTests)
#include "LinuxUninstallPolicyTests.moc"
