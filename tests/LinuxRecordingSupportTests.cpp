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
        // Timestamp-less keepalive frames must be dropped, not passed on.
        QVERIFY(!args.contains(QStringLiteral("drop-only=true")));
        QCOMPARE(args.at(rate + 1), QStringLiteral("skip-to-first=true"));
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

    void listsHardwareEncodersInPreferenceOrder()
    {
        QCOMPARE(hardwareGstH264Encoders(), (QStringList{QStringLiteral("vah264enc"),
                                                         QStringLiteral("vah264lpenc"),
                                                         QStringLiteral("nvh264enc")}));
        QVERIFY(isHardwareGstH264Encoder(QStringLiteral("nvh264enc")));
        QVERIFY(!isHardwareGstH264Encoder(QStringLiteral("x264enc")));
        QVERIFY(!isHardwareGstH264Encoder(QStringLiteral("openh264enc")));

        const QStringList catalog = {
            QStringLiteral("va:  vah264enc: VA-API H.264 Encoder"),
            QStringLiteral("nvh264enc"), QStringLiteral("cudaupload"),
            QStringLiteral("x264enc")};
        // vah264enc without vapostproc and nvh264enc without cudaconvertscale
        // cannot be fed.
        QVERIFY(installedHardwareGstH264Encoders(catalog).isEmpty());
        QCOMPARE(installedHardwareGstH264Encoders(catalog + QStringList{
                     QStringLiteral("vapostproc"), QStringLiteral("cudaconvertscale")}),
                 (QStringList{QStringLiteral("vah264enc"), QStringLiteral("nvh264enc")}));

        const QStringList optional = optionalPortalRecordingElements();
        QVERIFY(optional.contains(QStringLiteral("vapostproc")));
        QVERIFY(optional.contains(QStringLiteral("cudaconvertscale")));
        QCOMPARE(optional.count(QStringLiteral("vapostproc")), 1);
        QVERIFY(!optional.contains(QStringLiteral("x264enc")));
    }

    void hardwareEncodersAreOptionalInThePreflight()
    {
        const QStringList base = {
            QStringLiteral("pipewiresrc"), QStringLiteral("videorate"), QStringLiteral("videocrop"),
            QStringLiteral("videoconvert"), QStringLiteral("h264parse"), QStringLiteral("mp4mux"),
            QStringLiteral("splitmuxsrc")};
        // Hardware alone is not enough: the software fallback is required.
        QCOMPARE(missingPortalRecordingElements(base + QStringList{QStringLiteral("vah264enc"),
                                                                   QStringLiteral("vapostproc")},
                                                false, false, true),
                 QStringList{QStringLiteral("x264enc")});
        QVERIFY(missingPortalRecordingElements(base + QStringList{QStringLiteral("x264enc")},
                                               false, false, true).isEmpty());
    }

    void selectsAWorkingHardwareEncoderForVideoOnly()
    {
        const QSize hd(1920, 1080);
        const QStringList both = {QStringLiteral("nvh264enc"), QStringLiteral("vah264enc")};
        QCOMPARE(selectPortalH264Encoder(both, QStringLiteral("x264enc"), true, false, hd),
                 QStringLiteral("vah264enc"));
        QCOMPARE(selectPortalH264Encoder({QStringLiteral("nvh264enc")}, QStringLiteral("x264enc"),
                                         true, false, hd),
                 QStringLiteral("nvh264enc"));
        // Setting off, GIF intermediate, nothing probed yet, unsupported sizes.
        QCOMPARE(selectPortalH264Encoder(both, QStringLiteral("x264enc"), false, false, hd),
                 QStringLiteral("x264enc"));
        QCOMPARE(selectPortalH264Encoder(both, QStringLiteral("x264enc"), true, true, hd),
                 QStringLiteral("x264enc"));
        QCOMPARE(selectPortalH264Encoder({}, QStringLiteral("openh264enc"), true, false, hd),
                 QStringLiteral("openh264enc"));
        QCOMPARE(selectPortalH264Encoder({}, QString(), true, false, hd), QStringLiteral("x264enc"));
        QCOMPARE(selectPortalH264Encoder(both, QStringLiteral("x264enc"), true, false, QSize(100, 300)),
                 QStringLiteral("x264enc"));
        QCOMPARE(selectPortalH264Encoder(both, QStringLiteral("x264enc"), true, false, QSize(150, 150)),
                 QStringLiteral("vah264enc"));
        QCOMPARE(selectPortalH264Encoder({QStringLiteral("nvh264enc")}, QStringLiteral("x264enc"),
                                         true, false, QSize(150, 150)),
                 QStringLiteral("x264enc"));
        QCOMPARE(selectPortalH264Encoder(both, QStringLiteral("x264enc"), true, false, QSize(5120, 1440)),
                 QStringLiteral("x264enc"));
        QVERIFY(gstH264EncoderSupportsSize(QStringLiteral("x264enc"), QSize(5120, 1440)));
        QVERIFY(gstH264EncoderSupportsSize(QStringLiteral("nvh264enc"), QSize(160, 64)));
        QVERIFY(!gstH264EncoderSupportsSize(QStringLiteral("vah264enc"), QSize(160, 64)));
    }

    void buildsTheVaapiBranch()
    {
        PortalVideoPipelineOptions options;
        options.pipewireFd = 7;
        options.crop = portalCropGeometry(QRect(10, 20, 1279, 719), QRect(), QPoint(0, 0),
                                          QSize(1920, 1080), QSize(1279, 719));
        options.fps = 30;
        options.crf = 24;
        options.h264Encoder = QStringLiteral("vah264enc");
        const QStringList args = portalVideoBranchArguments(options);
        const qsizetype crop = args.indexOf(QStringLiteral("videocrop"));
        const qsizetype post = args.indexOf(QStringLiteral("vapostproc"));
        const qsizetype encoder = args.indexOf(QStringLiteral("vah264enc"));
        QVERIFY(crop > 0 && crop < post && post < encoder);
        QVERIFY(!args.contains(QStringLiteral("videoconvert")));
        QVERIFY(args.contains(QStringLiteral(
            "video/x-raw(memory:VAMemory),format=NV12,width=1280,height=720,framerate=30/1")));
        QCOMPARE(args.mid(encoder, 6), (QStringList{
            QStringLiteral("vah264enc"), QStringLiteral("rate-control=cqp"),
            QStringLiteral("qpi=24"), QStringLiteral("qpp=26"), QStringLiteral("qpb=28"),
            QStringLiteral("key-int-max=60")}));
        QCOMPARE(args.last(), QStringLiteral("h264parse"));

        // vapostproc also scales; no CPU videoscale.
        options.crop = portalCropGeometry(QRect(0, 0, 1920, 1080), QRect(), QPoint(0, 0),
                                          QSize(1920, 1080), QSize(1280, 720));
        const QStringList scaled = portalVideoBranchArguments(options);
        QVERIFY(!scaled.contains(QStringLiteral("videoscale")));
        QVERIFY(scaled.contains(QStringLiteral(
            "video/x-raw(memory:VAMemory),format=NV12,width=1280,height=720,framerate=30/1")));
    }

    void buildsTheNvencBranch()
    {
        PortalVideoPipelineOptions options;
        options.pipewireFd = 7;
        options.crop = portalCropGeometry(QRect(0, 0, 1920, 1080), QRect(), QPoint(0, 0),
                                          QSize(1920, 1080), QSize(1920, 1080));
        options.fps = 60;
        options.crf = 30;
        options.h264Encoder = QStringLiteral("nvh264enc");
        const QStringList args = portalVideoBranchArguments(options);
        const qsizetype upload = args.indexOf(QStringLiteral("cudaupload"));
        const qsizetype convert = args.indexOf(QStringLiteral("cudaconvertscale"));
        const qsizetype encoder = args.indexOf(QStringLiteral("nvh264enc"));
        QVERIFY(args.indexOf(QStringLiteral("videocrop")) < upload);
        QVERIFY(upload < convert && convert < encoder);
        QVERIFY(args.contains(QStringLiteral(
            "video/x-raw(memory:CUDAMemory),format=NV12,width=1920,height=1080,framerate=60/1")));
        QVERIFY(args.contains(QStringLiteral("rc-mode=vbr")));
        QVERIFY(args.contains(QStringLiteral("const-quality=32")));
        QVERIFY(args.contains(QStringLiteral("gop-size=120")));
        QVERIFY(args.contains(QStringLiteral("preset=p2")));
        QVERIFY(args.contains(QStringLiteral("video/x-h264,profile=high")));
        QCOMPARE(args.last(), QStringLiteral("h264parse"));

        QCOMPARE(hardwareH264QpForCrf(10), 20);
        QCOMPARE(hardwareH264QpForCrf(24), 26);
        QCOMPARE(hardwareH264QpForCrf(40), 34);
    }

    void probesWithTheRecordingChain()
    {
        const QStringList va = gstH264EncoderProbeArguments(QStringLiteral("vah264enc"));
        QCOMPARE(va.first(), QStringLiteral("-q"));
        QVERIFY(va.contains(QStringLiteral("num-buffers=5")));
        QVERIFY(va.contains(QStringLiteral("video/x-raw,format=BGRx,width=320,height=240,framerate=30/1")));
        QVERIFY(va.contains(QStringLiteral("vapostproc")));
        QVERIFY(va.contains(QStringLiteral("rate-control=cqp")));
        QCOMPARE(va.mid(va.size() - 3), (QStringList{QStringLiteral("h264parse"), QStringLiteral("!"),
                                                     QStringLiteral("fakesink")}));
        const QStringList nv = gstH264EncoderProbeArguments(QStringLiteral("nvh264enc"));
        QVERIFY(nv.contains(QStringLiteral("cudaupload")));
        QVERIFY(nv.contains(QStringLiteral("video/x-h264,profile=high")));
        QVERIFY(gstH264EncoderProbeTimeoutMs() >= 5000);
    }

    void probesUntilTheFirstWorkingEncoder()
    {
        GstH264ProbeState state;
        QCOMPARE(nextGstH264EncoderToProbe(state), QStringLiteral("vah264enc"));
        state.working = {QStringLiteral("vah264enc")};
        // VA-API works: the NVIDIA GPU is never woken for a probe.
        QVERIFY(nextGstH264EncoderToProbe(state).isEmpty());
        state.working.clear();
        state.failed = {QStringLiteral("vah264enc")};
        QCOMPARE(nextGstH264EncoderToProbe(state), QStringLiteral("vah264lpenc"));
        state.failed << QStringLiteral("vah264lpenc");
        QCOMPARE(nextGstH264EncoderToProbe(state), QStringLiteral("nvh264enc"));
        state.failed << QStringLiteral("nvh264enc");
        QVERIFY(nextGstH264EncoderToProbe(state).isEmpty());
        state.failed = {QStringLiteral("vah264enc"), QStringLiteral("vah264lpenc")};
        state.working = {QStringLiteral("nvh264enc")};
        QVERIFY(nextGstH264EncoderToProbe(state).isEmpty());
    }

    void forgetsProbeResultsOfAnotherInstallation()
    {
        const QString signature = gstH264ProbeSignature(
            QStringLiteral("/usr/bin/gst-launch-1.0"), 100, {200, 300}, QStringLiteral("NVRM 580\n"));
        QVERIFY(signature.contains(QStringLiteral("/usr/bin/gst-launch-1.0")));
        QVERIFY(signature.contains(QStringLiteral("NVRM 580")));
        QVERIFY(signature != gstH264ProbeSignature(QStringLiteral("/usr/bin/gst-launch-1.0"), 100,
                                                   {200, 301}, QStringLiteral("NVRM 580")));

        GstH264ProbeState stored{signature, {QStringLiteral("vah264enc")}, {QStringLiteral("nvh264enc")}};
        const GstH264ProbeState same = gstH264ProbeStateForSignature(stored, signature);
        QCOMPARE(same.working, stored.working);
        QCOMPARE(same.failed, stored.failed);
        const GstH264ProbeState other = gstH264ProbeStateForSignature(stored, QStringLiteral("new"));
        QCOMPARE(other.signature, QStringLiteral("new"));
        QVERIFY(other.working.isEmpty());
        QVERIFY(other.failed.isEmpty());
    }

    void retriesAFailedHardwareStartWithSoftware()
    {
        const QString va = QStringLiteral("vah264enc");
        QVERIFY(shouldRetryPortalStartWithSoftwareEncoder(va, true, false, false, false, 300, false));
        QVERIFY(shouldRetryPortalStartWithSoftwareEncoder(va, true, false, false, true, 2500, false));
        QVERIFY(!shouldRetryPortalStartWithSoftwareEncoder(va, true, false, false, true, 60000, false));
        QVERIFY(!shouldRetryPortalStartWithSoftwareEncoder(QStringLiteral("x264enc"),
                                                           true, false, false, false, 300, false));
        QVERIFY(!shouldRetryPortalStartWithSoftwareEncoder(va, false, false, false, false, 300, false));
        QVERIFY(!shouldRetryPortalStartWithSoftwareEncoder(va, true, true, false, false, 300, false));
        QVERIFY(!shouldRetryPortalStartWithSoftwareEncoder(va, true, false, true, false, 300, false));
        QVERIFY(!shouldRetryPortalStartWithSoftwareEncoder(va, true, false, false, false, 300, true));
    }

    void joinsPausedPartsWithFfmpegWhenInstalled()
    {
        QVERIFY(joinPortalSegmentsWithFfmpeg(true));
        QVERIFY(!joinPortalSegmentsWithFfmpeg(false));
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
