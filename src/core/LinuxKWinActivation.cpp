#include "LinuxKWinActivation.h"

#include <QJsonArray>
#include <QJsonDocument>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#endif

namespace LinuxKWinActivation {

namespace {

QString jsString(const QString &value)
{
    // A one-element JSON array gives correctly escaped string syntax.
    const QByteArray json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
QDBusMessage callScripting(const QString &method, const QList<QVariant> &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
        QStringLiteral("org.kde.kwin.Scripting"), method);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, 1000);
}
#endif

}

QString activationScript(qint64 pid, const QString &caption)
{
    return QStringLiteral(
        "(function () {\n"
        "    var pid = %1;\n"
        "    var caption = %2;\n"
        "    var windows = typeof workspace.windowList === \"function\"\n"
        "        ? workspace.windowList() : workspace.clientList();\n"
        "    for (var i = 0; i < windows.length; ++i) {\n"
        "        var window = windows[i];\n"
        "        if (window.pid !== pid || String(window.caption).indexOf(caption) !== 0)\n"
        "            continue;\n"
        "        if (\"activeWindow\" in workspace)\n"
        "            workspace.activeWindow = window;\n"
        "        else\n"
        "            workspace.activeClient = window;\n"
        "        break;\n"
        "    }\n"
        "})();\n").arg(QString::number(pid), jsString(caption));
}

QString pluginName(qint64 pid)
{
    return QStringLiteral("eshot-activate-%1").arg(pid);
}

bool requestActivation(qint64 pid, const QString &caption)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!QDBusConnection::sessionBus().isConnected())
        return false;
    QString directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty())
        directory = QDir::tempPath();
    QTemporaryFile file(QDir(directory).filePath(QStringLiteral("eshot-kwin-XXXXXX.js")));
    file.setAutoRemove(false);
    if (!file.open()) {
        qWarning() << "[KWinActivation] could not write the script:" << file.errorString();
        return false;
    }
    file.write(activationScript(pid, caption).toUtf8());
    const QString path = file.fileName();
    file.close();

    const QString plugin = pluginName(pid);
    callScripting(QStringLiteral("unloadScript"), {plugin});
    const QDBusMessage loaded = callScripting(QStringLiteral("loadScript"), {path, plugin});
    bool ok = loaded.type() == QDBusMessage::ReplyMessage
        && !loaded.arguments().isEmpty() && loaded.arguments().constFirst().toInt() >= 0;
    if (ok) {
        const QDBusMessage started = callScripting(QStringLiteral("start"), {});
        ok = started.type() == QDBusMessage::ReplyMessage;
    }
    if (!ok) {
        qWarning() << "[KWinActivation] KWin refused the activation script:"
                   << loaded.errorName() << loaded.errorMessage();
    } else {
        qInfo() << "[KWinActivation] activation script started for" << caption;
    }

    // KWin reads the file asynchronously; clean up once it has run.
    QTimer::singleShot(5000, QCoreApplication::instance(), [path, plugin]() {
        QFile::remove(path);
        callScripting(QStringLiteral("unloadScript"), {plugin});
    });
    return ok;
#else
    Q_UNUSED(pid);
    Q_UNUSED(caption);
    return false;
#endif
}

}
