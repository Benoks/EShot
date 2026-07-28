#include "LinuxPortalScreenshot.h"
#include "LinuxPortalRequest.h"
#include "LinuxScreenshotPolicy.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QWidget>
#include <QWindow>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QUuid>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
QPixmap grabKWinScreenshot(const QString &method, QList<QVariant> arguments, int timeoutMs)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "[LinuxScreenshot]" << method << "failed: session bus is unavailable";
        return {};
    }

    int pipeFds[2] = {-1, -1};
    if (pipe2(pipeFds, O_CLOEXEC) != 0) {
        qWarning() << "[LinuxScreenshot]" << method << "pipe creation failed:"
                   << std::strerror(errno);
        return {};
    }

    QVariantMap options;
    for (const QVariant &argument : std::as_const(arguments)) {
        if (argument.metaType() == QMetaType::fromType<QVariantMap>()) {
            options = argument.toMap();
            break;
        }
    }
    const QVariant includeCursor = options.value(QStringLiteral("include-cursor"));
    const QVariant nativeResolution = options.value(QStringLiteral("native-resolution"));
    qInfo() << "[LinuxScreenshot] KWin request" << method
            << "include-cursor=" << includeCursor
            << "type=" << includeCursor.metaType().name()
            << "native-resolution=" << nativeResolution
            << "type=" << nativeResolution.metaType().name();

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin.ScreenShot2"),
        QStringLiteral("/org/kde/KWin/ScreenShot2"),
        QStringLiteral("org.kde.KWin.ScreenShot2"),
        method);
    arguments.append(QVariant::fromValue(QDBusUnixFileDescriptor(pipeFds[1])));
    message.setArguments(arguments);

    QDBusPendingCallWatcher watcher(bus.asyncCall(message, timeoutMs));
    close(pipeFds[1]);

    QEventLoop loop;
    QPixmap result;
    QObject::connect(&watcher, &QDBusPendingCallWatcher::finished, &loop, [&]() {
        QDBusPendingReply<QVariantMap> reply = watcher;
        if (reply.isError()) {
            qWarning() << "[LinuxScreenshot] KWin" << method << "failed:"
                       << reply.error().name() << reply.error().message();
            close(pipeFds[0]);
            loop.quit();
            return;
        }

        const QVariantMap metadata = reply.value();
        qInfo() << "[LinuxScreenshot] KWin" << method << "metadata:" << metadata;
        const int width = metadata.value(QStringLiteral("width")).toInt();
        const int height = metadata.value(QStringLiteral("height")).toInt();
        const int format = metadata.value(QStringLiteral("format")).toInt();
        const qreal scale = metadata.value(QStringLiteral("scale"), 1.0).toReal();
        if (metadata.value(QStringLiteral("type")).toString() != QStringLiteral("raw")
            || width <= 0 || height <= 0
            || format <= QImage::Format_Invalid || format >= QImage::NImageFormats) {
            qWarning() << "[LinuxScreenshot] KWin" << method
                       << "returned invalid screenshot metadata";
            close(pipeFds[0]);
            loop.quit();
            return;
        }

        QImage image(width, height, static_cast<QImage::Format>(format));
        image.setDevicePixelRatio(scale > 0.0 ? scale : 1.0);
        QFile file;
        if (!file.open(pipeFds[0], QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            qWarning() << "[LinuxScreenshot] KWin" << method
                       << "could not open the screenshot pipe:" << file.errorString();
            close(pipeFds[0]);
            loop.quit();
            return;
        }

        const qint64 bytesRead = file.read(reinterpret_cast<char *>(image.bits()), image.sizeInBytes());
        if (bytesRead != image.sizeInBytes()) {
            qWarning() << "[LinuxScreenshot] KWin" << method << "short image read:"
                       << bytesRead << "of" << image.sizeInBytes();
            loop.quit();
            return;
        }

        result = QPixmap::fromImage(image);
        qInfo() << "[LinuxScreenshot] selected backend=kwin method=" << method
                << "size=" << result.size() << "dpr=" << result.devicePixelRatio();
        loop.quit();
    });
    loop.exec();
    return result;
}
#endif

}

LinuxPortalScreenshot::LinuxPortalScreenshot(QObject *parent)
    : QObject(parent)
{
}

