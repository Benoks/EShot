#ifndef LINUXDESKTOPNOTIFICATION_H
#define LINUXDESKTOPNOTIFICATION_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class LinuxDesktopNotification : public QObject
{
    Q_OBJECT

public:
    struct Action {
        QString key;
        QString label;
    };

    explicit LinuxDesktopNotification(QObject *parent = nullptr);

    bool show(const QString &title, const QString &body, const QString &path,
              const QString &actionLabel, int timeoutMs);
    // Notification with several buttons. Clicking the body ("default") still
    // emits pathActivated(); a button emits actionInvoked(path, key).
    bool showWithActions(const QString &title, const QString &body, const QString &path,
                         const QList<Action> &actions, int timeoutMs);

    static QStringList actions(const QString &actionLabel);
    // Flattened key/label pairs for the Notify call, without "default".
    static QStringList actions(const QList<Action> &actions);
    static QVariantMap hintsForPath(const QString &path);
    // Arguments of the org.freedesktop.Notifications.Notify call. Every
    // notification is a new one (replaces_id 0): Plasma replaces an existing
    // notification "atomically with no visual cues", so reusing the id of an
    // earlier, already expired notification only updated that history entry
    // and the new message never appeared as a popup.
    static QVariantList notifyArguments(const QString &title, const QString &body,
                                        const QString &path, const QStringList &actionList,
                                        int timeoutMs);

signals:
    void pathActivated(const QString &path);
    void actionInvoked(const QString &path, const QString &actionKey);

private slots:
    void onActionInvoked(uint id, const QString &actionKey);
    void onNotificationClosed(uint id, uint reason);

private:
    bool notify(const QString &title, const QString &body, const QString &path,
                QStringList actionList, int timeoutMs);

    QHash<uint, QString> m_paths;
};

#endif
