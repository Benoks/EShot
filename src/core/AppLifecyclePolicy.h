#pragma once

// Pure decisions for app lifecycle edge cases (open dialogs during a capture,
// pending work at quit). Kept free of widgets so they can be unit tested.
#include <QByteArray>

namespace AppLifecyclePolicy {

enum class CaptureDialogKind {
    Settings,        // the Settings dialog itself
    SettingsChild,   // a dialog opened from Settings (file picker, installer...)
    CaptureSession,  // OCR or upload dialog of the capture that is still open
    Installer,       // the Linux dependency installer dialog
    Wizard,          // the first-run wizard
    Other            // About, Control Center, message boxes
};

enum class CaptureDialogAction {
    Ignore,            // leave it alone and capture anyway
    HideUntilCaptureEnds,
    Close,             // nothing to lose: reject it
    KeepAndSkipCapture // busy or holds user data: raise it, do not capture
};

// What the capture hotkey does with an open dialog. Nothing a user typed,
// recognised or is uploading/installing may be thrown away.
CaptureDialogAction captureDialogAction(CaptureDialogKind kind);

// A hidden Settings dialog comes back once the capture (and a recording
// started from it) is over and no OCR/upload dialog of that capture is open.
bool settingsMayReturnAfterCapture(bool overlayVisible, bool recordingBusy,
                                   bool captureSessionDialogVisible);

// Task Manager's Startup tab stores its switch for a Run entry as a binary
// value under Explorer\StartupApproved\Run: an odd first byte means the
// user disabled it there. A missing value means enabled.
bool windowsStartupApprovedDisabled(const QByteArray &value);

}
