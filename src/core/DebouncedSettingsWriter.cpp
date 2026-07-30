#include "DebouncedSettingsWriter.h"

#include <QSettings>

DebouncedSettingsWriter::DebouncedSettingsWriter(const QString &organization,
                                                 const QString &application,
                                                 int delayMs,
                                                 QObject *parent)
    : QObject(parent)
    , m_organization(organization)
    , m_application(application)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(qMax(0, delayMs));
    connect(&m_timer, &QTimer::timeout, this, &DebouncedSettingsWriter::flush);
}

DebouncedSettingsWriter::~DebouncedSettingsWriter()
{
    flush();
}

void DebouncedSettingsWriter::schedule(const QString &key, const QVariant &value)
{
    if (key.isEmpty())
        return;
    m_pendingValues.insert(key, value);
    m_timer.start();
}

void DebouncedSettingsWriter::flush()
{
    if (m_pendingValues.isEmpty())
        return;

    m_timer.stop();
    QSettings settings(m_organization, m_application);
    const int keyCount = m_pendingValues.size();
    for (auto it = m_pendingValues.cbegin(); it != m_pendingValues.cend(); ++it)
        settings.setValue(it.key(), it.value());
    m_pendingValues.clear();
    emit flushed(keyCount);
}
