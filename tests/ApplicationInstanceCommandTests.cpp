#include <QtTest>

#include "core/ApplicationInstanceCommand.h"

class ApplicationInstanceCommandTests : public QObject
{
    Q_OBJECT

private slots:
    void choosesExplicitCommands();
    void opensSettingsForAPlainSecondaryLaunch();
    void roundTripsControlCommand();
    void rejectsUnknownWireCommands();
};

void ApplicationInstanceCommandTests::choosesExplicitCommands()
{
        QCOMPARE(ApplicationInstanceCommand::fromInvocation(true, false, false, false, false),
             ApplicationInstanceCommand::Capture);
        QCOMPARE(ApplicationInstanceCommand::fromInvocation(false, true, false, false, false),
             ApplicationInstanceCommand::Settings);
        QCOMPARE(ApplicationInstanceCommand::fromInvocation(false, false, true, false, false),
             ApplicationInstanceCommand::Capture);
        QCOMPARE(ApplicationInstanceCommand::fromInvocation(false, false, false, true, false),
                 ApplicationInstanceCommand::Quit);
}

void ApplicationInstanceCommandTests::opensSettingsForAPlainSecondaryLaunch()
{
        QCOMPARE(ApplicationInstanceCommand::fromInvocation(false, false, false, false, true),
             ApplicationInstanceCommand::Settings);
        QCOMPARE(ApplicationInstanceCommand::fromInvocation(false, false, false, false, false),
             ApplicationInstanceCommand::None);
}

void ApplicationInstanceCommandTests::roundTripsControlCommand()
{
    QCOMPARE(ApplicationInstanceCommand::fromWire(QByteArrayLiteral("control\n")),
             ApplicationInstanceCommand::Control);
    QCOMPARE(ApplicationInstanceCommand::toWire(ApplicationInstanceCommand::Control),
             QByteArrayLiteral("control\n"));
}

void ApplicationInstanceCommandTests::rejectsUnknownWireCommands()
{
    QCOMPARE(ApplicationInstanceCommand::fromWire(QByteArrayLiteral("capture\n")),
             ApplicationInstanceCommand::Capture);
        QCOMPARE(ApplicationInstanceCommand::fromWire(QByteArrayLiteral("settings\n")),
                 ApplicationInstanceCommand::Settings);
        QCOMPARE(ApplicationInstanceCommand::fromWire(QByteArrayLiteral("quit\n")),
                 ApplicationInstanceCommand::Quit);
    QCOMPARE(ApplicationInstanceCommand::fromWire(QByteArrayLiteral("unknown\n")),
             ApplicationInstanceCommand::None);
}

QTEST_APPLESS_MAIN(ApplicationInstanceCommandTests)
#include "ApplicationInstanceCommandTests.moc"
