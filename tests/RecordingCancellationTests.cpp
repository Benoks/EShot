#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <atomic>
#include <thread>

// Seed the encoder boundary without asking for screen/portal access. The
// cancellation implementation and process-finished handlers remain real.
#include "recording/ScreenRecorder.h"
#include "recording/VideoRecorder.h"

class RecordingCancellationTests : public QObject
{
    Q_OBJECT

    static QProcess *encoder(QObject *parent)
    {
        auto *process = new QProcess(parent);
        process->start(QCoreApplication::applicationFilePath(), {QStringLiteral("--encoder-child")});
        if (!process->waitForStarted())
            qFatal("Could not start encoder child");
        return process;
    }

private slots:
    void videoCancelCompletesAfterEncoderCleanup_data()
    {
        QTest::addColumn<bool>("portal");
        QTest::addColumn<bool>("starting");
        QTest::newRow("ffmpeg-running") << false << false;
        QTest::newRow("ffmpeg-starting") << false << true;
        QTest::newRow("portal-running") << true << false;
        QTest::newRow("portal-starting") << true << true;
    }

    void videoCancelCompletesAfterEncoderCleanup()
    {
        QFETCH(bool, portal);
        QFETCH(bool, starting);
        VideoRecorder recorder;
        QSignalSpy canceled(&recorder, &VideoRecorder::recordingCanceled);
        bool inactiveAtCompletion = false;
        connect(&recorder, &VideoRecorder::recordingCanceled, &recorder, [&]() {
            inactiveAtCompletion = !recorder.isRecording() && !recorder.isPaused()
                && !recorder.isFinalizing() && recorder.m_process == nullptr;
        });
        QSignalSpy started(&recorder, &VideoRecorder::recordingStarted);
        QSignalSpy saved(&recorder, &VideoRecorder::recordingStopped);
        QSignalSpy failed(&recorder, &VideoRecorder::recordingFailed);
        QTemporaryDir dir;
        recorder.m_segmentPath = dir.filePath(QStringLiteral("segment.mp4"));
        recorder.m_recording = !starting;
        recorder.m_starting = starting && !portal;
        recorder.m_gstStarting = starting && portal;
        if (portal) recorder.m_recording = true;
        recorder.m_usesGStreamer = portal;
        recorder.m_process = encoder(&recorder);
        connect(recorder.m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                &recorder, &VideoRecorder::onProcessFinished);
        recorder.cancel();
        // Progress already queued by the dying encoder must not restart UI.
        if (starting) {
            if (portal) recorder.onGstSegmentPlaying();
            else recorder.handleFfmpegLine(QStringLiteral("progress=continue"));
        }
        QTRY_COMPARE(canceled.count(), 1);
        QCOMPARE(started.count(), 0);
        QVERIFY(inactiveAtCompletion);
        QVERIFY(!recorder.isRecording());
        QVERIFY(!recorder.isPaused());
        QVERIFY(!recorder.isFinalizing());
        QVERIFY(recorder.m_process == nullptr);
        QCOMPARE(saved.count(), 0);
        QCOMPARE(failed.count(), 0);
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
    }

    void videoPausedCancelCompletesWithoutEncoder()
    {
        VideoRecorder recorder;
        QSignalSpy canceled(&recorder, &VideoRecorder::recordingCanceled);
        recorder.m_recording = true;
        recorder.m_paused = true;
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
        QVERIFY(!recorder.isRecording());
        QVERIFY(!recorder.isPaused());
    }

    void videoFinalizationCancelCompletes_data()
    {
        QTest::addColumn<bool>("mux");
        QTest::newRow("mux") << true;
        QTest::newRow("concat") << false;
    }

    void videoFinalizationCancelCompletes()
    {
        QFETCH(bool, mux);
        VideoRecorder recorder;
        QSignalSpy canceled(&recorder, &VideoRecorder::recordingCanceled);
        QTemporaryDir dir;
        recorder.m_outputPath = dir.filePath(QStringLiteral("discard.mp4"));
        QFile output(recorder.m_outputPath);
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write("partial recording");
        output.close();
        auto *process = encoder(&recorder);
        if (mux)
            recorder.m_muxProcess = process;
        else
            recorder.m_concatProcess = process;
        QVERIFY(recorder.isFinalizing());
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
        QVERIFY(!recorder.isRecording());
        QVERIFY(!recorder.isFinalizing());
        QVERIFY(!QFile::exists(recorder.m_outputPath));
        if (process->state() != QProcess::NotRunning)
            QVERIFY(process->waitForFinished(3000));
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
    }

    void gifFinalizationCancelCompletes()
    {
        ScreenRecorder recorder;
        QSignalSpy canceled(&recorder, &ScreenRecorder::recordingCanceled);
        auto *process = encoder(&recorder);
        recorder.m_conversionProcess = process;
        QVERIFY(recorder.isFinalizing());
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
        QVERIFY(!recorder.isRecording());
        QVERIFY(!recorder.isFinalizing());
        if (process->state() != QProcess::NotRunning)
            QVERIFY(process->waitForFinished(3000));
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
    }

    void gifCancelCompletesAfterCleanup()
    {
        ScreenRecorder recorder;
        QSignalSpy canceled(&recorder, &ScreenRecorder::recordingCanceled);
        QSignalSpy saved(&recorder, &ScreenRecorder::recordingStopped);
        QSignalSpy failed(&recorder, &ScreenRecorder::recordingFailed);
        recorder.m_recording = true;
        recorder.m_portalRecording = true;
        recorder.m_paused = true;
        auto *process = encoder(&recorder);
        recorder.m_process = process;
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                &recorder, &ScreenRecorder::onPortalProcessFinished);
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
        QVERIFY(!recorder.isRecording());
        QVERIFY(!recorder.isPaused());
        QVERIFY(!recorder.isFinalizing());
        QVERIFY(recorder.m_process == nullptr);
        if (process->state() != QProcess::NotRunning)
            QVERIFY(process->waitForFinished(3000));
        QCoreApplication::processEvents();
        QCOMPARE(saved.count(), 0);
        QCOMPARE(failed.count(), 0);
        recorder.cancel();
        QCOMPARE(canceled.count(), 1);
    }

};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--encoder-child")))
        return app.exec();
    RecordingCancellationTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "RecordingCancellationTests.moc"
