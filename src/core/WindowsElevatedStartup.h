#pragma once

#include <QString>

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

}
