#include <QtTest>

#include "core/DebouncedSettingsWriter.h"

class DebouncedSettingsWriterTests : public QObject
{
    Q_OBJECT

private slots:
    void repeatedWritesFlushOnceWithLatestValue()
    {
        const QString organization = QStringLiteral("EShotTests");
        const QString application = QStringLiteral("DebouncedSettingsWriter");
        QSettings settings(organization, application);
        settings.clear();

        DebouncedSettingsWriter writer(organization, application, 20);
        QSignalSpy flushedSpy(&writer, &DebouncedSettingsWriter::flushed);
        writer.schedule(QStringLiteral("volume"), 10);
        writer.schedule(QStringLiteral("volume"), 40);
        writer.schedule(QStringLiteral("volume"), 80);

        QTRY_COMPARE_WITH_TIMEOUT(flushedSpy.count(), 1, 250);
        QCOMPARE(settings.value(QStringLiteral("volume")).toInt(), 80);
        QCOMPARE(flushedSpy.first().first().toInt(), 1);
    }
};

QTEST_GUILESS_MAIN(DebouncedSettingsWriterTests)
#include "DebouncedSettingsWriterTests.moc"
