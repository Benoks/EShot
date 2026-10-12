#include "RecordingSavedNotification.h"

#include "../core/TranslationManager.h"

#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

QString formatRecordingFileSize(qint64 bytes, const QLocale &locale)
{
    constexpr double Kilobyte = 1024.0;
    constexpr double Megabyte = Kilobyte * 1024.0;
    constexpr double Gigabyte = Megabyte * 1024.0;
    const double value = static_cast<double>(qMax<qint64>(0, bytes));
    if (value >= Gigabyte)
        return QStringLiteral("%1 GB").arg(locale.toString(value / Gigabyte, 'f', 1));
    if (value >= Megabyte)
        return QStringLiteral("%1 MB").arg(locale.toString(value / Megabyte, 'f', 1));
    return QStringLiteral("%1 KB").arg(locale.toString(qMax(1.0, value / Kilobyte), 'f', 0));
}

QString recordingSavedNotificationText(RecordingSavedKind kind, const QString &filePath,
                                       qint64 bytes, const QLocale &locale)
{
    const QString headline = TranslationManager::tr(kind == RecordingSavedKind::Gif
                                                        ? "recGifSavedSize"
                                                        : "recVideoSavedSize")
                                 .arg(formatRecordingFileSize(bytes, locale));
    const QString fileName = QFileInfo(filePath).fileName();
    return fileName.isEmpty() ? headline : headline + QLatin1Char('\n') + fileName;
}

QMimeData *recordingFileMimeData(const QString &filePath)
{
    auto *data = new QMimeData();
    const QUrl url = QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath());
    // setUrls() provides text/uri-list (and CF_HDROP on Windows).
    data->setUrls({url});
    data->setData(QStringLiteral("x-special/gnome-copied-files"),
                  QByteArray("copy\n") + url.toEncoded());
    return data;
}
