#include "KWinWindowListClient.h"

#include "core/LinuxKWinScript.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QUuid>

QString KWinWindowListClient::objectPath()
{
    return QStringLiteral("/io/github/benoks/EShot/KWinWindowList");
}

QString KWinWindowListClient::interfaceName()
{
    return QStringLiteral("io.github.benoks.EShot.KWinWindowList");
}

KWinWindowListClient::KWinWindowListClient(QObject *parent)
    : QObject(parent)
{
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this]() {
        if (m_token.isEmpty())
            return;
        // A screenshot that blocked the GUI thread past the deadline makes
        // the timer fire late; give a reply already queued on the bus one
        // more pass of the event loop before giving up.
        if (!m_lateTimeoutRetried && m_elapsed.elapsed() > m_timeoutMs + 100) {
            m_lateTimeoutRetried = true;
            m_timeout->start(50);
            return;
        }
        qWarning() << "[KWinWindowList] no window list from KWin within"
                   << m_elapsed.elapsed() << "ms; window selection is unavailable for this capture";
        finishRequest();
        emit windowListUnavailable();
    });
}

KWinWindowListClient::~KWinWindowListClient()
{
    cancel();
    if (m_registered)
        QDBusConnection::sessionBus().unregisterObject(objectPath());
}

bool KWinWindowListClient::ensureRegistered()
{
    if (m_registered)
        return true;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;
    m_registered = bus.registerObject(objectPath(), this,
                                      QDBusConnection::ExportScriptableSlots);
    if (!m_registered)
        qWarning() << "[KWinWindowList] could not register" << objectPath() << "on the session bus";
    return m_registered;
}

bool KWinWindowListClient::request(int timeoutMs)
{
    cancel();
    if (!ensureRegistered())
        return false;

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString text = KWinWindowList::script(
        QDBusConnection::sessionBus().baseService(), objectPath(), interfaceName(),
        QStringLiteral("deliverWindowList"), token);
    const QString path = LinuxKWinScript::writeScriptFile(text);
    if (path.isEmpty())
        return false;
    const QString plugin = KWinWindowList::pluginName(QCoreApplication::applicationPid());
    if (!LinuxKWinScript::loadAndStartAsync(path, plugin)) {
        QFile::remove(path);
        return false;
    }
    m_token = token;
    m_scriptPath = path;
    m_elapsed.start();
    m_timeoutMs = timeoutMs;
    m_lateTimeoutRetried = false;
    m_timeout->start(timeoutMs);
    return true;
}

void KWinWindowListClient::cancel()
{
    if (m_token.isEmpty())
        return;
    finishRequest();
}

void KWinWindowListClient::finishRequest()
{
    m_timeout->stop();
    m_token.clear();
    LinuxKWinScript::unloadAsync(KWinWindowList::pluginName(QCoreApplication::applicationPid()));
    if (!m_scriptPath.isEmpty()) {
        QFile::remove(m_scriptPath);
        m_scriptPath.clear();
    }
}

void KWinWindowListClient::deliverWindowList(const QString &token, const QString &json)
{
    if (m_token.isEmpty() || token != m_token) {
        qInfo() << "[KWinWindowList] ignoring a stale window list reply";
        return;
    }
    const qint64 elapsed = m_elapsed.elapsed();
    finishRequest();
    const KWinWindowSnapshot snapshot = KWinWindowList::parse(json.toUtf8());
    if (!snapshot.valid) {
        qWarning() << "[KWinWindowList] KWin sent an unreadable window list";
        emit windowListUnavailable();
        return;
    }
    if (!snapshot.error.isEmpty())
        qWarning() << "[KWinWindowList] KWin script error:" << snapshot.error;
    qInfo() << "[KWinWindowList] received" << snapshot.windows.size() << "windows and"
            << snapshot.outputs.size() << "outputs in" << elapsed << "ms";
    emit windowListReceived(snapshot);
}
