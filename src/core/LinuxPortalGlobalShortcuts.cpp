#include "LinuxPortalGlobalShortcuts.h"
#include "LinuxDesktopIntegration.h"
#include "LinuxGnomeShortcutInstaller.h"
#include "LinuxPortalHostRegistry.h"
#include "LinuxPortalRequest.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QSettings>
#include <QStringList>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QUuid>

Q_DECLARE_METATYPE(PortalShortcut)
Q_DECLARE_METATYPE(QList<PortalShortcut>)

QString objectPathString(const QVariant &value)
{
    const QDBusObjectPath path = qvariant_cast<QDBusObjectPath>(value);
    if (!path.path().isEmpty())
        return path.path();
    return value.toString();
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut)
{
    argument.beginStructure();
    argument << shortcut.id << shortcut.options;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut)
{
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.options;
    argument.endStructure();
    return argument;
}
#endif

LinuxPortalGlobalShortcuts::LinuxPortalGlobalShortcuts(QObject *parent)
    : QObject(parent)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    if (!LinuxPortalHostRegistry::portalMayIdentifyApp(
            LinuxPortalHostRegistry::state())) {
        qWarning() << "[HotkeyManager] GlobalShortcuts disabled because the portal could not identify EShot";
        return;
    }

    QDBusInterface portal(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        bus);
    m_available = portal.isValid();
    m_version = portal.property("version").toUInt();

    if (m_available) {
        qInfo() << "[HotkeyManager] GlobalShortcuts portal available, version=" << m_version;
        bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"),
                    QStringLiteral("/org/freedesktop/portal/desktop"),
                    QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
                    QStringLiteral("Activated"),
                    this,
                    SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    }
#endif
}

LinuxPortalGlobalShortcuts::~LinuxPortalGlobalShortcuts()
{
    closeSession();
}

bool LinuxPortalGlobalShortcuts::isAvailable() const
{
    return m_available;
}

bool LinuxPortalGlobalShortcuts::desktopPortalAvailable()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!LinuxPortalHostRegistry::portalMayIdentifyApp(
            LinuxPortalHostRegistry::state())) {
        return false;
    }
    QDBusInterface portal(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QDBusConnection::sessionBus());
    return portal.isValid();
#else
    return false;
#endif
}

void LinuxPortalGlobalShortcuts::setShortcuts(const QHash<int, QPair<UINT, UINT>> &shortcuts)
{
    m_shortcuts = shortcuts;
    m_ids.clear();
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it)
        m_ids.insert(shortcutIdForInt(it.key()), it.key());

    if (!m_available)
        return;

    if (m_shortcuts.isEmpty()) {
        closeSession();
        return;
    }

    if (m_bindCompleted)
        closeSession();

    if (m_sessionHandle.isEmpty())
        createSession();
    else
        bindShortcuts();
}

bool LinuxPortalGlobalShortcuts::requestRebind()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!m_available || m_shortcuts.isEmpty())
        return false;

    if (m_bindCompleted && !m_sessionHandle.isEmpty() && supportsConfiguration(m_version)) {
        QDBusInterface portal(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
            QDBusConnection::sessionBus());
        const QDBusReply<void> reply = portal.call(
            QStringLiteral("ConfigureShortcuts"),
            QDBusObjectPath(m_sessionHandle), QString(), QVariantMap());
        if (reply.isValid())
            return true;
        qWarning() << "[HotkeyManager] Global shortcuts configuration failed:"
                   << reply.error().name() << reply.error().message();
    }

    closeSession();
    createSession();
    return m_createPending || !m_sessionHandle.isEmpty();
#else
    return false;
#endif
}

