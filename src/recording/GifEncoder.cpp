#include "GifEncoder.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <climits>
#include <QtGlobal>

GifEncoder::GifEncoder(QObject *parent) : QObject(parent) {}

GifEncoder::~GifEncoder()
{
    if (m_fileOpen) close();
}

bool GifEncoder::open(const QString &path, int width, int height, int loopCount)
{
    if (m_fileOpen) {
        m_lastError = QStringLiteral("already open");
        return false;
    }
    if (width <= 0 || height <= 0 || width > 65535 || height > 65535) {
        m_lastError = QStringLiteral("invalid dimensions");
        return false;
    }

    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    m_path = path;
    m_width = width;
    m_height = height;
    m_lastError.clear();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = QStringLiteral("cannot open output: %1 (%2)").arg(path, m_file.errorString());
        return false;
    }

    m_fileOpen = true;
    m_cachedLutValid = false;
    if (!writeHeader()) {
        m_file.close();
        m_fileOpen = false;
        return false;
    }

    if (loopCount >= 0 && !writeNetscapeLoopExtension(loopCount)) {
        m_file.close();
        m_fileOpen = false;
        return false;
    }

    return true;
}

bool GifEncoder::addFrame(const QImage &image, int delayCs)
{
    if (!m_fileOpen) {
        m_lastError = QStringLiteral("not open");
        return false;
    }
    if (image.isNull()) {
        m_lastError = QStringLiteral("null frame");
        return false;
    }

    QImage frame = image.convertToFormat(QImage::Format_RGB32);
    if (frame.width() != m_width || frame.height() != m_height) {
        frame = frame.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    QVector<Color> palette;
    const QByteArray indices = quantizeToPalette(frame, palette);
    const int tableBits = colorTableBitsForPalette(palette.size());
    return writeGraphicControlExtension(qMax(1, delayCs))
        && writeImageDescriptor(0, 0, m_width, m_height, tableBits)
        && writeColorTable(palette, 1 << tableBits)
        && writeIndexedImageData(indices, tableBits);
}

bool GifEncoder::close()
{
    if (!m_fileOpen) return true;

    const bool trailerOk = writeByte(0x3B);
    m_fileOpen = false;
    const bool flushOk = m_file.flush();
    m_file.close();

    if (!trailerOk || !flushOk) {
        m_lastError = QStringLiteral("write failed");
        QFile::remove(m_path);
        return false;
    }
    return true;
}

bool GifEncoder::writeByte(quint8 b)
{
    if (m_file.putChar(static_cast<char>(b))) return true;
    m_lastError = QStringLiteral("write failed");
    return false;
}

bool GifEncoder::writeBytes(const char *data, qsizetype size)
{
    if (m_file.write(data, size) == size) return true;
    m_lastError = QStringLiteral("write failed");
    return false;
}

bool GifEncoder::writeShort(quint16 v)
{
    return writeByte(static_cast<quint8>(v & 0xFF))
        && writeByte(static_cast<quint8>((v >> 8) & 0xFF));
}

bool GifEncoder::writeHeader()
{
    static const char sig[] = "GIF89a";
    return writeBytes(sig, 6)
        && writeLogicalScreenDescriptor(m_width, m_height);
}

bool GifEncoder::writeLogicalScreenDescriptor(int width, int height)
{
    return writeShort(static_cast<quint16>(width))
        && writeShort(static_cast<quint16>(height))
        && writeByte(0x70)
        && writeByte(0)
        && writeByte(0);
}

int GifEncoder::colorTableBitsForPalette(int paletteSize)
{
    int bits = 1;
    while (bits < 8 && (1 << bits) < paletteSize)
        ++bits;
    return bits;
}

bool GifEncoder::writeColorTable(const QVector<Color> &palette, int tableSize)
{
    for (int i = 0; i < tableSize; ++i) {
        const Color c = i < palette.size() ? palette[i] : Color{0, 0, 0};
        if (!writeByte(c.r) || !writeByte(c.g) || !writeByte(c.b)) return false;
    }
    return true;
}

bool GifEncoder::writeNetscapeLoopExtension(int loopCount)
{
    static const char appId[] = "NETSCAPE2.0";
    return writeByte(0x21)
        && writeByte(0xFF)
        && writeByte(0x0B)
        && writeBytes(appId, 11)
        && writeByte(0x03)
        && writeByte(0x01)
        && writeShort(static_cast<quint16>(loopCount))
        && writeByte(0x00);
}

bool GifEncoder::writeGraphicControlExtension(int delayCs)
{
    return writeByte(0x21)
        && writeByte(0xF9)
        && writeByte(0x04)
        && writeByte(0x00)
        && writeShort(static_cast<quint16>(qBound(1, delayCs, 65535)))
        && writeByte(0x00)
        && writeByte(0x00);
}

bool GifEncoder::writeImageDescriptor(int left, int top, int width, int height, int colorTableBits)
{
    return writeByte(0x2C)
        && writeShort(static_cast<quint16>(left))
        && writeShort(static_cast<quint16>(top))
        && writeShort(static_cast<quint16>(width))
        && writeShort(static_cast<quint16>(height))
        // Local color table present; its size is 2^colorTableBits entries.
        && writeByte(static_cast<quint8>(0x80 | ((colorTableBits - 1) & 0x07)));
}

QByteArray GifEncoder::quantizeToPalette(const QImage &image, QVector<Color> &palette) const
{
    constexpr int BITS = 5;
    constexpr int LEVELS = 1 << BITS;
    constexpr int BUCKETS = LEVELS * LEVELS * LEVELS;
    QVector<quint32> rSum(BUCKETS), gSum(BUCKETS), bSum(BUCKETS), count(BUCKETS);

    for (int y = 0; y < m_height; ++y) {
        const quint32 *line = reinterpret_cast<const quint32 *>(image.constScanLine(y));
        for (int x = 0; x < m_width; ++x) {
            const quint32 px = line[x];
            const int r = (px >> 16) & 0xFF;
            const int g = (px >> 8) & 0xFF;
            const int b = px & 0xFF;
            const int key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
            rSum[key] += r;
            gSum[key] += g;
            bSum[key] += b;
            ++count[key];
        }
    }

    struct Entry { quint32 count; int key; };
    QVector<Entry> entries;
    entries.reserve(BUCKETS);
    for (int i = 0; i < BUCKETS; ++i) {
        if (count[i]) entries.append({count[i], i});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.count > b.count;
    });

    palette.clear();
    palette.reserve(256);
    for (const Entry &e : entries) {
        if (palette.size() >= 256) break;
        const quint32 c = count[e.key];
        palette.append(Color{
            static_cast<quint8>(rSum[e.key] / c),
            static_cast<quint8>(gSum[e.key] / c),
            static_cast<quint8>(bSum[e.key] / c)
        });
    }
    // Pad the palette up to the next power-of-two color table size so the
    // per-frame local color table only carries the entries actually used.
    const int tableSize = 1 << colorTableBitsForPalette(palette.size());
    while (palette.size() < tableSize) palette.append(Color{0, 0, 0});

    QVector<quint8> lut;
    if (m_cachedLutValid && m_cachedLutPalette == palette) {
        // Same palette as the previous frame: reuse the LZW LUT instead of
        // rebuilding the nearest-color table for all buckets.
        lut = m_cachedLut;
    } else {
        lut.resize(BUCKETS);
        for (int key = 0; key < BUCKETS; ++key) {
            const int r = (((key >> 10) & 31) << 3) | 4;
            const int g = (((key >> 5) & 31) << 3) | 4;
            const int b = ((key & 31) << 3) | 4;
            int best = 0;
            int bestDist = INT_MAX;
            for (int i = 0; i < palette.size(); ++i) {
                const int dr = r - palette[i].r;
                const int dg = g - palette[i].g;
                const int db = b - palette[i].b;
                const int dist = dr * dr + dg * dg + db * db;
                if (dist < bestDist) {
                    bestDist = dist;
                    best = i;
                    if (dist == 0) break;
                }
            }
            lut[key] = static_cast<quint8>(best);
        }
        m_cachedLutPalette = palette;
        m_cachedLut = lut;
        m_cachedLutValid = true;
    }

    QByteArray indices;
    indices.resize(m_width * m_height);
    uchar *out = reinterpret_cast<uchar *>(indices.data());

    for (int y = 0; y < m_height; ++y) {
        const quint32 *line = reinterpret_cast<const quint32 *>(image.constScanLine(y));
        for (int x = 0; x < m_width; ++x) {
            const quint32 px = line[x];
            const int r = (px >> 16) & 0xFF;
            const int g = (px >> 8) & 0xFF;
            const int b = px & 0xFF;
            const int key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
            *out++ = lut[key];
        }
    }
    return indices;
}

