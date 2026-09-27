#pragma once

#include <QString>

namespace LinuxGnomeShortcutInstaller {

struct Result {
    bool success = false;
    QString error;
};

QString captureCommand(const QString &executablePath);
QString gsettingsStringValue(const QString &value);
QString acceleratorFromPortableSequence(const QString &portableSequence);
QString acceleratorFromPortalTrigger(const QString &trigger);
QString preferredExecutable(const QString &appImagePath,
                            const QString &applicationFilePath,
                            const QString &integratedAppImagePath);
Result installCaptureShortcut(const QString &captureCommand, const QString &binding);
Result installPrintScreen(const QString &captureCommand);
Result uninstallCaptureShortcut(bool restoreBuiltInPrintScreen = true);

}