void LinuxPortalGlobalShortcuts::createSession()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_createPending)
        return;

    QDBusInterface portal(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QDBusConnection::sessionBus());
    if (!portal.isValid()) {
        m_available = false;
        return;
    }

    const QString token = QStringLiteral("eshot_shortcuts_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128));
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("session_handle_token"), token + QStringLiteral("_session"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString predictedPath = LinuxPortalRequest::requestPath(bus.baseService(), token);
    m_createRequestPaths.clear();
    auto connectResponse = [&](const QString &path) {
        if (path.isEmpty() || m_createRequestPaths.contains(path))
            return;
        if (bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                        QStringLiteral("org.freedesktop.portal.Request"),
                        QStringLiteral("Response"), this,
                        SLOT(onCreateSessionResponse(uint,QVariantMap)))) {
            m_createRequestPaths.append(path);
        }
    };
    connectResponse(predictedPath);
    m_createPending = true;

    QDBusReply<QDBusObjectPath> reply = portal.call(QStringLiteral("CreateSession"), options);
    if (!reply.isValid()) {
        qWarning() << "[HotkeyManager] Global shortcuts portal session failed:"
                   << reply.error().name() << reply.error().message();
        m_createPending = false;
        m_available = false;
        disconnectCreateRequests();
        emit portalFailed(reply.error().name(), reply.error().message());
        return;
    }
    connectResponse(reply.value().path());
#endif
}

void LinuxPortalGlobalShortcuts::bindShortcuts()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (m_bindPending || m_sessionHandle.isEmpty() || m_bindCompleted)
        return;

    QList<PortalShortcut> shortcuts;
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        const UINT modifiers = it.value().first;
        const UINT virtualKey = it.value().second;
        if (virtualKey == 0)
            continue;

        QVariantMap description;
        description.insert(QStringLiteral("description"), descriptionForInt(it.key()));
        const QString trigger = preferredTrigger(modifiers, virtualKey);
        if (!trigger.isEmpty())
            description.insert(QStringLiteral("preferred_trigger"), trigger);
        shortcuts.append({shortcutIdForInt(it.key()), description});
    }
    if (shortcuts.isEmpty())
        return;

    QDBusInterface portal(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QDBusConnection::sessionBus());
    if (!portal.isValid()) {
        m_available = false;
        return;
    }

    const QString token = QStringLiteral("eshot_bind_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128));
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);

    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString predictedPath = LinuxPortalRequest::requestPath(bus.baseService(), token);
    m_bindRequestPaths.clear();
    auto connectResponse = [&](const QString &path) {
        if (path.isEmpty() || m_bindRequestPaths.contains(path))
            return;
        if (bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                        QStringLiteral("org.freedesktop.portal.Request"),
                        QStringLiteral("Response"), this,
                        SLOT(onBindShortcutsResponse(uint,QVariantMap)))) {
            m_bindRequestPaths.append(path);
        }
    };
    connectResponse(predictedPath);
    m_bindPending = true;

    QDBusReply<QDBusObjectPath> reply = portal.call(
        QStringLiteral("BindShortcuts"),
        QDBusObjectPath(m_sessionHandle),
        QVariant::fromValue(shortcuts),
        QString(),
        options);
    if (!reply.isValid()) {
        qWarning() << "[HotkeyManager] Global shortcuts portal binding failed:"
                   << reply.error().name() << reply.error().message();
        m_bindPending = false;
        m_available = false;
        disconnectBindRequests();
        emit portalFailed(reply.error().name(), reply.error().message());
        return;
    }
    connectResponse(reply.value().path());
#endif
}

void LinuxPortalGlobalShortcuts::closeSession()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    disconnectCreateRequests();
    disconnectBindRequests();
    if (!m_sessionHandle.isEmpty()) {
        QDBusInterface session(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            m_sessionHandle,
            QStringLiteral("org.freedesktop.portal.Session"),
            QDBusConnection::sessionBus());
        if (session.isValid())
            session.call(QStringLiteral("Close"));
    }

    m_sessionHandle.clear();
    m_createPending = false;
    m_bindPending = false;
    m_bindCompleted = false;
#endif
}

