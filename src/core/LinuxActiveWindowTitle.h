#pragma once

#include <QString>

namespace LinuxActiveWindowTitle {

// Caption of the active window, for the %T file name token. Read from the X
// server's _NET_ACTIVE_WINDOW, which also covers XWayland apps (Proton games)
// on a Wayland session. Empty when the active window is a native Wayland
// window, on non-X11 platforms, or when the title cannot be read.
QString current();

}
