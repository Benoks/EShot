#include "LinuxRecordingSupport.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QStandardPaths>

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

    int cropWidth = captureRect.width();
    int cropHeight = captureRect.height();
    geometry.right = sourcePixelSize.width() - geometry.left - cropWidth;
    geometry.bottom = sourcePixelSize.height() - geometry.top - cropHeight;
    // H.264 needs even dimensions. Grow an odd crop by one source pixel (or
    // shrink it at the stream edge) instead of rescaling W to W-1, which
    // blurs every frame.
    if (cropWidth % 2 != 0) {
        if (geometry.right > 0) {
            ++cropWidth;
            --geometry.right;
        } else {
            --cropWidth;
            ++geometry.right;
        }
    }
    if (cropHeight % 2 != 0) {
        if (geometry.bottom > 0) {
            ++cropHeight;
            --geometry.bottom;
        } else {
            --cropHeight;
            ++geometry.bottom;
        }
    }
    if (cropWidth < 2 || cropHeight < 2)
        return {};
    geometry.cropSize = QSize(cropWidth, cropHeight);

    const bool downscale = requestedOutputSize.width() < captureRect.width()
        || requestedOutputSize.height() < captureRect.height();
    geometry.outputSize = downscale
        ? evenRecordingSize(QSize(qMin(requestedOutputSize.width(), cropWidth),
                                  qMin(requestedOutputSize.height(), cropHeight)))
        : geometry.cropSize;
    geometry.needsScale = geometry.outputSize != geometry.cropSize;
    geometry.valid = geometry.outputSize.isValid();
    return geometry;
}

QSize evenRecordingSize(const QSize &size)
{
    if (!size.isValid()) return QSize();
    return QSize(qMax(8, size.width() - (size.width() % 2)),
                 qMax(8, size.height() - (size.height() % 2)));
}

QString gstFailureReason(const QString &log)
{
    QStringList errors;
    QString last;
    for (const QString &rawLine : log.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        last = line;
        if (line.startsWith(QStringLiteral("ERROR")))
            errors.append(line);
    }
    return errors.isEmpty() ? last : errors.join(QLatin1Char('\n'));
}

bool gstPipelineReachedPlaying(const QString &log)
{
    return log.contains(QStringLiteral("Setting pipeline to PLAYING"));
}

namespace {
bool catalogHasElement(const QStringList &availableElements, const QString &name)
{
    for (const QString &element : availableElements) {
        if (element == name)
            return true;

        // `gst-inspect-1.0` without an element argument prints catalog
        // rows as "plugin: element: description". Accept that form too
        // so the fast, single-process discovery path sees installed
        // elements instead of treating the full row as an element name.
        const int firstColon = element.indexOf(QLatin1Char(':'));
        const int secondColon = firstColon < 0
            ? -1
            : element.indexOf(QLatin1Char(':'), firstColon + 1);
        if (firstColon >= 0 && secondColon > firstColon
            && element.mid(firstColon + 1, secondColon - firstColon - 1).trimmed() == name) {
            return true;
        }
    }
    return false;
}

QString firstAvailable(const QStringList &availableElements, const QStringList &preference)
{
    for (const QString &name : preference) {
        if (catalogHasElement(availableElements, name))
            return name;
    }
    return {};
}

QHash<QString, QStringList> &gstElementCache()
{
    static QHash<QString, QStringList> cache;
    return cache;
}
}

QString preferredGstAacEncoder(const QStringList &availableElements)
{
    return firstAvailable(availableElements,
                          {QStringLiteral("fdkaacenc"), QStringLiteral("avenc_aac"),
                           QStringLiteral("faac"), QStringLiteral("voaacenc")});
}

QString preferredGstH264Encoder(const QStringList &availableElements)
{
    return firstAvailable(availableElements,
                          {QStringLiteral("x264enc"), QStringLiteral("openh264enc")});
}

QStringList gstElementNamesFromInspect(const QString &inspectOutput)
{
    QStringList names;
    for (const QString &line : inspectOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const int firstColon = line.indexOf(QLatin1Char(':'));
        if (firstColon <= 0)
            continue;
        const int secondColon = line.indexOf(QLatin1Char(':'), firstColon + 1);
        if (secondColon < 0)
            continue;
        const QString name = line.mid(firstColon + 1, secondColon - firstColon - 1).trimmed();
        if (!name.isEmpty() && !name.contains(QLatin1Char(' ')))
            names.append(name);
    }
    names.removeDuplicates();
    return names;
}

