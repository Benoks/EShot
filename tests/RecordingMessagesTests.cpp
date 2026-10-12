#include <QtTest>

#include <QMimeData>
#include <QUrl>

#include <memory>

#include "core/TranslationManager.h"
#include "recording/RecordingFailureMessages.h"
#include "recording/RecordingSavedNotification.h"

class RecordingMessagesTests : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        TranslationManager::setLanguage(TranslationManager::English, false);
    }

    void mapsEveryKnownRecorderReason_data()
    {
        QTest::addColumn<QString>("reason");
        QTest::addColumn<QString>("key");
        const QList<QPair<const char *, const char *>> rows = {
            {"ffmpeg.exe not found", "videoFfmpegMissing"},
            {"ffmpeg not found", "videoFfmpegMissing"},
            {"gstreamer not found", "videoGstreamerMissing"},
            {"Wayland ScreenCast portal is not available", "videoWaylandPortalMissing"},
            {"Wayland screen recording permission was not granted", "videoWaylandPermissionDenied"},
            {"Wayland recording source does not contain the selected region", "videoWaylandWrongSource"},
            {"cannot start gstreamer", "videoGstreamerStartFailed"},
            {"Wayland PipeWire remote could not be opened", "videoPipeWireRemoteFailed"},
            {"region too small", "recFailRegionTooSmall"},
            {"no frames captured", "recFailNoFrames"},
            {"no video was recorded", "recFailNoVideo"},
            {"cannot create output directory", "recFailOutputDir"},
            {"cannot start ffmpeg", "recFailFfmpegStart"},
            {"failed to mux system audio", "recFailAudioMux"},
            {"No GStreamer AAC encoder is installed", "recFailAacEncoder"},
            {"cannot initialize screen capture", "recFailCaptureInit"},
            {"already recording", "recFailAlreadyRecording"},
            {"failed to convert portal video to GIF", "recFailGifConvert"},
            {"gstreamer pipeline exited during startup", "recFailGstStartup"},
        };
        for (const auto &row : rows)
            QTest::newRow(row.first) << QString::fromLatin1(row.first) << QString::fromLatin1(row.second);
    }

    void mapsEveryKnownRecorderReason()
    {
        QFETCH(QString, reason);
        QFETCH(QString, key);
        const RecordingFailureMessage message = recordingFailureMessage(reason);
        QVERIFY(!message.silent);
        QVERIFY(!message.unexpected);
        QCOMPARE(message.text, TranslationManager::tr(key.toUtf8().constData()));
        QVERIFY(message.text != key);
    }

    void localizesKnownReasonsInTurkish()
    {
        TranslationManager::setLanguage(TranslationManager::Turkish, false);
        QCOMPARE(recordingFailureMessage(QStringLiteral("region too small")).text,
                 QString::fromUtf8("Seçilen alan kaydetmek için çok küçük."));
    }

    void mapsProcessExitCodes()
    {
        const RecordingFailureMessage ffmpeg =
            recordingFailureMessage(QStringLiteral("ffmpeg exited with code 1"));
        QVERIFY(!ffmpeg.unexpected);
        QCOMPARE(ffmpeg.text, QStringLiteral(
            "FFmpeg stopped unexpectedly (exit code 1). Details are in the log."));
        const RecordingFailureMessage gst =
            recordingFailureMessage(QStringLiteral("gstreamer exited with code -9"));
        QCOMPARE(gst.text, QStringLiteral(
            "GStreamer stopped unexpectedly (exit code -9). Details are in the log."));
    }

    void outputFileErrorsHideTheRawPath()
    {
        const RecordingFailureMessage message = recordingFailureMessage(
            QStringLiteral("cannot open output: /tmp/x.gif (Permission denied)"));
        QCOMPARE(message.text, TranslationManager::tr("recFailOutputFile"));
    }

    void keepsDetailLinesOfAKnownReason()
    {
        const RecordingFailureMessage message = recordingFailureMessage(
            QStringLiteral("failed to convert portal video to GIF\n")
            + TranslationManager::tr("recVideoKeptAt").arg(QStringLiteral("/tmp/a.mp4")));
        QCOMPARE(message.text,
                 TranslationManager::tr("recFailGifConvert") + QStringLiteral("\n")
                     + QStringLiteral("The recorded video was kept at: /tmp/a.mp4"));
    }

    void summarizesRawMultiLineOutputButKeepsKeptFiles()
    {
        const QString kept = TranslationManager::tr("recVideoKeptAt").arg(QStringLiteral("/tmp/b.mp4"));
        const QString raw = QStringLiteral(
            "ERROR: from element /GstPipeline:pipeline0/GstPipeWireSrc:pipewiresrc0: stream error\n"
            "Additional debug info:\n"
            "../src/gst/gstpipewiresrc.c(1125): gst_pipewire_src_create ()\n")
            + kept;
        const RecordingFailureMessage message = recordingFailureMessage(raw);
        QVERIFY(message.unexpected);
        QVERIFY(!message.silent);
        const QStringList lines = message.text.split(QLatin1Char('\n'));
        QCOMPARE(lines.size(), 2);
        QVERIFY(lines.first().startsWith(QStringLiteral("The recorder stopped unexpectedly (ERROR: from element")));
        QVERIFY(lines.first().endsWith(QStringLiteral("). Details are in the log.")));
        QVERIFY(!message.text.contains(QStringLiteral("Additional debug info")));
        QCOMPARE(lines.last(), kept);
    }

    void truncatesLongRawReasons()
    {
        const QString longLine(300, QLatin1Char('x'));
        const RecordingFailureMessage message = recordingFailureMessage(longLine);
        QVERIFY(message.unexpected);
        const QString summary = recordingFailureSummaryLine(longLine);
        QCOMPARE(summary.size(), 120);
        QVERIFY(summary.endsWith(QChar(0x2026)));
        QCOMPARE(message.text, TranslationManager::tr("recFailUnexpected").arg(summary));
    }

    void reportsMissingGStreamerElements()
    {
        const RecordingFailureMessage message = recordingFailureMessage(
            QStringLiteral("missing gstreamer elements: pipewiresrc, x264enc"));
        QVERIFY(!message.unexpected);
        QCOMPARE(message.text, QStringLiteral(
            "Missing GStreamer components: pipewiresrc, x264enc. Install the GStreamer plugin "
            "packages (good, bad, ugly, libav) and try again."));
    }

    void staysSilentWhenThePortalWasCanceled()
    {
        const RecordingFailureMessage message =
            recordingFailureMessage(QStringLiteral("portal canceled"));
        QVERIFY(message.silent);
        QVERIFY(message.text.isEmpty());
        QVERIFY(recordingFailureMessage(QStringLiteral("Portal Canceled\nby user")).silent);
    }

    void keepsAlreadyLocalizedReasons()
    {
        const QString joined = TranslationManager::tr("recPartsNotJoined").arg(QStringLiteral("timed out"))
            + QStringLiteral("\n")
            + TranslationManager::tr("recVideoKeptAt").arg(QStringLiteral("/tmp/p1.mp4"));
        const RecordingFailureMessage message = recordingFailureMessage(joined);
        QVERIFY(!message.unexpected);
        QCOMPARE(message.text, joined);

        const RecordingFailureMessage timedOut =
            recordingFailureMessage(TranslationManager::tr("gifConversionTimedOut"));
        QVERIFY(!timedOut.unexpected);
        QCOMPARE(timedOut.text, TranslationManager::tr("gifConversionTimedOut"));
    }

    void emptyReasonIsUnexpected()
    {
        const RecordingFailureMessage message = recordingFailureMessage(QStringLiteral("\n  \n"));
        QVERIFY(message.unexpected);
        QVERIFY(!message.text.isEmpty());
    }

    void formatsFileSizes()
    {
        QCOMPARE(formatRecordingFileSize(0), QStringLiteral("1 KB"));
        QCOMPARE(formatRecordingFileSize(512 * 1024), QStringLiteral("512 KB"));
        QCOMPARE(formatRecordingFileSize(12 * 1024 * 1024 + 300 * 1024), QStringLiteral("12.3 MB"));
        QCOMPARE(formatRecordingFileSize(qint64(3) * 1024 * 1024 * 1024 / 2), QStringLiteral("1.5 GB"));
        QCOMPARE(formatRecordingFileSize(12 * 1024 * 1024 + 300 * 1024, QLocale(QLocale::Turkish)),
                 QStringLiteral("12,3 MB"));
    }

    void savedNotificationShowsKindSizeAndFileNameOnly()
    {
        QCOMPARE(recordingSavedNotificationText(RecordingSavedKind::Video,
                                                QStringLiteral("/home/u/Videos/EShot_Video_1.mp4"),
                                                5 * 1024 * 1024),
                 QString::fromUtf8("Video saved · 5.0 MB\nEShot_Video_1.mp4"));
        QCOMPARE(recordingSavedNotificationText(RecordingSavedKind::Gif,
                                                QStringLiteral("/home/u/GIFs/EShot_GIF_1.gif"),
                                                200 * 1024),
                 QString::fromUtf8("GIF saved · 200 KB\nEShot_GIF_1.gif"));
        TranslationManager::setLanguage(TranslationManager::German, false);
        QCOMPARE(recordingSavedNotificationText(RecordingSavedKind::Gif,
                                                QStringLiteral("/tmp/a.gif"), 1024 * 1024,
                                                QLocale(QLocale::German)),
                 QString::fromUtf8("GIF gespeichert · 1,0 MB\na.gif"));
    }

    void copiedFileMimeDataCarriesTheFileUrl()
    {
        const std::unique_ptr<QMimeData> data(
            recordingFileMimeData(QStringLiteral("/tmp/EShot/video one.mp4")));
        QVERIFY(data->hasUrls());
        QCOMPARE(data->urls(), QList<QUrl>({QUrl::fromLocalFile(QStringLiteral("/tmp/EShot/video one.mp4"))}));
        QVERIFY(data->formats().contains(QStringLiteral("text/uri-list")));
        QCOMPARE(data->data(QStringLiteral("x-special/gnome-copied-files")),
                 QByteArray("copy\nfile:///tmp/EShot/video%20one.mp4"));
    }
};

QTEST_APPLESS_MAIN(RecordingMessagesTests)
#include "RecordingMessagesTests.moc"
