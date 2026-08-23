#include "LinuxPortalScreenCast.h"
#include "LinuxDesktopIntegration.h"
#include "LinuxPortalRequest.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QGuiApplication>
#include <QSettings>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <QUrl>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QUuid>

#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

QString restoreTokenSettingsKey(const QString &persistenceId)
{
    const QString id = persistenceId.trimmed().isEmpty()
        ? QStringLiteral("default") : persistenceId.trimmed();
    return QStringLiteral("linux/screencastRestoreToken/%1")
        .arg(QString::fromLatin1(QUrl::toPercentEncoding(id)));
}

}

LinuxPortalScreenCast::LinuxPortalScreenCast(QObject *parent)
    : QObject(parent)
{
}

bool LinuxPortalScreenCast::isWaylandSession()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return isWaylandSessionType(qEnvironmentVariable("XDG_SESSION_TYPE"),
                                QGuiApplication::platformName());
#else
    return false;
#endif
}

bool LinuxPortalScreenCast::isWaylandSessionType(const QString &sessionType,
                                                 const QString &platformName)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return LinuxDesktopIntegration::isWayland(sessionType, platformName);
#else
    Q_UNUSED(sessionType);
    Q_UNUSED(platformName);
    return false;
#endif
}

bool LinuxPortalScreenCast::isAvailable()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    QDBusInterface screencast(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.ScreenCast"),
        bus);
    return screencast.isValid();
#else
    return false;
#endif
}

QVariantMap LinuxPortalScreenCast::sourceOptions(uint portalVersion,
                                                 uint availableCursorModes,
                                                 const QString &restoreToken)
{
    QVariantMap options;
    options.insert(QStringLiteral("types"), 1u);
    options.insert(QStringLiteral("multiple"), false);
    constexpr uint HiddenCursor = 1u;
    constexpr uint EmbeddedCursor = 2u;
    if ((availableCursorModes & EmbeddedCursor) != 0u)
        options.insert(QStringLiteral("cursor_mode"), EmbeddedCursor);
    else if ((availableCursorModes & HiddenCursor) != 0u)
        options.insert(QStringLiteral("cursor_mode"), HiddenCursor);

    if (portalVersion >= 4u) {
        options.insert(QStringLiteral("persist_mode"), 2u);
        if (!restoreToken.trimmed().isEmpty())
            options.insert(QStringLiteral("restore_token"), restoreToken.trimmed());
    }
    return options;
}

LinuxPortalScreenCast::Stream LinuxPortalScreenCast::selectStream(
    QWidget *parent, int timeoutMs, const QString &persistenceId)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    LinuxPortalScreenCast portal;
    return portal.select(parent, timeoutMs, persistenceId);
#else
    Q_UNUSED(parent);
    Q_UNUSED(timeoutMs);
    Q_UNUSED(persistenceId);
    return {};
#endif
}

