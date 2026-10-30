#pragma once

#include <QImage>
#include <QRect>

// Windows desktop snapshots for the capture overlay and "Capture screen now".
// physicalRect is in virtual-desktop physical pixels. Every function returns
// a null image on failure (and on other platforms) instead of uninitialized
// pixels; the result is opaque RGB32.

// A Direct3D app runs in exclusive fullscreen (SHQueryUserNotificationState).
// Taking the foreground then makes many games minimise or change mode.
bool windowsExclusiveFullscreenRunning();

// GDI BitBlt of the screen. GDI often returns black for flip-model and
// exclusive fullscreen games.
QImage grabWindowsDesktopGdi(const QRect &physicalRect);

// DXGI Desktop Duplication of every output intersecting physicalRect. Sees
// what the compositor shows, including most fullscreen games.
QImage grabWindowsDesktopDuplication(const QRect &physicalRect);
