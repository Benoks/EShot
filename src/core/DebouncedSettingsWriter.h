#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>

class DebouncedSettingsWriter final : public QObject
{
    Q_OBJECT

public:
    explicit DebouncedSettingsWriter(const QString &organization,
                                     const QString &application,
                                     int delayMs = 200,
                                     QObject *parent = nullptr);
    ~DebouncedSettingsWriter() override;

    void schedule(const QString &key, const QVariant &value);
    void flush();

signals:
    void flushed(int keyCount);

private:
    QString m_organization;
    QString m_application;
    QHash<QString, QVariant> m_pendingValues;
    QTimer m_timer;
};
