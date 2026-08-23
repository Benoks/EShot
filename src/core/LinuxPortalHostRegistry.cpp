#include "LinuxPortalHostRegistry.h"

#include <QDebug>
#include <QVariantMap>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusConnection>
#include <QDBusMessage>
#endif

namespace {

LinuxPortalHostRegistrationState registrationState =
    LinuxPortalHostRegistrationState::NotAttempted;

}

namespace LinuxPortalHostRegistry {

QString applicationId()
{
    return QStringLiteral("io.github.benoks.EShot");
}

LinuxPortalHostRegistrationState classifyReply(bool success, const QString &errorName,
                                                const QString &errorMessage)
{
    if (success)
        return LinuxPortalHostRegistrationState::Registered;

    // Register() is connection-scoped. A second registration attempt on the
    // same D-Bus connection means the portal already knows our application ID.
    if (errorName == QStringLiteral("org.freedesktop.portal.Error.Failed")
        && errorMessage.contains(QStringLiteral("already associated with an application ID"),
                                 Qt::CaseInsensitive)) {
        return LinuxPortalHostRegistrationState::Registered;
    }

    if (errorName == QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface")
        || errorName == QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod")
        || errorName == QStringLiteral("org.freedesktop.DBus.Error.UnknownObject")
        || errorName == QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown")) {
        return LinuxPortalHostRegistrationState::Unsupported;
    }
    return LinuxPortalHostRegistrationState::Failed;
}

bool portalMayIdentifyApp(LinuxPortalHostRegistrationState state)
{
    return state == LinuxPortalHostRegistrationState::Registered
        || state == LinuxPortalHostRegistrationState::Unsupported;
}

LinuxPortalHostRegistrationState registerApplication()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // A transient failure (e.g. the portal not yet running) must not disable
    // registration for the whole session; only stable states are cached.
    if (registrationState != LinuxPortalHostRegistrationState::NotAttempted
        && registrationState != LinuxPortalHostRegistrationState::Failed)
        return registrationState;

    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        registrationState = LinuxPortalHostRegistrationState::Failed;
        qWarning() << "[PortalRegistry] session bus is unavailable";
        return registrationState;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.host.portal.Registry"),
        QStringLiteral("Register"));
    message.setArguments({applicationId(), QVariantMap()});
    const QDBusMessage reply = bus.call(message, QDBus::Block, 5000);
    const bool success = reply.type() != QDBusMessage::ErrorMessage;
    registrationState = classifyReply(success, reply.errorName(), reply.errorMessage());
    if (registrationState == LinuxPortalHostRegistrationState::Registered) {
        qInfo() << "[PortalRegistry] registered host app id=" << applicationId();
    } else if (registrationState == LinuxPortalHostRegistrationState::Unsupported) {
        qInfo() << "[PortalRegistry] host app registry is unavailable; using portal auto-detection";
    } else {
        qWarning() << "[PortalRegistry] host app registration failed:"
                   << reply.errorName() << reply.errorMessage();
    }
    return registrationState;
#else
    registrationState = LinuxPortalHostRegistrationState::Unsupported;
    return registrationState;
#endif
}

LinuxPortalHostRegistrationState state()
{
    return registrationState;
}

}