void LinuxPortalGlobalShortcuts::disconnectCreateRequests()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const QString &path : std::as_const(m_createRequestPaths)) {
        bus.disconnect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                       QStringLiteral("org.freedesktop.portal.Request"),
                       QStringLiteral("Response"), this,
                       SLOT(onCreateSessionResponse(uint,QVariantMap)));
    }
#endif
    m_createRequestPaths.clear();
}

void LinuxPortalGlobalShortcuts::disconnectBindRequests()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const QString &path : std::as_const(m_bindRequestPaths)) {
        bus.disconnect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                       QStringLiteral("org.freedesktop.portal.Request"),
                       QStringLiteral("Response"), this,
                       SLOT(onBindShortcutsResponse(uint,QVariantMap)));
    }
#endif
    m_bindRequestPaths.clear();
}

QString LinuxPortalGlobalShortcuts::shortcutIdForInt(int id) const
{
    return QStringLiteral("eshot_%1").arg(id);
}

QString LinuxPortalGlobalShortcuts::descriptionForInt(int id) const
{
    switch (id) {
    case 1: return QStringLiteral("Capture");
    case 2: return QStringLiteral("Pause recording");
    case 3: return QStringLiteral("Stop recording");
    case 4: return QStringLiteral("Cancel recording");
    case 5: return QStringLiteral("Instant capture");
    case 6: return QStringLiteral("GIF capture");
    case 7: return QStringLiteral("Video capture");
    default: return QStringLiteral("EShot shortcut");
    }
}

QString LinuxPortalGlobalShortcuts::preferredTrigger(UINT modifiers, UINT virtualKey)
{
    QStringList parts;
    if (modifiers & MOD_CONTROL) parts << QStringLiteral("CTRL");
    if (modifiers & MOD_ALT) parts << QStringLiteral("ALT");
    if (modifiers & MOD_SHIFT) parts << QStringLiteral("SHIFT");
    if (modifiers & MOD_WIN) parts << QStringLiteral("SUPER");

    const int modifierCount = parts.size();
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        parts << QString(QChar(static_cast<char>(virtualKey))).toLower();
    } else if (virtualKey >= '0' && virtualKey <= '9') {
        parts << QString(QChar(static_cast<char>(virtualKey)));
    } else if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        parts << QStringLiteral("F%1").arg(virtualKey - VK_F1 + 1);
    } else {
        switch (virtualKey) {
        case VK_SNAPSHOT: parts << QStringLiteral("Print"); break;
        case VK_SCROLL: parts << QStringLiteral("Scroll_Lock"); break;
        case VK_SPACE: parts << QStringLiteral("space"); break;
        case VK_RETURN: parts << QStringLiteral("Return"); break;
        case VK_ESCAPE: parts << QStringLiteral("Escape"); break;
        case VK_TAB: parts << QStringLiteral("Tab"); break;
        case VK_BACK: parts << QStringLiteral("BackSpace"); break;
        case VK_INSERT: parts << QStringLiteral("Insert"); break;
        case VK_DELETE: parts << QStringLiteral("Delete"); break;
        case VK_HOME: parts << QStringLiteral("Home"); break;
        case VK_END: parts << QStringLiteral("End"); break;
        case VK_PRIOR: parts << QStringLiteral("Page_Up"); break;
        case VK_NEXT: parts << QStringLiteral("Page_Down"); break;
        case VK_LEFT: parts << QStringLiteral("Left"); break;
        case VK_RIGHT: parts << QStringLiteral("Right"); break;
        case VK_UP: parts << QStringLiteral("Up"); break;
        case VK_DOWN: parts << QStringLiteral("Down"); break;
        default: break;
        }
    }

    // A modifiers-only string is not a usable trigger; report none instead.
    if (parts.size() <= modifierCount)
        return {};
    return parts.join(QStringLiteral("+"));
}

