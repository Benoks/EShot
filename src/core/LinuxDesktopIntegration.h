#pragma once

#include <QString>

enum class LinuxDesktopEnvironment {
    Kde,
    Gnome,
    Other
};

enum class LinuxDesktopSupportLevel {
    Supported,
    Limited,
    Unsupported
};

namespace LinuxDesktopIntegration {

LinuxDesktopEnvironment detect(const QString &currentDesktop,
                               const QString &sessionDesktop);
bool isWayland(const QString &sessionType, const QString &platformName = QString());
// KDE and GNOME Wayland use the XWayland canvas. Native Wayland cannot place
// top-level windows (pinned images, recording frame), so KDE's per-output
// native surfaces are opt-in with ESHOT_CAPTURE_BACKEND=wayland.
bool useXWaylandOverlay(LinuxDesktopEnvironment desktop, const QString &sessionType,
                        const QString &captureBackend = QString());
bool useNativeWaylandOverlay(LinuxDesktopEnvironment desktop, const QString &sessionType,
                             const QString &captureBackend = QString());
LinuxDesktopSupportLevel startupSupportLevel(LinuxDesktopEnvironment desktop,
                                             const QString &sessionType);
bool deferFirstRunHotkeyRegistration(LinuxDesktopEnvironment desktop);
bool deferOverlayPrewarmUntilFirstRunCompletes(bool firstRunInProgress);
bool useGnomeShortcutFallback(LinuxDesktopEnvironment desktop, bool portalAvailable);
bool shouldShowControlCenter(LinuxDesktopEnvironment desktop, bool trayAvailable, bool silent);
QString displayName(LinuxDesktopEnvironment desktop);

}
