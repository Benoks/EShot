#include "LinuxDesktopNotification.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
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

QStringList LinuxDesktopNotification::actions(const QList<Action> &actions)
{
    QStringList list;
    for (const Action &action : actions) {
        if (action.key.isEmpty() || action.label.isEmpty()
            || action.key == QStringLiteral("default")) {
            continue;
        }
        list << action.key << action.label;
    }
    return list;
}

QVariantMap LinuxDesktopNotification::hintsForPath(const QString &path)
{
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("io.github.benoks.EShot"));
    hints.insert(QStringLiteral("x-kde-urls"),
                 QStringList({QUrl::fromLocalFile(path).toString()}));
    return hints;
}

QVariantList LinuxDesktopNotification::notifyArguments(const QString &title,
                                                       const QString &body,
                                                       const QString &path,
                                                       const QStringList &actionList,
                                                       int timeoutMs)
{
    // The implicit "default" action fires when the notification body itself
    // is activated, so a click always opens the captured file.
    QStringList allActions = actionList;
    allActions << QStringLiteral("default") << title;
    return {QStringLiteral("EShot"), 0u, QStringLiteral("io.github.benoks.EShot-v4"),
            title, body, allActions, hintsForPath(path), timeoutMs};
}

bool LinuxDesktopNotification::show(const QString &title, const QString &body,
                                    const QString &path, const QString &actionLabel,
                                    int timeoutMs)
{
    return notify(title, body, path, actions(actionLabel), timeoutMs);
}

bool LinuxDesktopNotification::showWithActions(const QString &title, const QString &body,
                                               const QString &path,
                                               const QList<Action> &actionList,
                                               int timeoutMs)
{
    return notify(title, body, path, actions(actionList), timeoutMs);
}

bool LinuxDesktopNotification::notify(const QString &title, const QString &body,
                                      const QString &path, QStringList actionList,
                                      int timeoutMs)
{
    QDBusInterface notifications(QString::fromLatin1(NotificationsService),
                                 QString::fromLatin1(NotificationsPath),
                                 QString::fromLatin1(NotificationsInterface),
                                 QDBusConnection::sessionBus());
    if (!notifications.isValid())
        return false;

    const QDBusReply<uint> reply = notifications.callWithArgumentList(
        QDBus::Block, QStringLiteral("Notify"),
        notifyArguments(title, body, path, actionList, timeoutMs));
    if (!reply.isValid()) {
        qWarning() << "[Notification] Notify failed:" << reply.error().name()
                   << reply.error().message();
        return false;
    }

    const uint id = reply.value();
    // Cap the tracked paths so long sessions cannot grow the map unbounded.
    constexpr int kMaxTrackedPaths = 32;
    while (m_paths.size() >= kMaxTrackedPaths) {
        uint oldestId = std::numeric_limits<uint>::max();
        for (uint trackedId : m_paths.keys())
            oldestId = qMin(oldestId, trackedId);
        m_paths.remove(oldestId);
    }
    m_paths.insert(id, path);
    return true;
}

void LinuxDesktopNotification::onActionInvoked(uint id, const QString &actionKey)
{
    if (!m_paths.contains(id))
        return;
    const QString path = m_paths.take(id);
    if (path.isEmpty())
        return;
    if (actionKey == QStringLiteral("open-folder") || actionKey == QStringLiteral("default"))
        emit pathActivated(path);
    else
        emit actionInvoked(path, actionKey);
}

void LinuxDesktopNotification::onNotificationClosed(uint id, uint reason)
{
    Q_UNUSED(reason)
    m_paths.remove(id);
}
