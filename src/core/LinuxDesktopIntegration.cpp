#include "LinuxDesktopIntegration.h"

namespace {

QString effectiveDesktop(const QString &currentDesktop, const QString &sessionDesktop)
{
    return currentDesktop.trimmed().isEmpty() ? sessionDesktop : currentDesktop;
}

}

namespace LinuxDesktopIntegration {

LinuxDesktopEnvironment detect(const QString &currentDesktop,
                               const QString &sessionDesktop)
{
    const QString desktop = effectiveDesktop(currentDesktop, sessionDesktop);
    if (desktop.contains(QStringLiteral("KDE"), Qt::CaseInsensitive)
        || desktop.contains(QStringLiteral("Plasma"), Qt::CaseInsensitive)) {
        return LinuxDesktopEnvironment::Kde;
    }
    if (desktop.contains(QStringLiteral("GNOME"), Qt::CaseInsensitive)
        || desktop.contains(QStringLiteral("Ubuntu"), Qt::CaseInsensitive)) {
        return LinuxDesktopEnvironment::Gnome;
    }
    return LinuxDesktopEnvironment::Other;
}

bool isWayland(const QString &sessionType, const QString &platformName)
{
    return sessionType.compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0
        || platformName.contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
}

bool useXWaylandOverlay(LinuxDesktopEnvironment desktop, const QString &sessionType)
{
    return isWayland(sessionType)
        && (desktop == LinuxDesktopEnvironment::Kde
            || desktop == LinuxDesktopEnvironment::Gnome);
}

bool deferFirstRunHotkeyRegistration(LinuxDesktopEnvironment desktop)
{
    return desktop == LinuxDesktopEnvironment::Gnome;
}

bool useGnomeShortcutFallback(LinuxDesktopEnvironment desktop, bool portalAvailable)
{
    return desktop == LinuxDesktopEnvironment::Gnome && !portalAvailable;
}

bool shouldShowControlCenter(LinuxDesktopEnvironment desktop, bool trayAvailable, bool silent)
{
    return desktop == LinuxDesktopEnvironment::Gnome && !trayAvailable && !silent;
}

QString displayName(LinuxDesktopEnvironment desktop)
{
    switch (desktop) {
    case LinuxDesktopEnvironment::Kde: return QStringLiteral("KDE Plasma");
    case LinuxDesktopEnvironment::Gnome: return QStringLiteral("GNOME");
    case LinuxDesktopEnvironment::Other: return QStringLiteral("Other");
    }
    return QStringLiteral("Other");
}

}
