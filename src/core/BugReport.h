#pragma once

#include <QString>
#include <QUrl>

// "Report a problem" opens a new GitHub issue with the environment already
// filled in, so the user only describes what happened. Nothing personal is
// included: no paths, user names or settings.
struct BugReportInfo
{
    QString eshotVersion;
    QString operatingSystem;
    QString architecture;
    QString qtVersion;
    QString platform;     // Qt platform plugin (xcb, wayland, windows)
    QString desktop;      // XDG_CURRENT_DESKTOP, empty on Windows
    QString sessionType;  // XDG_SESSION_TYPE, empty on Windows
    QString installKind;  // AppImage, package, portable, installer
};

BugReportInfo currentBugReportInfo();
QUrl bugReportUrl(const BugReportInfo &info);
