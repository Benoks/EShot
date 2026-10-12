#include <QtTest>

#include <QImageReader>
#include <QTemporaryDir>

#include "recording/GifEncodeWorker.h"
#include "recording/GifEncoder.h"

namespace {
QImage solidFrame(const QSize &size, QRgb color)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    return image;
}
}

class GifEncodeWorkerTests : public QObject
{
    Q_OBJECT

private slots:
    void encodesQueuedFramesInOrder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("worker.gif"));
        const QSize size(40, 30);
        // Values the encoder's quantizer reproduces exactly.
        const QList<QRgb> colors = {qRgb(224, 0, 0), qRgb(0, 224, 0), qRgb(0, 0, 192),
                                    qRgb(224, 224, 0), qRgb(0, 0, 0)};

        GifEncoder encoder;
        QVERIFY(encoder.open(path, size.width(), size.height(), 0));
        {
            // A queue of one makes addFrame() wait for the worker.
            GifEncodeWorker worker(&encoder, 1);
            for (QRgb color : colors)
                QVERIFY(worker.addFrame(solidFrame(size, color), 7));
            QVERIFY(worker.finish());
            QVERIFY(worker.error().isEmpty());
            QVERIFY(!worker.addFrame(solidFrame(size, colors.first()), 7));
        }
        QVERIFY(encoder.close());

        if (!QImageReader::supportedImageFormats().contains("gif"))
            QSKIP("Qt gif image plugin is not available");
        QImageReader reader(path);
        QCOMPARE(reader.imageCount(), colors.size());
        for (QRgb color : colors) {
            const QImage frame = reader.read();
            QVERIFY(!frame.isNull());
            QCOMPARE(QColor(frame.pixel(5, 5)).rgb(), QColor(color).rgb());
        }
    }

    void reportsEncoderFailureAndRejectsLaterFrames()
    {
        // Never opened: the first addFrame() fails inside the worker.
        GifEncoder encoder;
        GifEncodeWorker worker(&encoder);
        worker.addFrame(solidFrame(QSize(8, 8), qRgb(1, 2, 3)), 5);
        QVERIFY(!worker.finish());
        QVERIFY(!worker.error().isEmpty());
        QVERIFY(!worker.addFrame(solidFrame(QSize(8, 8), qRgb(1, 2, 3)), 5));
    }

    void destructionDropsQueuedFrames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GifEncoder encoder;
        QVERIFY(encoder.open(dir.filePath(QStringLiteral("dropped.gif")), 16, 16, 0));
        {
            GifEncodeWorker worker(&encoder, 64);
            for (int i = 0; i < 20; ++i)
                QVERIFY(worker.addFrame(solidFrame(QSize(16, 16), qRgb(i * 10, 0, 0)), 5));
        }
        // The encoder is usable again once the worker is gone.
        QVERIFY(encoder.close());
    }
};

QTEST_GUILESS_MAIN(GifEncodeWorkerTests)
#include "GifEncodeWorkerTests.moc"
