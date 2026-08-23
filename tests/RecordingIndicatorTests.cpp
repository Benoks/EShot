#include <QtTest>

#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolButton>

#include "core/TranslationManager.h"
#include "recording/RecordingIndicator.h"

class RecordingIndicatorTests : public QObject
{
    Q_OBJECT

private slots:
    void createsARealRecordingControlBar()
    {
        TranslationManager::setLanguage(TranslationManager::Turkish, false);
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);

        auto *bar = indicator.findChild<QFrame *>(QStringLiteral("recordingControlBar"));
        auto *status = indicator.findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
        auto *pause = indicator.findChild<QToolButton *>(QStringLiteral("recordingPauseButton"));
        auto *stop = indicator.findChild<QPushButton *>(QStringLiteral("recordingStopButton"));
        auto *cancel = indicator.findChild<QToolButton *>(QStringLiteral("recordingCancelButton"));
        auto *details = indicator.findChild<QToolButton *>(QStringLiteral("recordingDetailsButton"));

        QVERIFY(bar);
        QVERIFY(status);
        QVERIFY(pause);
        QVERIFY(stop);
        QVERIFY(cancel);
        QVERIFY(details);
        QCOMPARE(bar->height(), 42);
        QCOMPARE(indicator.property("recordingBorderWidth").toInt(), 2);
        QVERIFY(!pause->icon().isNull());
        QVERIFY(!stop->icon().isNull());
        QVERIFY(!cancel->icon().isNull());
        QVERIFY(!details->icon().isNull());

        const QString style = bar->styleSheet();
        QVERIFY(style.contains(QStringLiteral("background-color: #2d2d2d")));
        QVERIFY(style.contains(QStringLiteral("border: 1px solid #404040")));
        QVERIFY(style.contains(QStringLiteral("background-color: #3a3a3a")));
        QVERIFY(style.contains(QStringLiteral("border: 1px solid #505050")));
        QCOMPARE(stop->text(), QStringLiteral("Durdur"));
        QCOMPARE(details->toolTip(), QString::fromUtf8("Ayrıntılar"));
        TranslationManager::setLanguage(TranslationManager::English, false);
    }

    void buttonsEmitTheirDedicatedSignals()
    {
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        auto *pause = indicator.findChild<QToolButton *>(QStringLiteral("recordingPauseButton"));
        auto *stop = indicator.findChild<QPushButton *>(QStringLiteral("recordingStopButton"));
        auto *cancel = indicator.findChild<QToolButton *>(QStringLiteral("recordingCancelButton"));
        QVERIFY(pause);
        QVERIFY(stop);
        QVERIFY(cancel);

        QSignalSpy pauseSpy(&indicator, &RecordingIndicator::pauseRequested);
        QSignalSpy stopSpy(&indicator, &RecordingIndicator::stopRequested);
        QSignalSpy cancelSpy(&indicator, &RecordingIndicator::cancelRequested);
        QTest::mouseClick(pause, Qt::LeftButton);
        QTest::mouseClick(stop, Qt::LeftButton);
        QTest::mouseClick(cancel, Qt::LeftButton);

        QCOMPARE(pauseSpy.count(), 1);
        QCOMPARE(stopSpy.count(), 1);
        QCOMPARE(cancelSpy.count(), 1);
    }

    void pauseButtonChangesToResumeState()
    {
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        auto *pause = indicator.findChild<QToolButton *>(QStringLiteral("recordingPauseButton"));
        QVERIFY(pause);

        indicator.setPaused(true);
        QCOMPARE(pause->property("recordingState").toString(), QStringLiteral("resume"));

        QSignalSpy resumeSpy(&indicator, &RecordingIndicator::resumeRequested);
        QTest::mouseClick(pause, Qt::LeftButton);
        QCOMPARE(resumeSpy.count(), 1);
    }

    void tooltipsExposeConfiguredRecordingShortcuts()
    {
        TranslationManager::setLanguage(TranslationManager::English, false);
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        indicator.setShortcutHints(QStringLiteral("Ctrl+Alt+P"),
                                   QStringLiteral("Ctrl+Alt+S"),
                                   QStringLiteral("Ctrl+Alt+X"));

        auto *pause = indicator.findChild<QToolButton *>(QStringLiteral("recordingPauseButton"));
        auto *stop = indicator.findChild<QPushButton *>(QStringLiteral("recordingStopButton"));
        auto *cancel = indicator.findChild<QToolButton *>(QStringLiteral("recordingCancelButton"));
        QVERIFY(pause);
        QVERIFY(stop);
        QVERIFY(cancel);
        QCOMPARE(pause->toolTip(), QStringLiteral("Pause / Resume (Ctrl+Alt+P)"));
        QCOMPARE(stop->toolTip(), QStringLiteral("Stop Recording (Ctrl+Alt+S)"));
        QCOMPARE(cancel->toolTip(), QStringLiteral("Cancel (Ctrl+Alt+X)"));
    }

    void pausesCaptureBeforePresentingInsideControls()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        RecordingIndicator indicator(screen->geometry(), nullptr, 2, true,
                                     RecordingIndicatorMode::Video);
        auto *bar = indicator.findChild<QFrame *>(QStringLiteral("recordingControlBar"));
        QVERIFY(bar);