LinuxHotkeyBackend LinuxPortalGlobalShortcuts::preferredBackend(bool portalAvailable,
                                                                bool x11Available)
{
    if (portalAvailable)
        return LinuxHotkeyBackend::Portal;
    if (x11Available)
        return LinuxHotkeyBackend::X11;
    return LinuxHotkeyBackend::Unavailable;
}

bool LinuxPortalGlobalShortcuts::supportsConfiguration(uint portalVersion)
{
    return portalVersion >= 2u;
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
void LinuxPortalGlobalShortcuts::onCreateSessionResponse(uint response, const QVariantMap &results)
{
    disconnectCreateRequests();
    m_createPending = false;
    if (response != 0) {
        qWarning() << "[HotkeyManager] Global shortcuts portal session rejected:" << response;
        m_available = false;
        emit portalFailed(QStringLiteral("org.freedesktop.portal.Error.Failed"),
                          QStringLiteral("CreateSession response %1").arg(response));
        return;
    }

    const QString session = objectPathString(results.value(QStringLiteral("session_handle")));
    if (session.isEmpty()) {
        qWarning() << "[HotkeyManager] Global shortcuts portal returned no session.";
        m_available = false;
        emit portalFailed(QStringLiteral("org.freedesktop.portal.Error.Failed"),
                          QStringLiteral("CreateSession returned no session handle"));
        return;
    }

    m_sessionHandle = session;
    bindShortcuts();
}

void LinuxPortalGlobalShortcuts::onBindShortcutsResponse(uint response, const QVariantMap &results)
{
    disconnectBindRequests();
    m_bindPending = false;
    if (response != 0) {
        qWarning() << "[HotkeyManager] Global shortcuts portal binding rejected:" << response;
        m_available = false;
        emit portalFailed(QStringLiteral("org.freedesktop.portal.Error.Failed"),
                          QStringLiteral("BindShortcuts response %1").arg(response));
        return;
    }
    qInfo() << "[HotkeyManager] Global shortcuts portal bound:" << results;
    m_bindCompleted = true;
    updateAssignedTriggers(results);

    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    if (desktop == LinuxDesktopEnvironment::Gnome) {
        QSettings once(QStringLiteral("EShot"), QStringLiteral("EShot"));
        if (!once.value(QStringLiteral("gnomeLegacyShortcutCleanupDone"), false).toBool()) {
            const auto cleanup = LinuxGnomeShortcutInstaller::uninstallCaptureShortcut(false);
            if (!cleanup.success) {
                qWarning() << "[HotkeyManager] Could not remove the legacy GNOME shortcut:"
                           << cleanup.error;
            } else {
                once.setValue(QStringLiteral("gnomeLegacyShortcutCleanupDone"), true);
                qInfo() << "[HotkeyManager] Removed the legacy GNOME shortcut after portal binding";
            }
        }
    }
}

void LinuxPortalGlobalShortcuts::updateAssignedTriggers(const QVariantMap &results)
{
    m_triggers.clear();
    const QVariant shortcuts = results.value(QStringLiteral("shortcuts"));
    if (!shortcuts.isValid())
        return;
    const QList<PortalShortcut> parsed = shortcuts.value<QList<PortalShortcut>>();
    for (const PortalShortcut &shortcut : parsed) {
        const QString trigger = shortcut.options.value(
            QStringLiteral("trigger_description")).toString();
        if (trigger.isEmpty())
            continue;
        m_triggers.insert(shortcut.id, trigger);
        qInfo() << "[HotkeyManager] Global shortcut" << shortcut.id
                << "assigned trigger:" << trigger;
    }
}

void LinuxPortalGlobalShortcuts::onActivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId,
                                             qulonglong timestamp, const QVariantMap &options)
{
    Q_UNUSED(timestamp);
    Q_UNUSED(options);
    if (!m_sessionHandle.isEmpty() && sessionHandle.path() != m_sessionHandle)
        return;
    if (!m_ids.contains(shortcutId))
        return;

    emit shortcutActivated(m_ids.value(shortcutId));
}
#endif