void LinuxPortalScreenshot::warmup()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    // Pinging the portal activates xdg-desktop-portal and its selected backend,
    // but unlike Screenshot it neither asks for nor writes any screen content.
    const QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.DBus.Peer"),
        QStringLiteral("Ping"));
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), QCoreApplication::instance());
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [](QDBusPendingCallWatcher *finishedWatcher) {
        QDBusReply<void> reply = *finishedWatcher;
        if (!reply.isValid())
            qInfo() << "[LinuxScreenshot] portal warmup skipped:" << reply.error().name();
        else
            qInfo() << "[LinuxScreenshot] portal warmup completed";
        finishedWatcher->deleteLater();
    });
#endif
}

QPixmap LinuxPortalScreenshot::grab(QWidget *parent, int timeoutMs)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    LinuxPortalScreenshot portal;
    return portal.waitForScreenshot(parent, timeoutMs);
#else
    Q_UNUSED(parent);
    Q_UNUSED(timeoutMs);
    return {};
#endif
}

QPixmap LinuxPortalScreenshot::grabScreen(QScreen *screen, QWidget *parent, int timeoutMs)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    Q_UNUSED(parent);
    if (!screen) {
        qWarning() << "[LinuxScreenshot] CaptureScreen failed: no screen";
        return {};
    }
    QVariantMap options;
    options.insert(QStringLiteral("native-resolution"), true);
    options.insert(QStringLiteral("include-cursor"), false);
    return grabKWinScreenshot(QStringLiteral("CaptureScreen"),
                              {QVariant(screen->name()), QVariant(options)}, timeoutMs);
#else
    Q_UNUSED(screen);
    Q_UNUSED(parent);
    Q_UNUSED(timeoutMs);
    return {};
#endif
}

QPixmap LinuxPortalScreenshot::grabWorkspace(QWidget *parent, int timeoutMs)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    Q_UNUSED(parent);
    QVariantMap options;
    options.insert(QStringLiteral("native-resolution"), true);
    options.insert(QStringLiteral("include-cursor"), false);
    return grabKWinScreenshot(QStringLiteral("CaptureWorkspace"), {QVariant(options)}, timeoutMs);
#else
    Q_UNUSED(parent);
    Q_UNUSED(timeoutMs);
    return {};
#endif
}

QPixmap LinuxPortalScreenshot::grabSpectacleWorkspace(QWidget *parent, int timeoutMs)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    Q_UNUSED(parent);
    const QString spectacle = QStandardPaths::findExecutable(QStringLiteral("spectacle"));
    if (spectacle.isEmpty()) {
        qWarning() << "[LinuxScreenshot] Spectacle fallback unavailable: executable not found";
        return {};
    }

    QTemporaryFile output(QDir(QDir::tempPath()).filePath(
        QStringLiteral("eshot-spectacle-XXXXXX.png")));
    output.setAutoRemove(true);
    if (!output.open()) {
        qWarning() << "[LinuxScreenshot] Spectacle fallback could not create output:"
                   << output.errorString();
        return {};
    }
    const QString outputPath = output.fileName();
    output.close();

    const QStringList arguments = LinuxScreenshotPolicy::spectacleWorkspaceArguments(outputPath);
    qInfo() << "[LinuxScreenshot] trying backend=spectacle program=" << spectacle
            << "arguments=" << arguments;
    QProcess process;
    process.setProgram(spectacle);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(qMin(timeoutMs, 5000))) {
        qWarning() << "[LinuxScreenshot] Spectacle fallback failed to start:"
                   << process.errorString();
        return {};
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished(1000);
        }
        qWarning() << "[LinuxScreenshot] Spectacle fallback timed out after" << timeoutMs << "ms";
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        qWarning() << "[LinuxScreenshot] Spectacle fallback failed: exitStatus="
                   << process.exitStatus() << "exitCode=" << process.exitCode()
                   << "output=" << QString::fromLocal8Bit(process.readAll()).trimmed();
        return {};
    }

    QPixmap result(outputPath);
    if (result.isNull()) {
        qWarning() << "[LinuxScreenshot] Spectacle fallback produced no readable image at"
                   << outputPath;
        return {};
    }
    result.setDevicePixelRatio(1.0);
    qInfo() << "[LinuxScreenshot] selected backend=spectacle size=" << result.size();
    return result;
#else
    Q_UNUSED(parent);
    Q_UNUSED(timeoutMs);
    return {};
#endif
}