#ifdef Q_OS_LINUX
        QVERIFY(!indicator.requiresCaptureSafePresentation());
        QVERIFY(bar->isVisible());
        QVERIFY(!(indicator.windowFlags() & Qt::WindowStaysOnTopHint));
        QSignalSpy pauseSpy(&indicator, &RecordingIndicator::pauseRequested);
        indicator.startCaptureSafePresentation();
        QCOMPARE(pauseSpy.count(), 0);
        QVERIFY(bar->isVisible());
#else
        QVERIFY(indicator.requiresCaptureSafePresentation());
        QVERIFY(!bar->isVisible());

        QSignalSpy pauseSpy(&indicator, &RecordingIndicator::pauseRequested);
        indicator.startCaptureSafePresentation();
        QCOMPARE(pauseSpy.count(), 0);
        QVERIFY(indicator.property("captureSafeInside").toBool());
        QVERIFY(!bar->isVisible());

        // A deliberate global/tray pause reveals inside controls. Merely
        // moving the pointer over their location must not change recording.
        indicator.setPaused(true);
        QTRY_VERIFY_WITH_TIMEOUT(bar->isVisible(), 200);

        auto *pause = indicator.findChild<QToolButton *>(QStringLiteral("recordingPauseButton"));
        QVERIFY(pause);
        QSignalSpy resumeSpy(&indicator, &RecordingIndicator::resumeRequested);
        QTest::mouseClick(pause, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(resumeSpy.count(), 1, 250);
        QVERIFY(!bar->isVisible());
#endif
    }

    void usesAControlOnlyWindowForFullscreenCapture()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect screenRect = screen->geometry();
        RecordingIndicator indicator(screenRect, nullptr, 2, true,
                                     RecordingIndicatorMode::Video);
        QVERIFY(indicator.geometry().height() < screenRect.height() / 2);
        QVERIFY(indicator.geometry().width() < screenRect.width() / 2);
    }

    void dragsTheControlBarFromItsStatusArea()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect available = screen->geometry().adjusted(80, 80, -80, -220);
        RecordingIndicator indicator(available, nullptr, 2, true,
                                     RecordingIndicatorMode::Video);
        auto *status = indicator.findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
        QVERIFY(status);
        QCOMPARE(status->cursor().shape(), Qt::SizeAllCursor);

        const QRect before = indicator.property("recordingControlGlobalRect").toRect();
        QVERIFY(before.isValid());
        const QPoint local = status->rect().center();
        const QPoint global = status->mapToGlobal(local);
        QMouseEvent press(QEvent::MouseButtonPress, local, global,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(status, &press);
        QMouseEvent move(QEvent::MouseMove, local + QPoint(32, 24),
                         global + QPoint(32, 24), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(status, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, local + QPoint(32, 24),
                            global + QPoint(32, 24), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(status, &release);

        const QRect after = indicator.property("recordingControlGlobalRect").toRect();
        QVERIFY(after.isValid());
        QVERIFY(after.topLeft() != before.topLeft());
    }

    void keepsVisibleControlsOutOfTheRecordedRegion()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect screenRect = screen->geometry();
        const QRect capture(screenRect.left() + 420, screenRect.top() + 100, 240, 220);
        RecordingIndicator indicator(capture, nullptr, 2, true,
                                     RecordingIndicatorMode::Video);
        auto *status = indicator.findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
        QVERIFY(status);

        const QRect before = indicator.property("recordingControlGlobalRect").toRect();
        QVERIFY(before.isValid());
        QVERIFY(!before.intersects(capture));

        const QPoint local = status->rect().center();
        const QPoint global = status->mapToGlobal(local);
        const QPoint requestedTopLeft = capture.topLeft() + QPoint(16, 16);
        const QPoint delta = requestedTopLeft - before.topLeft();
        QMouseEvent press(QEvent::MouseButtonPress, local, global,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(status, &press);
        QMouseEvent move(QEvent::MouseMove, local + delta, global + delta,
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(status, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, local + delta, global + delta,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(status, &release);

        const QRect after = indicator.property("recordingControlGlobalRect").toRect();
#ifdef Q_OS_LINUX
        QVERIFY(after.intersects(capture));
#else
        QCOMPARE(after, before);
        QVERIFY(!after.intersects(capture));
#endif
    }

    void togglesBorderLockWithoutHidingTheBorder()
    {
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        auto *lock = indicator.findChild<QToolButton *>(QStringLiteral("recordingBorderLockButton"));
        QVERIFY(lock);
        QWidget *border = nullptr;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (widget && widget->objectName() == QStringLiteral("recordingBorderOverlay")) {
                border = widget;
                break;
            }
        }
        QVERIFY(border);
        QVERIFY(border->isVisible());

        QTest::mouseClick(lock, Qt::LeftButton);
        QTRY_VERIFY(border->isVisible());
        QTest::mouseClick(lock, Qt::LeftButton);
        QTRY_VERIFY(border->isVisible());
    }

    void hidesFullscreenBorderWhenTheCaptureBackendCannotExcludeIt()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        RecordingIndicator indicator(screen->geometry(), nullptr, 2, true,
                                     RecordingIndicatorMode::Video);

        QWidget *border = nullptr;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (widget && widget->objectName() == QStringLiteral("recordingBorderOverlay")) {
                border = widget;
                break;
            }
        }
        QVERIFY(border);
        QVERIFY(!border->isVisible());
    }

};

QTEST_MAIN(RecordingIndicatorTests)
#include "RecordingIndicatorTests.moc"