LinuxPortalScreenCast::Stream LinuxPortalScreenCast::select(
    QWidget *parent, int timeoutMs, const QString &persistenceId)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_selectInProgress) {
        qWarning() << "[LinuxScreenCast] a screen cast selection is already in progress";
        return {};
    }
    m_selectInProgress = true;
    struct SelectGuard {
        LinuxPortalScreenCast *portal;
        ~SelectGuard() { portal->m_selectInProgress = false; }
    } selectGuard{this};

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return {};

    QDBusInterface screencast(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.ScreenCast"),
        bus);
    if (!screencast.isValid())
        return {};

    const uint portalVersion = screencast.property("version").toUInt();
    const uint availableCursorModes = screencast.property("AvailableCursorModes").toUInt();
    qInfo() << "[LinuxScreenCast] portal version=" << portalVersion
            << "availableCursorModes=" << availableCursorModes;

    const QString token = QStringLiteral("eshot_screencast_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128));
    const QString createToken = token + QStringLiteral("_create");
    QVariantMap sessionOptions;
    sessionOptions.insert(QStringLiteral("handle_token"), createToken);
    sessionOptions.insert(QStringLiteral("session_handle_token"), token + QStringLiteral("_session"));

    if (!callAndWait(screencast, QStringLiteral("CreateSession"),
                     {sessionOptions}, createToken, timeoutMs))
        return {};
    if (m_response != 0)
        return {};

    const QString sessionHandle = objectPathString(m_results.value(QStringLiteral("session_handle")));
    if (sessionHandle.isEmpty())
        return {};

    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    const QString restoreKey = restoreTokenSettingsKey(persistenceId);
    const QString restoreToken = settings.value(restoreKey).toString();
    const QString sourcesToken = token + QStringLiteral("_sources");
    QVariantMap selectOptions = sourceOptions(
        portalVersion, availableCursorModes, restoreToken);
    selectOptions.insert(QStringLiteral("handle_token"), sourcesToken);
    if (!callAndWait(screencast, QStringLiteral("SelectSources"),
                     {QVariant::fromValue(QDBusObjectPath(sessionHandle)), selectOptions},
                     sourcesToken, timeoutMs)) {
        closeSession(sessionHandle);
        return {};
    }
    if (m_response != 0) {
        closeSession(sessionHandle);
        return {};
    }

    const bool isX11 = QGuiApplication::platformName().contains(QStringLiteral("xcb"), Qt::CaseInsensitive);
    const QString parentWindow = isX11 && parent && parent->windowHandle()
        ? QStringLiteral("x11:%1").arg(QString::number(parent->windowHandle()->winId(), 16))
        : QString();

    QVariantMap startOptions;
    const QString startToken = token + QStringLiteral("_start");
    startOptions.insert(QStringLiteral("handle_token"), startToken);
    if (!callAndWait(screencast, QStringLiteral("Start"),
                     {QVariant::fromValue(QDBusObjectPath(sessionHandle)),
                      parentWindow, startOptions}, startToken, timeoutMs)) {
        closeSession(sessionHandle);
        return {};
    }
    if (m_response != 0) {
        closeSession(sessionHandle);
        return {};
    }

    const QString refreshedRestoreToken = m_results.value(
        QStringLiteral("restore_token")).toString().trimmed();
    if (!refreshedRestoreToken.isEmpty()) {
        settings.setValue(restoreKey, refreshedRestoreToken);
    }

    Stream stream = firstStreamFromResults(m_results);
    stream.sessionHandle = sessionHandle;
    stream.usedRestoreToken = !restoreToken.isEmpty();
    stream.pipewireFd = openPipeWireRemote(sessionHandle);
    return stream;
#else
    Q_UNUSED(parent);
    Q_UNUSED(timeoutMs);
    Q_UNUSED(persistenceId);
    return {};
#endif
}

void LinuxPortalScreenCast::clearRestoreToken(const QString &persistenceId)
{
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    settings.remove(restoreTokenSettingsKey(persistenceId));
}

void LinuxPortalScreenCast::closeSession(const QString &sessionHandle)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (sessionHandle.isEmpty()) return;
    QDBusInterface session(QStringLiteral("org.freedesktop.portal.Desktop"), sessionHandle,
                           QStringLiteral("org.freedesktop.portal.Session"),
                           QDBusConnection::sessionBus());
    if (session.isValid()) session.call(QStringLiteral("Close"));
#else
    Q_UNUSED(sessionHandle);
#endif
}

bool LinuxPortalScreenCast::callAndWait(QDBusInterface &portal,
                                        const QString &method,
                                        const QList<QVariant> &arguments,
                                        const QString &handleToken,
                                        int timeoutMs)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_response = 2;
    m_results.clear();
    m_finished = false;

    QStringList connectedPaths;
    auto connectResponse = [&](const QString &path) {
        if (path.isEmpty() || connectedPaths.contains(path))
            return;
        if (bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                        QStringLiteral("org.freedesktop.portal.Request"),
                        QStringLiteral("Response"), this,
                        SLOT(onPortalResponse(uint,QVariantMap)))) {
            connectedPaths.append(path);
        }
    };
    connectResponse(LinuxPortalRequest::requestPath(bus.baseService(), handleToken));

    const QDBusMessage reply = portal.callWithArgumentList(QDBus::Block, method, arguments);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "[LinuxScreenCast]" << method << "failed:"
                   << reply.errorName() << reply.errorMessage();
        for (const QString &path : std::as_const(connectedPaths)) {
            bus.disconnect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                           QStringLiteral("org.freedesktop.portal.Request"),
                           QStringLiteral("Response"), this,
                           SLOT(onPortalResponse(uint,QVariantMap)));
        }
        return false;
    }

    if (!reply.arguments().isEmpty()) {
        const QDBusObjectPath actualPath = qvariant_cast<QDBusObjectPath>(reply.arguments().first());
        connectResponse(actualPath.path());
    }
    if (connectedPaths.isEmpty())
        return false;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &LinuxPortalScreenCast::destroyed, &loop, &QEventLoop::quit);
    m_loop = &loop;
    timeout.start(timeoutMs);
    if (!m_finished)
        loop.exec();
    m_loop = nullptr;

    for (const QString &path : std::as_const(connectedPaths)) {
        bus.disconnect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                       QStringLiteral("org.freedesktop.portal.Request"),
                       QStringLiteral("Response"), this,
                       SLOT(onPortalResponse(uint,QVariantMap)));
    }

    if (!m_finished)
        qWarning() << "[LinuxScreenCast]" << method << "timed out after" << timeoutMs << "ms";

    return m_finished;