QStringList missingPortalRecordingElements(const QStringList &availableElements,
                                           bool withAudio, bool mixesAudio,
                                           bool joinsSegments)
{
    QStringList required = {
        QStringLiteral("pipewiresrc"), QStringLiteral("videorate"),
        QStringLiteral("videocrop"), QStringLiteral("videoconvert"),
        QStringLiteral("h264parse"), QStringLiteral("mp4mux")
    };
    if (joinsSegments)
        required << QStringLiteral("splitmuxsrc");
    if (withAudio) {
        required << QStringLiteral("pulsesrc") << QStringLiteral("audioconvert")
                 << QStringLiteral("audioresample") << QStringLiteral("volume");
        if (mixesAudio)
            required << QStringLiteral("audiomixer");
    }

    QStringList missing;
    for (const QString &element : std::as_const(required)) {
        if (!catalogHasElement(availableElements, element))
            missing << element;
    }
    if (preferredGstH264Encoder(availableElements).isEmpty())
        missing << QStringLiteral("x264enc");
    if (withAudio && preferredGstAacEncoder(availableElements).isEmpty())
        missing << QStringLiteral("avenc_aac/fdkaacenc/faac/voaacenc");
    return missing;
}

QString gstMissingElementsReason(const QStringList &missingElements)
{
    return QStringLiteral("missing gstreamer elements: %1")
        .arg(missingElements.join(QStringLiteral(", ")));
}

QString portalCanceledReason()
{
    return QStringLiteral("portal canceled");
}

QStringList cachedGstElements(const QString &inspectProgram, bool *ok)
{
    const QString program = inspectProgram.isEmpty()
        ? QStringLiteral("gst-inspect-1.0") : inspectProgram;
    auto &cache = gstElementCache();
    const auto cached = cache.constFind(program);
    if (cached != cache.constEnd()) {
        if (ok)
            *ok = true;
        return cached.value();
    }

    // A single bare gst-inspect-1.0 call lists every installed element. It
    // runs once per app run; later recordings use the cached list.
    QProcess inspect;
    inspect.setProgram(program);
    inspect.setStandardErrorFile(QProcess::nullDevice());
    inspect.start();
    bool finished = inspect.waitForStarted(2000);
    if (finished && !inspect.waitForFinished(15000)) {
        inspect.kill();
        inspect.waitForFinished(1000);
        finished = false;
    }
    if (!finished || inspect.exitStatus() != QProcess::NormalExit || inspect.exitCode() != 0) {
        if (ok)
            *ok = false;
        return {};
    }
    const QStringList elements = gstElementNamesFromInspect(
        QString::fromLocal8Bit(inspect.readAllStandardOutput()));
    if (ok)
        *ok = !elements.isEmpty();
    if (!elements.isEmpty())
        cache.insert(program, elements);
    return elements;
}

void invalidateGstElementCache()
{
    gstElementCache().clear();
}

QString discoverGstAacEncoder(const QString &inspectProgram)
{
    return preferredGstAacEncoder(cachedGstElements(inspectProgram));
}

QString gstInspectPathFor(const QString &gstLaunchPath)
{
    if (!gstLaunchPath.isEmpty()) {
        const QString sibling = QFileInfo(gstLaunchPath).dir().filePath(QStringLiteral("gst-inspect-1.0"));
        if (QFileInfo(sibling).isExecutable())
            return sibling;
    }
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("gst-inspect-1.0"));
    return onPath.isEmpty() ? QStringLiteral("gst-inspect-1.0") : onPath;
}

int pipeWireKeepaliveMs(int fps)
{
    return qMax(50, 1000 / qMax(1, fps));
}

QString portalEncoderInputFormat(const QString &encoder, bool lossless, const QSize &size)
{
    // Full chroma keeps coloured text sharp for the GIF palette; above
    // ~1920x1200 a 4:4:4 lossless encode no longer keeps up in real time.
    if (lossless && encoder == QStringLiteral("x264enc")
        && static_cast<qint64>(size.width()) * size.height() <= 1920LL * 1200)
        return QStringLiteral("Y444");
    return QStringLiteral("I420");
}

