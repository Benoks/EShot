#ifndef LINUXPORTALSCREENCAST_H
#define LINUXPORTALSCREENCAST_H

#include <QObject>
#include <QPoint>
#include <QSharedPointer>
#include <QSize>
#include <QVariantMap>
#include <QList>

#include <functional>

class QWidget;
class QEventLoop;

class LinuxPortalScreenCast : public QObject {
    Q_OBJECT

public:
    struct Stream {
        uint nodeId = 0;
        quint64 pipewireSerial = 0;
        QSharedPointer<int> pipewireFd;
        QPoint position;
        QSize size;
        QVariantMap properties;
        QString sessionHandle;
        bool usedRestoreToken = false;
        // The user closed the source picker (portal response 1); not an error.
        bool canceled = false;
        bool isValid() const { return nodeId != 0 || pipewireSerial != 0; }
        int remoteFd() const { return pipewireFd ? *pipewireFd : -1; }
    };

    explicit LinuxPortalScreenCast(QObject *parent = nullptr);

    static bool isWaylandSession();
    static bool isWaylandSessionType(const QString &sessionType, const QString &platformName);
    static bool isAvailable();
    static QVariantMap sourceOptions(uint portalVersion,
                                     uint availableCursorModes,
                                     const QString &restoreToken);
    static Stream selectStream(QWidget *parent = nullptr, int timeoutMs = 120000,
                               const QString &persistenceId = QString());
    static void closeSession(const QString &sessionHandle);
    static void clearRestoreToken(const QString &persistenceId);
    // A fresh PipeWire connection to an already started session, e.g. for
    // the encoder that continues a paused recording.
    static QSharedPointer<int> openPipeWireRemote(const QString &sessionHandle);
    // Same without blocking the caller; `done` runs on `context`'s thread
    // with a null pointer on failure, and not at all once context is gone.
    static void openPipeWireRemoteAsync(const QString &sessionHandle, QObject *context,
                                        const std::function<void(QSharedPointer<int>)> &done);

private slots:
    void onPortalResponse(uint response, const QVariantMap &results);

private:
    Stream select(QWidget *parent, int timeoutMs, const QString &persistenceId);
    bool callAndWait(class QDBusInterface &portal,
                     const QString &method,
                     const QList<QVariant> &arguments,
                     const QString &handleToken,
                     int timeoutMs);
    QString objectPathString(const QVariant &value) const;
    Stream firstStreamFromResults(const QVariantMap &results) const;

    uint m_response = 2;
    QVariantMap m_results;
    bool m_finished = false;
    bool m_selectInProgress = false;
    QEventLoop *m_loop = nullptr;
};

// Emits closed() when the portal ends a screen cast session on its own, e.g.
// when the user stops sharing from the desktop's sharing indicator.
class LinuxPortalSessionWatcher : public QObject {
    Q_OBJECT

public:
    explicit LinuxPortalSessionWatcher(const QString &sessionHandle, QObject *parent = nullptr);
    ~LinuxPortalSessionWatcher() override;

signals:
    void closed();

private slots:
    void onSessionClosed(const QVariantMap &details);

private:
    QString m_sessionHandle;
    bool m_connected = false;
};

#endif
