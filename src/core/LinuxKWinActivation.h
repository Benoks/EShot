#pragma once

#include <QString>
#include <QtGlobal>

// Activates one of EShot's own windows through a one-shot KWin script. KWin's
// focus-stealing prevention may refuse a plain activation request while a
// fullscreen app is active; a script runs inside KWin and is not limited.
namespace LinuxKWinActivation {

// The script text: finds the window owned by pid whose caption starts with
// caption and makes it the active window (KWin 6 and KWin 5 API names).
QString activationScript(qint64 pid, const QString &caption);

// Unique KWin plugin name for this process, so repeated requests replace
// the previous script instead of piling up.
QString pluginName(qint64 pid);

// Loads and starts the script over org.kde.KWin /Scripting. Returns false
// when KWin is not reachable or refuses the script. KDE only.
bool requestActivation(qint64 pid, const QString &caption);

// Script that activates the window with this KWin internalId again, unless
// it was closed or minimized meanwhile.
QString restoreScript(const QString &internalId);

// After a capture: gives focus back to the window that was active before it,
// so Alt+Tab continues from there. Does not wait for KWin. KDE only.
bool requestRestore(qint64 pid, const QString &internalId);

}
