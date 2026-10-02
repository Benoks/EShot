#include <QtTest>

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>

#include "recording/RecordingStartCountdown.h"

class RecordingStartCountdownTests : public QObject
{
    Q_OBJECT

private slots:
    void finishesHiddenAfterDelay()
    {
        RecordingStartCountdown countdown(QRect(100, 100, 400, 300), 1000, QStringLiteral("Ctrl+Alt+X"));
        QSignalSpy finished(&countdown, &RecordingStartCountdown::finished);
        QVERIFY(countdown.isVisible());
        QCOMPARE(countdown.remainingSeconds(), 1);

        QVERIFY(finished.wait(3000));
        QCOMPARE(finished.count(), 1);
        QVERIFY(!countdown.isVisible());
        QCOMPARE(countdown.remainingSeconds(), 0);
    }

    void escapeAndCancelButtonRequestCancel()
    {
        RecordingStartCountdown countdown(QRect(100, 100, 400, 300), 5000, QString());
        QSignalSpy canceled(&countdown, &RecordingStartCountdown::cancelRequested);
        QSignalSpy finished(&countdown, &RecordingStartCountdown::finished);

        QTest::keyClick(&countdown, Qt::Key_Escape);
        QCOMPARE(canceled.count(), 1);

        auto *button = countdown.findChild<QPushButton *>(
            QStringLiteral("recordingStartCountdownCancel"));
        QVERIFY(button);
        button->click();
        QCOMPARE(canceled.count(), 2);
        QCOMPARE(finished.count(), 0);
    }
};

QTEST_MAIN(RecordingStartCountdownTests)
#include "RecordingStartCountdownTests.moc"
