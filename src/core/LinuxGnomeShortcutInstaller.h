#pragma once

#include <QString>

namespace LinuxGnomeShortcutInstaller {

struct Result {
    bool success = false;
    QString error;
};

QString captureCommand(const QString &executablePath);
// One-line sh script that writes "capture" to the running EShot's
// single-instance socket (python3, else socat) and starts the executable
// given as $1 only when that fails. It spares an AppImage mount and a full
// Qt start-up on every key press of the GNOME fallback shortcut.
QString captureForwarderScript();
// "/bin/sh -c '<forwarder>' eshot-capture '<executable>'"; empty without a path.
QString forwardingCaptureCommand(const QString &executablePath);
QString gsettingsStringValue(const QString &value);
QString acceleratorFromPortableSequence(const QString &portableSequence);
QString acceleratorFromPortalTrigger(const QString &trigger);
QString preferredExecutable(const QString &appImagePath,
                            const QString &applicationFilePath,
                            const QString &integratedAppImagePath);
Result installCaptureShortcut(const QString &captureCommand, const QString &binding);
Result installPrintScreen(const QString &captureCommand);
Result uninstallCaptureShortcut(bool restoreBuiltInPrintScreen = true);
// True for a gsettings string array without entries ("[]" or "@as []").
bool isEmptyBindingList(const QString &value);
// Used when EShot is removed from the system: drops EShot's custom
// keybinding and gives Print back to GNOME's screenshot UI. Succeeds without
// changes when gsettings or the GNOME schemas are missing.
Result restoreDesktopScreenshotShortcut();

}
