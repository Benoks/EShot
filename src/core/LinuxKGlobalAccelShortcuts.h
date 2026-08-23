#pragma once

#include <QObject>
#include <QHash>
#include <QPair>
#include <QStringList>

#include "PlatformHotkey.h"

class LinuxKGlobalAccelShortcuts : public QObject {
    Q_OBJECT
public:
    explicit LinuxKGlobalAccelShortcuts(QObject *parent = nullptr);

    bool isAvailable() const;
    bool setShortcuts(const QHash<int, QPair<UINT, UINT>> &shortcuts);
    void deRegisterShortcut(int id);
    static bool isKdeDesktop(const QString &desktop);
    static QStringList actionId(int id);
    static uint registrationFlags();
    static uint defaultRegistrationFlags();
    static bool shouldUsePortalFallback(bool kdeRegistrationSucceeded,
                                        bool portalAvailable);

signals:
    void shortcutActivated(int id);

private slots:
    void onGlobalShortcutPressed(const QString &componentUnique,
                                 const QString &shortcutUnique,
                                 qlonglong timestamp);

private:
    bool ensureSignalConnection();
    bool m_available = false;
    QString m_componentPath;
    QHash<QString, int> m_ids;
};
