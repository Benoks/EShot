#include "LinuxKWinScript.h"

#include <QJsonArray>
#include <QJsonDocument>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryFile>
#endif

namespace LinuxKWinScript {

QString jsString(const QString &value)
{
    // A one-element JSON array gives correctly escaped string syntax.
    const QByteArray json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
namespace {

QDBusMessage scriptingCall(const QString &method, const QList<QVariant> &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
        QStringLiteral("org.kde.kwin.Scripting"), method);
    message.setArguments(arguments);
    return message;
}

}
#endif

QString writeScriptFile(const QString &text)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QString directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty())
        directory = QDir::tempPath();
    QTemporaryFile file(QDir(directory).filePath(QStringLiteral("eshot-kwin-XXXXXX.js")));
    file.setAutoRemove(false);
    if (!file.open()) {
        qWarning() << "[KWinScript] could not write the script:" << file.errorString();
        return {};
    }
    file.write(text.toUtf8());
    const QString path = file.fileName();
    file.close();
    return path;
#else
    Q_UNUSED(text);
    return {};
#endif
}

bool loadAndStartAsync(const QString &path, const QString &plugin)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || path.isEmpty())
        return false;
    bus.send(scriptingCall(QStringLiteral("unloadScript"), {plugin}));
    // Run only our script: Scripting.start() would also start any other
    // loaded-but-idle KWin script of the user. loadScript returns its id.
    auto *watcher = new QDBusPendingCallWatcher(
        bus.asyncCall(scriptingCall(QStringLiteral("loadScript"), {path, plugin})));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<int> reply = *call;
        QDBusConnection connection = QDBusConnection::sessionBus();
        if (reply.isError() || reply.value() < 0) {
            connection.send(scriptingCall(QStringLiteral("start"), {}));
            return;
        }
        const int id = reply.value();
        // KWin 6 exports /Scripting/Script<id>; KWin 5 used /<id>.
        auto *runWatcher = new QDBusPendingCallWatcher(connection.asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                           QStringLiteral("/Scripting/Script%1").arg(id),
                                           QStringLiteral("org.kde.kwin.Script"),
                                           QStringLiteral("run"))));
        QObject::connect(runWatcher, &QDBusPendingCallWatcher::finished, runWatcher,
                         [id](QDBusPendingCallWatcher *run) {
            run->deleteLater();
            if (!run->isError())
                return;
            QDBusConnection fallback = QDBusConnection::sessionBus();
            fallback.send(QDBusMessage::createMethodCall(
                QStringLiteral("org.kde.KWin"), QStringLiteral("/%1").arg(id),
                QStringLiteral("org.kde.kwin.Script"), QStringLiteral("run")));
        });
    });
    return true;
#else
    Q_UNUSED(path);
    Q_UNUSED(plugin);
    return false;
#endif
}

void unloadAsync(const QString &plugin)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected())
        bus.send(scriptingCall(QStringLiteral("unloadScript"), {plugin}));
#else
    Q_UNUSED(plugin);
#endif
}

}
