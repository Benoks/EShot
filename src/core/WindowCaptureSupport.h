#pragma once

#include <QtGlobal>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include "LinuxDesktopIntegration.h"
#endif

// Whether this session offers window capture (the window-capture hotkey,
// tray entry and window selection): always on Windows, KDE Plasma on Linux.
inline bool windowCaptureSupported()
{
#if defined(Q_OS_WIN)
    return true;
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return LinuxDesktopIntegration::supportsWindowCapture(LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"), qEnvironmentVariable("XDG_SESSION_DESKTOP")));
#else
    return false;
#endif
}
