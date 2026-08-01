#include "LinuxRecordingSupport.h"

#include <QProcess>

QString pipeWireSourcePath(uint nodeId, quint64 pipewireSerial)
{
    if (pipewireSerial > 0)
        return QStringLiteral("target-object=%1").arg(pipewireSerial);
    return nodeId > 0 ? QStringLiteral("path=%1").arg(nodeId) : QString();
}

PortalCropGeometry portalCropGeometry(const QRect &captureRect,
                                      const QRect &displayRect,
                                      const QPoint &streamPosition,
                                      const QSize &streamDisplaySize,
                                      const QSize &requestedOutputSize)
{
    PortalCropGeometry geometry;
    if (!captureRect.isValid() || !requestedOutputSize.isValid())
        return geometry;

    QSize sourcePixelSize;
    if (displayRect.isValid() && streamDisplaySize.isValid()) {
        const QRect streamDisplayRect(streamPosition, streamDisplaySize);
        if (!streamDisplayRect.contains(displayRect))
            return geometry;

        const qreal scaleX = captureRect.width() / static_cast<qreal>(displayRect.width());
        const qreal scaleY = captureRect.height() / static_cast<qreal>(displayRect.height());
        sourcePixelSize = QSize(qRound(streamDisplaySize.width() * scaleX),
                                qRound(streamDisplaySize.height() * scaleY));
        geometry.left = qRound((displayRect.x() - streamPosition.x()) * scaleX);
        geometry.top = qRound((displayRect.y() - streamPosition.y()) * scaleY);
    } else {
        sourcePixelSize = streamDisplaySize.isValid() ? streamDisplaySize : captureRect.size();
        geometry.left = captureRect.x() - streamPosition.x();
        geometry.top = captureRect.y() - streamPosition.y();
    }

    if (geometry.left < 0 || geometry.top < 0
        || geometry.left + captureRect.width() > sourcePixelSize.width()
        || geometry.top + captureRect.height() > sourcePixelSize.height()) {
        return {};
    }

    geometry.right = sourcePixelSize.width() - geometry.left - captureRect.width();
    geometry.bottom = sourcePixelSize.height() - geometry.top - captureRect.height();
    geometry.outputSize = evenRecordingSize(QSize(
        qMin(requestedOutputSize.width(), captureRect.width()),
        qMin(requestedOutputSize.height(), captureRect.height())));
    geometry.valid = geometry.outputSize.isValid();
    return geometry;
}

QSize evenRecordingSize(const QSize &size)
{
    if (!size.isValid()) return QSize();
    return QSize(qMax(8, size.width() - (size.width() % 2)),
                 qMax(8, size.height() - (size.height() % 2)));
}

QString preferredGstAacEncoder(const QStringList &availableElements)
{
    const QStringList preference = {QStringLiteral("fdkaacenc"), QStringLiteral("avenc_aac"),
                                    QStringLiteral("faac"), QStringLiteral("voaacenc")};
    for (const QString &encoder : preference)
        if (availableElements.contains(encoder)) return encoder;
    return {};
}

QString discoverGstAacEncoder()
{
    for (const QString &encoder : {QStringLiteral("fdkaacenc"), QStringLiteral("avenc_aac"),
                                   QStringLiteral("faac"), QStringLiteral("voaacenc")}) {
        QProcess inspect;
        inspect.setProgram(QStringLiteral("gst-inspect-1.0"));
        inspect.setArguments({encoder});
        inspect.setStandardOutputFile(QProcess::nullDevice());
        inspect.setStandardErrorFile(QProcess::nullDevice());
        inspect.start();
        if (!inspect.waitForStarted(2000))
            continue;
        if (inspect.waitForFinished(15000)
            && inspect.exitStatus() == QProcess::NormalExit
            && inspect.exitCode() == 0) {
            return encoder;
        }
        if (inspect.state() != QProcess::NotRunning) {
            inspect.kill();
            inspect.waitForFinished(1000);
        }
    }
    return {};
}