QStringList gstH264EncoderArguments(const QString &encoder, int fps, int crf,
                                    const QSize &outputSize, bool lossless)
{
    const int safeFps = qMax(1, fps);
    const qint64 pixels = static_cast<qint64>(qMax(1, outputSize.width()))
        * qMax(1, outputSize.height());
    const QString keyframeInterval = QString::number(2 * safeFps);
    if (encoder == QStringLiteral("openh264enc")) {
        const qint64 bitsPerSecond = lossless
            ? qBound(qint64(4000000), pixels * safeFps, qint64(200000000))
            : qBound(qint64(1000000), pixels * safeFps / 10, qint64(40000000));
        return {
            encoder,
            QStringLiteral("usage-type=screen"),
            QStringLiteral("complexity=low"),
            QStringLiteral("rate-control=quality"),
            QStringLiteral("bitrate=%1").arg(bitsPerSecond),
            QStringLiteral("qp-max=%1").arg(lossless ? 12 : qBound(18, crf + 6, 51)),
            QStringLiteral("gop-size=%1").arg(keyframeInterval)
        };
    }
    if (lossless) {
        return {
            encoder,
            QStringLiteral("speed-preset=ultrafast"),
            QStringLiteral("pass=quant"),
            QStringLiteral("quantizer=0"),
            QStringLiteral("key-int-max=%1").arg(keyframeInterval)
        };
    }
    const bool heavy = pixels >= 3840LL * 2160 || safeFps >= 60;
    return {
        encoder,
        heavy ? QStringLiteral("speed-preset=superfast") : QStringLiteral("speed-preset=veryfast"),
        QStringLiteral("pass=qual"),
        QStringLiteral("quantizer=%1").arg(qBound(18, crf, 32)),
        QStringLiteral("rc-lookahead=10"),
        QStringLiteral("bframes=2"),
        QStringLiteral("key-int-max=%1").arg(keyframeInterval)
    };
}

QStringList portalVideoBranchArguments(const PortalVideoPipelineOptions &options)
{
    const PortalCropGeometry &crop = options.crop;
    const int fps = qMax(1, options.fps);
    const QString encoder = options.h264Encoder.isEmpty()
        ? QStringLiteral("x264enc") : options.h264Encoder;
    QStringList args;
    args << QStringLiteral("pipewiresrc")
         << QStringLiteral("fd=%1").arg(options.pipewireFd);
    if (!options.sourcePath.isEmpty())
        args << options.sourcePath;
    // keepalive-time resends the last frame while the screen is static, so
    // the encoder keeps receiving frames; on-disconnect=eos finishes the file
    // when the user stops sharing from the desktop.
    args << QStringLiteral("do-timestamp=true")
         << QStringLiteral("keepalive-time=%1").arg(pipeWireKeepaliveMs(fps))
         << QStringLiteral("on-disconnect=eos")
         << QStringLiteral("!")
         << QStringLiteral("queue")
         << QStringLiteral("max-size-buffers=4")
         << QStringLiteral("max-size-bytes=0")
         << QStringLiteral("max-size-time=0")
         << QStringLiteral("leaky=downstream")
         << QStringLiteral("!")
         // Drop surplus frames before the costly crop/convert; never
         // duplicate them.
         << QStringLiteral("videorate")
         << QStringLiteral("drop-only=true")
         << QStringLiteral("max-rate=%1").arg(fps)
         << QStringLiteral("!")
         << QStringLiteral("videocrop")
         << QStringLiteral("left=%1").arg(crop.left)
         << QStringLiteral("right=%1").arg(crop.right)
         << QStringLiteral("top=%1").arg(crop.top)
         << QStringLiteral("bottom=%1").arg(crop.bottom)
         << QStringLiteral("!")
         << QStringLiteral("videoconvert")
         << QStringLiteral("!");
    if (crop.needsScale)
        args << QStringLiteral("videoscale") << QStringLiteral("!");
    args << QStringLiteral("video/x-raw,format=%1,width=%2,height=%3,framerate=%4/1")
                .arg(portalEncoderInputFormat(encoder, options.losslessIntermediate, crop.outputSize))
                .arg(crop.outputSize.width())
                .arg(crop.outputSize.height())
                .arg(fps)
         << QStringLiteral("!")
         << gstH264EncoderArguments(encoder, fps, options.crf, crop.outputSize,
                                    options.losslessIntermediate)
         << QStringLiteral("!")
         << QStringLiteral("h264parse");
    return args;
}

QStringList portalRecordingPipelineArguments(const PortalVideoPipelineOptions &options,
                                             const QString &outputPath,
                                             const QStringList &audioArguments)
{
    QStringList args;
    // Fragments every second: a gst-launch that has to be killed still
    // leaves a playable file behind.
    args << QStringLiteral("-e")
         << QStringLiteral("mp4mux")
         << QStringLiteral("name=mux")
         << QStringLiteral("fragment-duration=1000")
         << QStringLiteral("!")
         << QStringLiteral("filesink")
         << QStringLiteral("location=%1").arg(outputPath)
         // Write each fragment through at once; stdio buffering would keep
         // the first seconds out of the file if the process is killed.
         << QStringLiteral("buffer-mode=unbuffered")
         << portalVideoBranchArguments(options)
         << QStringLiteral("!")
         << QStringLiteral("queue")
         << QStringLiteral("max-size-time=3000000000")
         << QStringLiteral("max-size-buffers=0")
         << QStringLiteral("max-size-bytes=0")
         << QStringLiteral("!")
         << QStringLiteral("mux.")
         << audioArguments;
    return args;
}

