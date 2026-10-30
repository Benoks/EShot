#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

// Window selection on KDE Plasma (Wayland and X11). A one-shot KWin script
// reports the windows in stacking order back to EShot over D-Bus; these pure
// helpers build that script, parse its reply, drop windows that cannot be
// selected and map KWin's compositor coordinates to overlay coordinates.

struct KWinOutputInfo {
    QString name;
    QRect geometry;     // compositor logical coordinates
};

struct KWinWindowInfo {
    QRect frame;        // frameGeometry, compositor logical coordinates
    QString caption;
    QString resourceClass;
    qint64 pid = 0;
    bool managed = true;
    bool deleted = false;
    bool minimized = false;
    bool hidden = false;
    bool onAllDesktops = false;
    QStringList desktops;    // empty: every desktop
    QStringList activities;  // empty: every activity
    bool normalWindow = false;
    bool dialog = false;
    bool utility = false;
    bool desktopWindow = false;
    bool dock = false;
    bool toolbar = false;
    bool menu = false;
    bool splash = false;
    bool dropdownMenu = false;
    bool popupMenu = false;
    bool tooltip = false;
    bool notification = false;
    bool criticalNotification = false;
    bool onScreenDisplay = false;
    bool appletPopup = false;
    bool comboBox = false;
    bool dndIcon = false;
    bool popupWindow = false;
    bool outline = false;
    bool fullScreen = false;
    bool keepAbove = false;
};

struct KWinWindowSnapshot {
    bool valid = false;
    QString error;                    // set when the script caught an exception
    QString currentDesktop;
    QString currentActivity;
    QVector<KWinOutputInfo> outputs;
    QVector<KWinWindowInfo> windows;  // topmost first
};
Q_DECLARE_METATYPE(KWinWindowSnapshot)

// A QScreen as the overlay sees it: name and geometry in Qt's global
// coordinates (X11 coordinates under XWayland, logical ones on Wayland).
struct KWinQtScreen {
    QString name;
    QRect geometry;
};

namespace KWinWindowList {

// Script that collects the window list and calls
// service/path/interface.method(token, json). Uses KWin 6 names
// (workspace.stackingOrder, screens, window.desktops) and falls back to the
// KWin 5 ones (clientList(), numScreens, window.desktop).
QString script(const QString &service, const QString &path, const QString &interface,
               const QString &method, const QString &token);

// Per-process plugin name, so a new request replaces the previous script.
QString pluginName(qint64 pid);

KWinWindowSnapshot parse(const QByteArray &json);

// Whether a window can be picked: not EShot's own, visible on the current
// desktop and activity, a real application window (no panel, desktop,
// notification, OSD, menu, tooltip or splash), and not empty.
bool isSelectable(const KWinWindowInfo &window, const KWinWindowSnapshot &snapshot,
                  qint64 ownPid);

// Maps a compositor rect to Qt global coordinates. Outputs are matched to
// screens by name (then by identical geometry); each corner is scaled with
// the output that contains it, so mixed or fractional scaling under
// XWayland keeps windows aligned. Without any match the rect is returned
// unchanged.
QRect mapToQt(const QRect &compositorRect, const QVector<KWinOutputInfo> &outputs,
              const QVector<KWinQtScreen> &screens);

// Selectable windows mapped into overlay-local coordinates (overlay origin
// in Qt global coordinates, overlay size), clipped to the overlay and kept
// in stacking order, topmost first. Windows smaller than 16x16 are dropped.
QVector<QRect> overlayRects(const KWinWindowSnapshot &snapshot, qint64 ownPid,
                            const QVector<KWinQtScreen> &screens,
                            const QPoint &overlayOrigin, const QSize &overlaySize);

}
