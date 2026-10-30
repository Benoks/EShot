#pragma once

#include <QString>
#include <QStringList>

// Optional "Start as administrator" mode on Windows. EShot normally runs as
// the signed-in user; Windows then keeps it from capturing or taking input
// over apps that run as administrator. In this mode a scheduled task with the
// highest run level starts EShot elevated without a UAC prompt each time.
namespace WindowsElevatedStartup {

// Passed by the task, so an elevated start is not handed back to the user
// and a still-running normal instance is replaced.
inline constexpr const char *TaskArgument = "--from-elevated-task";

bool isSupported();
bool isProcessElevated();

// Creates (enable) or removes the task. Shows one UAC prompt; returns false
// when it was declined or the task could not be changed.
bool configure(bool enable, bool startAtLogon);

// Starts EShot through the task: elevated, without a UAC prompt.
bool launchElevated();

// Base64 of the UTF-16LE script, as powershell.exe -EncodedCommand expects.
// The elevated script travels on the command line instead of through a
// user-writable file that another process could swap before UAC approval.
QString encodedPowerShellCommand(const QString &script);

// True when appDirectory lies inside one of protectedRoots (Program Files),
// where only administrators can replace EShot.exe or plant DLLs next to it.
// A per-user or portable copy is writable by any program the user runs, so
// starting it elevated without a prompt hands those programs admin rights.
bool installLocationIsAdminProtected(const QString &appDirectory,
                                     const QStringList &protectedRoots);
bool currentInstallIsAdminProtected();

}
