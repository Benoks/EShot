#include "LinuxKGlobalAccelShortcuts.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDebug>
#include <QKeyCombination>

#include <algorithm>

namespace {
constexpr auto Service = "org.kde.kglobalaccel";
constexpr auto RootPath = "/kglobalaccel";
constexpr auto RootInterface = "org.kde.KGlobalAccel";
constexpr auto Component = "io.github.benoks.EShot";

QString actionName(int id)
{
    switch (id) {
    case 1: return QStringLiteral("eshot_capture");
    case 2: return QStringLiteral("eshot_recording_pause");
    case 3: return QStringLiteral("eshot_recording_stop");
    case 4: return QStringLiteral("eshot_recording_cancel");
    case 5: return QStringLiteral("eshot_instant_capture");
    case 6: return QStringLiteral("eshot_gif_capture");
    case 7: return QStringLiteral("eshot_video_capture");
    case 8: return QStringLiteral("eshot_window_capture");
    case 9: return QStringLiteral("eshot_screen_capture");
    default: return QStringLiteral("eshot_%1").arg(id);
    }
}

QString actionDescription(int id)
{
    switch (id) {
    case 1: return QStringLiteral("Capture");
    case 2: return QStringLiteral("Pause recording");
    case 3: return QStringLiteral("Stop recording");
    case 4: return QStringLiteral("Cancel recording");
    case 5: return QStringLiteral("Instant capture");
    case 6: return QStringLiteral("GIF capture");
    case 7: return QStringLiteral("Video capture");
    case 8: return QStringLiteral("Capture window");
    case 9: return QStringLiteral("Capture screen now");
    default: return QStringLiteral("EShot shortcut");
    }
}

int qtKey(UINT virtualKey)
{
    if (virtualKey >= 'A' && virtualKey <= 'Z') return Qt::Key_A + int(virtualKey - 'A');
    if (virtualKey >= '0' && virtualKey <= '9') return Qt::Key_0 + int(virtualKey - '0');
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) return Qt::Key_F1 + int(virtualKey - VK_F1);
    switch (virtualKey) {
    case VK_SNAPSHOT: return Qt::Key_Print;
    case VK_SPACE: return Qt::Key_Space;
    case VK_RETURN: return Qt::Key_Return;
    case VK_ESCAPE: return Qt::Key_Escape;
    case VK_TAB: return Qt::Key_Tab;
    case VK_BACK: return Qt::Key_Backspace;
    case VK_INSERT: return Qt::Key_Insert;
    case VK_DELETE: return Qt::Key_Delete;
    case VK_HOME: return Qt::Key_Home;
    case VK_END: return Qt::Key_End;
    case VK_PRIOR: return Qt::Key_PageUp;
    case VK_NEXT: return Qt::Key_PageDown;
    case VK_LEFT: return Qt::Key_Left;
    case VK_RIGHT: return Qt::Key_Right;
    case VK_UP: return Qt::Key_Up;
    case VK_DOWN: return Qt::Key_Down;
    default: return 0;
    }
}

int combinedKey(UINT modifiers, UINT virtualKey)
{
    Qt::KeyboardModifiers qtModifiers = Qt::NoModifier;
    if (modifiers & MOD_SHIFT) qtModifiers |= Qt::ShiftModifier;
    if (modifiers & MOD_CONTROL) qtModifiers |= Qt::ControlModifier;
    if (modifiers & MOD_ALT) qtModifiers |= Qt::AltModifier;
    if (modifiers & MOD_WIN) qtModifiers |= Qt::MetaModifier;
    const int key = qtKey(virtualKey);
    return key == 0 ? 0 : QKeyCombination(qtModifiers, Qt::Key(key)).toCombined();
}
}

LinuxKGlobalAccelShortcuts::LinuxKGlobalAccelShortcuts(QObject *parent) : QObject(parent)
{
    QDBusInterface accel(QString::fromLatin1(Service), QString::fromLatin1(RootPath),
                         QString::fromLatin1(RootInterface), QDBusConnection::sessionBus());
    m_available = accel.isValid();
}

bool LinuxKGlobalAccelShortcuts::isAvailable() const { return m_available; }

bool LinuxKGlobalAccelShortcuts::isKdeDesktop(const QString &desktop)
{
    return desktop.contains(QStringLiteral("KDE"), Qt::CaseInsensitive)
        || desktop.contains(QStringLiteral("Plasma"), Qt::CaseInsensitive);
}

QStringList LinuxKGlobalAccelShortcuts::actionId(int id)
{
    return {QString::fromLatin1(Component), actionName(id), QStringLiteral("EShot"), actionDescription(id)};
}

uint LinuxKGlobalAccelShortcuts::registrationFlags()
{
    constexpr uint SetPresent = 2;
    constexpr uint NoAutoloading = 4;
    return SetPresent | NoAutoloading;
}

uint LinuxKGlobalAccelShortcuts::defaultRegistrationFlags()
{
    constexpr uint NoAutoloading = 4;
    constexpr uint IsDefault = 8;
    return NoAutoloading | IsDefault;
}

bool LinuxKGlobalAccelShortcuts::shouldUsePortalFallback(bool kdeRegistrationSucceeded,
                                                          bool portalAvailable)
{
    return !kdeRegistrationSucceeded && portalAvailable;
}

