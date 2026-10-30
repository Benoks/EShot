#include "LinuxAutoStartPolicy.h"
#include "LinuxDesktopIntegration.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

QString LinuxAutoStartPolicy::executablePath(const QString &appImagePath,
                                             const QString &applicationFilePath)
{
    const QFileInfo appImage(appImagePath);
    if (!appImagePath.isEmpty() && appImage.isFile())
        return appImage.absoluteFilePath();

    return applicationFilePath;
}

QString LinuxAutoStartPolicy::commandLine(const QString &executablePath,
                                          const QString &currentDesktop,
                                          const QString &sessionDesktop,
                                          const QString &sessionType,
                                          const QString &captureBackend)
{
    QString escapedPath = executablePath;
    escapedPath.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escapedPath.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    QString command = QStringLiteral("\"") + escapedPath + QStringLiteral("\" --silent");
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        currentDesktop, sessionDesktop);
    if (LinuxDesktopIntegration::useNativeWaylandOverlay(desktop, sessionType, captureBackend)) {
        command.prepend(QStringLiteral(
            "/usr/bin/env QT_QPA_PLATFORM=wayland ESHOT_WAYLAND_XWAYLAND_OVERLAY=0 "
            "ESHOT_CAPTURE_BACKEND=wayland "));
    } else if (LinuxDesktopIntegration::useXWaylandOverlay(desktop, sessionType, captureBackend)) {
        command.prepend(QStringLiteral(
            "/usr/bin/env QT_QPA_PLATFORM=\"xcb;wayland\" ESHOT_WAYLAND_XWAYLAND_OVERLAY=1 "));
    }
    return command;
}

namespace {
QString desktopEntryValue(const QString &text, const QString &key)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    bool inMainGroup = false;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inMainGroup = line == QStringLiteral("[Desktop Entry]");
            continue;
        }
        if (!inMainGroup)
            continue;
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals > 0 && line.left(equals).trimmed() == key)
            return line.mid(equals + 1).trimmed();
    }
    return {};
}

QStringList execTokens(const QString &exec)
{
    QStringList tokens;
    QString current;
    bool inQuotes = false;
    bool hasToken = false;
    for (int i = 0; i < exec.size(); ++i) {
        const QChar ch = exec.at(i);
        if (inQuotes && ch == QLatin1Char('\\') && i + 1 < exec.size()) {
            current += exec.at(++i);
        } else if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            hasToken = true;
        } else if (!inQuotes && ch.isSpace()) {
            if (hasToken)
                tokens << current;
            current.clear();
            hasToken = false;
        } else {
            current += ch;
            hasToken = true;
        }
    }
    if (hasToken)
        tokens << current;
    return tokens;
}
}

bool LinuxAutoStartPolicy::entryEnabled(const QString &desktopEntryText)
{
    if (desktopEntryText.trimmed().isEmpty())
        return false;
    if (desktopEntryValue(desktopEntryText, QStringLiteral("Hidden"))
            .compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return desktopEntryValue(desktopEntryText, QStringLiteral("X-GNOME-Autostart-enabled"))
               .compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0;
}

QString LinuxAutoStartPolicy::entryExecutable(const QString &desktopEntryText)
{
    static const QRegularExpression assignment(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*="));
    const QStringList tokens = execTokens(desktopEntryValue(desktopEntryText, QStringLiteral("Exec")));
    for (const QString &token : tokens) {
        if (token == QStringLiteral("env") || token.endsWith(QStringLiteral("/env"))
            || assignment.match(token).hasMatch()) {
            continue;
        }
        return token;
    }
    return {};
}

bool LinuxAutoStartPolicy::shouldRepointAtStartup(const QString &entryExecutable,
                                                  bool entryExecutableExists,
                                                  const QString &currentExecutable,
                                                  const QString &integratedExecutable)
{
    if (currentExecutable.isEmpty() || entryExecutable == currentExecutable)
        return false;
    if (entryExecutable.isEmpty() || !entryExecutableExists)
        return true;
    return !integratedExecutable.isEmpty() && currentExecutable == integratedExecutable;
}
