#include "AppLifecyclePolicy.h"

namespace AppLifecyclePolicy {

CaptureDialogAction captureDialogAction(CaptureDialogKind kind)
{
    switch (kind) {
    case CaptureDialogKind::Settings:
        return CaptureDialogAction::HideUntilCaptureEnds;
    case CaptureDialogKind::SettingsChild:
    case CaptureDialogKind::CaptureSession:
    case CaptureDialogKind::Installer:
        return CaptureDialogAction::KeepAndSkipCapture;
    case CaptureDialogKind::Wizard:
        return CaptureDialogAction::Ignore;
    case CaptureDialogKind::Other:
        return CaptureDialogAction::Close;
    }
    return CaptureDialogAction::Close;
}

bool settingsMayReturnAfterCapture(bool overlayVisible, bool recordingBusy,
                                   bool captureSessionDialogVisible)
{
    return !overlayVisible && !recordingBusy && !captureSessionDialogVisible;
}

}
