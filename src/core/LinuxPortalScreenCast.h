#ifndef LINUXPORTALSCREENCAST_H
#define LINUXPORTALSCREENCAST_H

#include <QObject>
#include <QPoint>
#include <QSharedPointer>
#include <QSize>
#include <QVariantMap>
#include <QList>

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

private slots:
    void onPortalResponse(uint response, const QVariantMap &results);

private:
    Stream select(QWidget *parent, int timeoutMs, const QString &persistenceId);
    bool callAndWait(class QDBusInterface &portal,
                     const QString &method,
                     const QList<QVariant> &arguments,
                     const QString &handleToken,
                     int timeoutMs);
    QSharedPointer<int> openPipeWireRemote(const QString &sessionHandle) const;
    QString objectPathString(const QVariant &value) const;
    Stream firstStreamFromResults(const QVariantMap &results) const;

    uint m_response = 2;
    QVariantMap m_results;
    bool m_finished = false;
    bool m_selectInProgress = false;
    QEventLoop *m_loop = nullptr;
};

#endif
