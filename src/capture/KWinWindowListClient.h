#pragma once

#include "KWinWindowList.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QTimer;

// Asks KWin for the window list without blocking the GUI thread. request()
// loads a one-shot KWin script that calls deliverWindowList() on this
// object over the session bus. Each request carries a random token, so a
// late reply to an earlier capture is ignored. Without a reply within the
// timeout, windowListUnavailable() is emitted and the capture keeps free
// selection only.
class KWinWindowListClient : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.benoks.EShot.KWinWindowList")

public:
    static QString objectPath();
    static QString interfaceName();

    explicit KWinWindowListClient(QObject *parent = nullptr);
    ~KWinWindowListClient() override;

    // Returns false when the request could not be sent (no session bus, the
    // object could not be registered or the script could not be written).
    bool request(int timeoutMs = 1000);
    void cancel();
    bool isPending() const { return !m_token.isEmpty(); }

public slots:
    // Called by the KWin script over D-Bus.
    Q_SCRIPTABLE void deliverWindowList(const QString &token, const QString &json);

signals:
    void windowListReceived(const KWinWindowSnapshot &snapshot);
    void windowListUnavailable();

private:
    bool ensureRegistered();
    void finishRequest();

    QTimer *m_timeout = nullptr;
    QElapsedTimer m_elapsed;
    int m_timeoutMs = 0;
    bool m_lateTimeoutRetried = false;
    QString m_token;
    QString m_scriptPath;
    bool m_registered = false;
};
