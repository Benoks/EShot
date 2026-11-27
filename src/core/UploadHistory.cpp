#include "UploadHistory.h"
#include "ImageUploader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

UploadHistory::UploadHistory(const QString &filePath) : m_filePath(filePath)
{
    load();
}

QString UploadHistory::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/upload-history.json");
}

bool UploadHistory::add(const Entry &entry)
{
    m_entries.prepend(entry);
    while (m_entries.size() > MaxEntries)
        m_entries.removeLast();
    return save();
}

bool UploadHistory::remove(int index)
{
    if (index < 0 || index >= m_entries.size()) return false;
    m_entries.removeAt(index);
    return save();
}

bool UploadHistory::clear()
{
    m_entries.clear();
    return save();
}

qint64 UploadHistory::lifetimeSeconds(int provider)
{
    switch (static_cast<ImageUploader::Provider>(provider)) {
    case ImageUploader::Provider::Uguu:      return 3 * 3600;
    case ImageUploader::Provider::TmpFiles:  return 3600;
    case ImageUploader::Provider::Litterbox: return 24 * 3600;
    case ImageUploader::Provider::TempSh:    return 3 * 24 * 3600;
    default:                                 return 0;
    }
}

bool UploadHistory::likelyExpired(const Entry &entry, const QDateTime &now)
{
    const qint64 lifetime = lifetimeSeconds(entry.provider);
    return lifetime > 0 && entry.time.secsTo(now) > lifetime;
}

void UploadHistory::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonArray items = QJsonDocument::fromJson(file.readAll()).object()
                                 .value(QStringLiteral("entries")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject o = value.toObject();
        Entry e;
        e.time = QDateTime::fromString(o.value(QStringLiteral("time")).toString(), Qt::ISODate);
        e.provider = o.value(QStringLiteral("provider")).toInt(-1);
        e.providerName = o.value(QStringLiteral("providerName")).toString();
        e.url = o.value(QStringLiteral("url")).toString();
        e.deleteUrl = o.value(QStringLiteral("deleteUrl")).toString();
        e.imageSize = QSize(o.value(QStringLiteral("width")).toInt(),
                            o.value(QStringLiteral("height")).toInt());
        e.thumbnailPng = QByteArray::fromBase64(
            o.value(QStringLiteral("thumbnail")).toString().toLatin1());
        if (!e.url.isEmpty() && m_entries.size() < MaxEntries)
            m_entries.append(e);
    }
}

bool UploadHistory::save() const
{
    QJsonArray items;
    for (const Entry &e : m_entries) {
        QJsonObject o;
        o.insert(QStringLiteral("time"), e.time.toUTC().toString(Qt::ISODate));
        o.insert(QStringLiteral("provider"), e.provider);
        o.insert(QStringLiteral("providerName"), e.providerName);
        o.insert(QStringLiteral("url"), e.url);
        if (!e.deleteUrl.isEmpty())
            o.insert(QStringLiteral("deleteUrl"), e.deleteUrl);
        o.insert(QStringLiteral("width"), e.imageSize.width());
        o.insert(QStringLiteral("height"), e.imageSize.height());
        if (!e.thumbnailPng.isEmpty())
            o.insert(QStringLiteral("thumbnail"), QString::fromLatin1(e.thumbnailPng.toBase64()));
        items.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("entries"), items);

    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}
