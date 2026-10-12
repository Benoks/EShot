#ifndef RECORDINGSAVEDNOTIFICATION_H
#define RECORDINGSAVEDNOTIFICATION_H

#include <QLocale>
#include <QString>

class QMimeData;

enum class RecordingSavedKind {
    Gif,
    Video
};

// "850 KB", "12.3 MB", "1.2 GB" with the locale's decimal separator.
QString formatRecordingFileSize(qint64 bytes, const QLocale &locale = QLocale::c());

// "<GIF|Video> saved · <size>\n<file name>". The full path is left out on
// purpose; it is logged and shown in the tray's "Last recording" menu.
QString recordingSavedNotificationText(RecordingSavedKind kind, const QString &filePath,
                                       qint64 bytes, const QLocale &locale = QLocale::c());

// Clipboard contents that paste the file itself into file managers, chats
// and mail clients (text/uri-list plus the GNOME copied-files format).
QMimeData *recordingFileMimeData(const QString &filePath);

#endif
