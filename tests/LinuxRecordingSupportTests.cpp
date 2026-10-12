#include <QtTest>

#include "recording/LinuxRecordingSupport.h"

class LinuxRecordingSupportTests : public QObject {
    Q_OBJECT
private slots:
    void gstFailureReasonSkipsProgressOutput()
    {
        const QString log = QStringLiteral(
            "Setting pipeline to PAUSED ...\nPipeline is live and does not need PREROLL ...\n"
            "ERROR: from element /GstPipeline:pipeline0/GstPulseSrc:pulsesrc0: Failed to connect\n"
            "Additional debug info:\nExecution ended after 0:00:00.1\n");
        QCOMPARE(gstFailureReason(log), QStringLiteral(
            "ERROR: from element /GstPipeline:pipeline0/GstPulseSrc:pulsesrc0: Failed to connect"));
        QCOMPARE(gstFailureReason(QStringLiteral("a\nlast line\n")), QStringLiteral("last line"));
        QVERIFY(gstFailureReason(QString()).isEmpty());
    }

    void usesStablePipeWireSerialWhenAvailable()
    {
        QCOMPARE(pipeWireSourcePath(77, 0), QStringLiteral("path=77"));
        QCOMPARE(pipeWireSourcePath(77, 9001), QStringLiteral("target-object=9001"));
        QCOMPARE(pipeWireSourcePath(0, 9001), QStringLiteral("target-object=9001"));
        QVERIFY(pipeWireSourcePath(0, 0).isEmpty());
    }

    void mapsLogicalPortalGeometryToPhysicalPixels()
    {
        const PortalCropGeometry crop = portalCropGeometry(
            QRect(200, 100, 800, 400), QRect(100, 50, 400, 200),
            QPoint(0, 0), QSize(1920, 1080), QSize(800, 400));
        QVERIFY(crop.valid);
        QCOMPARE(crop.left, 200);
        QCOMPARE(crop.top, 100);
        QCOMPARE(crop.right, 3840 - 200 - 800);
        QCOMPARE(crop.bottom, 2160 - 100 - 400);
        QCOMPARE(crop.outputSize, QSize(800, 400));
        QCOMPARE(crop.cropSize, QSize(800, 400));
        QVERIFY(!crop.needsScale);
    }

    void growsAnOddCropInsteadOfRescalingIt()
    {
        const PortalCropGeometry crop = portalCropGeometry(
            QRect(100, 50, 801, 603), QRect(), QPoint(0, 0), QSize(1920, 1080), QSize(801, 603));
        QVERIFY(crop.valid);
        QCOMPARE(crop.cropSize, QSize(802, 604));
        QCOMPARE(crop.outputSize, crop.cropSize);
        QVERIFY(!crop.needsScale);
        QCOMPARE(crop.left, 100);
        QCOMPARE(crop.top, 50);
        QCOMPARE(crop.right, 1920 - 100 - 802);
        QCOMPARE(crop.bottom, 1080 - 50 - 604);
        QCOMPARE(crop.left + crop.cropSize.width() + crop.right, 1920);
        QCOMPARE(crop.top + crop.cropSize.height() + crop.bottom, 1080);
    }

    void shrinksAnOddCropThatTouchesTheStreamEdge()
    {
        const PortalCropGeometry crop = portalCropGeometry(
            QRect(1920 - 301, 1080 - 201, 301, 201), QRect(), QPoint(0, 0),
            QSize(1920, 1080), QSize(301, 201));
        QVERIFY(crop.valid);
        QCOMPARE(crop.right, 1);
        QCOMPARE(crop.bottom, 1);
        QCOMPARE(crop.cropSize, QSize(300, 200));
        QCOMPARE(crop.outputSize, QSize(300, 200));
        QVERIFY(!crop.needsScale);

        const PortalCropGeometry full = portalCropGeometry(
            QRect(0, 0, 1365, 767), QRect(), QPoint(0, 0), QSize(1365, 767), QSize(1365, 767));
        QVERIFY(full.valid);
        QCOMPARE(full.cropSize, QSize(1364, 766));
        QCOMPARE(full.right, 1);
        QCOMPARE(full.bottom, 1);
    }

