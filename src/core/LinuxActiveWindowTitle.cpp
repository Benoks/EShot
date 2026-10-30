#include "LinuxActiveWindowTitle.h"

#include <QGuiApplication>

#if defined(ESHOT_HAVE_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace LinuxActiveWindowTitle {

#if defined(ESHOT_HAVE_X11)
namespace {

QByteArray windowProperty(Display *display, Window window, Atom property, Atom type)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long items = 0;
    unsigned long remaining = 0;
    unsigned char *data = nullptr;
    const int status = XGetWindowProperty(display, window, property, 0, 1024, False, type,
                                          &actualType, &actualFormat, &items, &remaining, &data);
    QByteArray result;
    if (status == Success && data && actualType == type) {
        const int bytes = actualFormat == 32 ? int(items * sizeof(long))
                                             : int(items * (actualFormat / 8));
        result = QByteArray(reinterpret_cast<const char *>(data), bytes);
    }
    if (data)
        XFree(data);
    return result;
}

}
#endif

QString current()
{
#if defined(ESHOT_HAVE_X11)
    if (!QGuiApplication::platformName().contains(QStringLiteral("xcb"), Qt::CaseInsensitive))
        return {};
    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return {};
    QString title;
    const Window root = DefaultRootWindow(display);
    const QByteArray active = windowProperty(
        display, root, XInternAtom(display, "_NET_ACTIVE_WINDOW", False), XA_WINDOW);
    if (active.size() >= int(sizeof(long))) {
        const Window window = static_cast<Window>(*reinterpret_cast<const long *>(active.constData()));
        if (window != None) {
            const QByteArray utf8Title = windowProperty(
                display, window, XInternAtom(display, "_NET_WM_NAME", False),
                XInternAtom(display, "UTF8_STRING", False));
            title = utf8Title.isEmpty()
                ? QString::fromLocal8Bit(windowProperty(display, window, XA_WM_NAME, XA_STRING))
                : QString::fromUtf8(utf8Title);
        }
    }
    XCloseDisplay(display);
    return title.trimmed();
#else
    return {};
#endif
}

}
