#include "LinuxKWinScript.h"

#include <QJsonArray>
#include <QJsonDocument>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusConnection>
#include <QDBusMessage>
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
    bus.send(scriptingCall(QStringLiteral("loadScript"), {path, plugin}));
    bus.send(scriptingCall(QStringLiteral("start"), {}));
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