    void scalesOnlyWhenTheOutputIsBounded()
    {
        const PortalCropGeometry crop = portalCropGeometry(
            QRect(0, 0, 1920, 1080), QRect(), QPoint(0, 0), QSize(1920, 1080), QSize(1280, 720));
        QVERIFY(crop.valid);
        QCOMPARE(crop.cropSize, QSize(1920, 1080));
        QCOMPARE(crop.outputSize, QSize(1280, 720));
        QVERIFY(crop.needsScale);
    }

    void rejectsAStreamFromTheWrongMonitor()
    {
        const PortalCropGeometry crop = portalCropGeometry(
            QRect(2000, 100, 500, 300), QRect(2000, 100, 500, 300),
            QPoint(0, 0), QSize(1920, 1080), QSize(500, 300));
        QVERIFY(!crop.valid);
    }

    void forcesEncoderFriendlyEvenDimensions()
    {
        QCOMPARE(evenRecordingSize(QSize(801, 603)), QSize(800, 602));
        QCOMPARE(evenRecordingSize(QSize(8, 8)), QSize(8, 8));
    }

    void choosesAnInstalledAacEncoder()
    {
        QCOMPARE(preferredGstAacEncoder({QStringLiteral("avenc_aac"), QStringLiteral("faac")}),
                 QStringLiteral("avenc_aac"));
        QCOMPARE(preferredGstAacEncoder({QStringLiteral("voaacenc"), QStringLiteral("fdkaacenc")}),
                 QStringLiteral("fdkaacenc"));
        QVERIFY(preferredGstAacEncoder({}).isEmpty());
    }

    void choosesEncoderFromGstInspectCatalogOutput()
    {
        QCOMPARE(preferredGstAacEncoder({
                     QStringLiteral("fdkaac:  fdkaacenc: FDK AAC audio encoder"),
                     QStringLiteral("libav:  avenc_aac: libav AAC audio encoder")}),
                 QStringLiteral("fdkaacenc"));
    }

