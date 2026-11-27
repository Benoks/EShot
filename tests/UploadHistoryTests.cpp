#include <QtTest/QTest>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/ImageUploader.h"
#include "core/UploadHistory.h"

class UploadHistoryTests : public QObject
{
    Q_OBJECT

private:
    static UploadHistory::Entry entry(const QString &url)
    {
        UploadHistory::Entry e;
        e.time = QDateTime::currentDateTimeUtc();
        e.provider = static_cast<int>(ImageUploader::Provider::Catbox);
        e.providerName = QStringLiteral("Catbox.moe");
        e.url = url;
        return e;
    }

private slots:
    void addsNewestFirst()
    {
        QTemporaryDir dir;
        UploadHistory history(dir.filePath(QStringLiteral("history.json")));
        QVERIFY(history.entries().isEmpty());
        QVERIFY(history.add(entry(QStringLiteral("https://a"))));
        QVERIFY(history.add(entry(QStringLiteral("https://b"))));
        QCOMPARE(history.entries().size(), 2);
        QCOMPARE(history.entries().first().url, QStringLiteral("https://b"));
    }

    void capsAtMaxEntries()
    {
        QTemporaryDir dir;
        UploadHistory history(dir.filePath(QStringLiteral("history.json")));
        for (int i = 0; i < UploadHistory::MaxEntries + 5; ++i)
            history.add(entry(QStringLiteral("https://%1").arg(i)));
        QCOMPARE(history.entries().size(), UploadHistory::MaxEntries);
        QCOMPARE(history.entries().first().url,
                 QStringLiteral("https://%1").arg(UploadHistory::MaxEntries + 4));
        QCOMPARE(history.entries().last().url, QStringLiteral("https://5"));
    }

    void removesAndClears()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("history.json"));
        UploadHistory history(path);
        history.add(entry(QStringLiteral("https://a")));
        history.add(entry(QStringLiteral("https://b")));
        QVERIFY(!history.remove(5));
        QVERIFY(history.remove(0));
        QCOMPARE(history.entries().size(), 1);
        QCOMPARE(history.entries().first().url, QStringLiteral("https://a"));
        QCOMPARE(UploadHistory(path).entries().size(), 1);
        QVERIFY(history.clear());
        QVERIFY(UploadHistory(path).entries().isEmpty());
    }

    void persistsEveryField()
    {
        QTemporaryDir dir;
        // A missing folder is created on the first save.
        const QString path = dir.filePath(QStringLiteral("sub/history.json"));
        UploadHistory::Entry e = entry(QStringLiteral("https://u.example/x.png"));
        e.time = QDateTime(QDate(2026, 5, 4), QTime(3, 2, 1), QTimeZone::UTC);
        e.deleteUrl = QStringLiteral("https://u.example/delete/x");
        e.imageSize = QSize(640, 480);
        e.thumbnailPng = QByteArray("\x89PNG\r\n\x1a\n-binary", 16);
        QVERIFY(UploadHistory(path).add(e));

        const UploadHistory reloaded(path);
        QCOMPARE(reloaded.entries().size(), 1);
        const UploadHistory::Entry &r = reloaded.entries().first();
        QCOMPARE(r.time, e.time);
        QCOMPARE(r.provider, e.provider);
        QCOMPARE(r.providerName, e.providerName);
        QCOMPARE(r.url, e.url);
        QCOMPARE(r.deleteUrl, e.deleteUrl);
        QCOMPARE(r.imageSize, e.imageSize);
        QCOMPARE(r.thumbnailPng, e.thumbnailPng);
    }

    void ignoresCorruptFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("history.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{not json");
        file.close();
        QVERIFY(UploadHistory(path).entries().isEmpty());
    }

    void flagsExpiredTemporaryUploads()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        UploadHistory::Entry e = entry(QStringLiteral("https://a"));
        e.time = now.addSecs(-2 * 3600);
        e.provider = static_cast<int>(ImageUploader::Provider::TmpFiles);
        QVERIFY(UploadHistory::likelyExpired(e, now));
        e.provider = static_cast<int>(ImageUploader::Provider::Uguu);
        QVERIFY(!UploadHistory::likelyExpired(e, now));
        e.time = now.addDays(-30);
        e.provider = static_cast<int>(ImageUploader::Provider::Catbox);
        QVERIFY(!UploadHistory::likelyExpired(e, now));
        QCOMPARE(UploadHistory::lifetimeSeconds(static_cast<int>(ImageUploader::Provider::TempSh)),
                 qint64(3 * 24 * 3600));
    }
};

QTEST_APPLESS_MAIN(UploadHistoryTests)
#include "UploadHistoryTests.moc"