int gstStopGraceMs(qint64 recordedMs)
{
    // gst-launch drains the encoder and writes the last fragment on EOS;
    // allow a second per recorded minute on top of 15 s.
    return static_cast<int>(qBound(qint64(15000), qint64(15000) + qMax<qint64>(0, recordedMs) / 60, qint64(60000)));
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

    // The video encoder holds back about half a second of frames (lookahead
    // and B-frames), so audio queues must cover that while mp4mux waits.
    const QString queueTime = QStringLiteral("max-size-time=3000000000");
    const QString stableCaps = QStringLiteral("audio/x-raw,rate=48000,channels=2");
    auto appendSource = [&stableCaps, &queueTime](QStringList &args, const AudioSource &source) {
        // pulsesrc timestamps from the audio clock; do-timestamp would
        // restamp with arrival time and add jitter.
        args << QStringLiteral("pulsesrc")
             << QStringLiteral("device=%1").arg(source.device)
             << QStringLiteral("buffer-time=500000")
             << QStringLiteral("latency-time=20000")
             << QStringLiteral("slave-method=resample")
             << QStringLiteral("!")
             << QStringLiteral("queue")
             << queueTime
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
    auto appendEncoder = [&](QStringList &args) {
        args << aacEncoder
             << QStringLiteral("bitrate=160000")
             << QStringLiteral("!")
             << QStringLiteral("queue")
             << queueTime
             << QStringLiteral("max-size-buffers=0")
             << QStringLiteral("max-size-bytes=0")
             << QStringLiteral("!")
             << QStringLiteral("mux.");
    };

    QStringList args;
    if (sources.size() == 1) {
        appendSource(args, sources.first());
        args << QStringLiteral("!");
        appendEncoder(args);
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
         << QStringLiteral("!");
    appendEncoder(args);
    for (const AudioSource &source : std::as_const(sources)) {
        appendSource(args, source);
        args << QStringLiteral("!") << QStringLiteral("mix.");
    }
    return args;
}

QString linuxDesktopAudioDevice(const QString &configuredDevice)
{
    // "@DEFAULT_SINK@.monitor" only resolves on pipewire-pulse; real
    // PulseAudio needs @DEFAULT_MONITOR@, which both understand.
    const QString device = configuredDevice.trimmed();
    if (device.isEmpty()
        || device == QStringLiteral("__wasapi__")
        || device == QStringLiteral("@DEFAULT_SINK@.monitor")
        || device.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("@DEFAULT_MONITOR@");
    return device;
}

LinuxMicrophoneResolution resolveLinuxMicrophoneDevice(
    const QString &configuredDevice,
    const QList<QPair<QString, QString>> &discoveredDevices,
    bool discoveryOk)
{
    LinuxMicrophoneResolution resolution;
    const QString device = configuredDevice.trimmed();
    if (device.isEmpty()
        || device.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0
        || device == QStringLiteral("@DEFAULT_SOURCE@")) {
        resolution.device = QStringLiteral("@DEFAULT_SOURCE@");
        resolution.missing = discoveryOk && discoveredDevices.isEmpty();
        return resolution;
    }

    resolution.device = device;
    if (discoveryOk) {
        resolution.missing = true;
        for (const auto &entry : discoveredDevices) {
            if (entry.second.compare(device, Qt::CaseInsensitive) == 0) {
                resolution.missing = false;
                break;
            }
        }
    }
    return resolution;
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
        else if (trimmed.startsWith(QStringLiteral("Monitor of Source:"))) monitor = trimmed.mid(18).trimmed();
    }
    flush();
    return result;
}

QList<QPair<QString, QString>> discoverLinuxMicrophoneDevices(bool *ok)
{
    if (ok)
        *ok = false;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QProcess pactl;
    // pactl output labels ("Name:", "Monitor of Sink:") may be translated;
    // LANGUAGE switches the messages only and keeps UTF-8 descriptions.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANGUAGE"), QStringLiteral("en"));
    pactl.setProcessEnvironment(env);
    pactl.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sources")});
    if (!pactl.waitForStarted(2000)) return {};
    if (!pactl.waitForFinished(2000)) {
        pactl.kill();
        pactl.waitForFinished(1000);
        return {};
    }
    if (pactl.exitStatus() != QProcess::NormalExit || pactl.exitCode() != 0) return {};
    if (ok)
        *ok = true;
    return linuxMicrophoneDevices(QString::fromLocal8Bit(pactl.readAllStandardOutput()));
#else
    return {};
#endif
}