    void exposesFriendlyMicrophoneLabelsAndIds()
    {
        const QString pactl = QStringLiteral(
            "Source #12\n\tName: alsa_output.headset.monitor\n\tDescription: Monitor of Headset\n\tMonitor of Sink: alsa_output.headset\n"
            "Source #13\n\tName: alsa_input.headset.mono-fallback\n\tDescription: G432 Gaming Headset Mono\n\tMonitor of Sink: n/a\n"
            "Source #14\n\tName: easyeffects_source\n\tDescription: Easy Effects Source\n\tMonitor of Sink: n/a\n"
            "Source #15\n\tName: alsa_output.speakers.monitor\n\tDescription: Monitor of Speakers\n\tMonitor of Source: alsa_output.speakers\n");
        const auto devices = linuxMicrophoneDevices(pactl);
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices.at(0).first, QStringLiteral("G432 Gaming Headset Mono"));
        QCOMPARE(devices.at(0).second, QStringLiteral("alsa_input.headset.mono-fallback"));
        QCOMPARE(devices.at(1).first, QStringLiteral("Easy Effects Source"));
        QCOMPARE(devices.at(1).second, QStringLiteral("easyeffects_source"));
    }

    void stabilizesMixedPulseAudioSourcesForWaylandRecording()
    {
        const QStringList args = waylandRecordingAudioArguments(
            true, 80, QStringLiteral("@DEFAULT_MONITOR@"),
            true, 70, QStringLiteral("easyeffects_source"),
            QStringLiteral("fdkaacenc"));

        QVERIFY(args.contains(QStringLiteral("ignore-inactive-pads=true")));
        QVERIFY(!args.contains(QStringLiteral("force-live=true")));
        QVERIFY(args.contains(QStringLiteral("latency=100000000")));
        // pulsesrc keeps its own clock-based timestamps.
        QCOMPARE(args.count(QStringLiteral("do-timestamp=true")), 0);
        // Two source queues and the encoder queue hold ~3 s while the video
        // encoder looks ahead.
        QCOMPARE(args.count(QStringLiteral("max-size-time=3000000000")), 3);
        QCOMPARE(args.count(QStringLiteral("buffer-time=500000")), 2);
        QCOMPARE(args.count(QStringLiteral("latency-time=20000")), 2);
        QCOMPARE(args.count(QStringLiteral("slave-method=resample")), 2);
        QVERIFY(args.count(QStringLiteral("audio/x-raw,rate=48000,channels=2")) >= 3);
    }

    void stabilizesAStandalonePulseAudioSourceWithoutAddingAMixer()
    {
        const QStringList args = waylandRecordingAudioArguments(
            true, 80, QStringLiteral("@DEFAULT_MONITOR@"),
            false, 0, QString(), QStringLiteral("fdkaacenc"));

        QVERIFY(!args.contains(QStringLiteral("audiomixer")));
        QVERIFY(!args.contains(QStringLiteral("do-timestamp=true")));
        QVERIFY(args.contains(QStringLiteral("device=@DEFAULT_MONITOR@")));
        QCOMPARE(args.last(), QStringLiteral("mux."));
        QVERIFY(args.contains(QStringLiteral("slave-method=resample")));
        QVERIFY(args.contains(QStringLiteral("audio/x-raw,rate=48000,channels=2")));
    }

    void usesPulseAudioDefaultsThatWorkOnPulseAndPipeWire()
    {
        QCOMPARE(linuxDesktopAudioDevice(QString()), QStringLiteral("@DEFAULT_MONITOR@"));
        QCOMPARE(linuxDesktopAudioDevice(QStringLiteral("__wasapi__")), QStringLiteral("@DEFAULT_MONITOR@"));
        QCOMPARE(linuxDesktopAudioDevice(QStringLiteral("@DEFAULT_SINK@.monitor")),
                 QStringLiteral("@DEFAULT_MONITOR@"));
        QCOMPARE(linuxDesktopAudioDevice(QStringLiteral("alsa_output.x.monitor")),
                 QStringLiteral("alsa_output.x.monitor"));
    }

    void resolvesTheDefaultMicrophoneAndReportsAMissingOne()
    {
        const QList<QPair<QString, QString>> mics = {
            qMakePair(QStringLiteral("Headset"), QStringLiteral("alsa_input.headset"))};

        LinuxMicrophoneResolution resolved = resolveLinuxMicrophoneDevice(QStringLiteral("default"), mics, true);
        QCOMPARE(resolved.device, QStringLiteral("@DEFAULT_SOURCE@"));
        QVERIFY(!resolved.missing);

        resolved = resolveLinuxMicrophoneDevice(QString(), {}, true);
        QCOMPARE(resolved.device, QStringLiteral("@DEFAULT_SOURCE@"));
        QVERIFY(resolved.missing);

        // Without pactl the default source is used unverified.
        resolved = resolveLinuxMicrophoneDevice(QStringLiteral("default"), {}, false);
        QVERIFY(!resolved.missing);

        resolved = resolveLinuxMicrophoneDevice(QStringLiteral("alsa_input.headset"), mics, true);
        QCOMPARE(resolved.device, QStringLiteral("alsa_input.headset"));
        QVERIFY(!resolved.missing);
        QVERIFY(resolveLinuxMicrophoneDevice(QStringLiteral("unplugged"), mics, true).missing);
    }

    void parsesTheGstInspectCatalog()
    {
        const QString catalog = QStringLiteral(
            "pipewire:  pipewiresrc: PipeWire source\n"
            "x264:  x264enc: x264 H.264 Encoder\n"
            "coretracers: latency (GstTracerFactory)\n"
            "\nTotal count: 2 plugins, 2 features\n");
        const QStringList names = gstElementNamesFromInspect(catalog);
        QVERIFY(names.contains(QStringLiteral("pipewiresrc")));
        QVERIFY(names.contains(QStringLiteral("x264enc")));
        QCOMPARE(names.size(), 2);
    }

    void fallsBackToOpenH264WhenX264IsMissing()
    {
        QCOMPARE(preferredGstH264Encoder({QStringLiteral("x264enc"), QStringLiteral("openh264enc")}),
                 QStringLiteral("x264enc"));
        QCOMPARE(preferredGstH264Encoder({QStringLiteral("openh264enc")}),
                 QStringLiteral("openh264enc"));
        QVERIFY(preferredGstH264Encoder({QStringLiteral("mp4mux")}).isEmpty());
    }

    void reportsMissingGstreamerElementsByName()
    {
        const QStringList video = {
            QStringLiteral("pipewiresrc"), QStringLiteral("videorate"), QStringLiteral("videocrop"),
            QStringLiteral("videoconvert"), QStringLiteral("h264parse"), QStringLiteral("mp4mux"),
            QStringLiteral("splitmuxsrc"), QStringLiteral("openh264enc")};
        QVERIFY(missingPortalRecordingElements(video, false, false, true).isEmpty());

        QStringList withoutEncoder = video;
        withoutEncoder.removeAll(QStringLiteral("openh264enc"));
        withoutEncoder.removeAll(QStringLiteral("splitmuxsrc"));
        QCOMPARE(missingPortalRecordingElements(withoutEncoder, false, false, false),
                 QStringList{QStringLiteral("x264enc")});
        QCOMPARE(missingPortalRecordingElements(withoutEncoder, false, false, true),
                 (QStringList{QStringLiteral("splitmuxsrc"), QStringLiteral("x264enc")}));

        const QStringList audioMissing = missingPortalRecordingElements(video, true, true, true);
        QVERIFY(audioMissing.contains(QStringLiteral("pulsesrc")));
        QVERIFY(audioMissing.contains(QStringLiteral("audiomixer")));
        QVERIFY(audioMissing.last().contains(QStringLiteral("avenc_aac")));

        QCOMPARE(gstMissingElementsReason({QStringLiteral("x264enc"), QStringLiteral("mp4mux")}),
                 QStringLiteral("missing gstreamer elements: x264enc, mp4mux"));
        QCOMPARE(portalCanceledReason(), QStringLiteral("portal canceled"));
    }

    void detectsThePlayingStateInGstLaunchOutput()
    {
        QVERIFY(!gstPipelineReachedPlaying(QStringLiteral("Setting pipeline to PAUSED ...\n")));
        QVERIFY(gstPipelineReachedPlaying(QStringLiteral(
            "Setting pipeline to PAUSED ...\nPipeline is live and does not need PREROLL ...\n"
            "Setting pipeline to PLAYING ...\n")));
    }

    void buildsTheCaptureBranchCheapestStepFirst()
    {
        PortalVideoPipelineOptions options;
        options.pipewireFd = 42;
        options.sourcePath = QStringLiteral("target-object=9001");
        options.crop = portalCropGeometry(QRect(10, 20, 641, 480), QRect(), QPoint(0, 0),
                                          QSize(1920, 1080), QSize(641, 480));
        options.fps = 30;
        options.crf = 24;
        const QStringList args = portalVideoBranchArguments(options);

        QCOMPARE(args.first(), QStringLiteral("pipewiresrc"));
        QVERIFY(args.contains(QStringLiteral("fd=42")));
        QVERIFY(args.contains(QStringLiteral("target-object=9001")));
        QVERIFY(args.contains(QStringLiteral("keepalive-time=50")));
        QVERIFY(args.contains(QStringLiteral("on-disconnect=eos")));
        const qsizetype rate = args.indexOf(QStringLiteral("videorate"));
        const qsizetype crop = args.indexOf(QStringLiteral("videocrop"));
        const qsizetype convert = args.indexOf(QStringLiteral("videoconvert"));
        const qsizetype encoder = args.indexOf(QStringLiteral("x264enc"));
        QVERIFY(args.indexOf(QStringLiteral("queue")) < rate);
        QVERIFY(rate < crop && crop < convert && convert < encoder);
        QCOMPARE(args.at(rate + 1), QStringLiteral("drop-only=true"));
        QCOMPARE(args.at(rate + 2), QStringLiteral("max-rate=30"));
        QVERIFY(!args.contains(QStringLiteral("videoscale")));
        QVERIFY(args.contains(QStringLiteral("right=%1").arg(1920 - 10 - 642)));
        QVERIFY(args.contains(QStringLiteral("video/x-raw,format=I420,width=642,height=480,framerate=30/1")));
        QVERIFY(!args.contains(QStringLiteral("tune=zerolatency")));
        QCOMPARE(args.last(), QStringLiteral("h264parse"));

        options.crop = portalCropGeometry(QRect(0, 0, 1920, 1080), QRect(), QPoint(0, 0),
                                          QSize(1920, 1080), QSize(1280, 720));
        const QStringList scaled = portalVideoBranchArguments(options);
        QVERIFY(scaled.indexOf(QStringLiteral("videoscale")) > scaled.indexOf(QStringLiteral("videoconvert")));
    }

    void wrapsTheBranchInAFragmentedMp4()
    {
        PortalVideoPipelineOptions options;
        options.pipewireFd = 3;
        options.crop = portalCropGeometry(QRect(0, 0, 640, 480), QRect(), QPoint(0, 0),
                                          QSize(640, 480), QSize(640, 480));
        const QStringList audio = {QStringLiteral("audiotestsrc"), QStringLiteral("!"),
                                   QStringLiteral("mux.")};
        const QStringList args = portalRecordingPipelineArguments(options, QStringLiteral("/v/out.mp4"), audio);
        QCOMPARE(args.first(), QStringLiteral("-e"));
        QCOMPARE(args.at(1), QStringLiteral("mp4mux"));
        QVERIFY(args.contains(QStringLiteral("fragment-duration=1000")));
        QVERIFY(args.contains(QStringLiteral("buffer-mode=unbuffered")));
        QVERIFY(args.contains(QStringLiteral("location=/v/out.mp4")));
        QCOMPARE(args.mid(args.size() - audio.size()), audio);
    }

    void tunesX264ForRecordingQuality()
    {
        const QStringList normal = gstH264EncoderArguments(
            QStringLiteral("x264enc"), 30, 24, QSize(1920, 1080), false);
        QCOMPARE(normal, (QStringList{
            QStringLiteral("x264enc"), QStringLiteral("speed-preset=veryfast"),
            QStringLiteral("pass=qual"), QStringLiteral("quantizer=24"),
            QStringLiteral("rc-lookahead=10"), QStringLiteral("bframes=2"),
            QStringLiteral("key-int-max=60")}));
        QVERIFY(gstH264EncoderArguments(QStringLiteral("x264enc"), 60, 24, QSize(1280, 720), false)
                    .contains(QStringLiteral("speed-preset=superfast")));
        QVERIFY(gstH264EncoderArguments(QStringLiteral("x264enc"), 30, 24, QSize(3840, 2160), false)
                    .contains(QStringLiteral("speed-preset=superfast")));
        QVERIFY(gstH264EncoderArguments(QStringLiteral("x264enc"), 30, 40, QSize(640, 480), false)
                    .contains(QStringLiteral("quantizer=32")));

        const QStringList lossless = gstH264EncoderArguments(
            QStringLiteral("x264enc"), 15, 24, QSize(800, 600), true);
        QVERIFY(lossless.contains(QStringLiteral("pass=quant")));
        QVERIFY(lossless.contains(QStringLiteral("quantizer=0")));
        QVERIFY(lossless.contains(QStringLiteral("speed-preset=ultrafast")));
        QCOMPARE(portalEncoderInputFormat(QStringLiteral("x264enc"), true, QSize(800, 600)),
                 QStringLiteral("Y444"));
        QCOMPARE(portalEncoderInputFormat(QStringLiteral("x264enc"), true, QSize(3840, 2160)),
                 QStringLiteral("I420"));
        QCOMPARE(portalEncoderInputFormat(QStringLiteral("openh264enc"), true, QSize(800, 600)),
                 QStringLiteral("I420"));

        const QStringList openh264 = gstH264EncoderArguments(
            QStringLiteral("openh264enc"), 30, 24, QSize(1920, 1080), false);
        QCOMPARE(openh264.first(), QStringLiteral("openh264enc"));
        QVERIFY(openh264.contains(QStringLiteral("gop-size=60")));
        QVERIFY(openh264.contains(QStringLiteral("usage-type=screen")));
    }

    void timingHelpersStayInRange()
    {
        QCOMPARE(pipeWireKeepaliveMs(30), 50);
        QCOMPARE(pipeWireKeepaliveMs(10), 100);
        QCOMPARE(pipeWireKeepaliveMs(1), 1000);
        QCOMPARE(gstStopGraceMs(0), 15000);
        QVERIFY(gstStopGraceMs(30LL * 60 * 1000) > 15000);
        QCOMPARE(gstStopGraceMs(100LL * 60 * 60 * 1000), 60000);
    }
};

QTEST_APPLESS_MAIN(LinuxRecordingSupportTests)
#include "LinuxRecordingSupportTests.moc"