LinuxKGlobalAccelShortcuts::ShortcutChanges LinuxKGlobalAccelShortcuts::planChanges(
    const QHash<int, QPair<UINT, UINT>> &current,
    const QHash<int, QPair<UINT, UINT>> &desired)
{
    ShortcutChanges changes;
    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        if (!desired.contains(it.key()) || desired.value(it.key()).second == 0)
            changes.remove.append(it.key());
    }
    for (auto it = desired.cbegin(); it != desired.cend(); ++it) {
        if (it.value().second == 0)
            continue;
        if (!current.contains(it.key()) || current.value(it.key()) != it.value())
            changes.assign.append(it.key());
    }
    std::sort(changes.remove.begin(), changes.remove.end());
    std::sort(changes.assign.begin(), changes.assign.end());
    return changes;
}

bool LinuxKGlobalAccelShortcuts::setShortcuts(const QHash<int, QPair<UINT, UINT>> &shortcuts)
{
    if (!m_available) return false;
    const ShortcutChanges changes = planChanges(m_applied, shortcuts);
    for (int id : changes.remove)
        unregisterShortcut(id);
    bool ok = true;
    for (int id : changes.assign) {
        if (!assignShortcut(id, shortcuts.value(id))) {
            ok = false;
            break;
        }
    }
    if (!ok)
        return false;
    // The component exists only after its first action was registered.
    if (m_applied.isEmpty())
        return true;
    return ensureSignalConnection();
}

bool LinuxKGlobalAccelShortcuts::assignShortcut(int shortcutId, const QPair<UINT, UINT> &definition)
{
    QDBusInterface accel(QString::fromLatin1(Service), QString::fromLatin1(RootPath),
                         QString::fromLatin1(RootInterface), QDBusConnection::sessionBus());
    const QStringList id = actionId(shortcutId);
    m_ids.remove(id.at(1));
    const QDBusReply<void> registered = accel.call(QStringLiteral("doRegister"), id);
    if (!registered.isValid()) {
        qWarning() << "[HotkeyManager] KGlobalAccel doRegister failed:"
                   << registered.error().name() << registered.error().message();
        return false;
    }
    // From here on kglobalaccel knows the action; remember it so a later
    // call can release it even when the key below is refused.
    m_applied.insert(shortcutId, qMakePair(UINT(0), UINT(0)));
    const int key = combinedKey(definition.first, definition.second);
    if (key == 0) return false;
    const QVariant keys = QVariant::fromValue(QList<int>{key});
    const QDBusReply<QList<int>> defaults = accel.call(
        QStringLiteral("setShortcut"), id, keys, defaultRegistrationFlags());
    if (!defaults.isValid()) {
        qWarning() << "[HotkeyManager] KGlobalAccel default shortcut registration failed:"
                   << defaults.error().name() << defaults.error().message();
        return false;
    }
    const QDBusReply<QList<int>> assigned = accel.call(
        QStringLiteral("setShortcut"), id, keys, registrationFlags());
    if (!assigned.isValid()) {
        qWarning() << "[HotkeyManager] KGlobalAccel shortcut registration failed:"
                   << assigned.error().name() << assigned.error().message();
        return false;
    }
    if (!assigned.value().contains(key)) {
        qWarning() << "[HotkeyManager] KGlobalAccel rejected shortcut because it is unavailable:"
                   << id << "requested=" << key << "assigned=" << assigned.value();
        return false;
    }
    m_applied.insert(shortcutId, definition);
    m_ids.insert(id.at(1), shortcutId);
    return true;
}

void LinuxKGlobalAccelShortcuts::unregisterShortcut(int id)
{
    const QStringList shortcutId = actionId(id);
    m_ids.remove(shortcutId.at(1));
    m_applied.remove(id);
    if (!m_available)
        return;
    QDBusInterface accel(QString::fromLatin1(Service), QString::fromLatin1(RootPath),
                         QString::fromLatin1(RootInterface), QDBusConnection::sessionBus());
    // org.kde.KGlobalAccel.unregister(componentUnique, shortcutUnique) drops
    // the action and ungrabs its key (there is no deRegisterShortcut method).
    const QDBusReply<bool> removed = accel.call(QStringLiteral("unregister"),
                                                shortcutId.at(0), shortcutId.at(1));
    if (!removed.isValid()) {
        qWarning() << "[HotkeyManager] KGlobalAccel unregister failed:"
                   << removed.error().name() << removed.error().message();
    }
}

bool LinuxKGlobalAccelShortcuts::ensureSignalConnection()
{
    if (!m_componentPath.isEmpty())
        return true;
    QDBusInterface accel(QString::fromLatin1(Service), QString::fromLatin1(RootPath),
                         QString::fromLatin1(RootInterface), QDBusConnection::sessionBus());
    const QDBusReply<QDBusObjectPath> component = accel.call(QStringLiteral("getComponent"),
                                                              QString::fromLatin1(Component));
    if (!component.isValid() || component.value().path().isEmpty()) {
        qWarning() << "[HotkeyManager] KGlobalAccel component lookup failed:"
                   << component.error().name() << component.error().message();
        return false;
    }
    const QString componentPath = component.value().path();
    const bool connected = QDBusConnection::sessionBus().connect(
        QString::fromLatin1(Service), componentPath,
        QStringLiteral("org.kde.kglobalaccel.Component"),
        QStringLiteral("globalShortcutPressed"), this,
        SLOT(onGlobalShortcutPressed(QString,QString,qlonglong)));
    if (!connected) {
        qWarning() << "[HotkeyManager] Could not subscribe to KGlobalAccel activation signals.";
        return false;
    }
    m_componentPath = componentPath;
    return true;
}

void LinuxKGlobalAccelShortcuts::onGlobalShortcutPressed(const QString &componentUnique,
                                                          const QString &shortcutUnique,
                                                          qlonglong timestamp)
{
    Q_UNUSED(timestamp);
    if (componentUnique == QString::fromLatin1(Component) && m_ids.contains(shortcutUnique))
        emit shortcutActivated(m_ids.value(shortcutUnique));
}
