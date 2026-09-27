#include <QtTest>

#include <QImageReader>
#include <QTemporaryDir>

#include "recording/GifEncoder.h"

namespace {
// Distinct colors that each fall into their own 5-bit quantizer bucket, so
// the encoder's palette reproduces them exactly.
QRgb paletteColor(int index)
{
    return qRgb((index & 7) << 5, ((index >> 3) & 7) << 5, ((index >> 6) & 3) << 6);
}

QImage patternFrame(const QSize &size, int colorCount, int seed)
{
    QImage image(size, QImage::Format_RGB32);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x)
            image.setPixel(x, y, paletteColor((x * 7 + y * 3 + seed) % colorCount));
    }
    return image;
}
}

class GifEncoderTests : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsThroughQtDecoder_data()
    {
        QTest::addColumn<int>("colorCount");
        QTest::newRow("2 colors") << 2;
        QTest::newRow("4 colors") << 4;
        QTest::newRow("16 colors") << 16;
        QTest::newRow("100 colors") << 100;
        QTest::newRow("256 colors") << 256;
    }

    void roundTripsThroughQtDecoder()
    {
        if (!QImageReader::supportedImageFormats().contains("gif"))
            QSKIP("Qt gif image plugin is not available");

        QFETCH(int, colorCount);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("roundtrip.gif"));

        // Odd dimensions and enough pixels to span many CLEAR codes and
        // several 255-byte data sub-blocks.
        const QSize size(97, 61);
        const QList<QImage> frames = {
            patternFrame(size, colorCount, 0),
            patternFrame(size, colorCount, 5)
        };

        GifEncoder encoder;
        QVERIFY2(encoder.open(path, size.width(), size.height()), qPrintable(encoder.errorString()));
        for (const QImage &frame : frames)
            QVERIFY2(encoder.addFrame(frame, 10), qPrintable(encoder.errorString()));
        QVERIFY2(encoder.close(), qPrintable(encoder.errorString()));

        QImageReader reader(path, "gif");
        for (int i = 0; i < frames.size(); ++i) {
            const QImage decoded = reader.read().convertToFormat(QImage::Format_RGB32);
            QVERIFY2(!decoded.isNull(), qPrintable(reader.errorString()));
            QCOMPARE(decoded.size(), size);
            for (int y = 0; y < size.height(); ++y) {
                for (int x = 0; x < size.width(); ++x) {
                    if (decoded.pixel(x, y) != frames[i].pixel(x, y)) {
                        QFAIL(qPrintable(QStringLiteral("frame %1 pixel (%2,%3) is %4, expected %5")
                                             .arg(i).arg(x).arg(y)
                                             .arg(decoded.pixel(x, y), 8, 16, QLatin1Char('0'))
                                             .arg(frames[i].pixel(x, y), 8, 16, QLatin1Char('0'))));
                    }
                }
            }
        }
    }
};

QTEST_GUILESS_MAIN(GifEncoderTests)
#include "GifEncoderTests.moc"