bool GifEncoder::writeDataBlock(QByteArray &block)
{
    if (block.isEmpty()) return true;
    if (!writeByte(static_cast<quint8>(block.size()))) return false;
    if (!writeBytes(block.constData(), block.size())) return false;
    block.clear();
    return true;
}

bool GifEncoder::writeIndexedImageData(const QByteArray &indices, int colorTableBits)
{
    const int MIN_CODE_SIZE = qBound(2, colorTableBits, 8);
    const int CLEAR_CODE = 1 << MIN_CODE_SIZE;
    const int END_CODE = CLEAR_CODE + 1;
    const int CODE_SIZE = MIN_CODE_SIZE + 1;

    if (!writeByte(MIN_CODE_SIZE)) return false;

    QByteArray block;
    block.reserve(255);
    quint32 bitBuffer = 0;
    int bitCount = 0;

    auto emitByte = [&](quint8 b) -> bool {
        block.append(static_cast<char>(b));
        if (block.size() == 255)
            return writeDataBlock(block);
        return true;
    };

    auto writeBits = [&](int code) -> bool {
        bitBuffer |= static_cast<quint32>(code) << bitCount;
        bitCount += CODE_SIZE;
        while (bitCount >= 8) {
            if (!emitByte(static_cast<quint8>(bitBuffer & 0xFF))) return false;
            bitBuffer >>= 8;
            bitCount -= 8;
        }
        return true;
    };

    auto flushBits = [&]() -> bool {
        if (bitCount > 0) {
            if (!emitByte(static_cast<quint8>(bitBuffer & 0xFF))) return false;
            bitBuffer = 0;
            bitCount = 0;
        }
        return writeDataBlock(block) && writeByte(0x00);
    };

    if (!writeBits(CLEAR_CODE)) return false;

    if (indices.isEmpty()) {
        return writeBits(END_CODE) && flushBits();
    }

    const uchar *data = reinterpret_cast<const uchar *>(indices.constData());
    const int count = indices.size();
    int codesSinceClear = 0;

    for (int i = 0; i < count; ++i) {
        if (codesSinceClear >= 250) {
            if (!writeBits(CLEAR_CODE)) return false;
            codesSinceClear = 0;
        }
        if (!writeBits(data[i])) return false;
        ++codesSinceClear;
    }

    return writeBits(END_CODE)
        && flushBits();
}
