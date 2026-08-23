#ifndef LINUXDESKTOPNOTIFICATION_H
#define LINUXDESKTOPNOTIFICATION_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class LinuxDesktopNotification : public QObject
{
    Q_OBJECT

public:
    explicit LinuxDesktopNotification(QObject *parent = nullptr);

    bool show(const QString &title, const QString &body, const QString &path,
              const QString &actionLabel, int timeoutMs);

    static QStringList actions(const QString &actionLabel);
    static QVariantMap hintsForPath(const QString &path);

signals:
    void pathActivated(const QString &path);

private slots:
    void onActionInvoked(uint id, const QString &actionKey);
    void onNotificationClosed(uint id, uint reason);

private:
    QHash<uint, QString> m_paths;
    uint m_lastId = 0;
};

#endif
