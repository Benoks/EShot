#include "LinuxDesktopNotification.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QUrl>

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
    return {QStringLiteral("default"), actionLabel};
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

    const QDBusReply<uint> reply = notifications.call(
        QStringLiteral("Notify"), QStringLiteral("EShot"), uint(0),
        QStringLiteral("io.github.benoks.EShot-v4"), title, body,
        actions(actionLabel), hintsForPath(path), timeoutMs);
    if (!reply.isValid())
        return false;

    m_paths.insert(reply.value(), path);
    return true;
}

void LinuxDesktopNotification::onActionInvoked(uint id, const QString &actionKey)
{
    if (actionKey != QStringLiteral("default"))
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
