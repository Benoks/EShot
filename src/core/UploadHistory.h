#ifndef UPLOADHISTORY_H
#define UPLOADHISTORY_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QSize>
#include <QString>

// Local list of successful uploads, newest first, kept in a JSON file.
class UploadHistory {
public:
    struct Entry {
        QDateTime time;
        int provider = -1; // ImageUploader::Provider value
        QString providerName;
        QString url;
        QString deleteUrl;
        QSize imageSize;
        QByteArray thumbnailPng; // small preview, may be empty
    };

    static constexpr int MaxEntries = 50;

    explicit UploadHistory(const QString &filePath = defaultFilePath());

    // upload-history.json in the app data folder, never the screenshots folder.
    static QString defaultFilePath();

    const QList<Entry> &entries() const { return m_entries; }

    // Each change is written to disk at once; false when that write failed.
    bool add(const Entry &entry);
    bool remove(int index);
    bool clear();

    // How long the provider keeps files, in seconds; 0 when it has no limit.
    static qint64 lifetimeSeconds(int provider);
    static bool likelyExpired(const Entry &entry, const QDateTime &now);

private:
    void load();
    bool save() const;

    QString m_filePath;
    QList<Entry> m_entries;
};

#endif