QPixmap LinuxPortalScreenshot::waitForScreenshot(QWidget *parent, int timeoutMs)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "[LinuxScreenshot] portal failed: session bus is unavailable";
        return {};
    }

    QDBusInterface screenshot(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Screenshot"),
        bus);
    if (!screenshot.isValid()) {
        qWarning() << "[LinuxScreenshot] portal interface is unavailable";
        return {};
    }

    const QString token = QStringLiteral("eshot_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128));
    const uint portalVersion = screenshot.property("version").toUInt();
    const uint availableTargets = screenshot.property("AvailableTargets").toUInt();
    const QVariantMap options = LinuxScreenshotPolicy::portalScreenshotOptions(
        token, portalVersion, availableTargets);

    const bool isX11 = QGuiApplication::platformName().contains(
        QStringLiteral("xcb"), Qt::CaseInsensitive);
    const QString parentWindow = isX11 && parent && parent->windowHandle()
        ? QStringLiteral("x11:%1").arg(QString::number(parent->windowHandle()->winId(), 16))
        : QString();
    qInfo() << "[LinuxScreenshot] portal request parent=" << parentWindow
            << "version=" << portalVersion
            << "availableTargets=" << availableTargets
            << "options=" << options;

    m_finished = false;
    m_pixmap = QPixmap();
    const QString predictedPath = LinuxPortalRequest::requestPath(bus.baseService(), token);
    QStringList connectedPaths;
    auto connectResponse = [&](const QString &path) {
        if (path.isEmpty() || connectedPaths.contains(path))
            return false;
        const bool connected = bus.connect(
            QStringLiteral("org.freedesktop.portal.Desktop"), path,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(onPortalResponse(uint,QVariantMap)));
        if (connected)
            connectedPaths.append(path);
        return connected;
    };
    connectResponse(predictedPath);

    QDBusReply<QDBusObjectPath> reply = screenshot.call(
        QStringLiteral("Screenshot"), parentWindow, options);
    if (!reply.isValid()) {
        qWarning() << "[LinuxScreenshot] portal request failed:"
                   << reply.error().name() << reply.error().message();
        for (const QString &path : std::as_const(connectedPaths)) {
            bus.disconnect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                           QStringLiteral("org.freedesktop.portal.Request"),
                           QStringLiteral("Response"),
                           this, SLOT(onPortalResponse(uint,QVariantMap)));
        }
        return {};
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    m_loop = &loop;
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &LinuxPortalScreenshot::destroyed, &loop, &QEventLoop::quit);

    const QString requestPath = reply.value().path();
    connectResponse(requestPath);
    if (connectedPaths.isEmpty()) {
        qWarning() << "[LinuxScreenshot] portal response signal connection failed for"
                   << requestPath;
        m_loop = nullptr;
        return {};
    }

    timeout.start(timeoutMs);
    if (!m_finished)
        loop.exec();
    m_loop = nullptr;
    for (const QString &path : std::as_const(connectedPaths)) {
        bus.disconnect(QStringLiteral("org.freedesktop.portal.Desktop"), path,
                       QStringLiteral("org.freedesktop.portal.Request"),
                       QStringLiteral("Response"),
                       this, SLOT(onPortalResponse(uint,QVariantMap)));
    }
    if (!m_finished)
        qWarning() << "[LinuxScreenshot] portal response timed out after" << timeoutMs << "ms";
    return m_pixmap;
#else
    Q_UNUSED(parent);
    Q_UNUSED(timeoutMs);
    return {};
#endif
}

void LinuxPortalScreenshot::onPortalResponse(uint response, const QVariantMap &results)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    m_finished = true;
    qInfo() << "[LinuxScreenshot] portal response=" << response << "results=" << results;
    if (m_loop)
        m_loop->quit();
    if (response != 0)
        return;

    const QUrl uri = results.value(QStringLiteral("uri")).toUrl();
    if (!uri.isLocalFile()) {
        qWarning() << "[LinuxScreenshot] portal returned a non-local URI:" << uri;
        return;
    }

    const QString path = uri.toLocalFile();
    if (!QFile::exists(path)) {
        qWarning() << "[LinuxScreenshot] portal output does not exist:" << path;
        return;
    }

    QPixmap pixmap(path);
    if (pixmap.isNull()) {
        qWarning() << "[LinuxScreenshot] portal output is not a readable image:" << path;
        return;
    }
    pixmap.setDevicePixelRatio(1.0);
    m_pixmap = pixmap;
    qInfo() << "[LinuxScreenshot] selected backend=portal size=" << pixmap.size();
#else
    Q_UNUSED(response);
    Q_UNUSED(results);
#endif
}