#else
    Q_UNUSED(portal);
    Q_UNUSED(method);
    Q_UNUSED(arguments);
    Q_UNUSED(handleToken);
    Q_UNUSED(timeoutMs);
    return false;
#endif
}

QSharedPointer<int> LinuxPortalScreenCast::openPipeWireRemote(const QString &sessionHandle) const
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (sessionHandle.isEmpty())
        return {};

    QDBusInterface screencast(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.ScreenCast"),
        QDBusConnection::sessionBus());
    if (!screencast.isValid())
        return {};

    QDBusReply<QDBusUnixFileDescriptor> reply = screencast.call(
        QStringLiteral("OpenPipeWireRemote"),
        QDBusObjectPath(sessionHandle),
        QVariantMap());
    if (!reply.isValid() || !reply.value().isValid())
        return {};

    const int duplicated = fcntl(reply.value().fileDescriptor(), F_DUPFD_CLOEXEC, 3);
    if (duplicated < 0)
        return {};

    return QSharedPointer<int>(
        new int(duplicated),
        [](int *fd) {
            if (fd) {
                if (*fd >= 0)
                    close(*fd);
                delete fd;
            }
        });
#else
    Q_UNUSED(sessionHandle);
    return {};
#endif
}

QString LinuxPortalScreenCast::objectPathString(const QVariant &value) const
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QDBusObjectPath path = qvariant_cast<QDBusObjectPath>(value);
    if (!path.path().isEmpty())
        return path.path();
#endif
    return value.toString();
}

LinuxPortalScreenCast::Stream LinuxPortalScreenCast::firstStreamFromResults(const QVariantMap &results) const
{
    Stream stream;
    const QVariant streamsValue = results.value(QStringLiteral("streams"));
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QDBusArgument argument = streamsValue.value<QDBusArgument>();
    if (argument.currentType() == QDBusArgument::ArrayType) {
        argument.beginArray();
        if (!argument.atEnd()) {
            QVariantMap properties;
            uint nodeId = 0;
            argument.beginStructure();
            argument >> nodeId >> properties;
            argument.endStructure();

            stream.nodeId = nodeId;
            stream.properties = properties;
        }
        argument.endArray();
    }
#endif
    if (!stream.isValid()) {
        const QVariantList streams = streamsValue.toList();
        if (!streams.isEmpty()) {
            const QVariant first = streams.first();
            const QVariantList tuple = first.toList();
            if (tuple.size() >= 2) {
                stream.nodeId = tuple.at(0).toUInt();
                stream.properties = tuple.at(1).toMap();
            }
        }
    }

    stream.pipewireSerial = stream.properties.value(QStringLiteral("pipewire-serial")).toULongLong();
    const QVariant sizeValue = stream.properties.value(QStringLiteral("size"));
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QDBusArgument sizeArgument = sizeValue.value<QDBusArgument>();
    if (sizeArgument.currentType() == QDBusArgument::StructureType) {
        int width = 0;
        int height = 0;
        sizeArgument.beginStructure();
        sizeArgument >> width >> height;
        sizeArgument.endStructure();
        if (width > 0 && height > 0)
            stream.size = QSize(width, height);
    }
#endif
    if (!stream.size.isValid()) {
        const QVariantList sizeList = sizeValue.toList();
        if (sizeList.size() >= 2)
            stream.size = QSize(sizeList.at(0).toInt(), sizeList.at(1).toInt());
    }

    const QVariant positionValue = stream.properties.value(QStringLiteral("position"));
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QDBusArgument positionArgument = positionValue.value<QDBusArgument>();
    if (positionArgument.currentType() == QDBusArgument::StructureType) {
        int x = 0;
        int y = 0;
        positionArgument.beginStructure();
        positionArgument >> x >> y;
        positionArgument.endStructure();
        stream.position = QPoint(x, y);
    }
#endif
    if (stream.position.isNull()) {
        const QVariantList positionList = positionValue.toList();
        if (positionList.size() >= 2)
            stream.position = QPoint(positionList.at(0).toInt(), positionList.at(1).toInt());
    }
    return stream;
}

void LinuxPortalScreenCast::onPortalResponse(uint response, const QVariantMap &results)
{
    m_response = response;
    m_results = results;
    m_finished = true;
    if (m_loop)
        m_loop->quit();
}
