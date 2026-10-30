#pragma once

#include <QString>

// Helpers for one-shot KWin scripts loaded over org.kde.KWin /Scripting.
// Used by LinuxKWinActivation (blocking, rare) and the KDE window list
// (asynchronous, once per window capture).
namespace LinuxKWinScript {

// A JavaScript string literal for value, correctly escaped.
QString jsString(const QString &value);

// Writes text to a new file in the runtime directory (falls back to the
// temporary directory) and returns its path; empty on failure. The caller
// removes the file once KWin has read it.
QString writeScriptFile(const QString &text);

// Loads the script under plugin (replacing an earlier one with that name)
// and starts it without waiting for KWin. Message order on the session bus
// keeps unload, load and start in sequence. Returns false when the session
// bus is not connected. KDE only.
bool loadAndStartAsync(const QString &path, const QString &plugin);

// Asks KWin to unload plugin without waiting for the reply.
void unloadAsync(const QString &plugin);

}
