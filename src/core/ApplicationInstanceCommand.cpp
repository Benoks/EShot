#include "ApplicationInstanceCommand.h"

namespace ApplicationInstanceCommand {

Command fromInvocation(bool captureRequested,
                       bool settingsRequested,
                       bool saveRequested,
                       bool quitRequested,
                       bool defaultToSettings)
{
    if (quitRequested)
        return Quit;
    if (captureRequested || saveRequested)
        return Capture;
    if (settingsRequested || defaultToSettings)
        return Settings;
    return None;
}

QByteArray toWire(Command command)
{
    switch (command) {
    case Capture: return QByteArrayLiteral("capture\n");
    case Settings: return QByteArrayLiteral("settings\n");
    case Control: return QByteArrayLiteral("control\n");
    case Quit: return QByteArrayLiteral("quit\n");
    case None: return {};
    }
    return {};
}

Command fromWire(const QByteArray &wireCommand)
{
    const QByteArray command = wireCommand.trimmed();
    if (command == QByteArrayLiteral("capture"))
        return Capture;
    if (command == QByteArrayLiteral("settings"))
        return Settings;
    if (command == QByteArrayLiteral("control"))
        return Control;
    if (command == QByteArrayLiteral("quit"))
        return Quit;
    return None;
}

}
