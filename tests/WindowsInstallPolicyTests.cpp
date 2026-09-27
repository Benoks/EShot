#include <QtTest>

#include "core/WindowsInstallPolicy.h"

class WindowsInstallPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void matchesTheInstalledExecutableRatherThanAnotherEShotCopy()
    {
        QCOMPARE(windowsInstallMode(QStringLiteral("C:/Users/Jane/AppData/Local/Programs/EShot"),
                                    QStringLiteral("c:\\users\\jane\\appdata\\local\\programs\\eshot\\"),
                                    QStringLiteral("C:/Program Files/EShot")),
                 WindowsInstallMode::CurrentUser);
        QCOMPARE(windowsInstallMode(QStringLiteral("C:/Program Files/EShot"),
                                    QStringLiteral("C:/Users/Jane/AppData/Local/Programs/EShot"),
                                    QStringLiteral("c:\\program files\\eshot")),
                 WindowsInstallMode::AllUsers);
        QCOMPARE(windowsInstallMode(QStringLiteral("D:/Portable/EShot"),
                                    QStringLiteral("C:/Users/Jane/AppData/Local/Programs/EShot"),
                                    QStringLiteral("C:/Program Files/EShot")),
                 WindowsInstallMode::Portable);
    }

    void installerKeepsTheCorrectPrivilegeMode()
    {
        QCOMPARE(windowsInstallerModeArgument(WindowsInstallMode::CurrentUser),
                 QStringLiteral("/CURRENTUSER"));
        QCOMPARE(windowsInstallerModeArgument(WindowsInstallMode::AllUsers),
                 QStringLiteral("/ALLUSERS"));
        QCOMPARE(windowsInstallerModeArgument(WindowsInstallMode::Portable),
                 QStringLiteral("/CURRENTUSER"));
    }
};

QTEST_APPLESS_MAIN(WindowsInstallPolicyTests)
#include "WindowsInstallPolicyTests.moc"
