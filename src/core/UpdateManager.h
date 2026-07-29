#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

class UpdateManager : public QObject {
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager() override;

    bool updateAvailable() const { return m_updateAvailable; }
    bool isBusy() const { return m_checking || m_downloading || m_installing; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString statusText() const { return m_statusText; }
    bool isSilentUpdate() const { return m_silentUpdate; }

    void checkForUpdates(bool manual = false);
    void installUpdate(bool silent = false);

signals:
    void statusChanged();
    void updateCheckFinished(bool available, const QString &version);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void installerLaunched();
    void failed(const QString &message);

private:
    void setStatus(const QString &status);
    void parseRelease(const QByteArray &data, bool manual);
    void downloadInstaller();
    void finishDownload();
    void launchInstaller(const QString &installerPath);
    void checkSilentUpdateEligibility();
    bool isSelfManagedInstall() const;
    QString updateCacheDir() const;

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_checkReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QNetworkReply *m_releaseListReply = nullptr;
    QFile *m_downloadFile = nullptr;

    bool m_checking = false;
    bool m_downloading = false;
    bool m_installing = false;
    bool m_installAfterCheck = false;
    bool m_silentUpdate = false;
    bool m_updateAvailable = false;
    QString m_latestVersion;
    QString m_releaseUrl;
    QString m_installerUrl;
    QString m_installerName;
    QString m_installerSha256;
    QString m_statusText;
    qint64 m_installerSize = 0;
};

#endif
