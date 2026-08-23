#include "LinuxDesktopNotification.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QUrl>
#include <limits>

namespace {
constexpr auto NotificationsService = "org.freedesktop.Notifications";
constexpr auto NotificationsPath = "/org/freedesktop/Notifications";
constexpr auto NotificationsInterface = "org.freedesktop.Notifications";
}

LinuxDesktopNotification::LinuxDesktopNotification(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString::fromLatin1(NotificationsService),
                QString::fromLatin1(NotificationsPath),
                QString::fromLatin1(NotificationsInterface),
                QStringLiteral("ActionInvoked"),
                this, SLOT(onActionInvoked(uint,QString)));
    bus.connect(QString::fromLatin1(NotificationsService),
                QString::fromLatin1(NotificationsPath),
                QString::fromLatin1(NotificationsInterface),
                QStringLiteral("NotificationClosed"),
                this, SLOT(onNotificationClosed(uint,uint)));
}

QStringList LinuxDesktopNotification::actions(const QString &actionLabel)
{
    if (actionLabel.isEmpty())
        return {};
    return {QStringLiteral("open-folder"), actionLabel};
}

QVariantMap LinuxDesktopNotification::hintsForPath(const QString &path)
{
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("io.github.benoks.EShot"));
    hints.insert(QStringLiteral("x-kde-urls"),
                 QStringList({QUrl::fromLocalFile(path).toString()}));
    return hints;
}

bool LinuxDesktopNotification::show(const QString &title, const QString &body,
                                    const QString &path, const QString &actionLabel,
                                    int timeoutMs)
{
    QDBusInterface notifications(QString::fromLatin1(NotificationsService),
                                 QString::fromLatin1(NotificationsPath),
                                 QString::fromLatin1(NotificationsInterface),
                                 QDBusConnection::sessionBus());
    if (!notifications.isValid())
        return false;

    QStringList actionList = actions(actionLabel);
    // The implicit "default" action fires when the notification body itself
    // is activated, so a click always opens the captured file.
    actionList << QStringLiteral("default") << title;

    const QDBusReply<uint> reply = notifications.call(
        QStringLiteral("Notify"), QStringLiteral("EShot"), m_lastId,
        QStringLiteral("io.github.benoks.EShot-v4"), title, body,
        actionList, hintsForPath(path), timeoutMs);
    if (!reply.isValid())
        return false;

    m_lastId = reply.value();
    // Cap the tracked paths so long sessions cannot grow the map unbounded.
    constexpr int kMaxTrackedPaths = 32;
    while (m_paths.size() >= kMaxTrackedPaths) {
        uint oldestId = std::numeric_limits<uint>::max();
        for (uint trackedId : m_paths.keys())
            oldestId = qMin(oldestId, trackedId);
        m_paths.remove(oldestId);
    }
    m_paths.insert(m_lastId, path);
    return true;
}

void LinuxDesktopNotification::onActionInvoked(uint id, const QString &actionKey)
{
    if (actionKey != QStringLiteral("open-folder")
        && actionKey != QStringLiteral("default"))
        return;
    const QString path = m_paths.take(id);
    if (!path.isEmpty())
        emit pathActivated(path);
}

void LinuxDesktopNotification::onNotificationClosed(uint id, uint reason)
{
    Q_UNUSED(reason)
    m_paths.remove(id);
}
