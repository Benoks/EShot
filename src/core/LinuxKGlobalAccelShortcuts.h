#pragma once

#include <QObject>
#include <QHash>
#include <QPair>
#include <QList>
#include <QStringList>

#include "PlatformHotkey.h"

class LinuxKGlobalAccelShortcuts : public QObject {
    Q_OBJECT
public:
    explicit LinuxKGlobalAccelShortcuts(QObject *parent = nullptr);

    bool isAvailable() const;
    // Makes kglobalaccel hold exactly `shortcuts`. Only ids whose key
    // changed are re-registered and ids that disappeared are unregistered, so
    // unchanged shortcuts cost no D-Bus round trips.
    bool setShortcuts(const QHash<int, QPair<UINT, UINT>> &shortcuts);
    // Removes one action from kglobalaccel so KDE releases its key.
    void unregisterShortcut(int id);

    struct ShortcutChanges {
        QList<int> remove;  // registered ids that must be released
        QList<int> assign;  // ids that need doRegister + setShortcut
    };
    // Pure diff between what kglobalaccel holds and what is wanted. Entries
    // with a zero virtual key count as absent. Both lists are sorted.
    static ShortcutChanges planChanges(const QHash<int, QPair<UINT, UINT>> &current,
                                       const QHash<int, QPair<UINT, UINT>> &desired);
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
    bool assignShortcut(int id, const QPair<UINT, UINT> &definition);
    QHash<QString, int> m_ids;
    // What kglobalaccel currently holds for EShot, as last applied. A failed
    // assignment is kept with a {0, 0} key so the next call retries or
    // removes it.
    QHash<int, QPair<UINT, UINT>> m_applied;
};
