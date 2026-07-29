#pragma once

#include <QByteArray>

namespace ApplicationInstanceCommand {

enum Command {
    None,
    Capture,
    Settings,
    Control,
    Quit
};

Command fromInvocation(bool captureRequested,
                       bool settingsRequested,
                       bool saveRequested,
                       bool quitRequested,
                       bool defaultToControl);
QByteArray toWire(Command command);
Command fromWire(const QByteArray &wireCommand);

}
