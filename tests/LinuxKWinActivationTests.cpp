#include <QtTest>

#include "core/LinuxKWinActivation.h"

class LinuxKWinActivationTests : public QObject
{
    Q_OBJECT

private slots:
    void scriptMatchesOwnPidAndCaption()
    {
        const QString script = LinuxKWinActivation::activationScript(4242, QStringLiteral("EShot Text Input"));
        QVERIFY(script.contains(QStringLiteral("var pid = 4242;")));
        QVERIFY(script.contains(QStringLiteral("var caption = \"EShot Text Input\";")));
        // KWin 6 and KWin 5 scripting names.
        QVERIFY(script.contains(QStringLiteral("workspace.windowList()")));
        QVERIFY(script.contains(QStringLiteral("workspace.clientList()")));
        QVERIFY(script.contains(QStringLiteral("workspace.activeWindow = window")));
        QVERIFY(script.contains(QStringLiteral("workspace.activeClient = window")));
    }

    void scriptEscapesCaption()
    {
        const QString script = LinuxKWinActivation::activationScript(
            1, QStringLiteral("a\"b\\c\n</script>"));
        QVERIFY(script.contains(QStringLiteral("var caption = \"a\\\"b\\\\c\\n</script>\";")));
    }

    void pluginNameIsPerProcess()
    {
        QCOMPARE(LinuxKWinActivation::pluginName(77), QStringLiteral("eshot-activate-77"));
        QVERIFY(LinuxKWinActivation::pluginName(77) != LinuxKWinActivation::pluginName(78));
    }
};

QTEST_GUILESS_MAIN(LinuxKWinActivationTests)
#include "LinuxKWinActivationTests.moc"