QStringList waylandRecordingAudioArguments(bool desktopEnabled, int desktopVolume,
                                           const QString &desktopDevice,
                                           bool microphoneEnabled, int microphoneVolume,
                                           const QString &microphoneDevice,
                                           const QString &aacEncoder)
{
    struct AudioSource {
        QString device;
        int volume;
    };

    QList<AudioSource> sources;
    if (desktopEnabled && desktopVolume > 0 && !desktopDevice.trimmed().isEmpty())
        sources.append({desktopDevice, qBound(0, desktopVolume, 100)});
    if (microphoneEnabled && microphoneVolume > 0 && !microphoneDevice.trimmed().isEmpty())
        sources.append({microphoneDevice, qBound(0, microphoneVolume, 100)});
    if (sources.isEmpty() || aacEncoder.trimmed().isEmpty())
        return {};

    const QString stableCaps = QStringLiteral("audio/x-raw,rate=48000,channels=2");
    auto appendSource = [&stableCaps](QStringList &args, const AudioSource &source) {
        args << QStringLiteral("pulsesrc")
             << QStringLiteral("device=%1").arg(source.device)
             << QStringLiteral("do-timestamp=true")
             << QStringLiteral("buffer-time=500000")
             << QStringLiteral("latency-time=20000")
             << QStringLiteral("slave-method=resample")
             << QStringLiteral("!")
             << QStringLiteral("queue")
             << QStringLiteral("max-size-time=1000000000")
             << QStringLiteral("max-size-buffers=0")
             << QStringLiteral("max-size-bytes=0")
             << QStringLiteral("!")
             << QStringLiteral("audioconvert")
             << QStringLiteral("!")
             << QStringLiteral("audioresample")
             << QStringLiteral("!")
             << stableCaps
             << QStringLiteral("!")
             << QStringLiteral("volume")
             << QStringLiteral("volume=%1").arg(
                    QString::number(source.volume / 100.0, 'f', 2));
    };

    QStringList args;
    if (sources.size() == 1) {
        appendSource(args, sources.first());
        args << QStringLiteral("!")
             << aacEncoder
             << QStringLiteral("bitrate=160000")
             << QStringLiteral("!")
             << QStringLiteral("queue")
             << QStringLiteral("!")
             << QStringLiteral("mux.");
        return args;
    }

    args << QStringLiteral("audiomixer")
         << QStringLiteral("name=mix")
         << QStringLiteral("ignore-inactive-pads=true")
         << QStringLiteral("latency=100000000")
         << QStringLiteral("!")
         << QStringLiteral("audioconvert")
         << QStringLiteral("!")
         << QStringLiteral("audioresample")
         << QStringLiteral("!")
         << stableCaps
         << QStringLiteral("!")
         << aacEncoder
         << QStringLiteral("bitrate=160000")
         << QStringLiteral("!")
         << QStringLiteral("queue")
         << QStringLiteral("!")
         << QStringLiteral("mux.");
    for (const AudioSource &source : std::as_const(sources)) {
        appendSource(args, source);
        args << QStringLiteral("!") << QStringLiteral("mix.");
    }
    return args;
}

QList<QPair<QString, QString>> linuxMicrophoneDevices(const QString &pactlSources)
{
    QList<QPair<QString, QString>> result;
    QString name;
    QString description;
    QString monitor;
    auto flush = [&]() {
        if (!name.isEmpty() && !name.endsWith(QStringLiteral(".monitor"), Qt::CaseInsensitive)
            && (monitor.isEmpty() || monitor == QStringLiteral("n/a"))) {
            QString label = description.trimmed();
            if (label.isEmpty() || label == QStringLiteral("(null)")) label = name;
            result.append(qMakePair(label, name));
        }
        name.clear(); description.clear(); monitor.clear();
    };
    const QStringList lines = pactlSources.split('\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Source #"))) { flush(); continue; }
        if (trimmed.startsWith(QStringLiteral("Name:"))) name = trimmed.mid(5).trimmed();
        else if (trimmed.startsWith(QStringLiteral("Description:"))) description = trimmed.mid(12).trimmed();
        else if (trimmed.startsWith(QStringLiteral("Monitor of Sink:"))) monitor = trimmed.mid(16).trimmed();
    }
    flush();
    return result;
}

QList<QPair<QString, QString>> discoverLinuxMicrophoneDevices()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QProcess pactl;
    pactl.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sources")});
    if (!pactl.waitForFinished(2000) || pactl.exitCode() != 0) return {};
    return linuxMicrophoneDevices(QString::fromLocal8Bit(pactl.readAllStandardOutput()));
#else
    return {};
#endif
}
