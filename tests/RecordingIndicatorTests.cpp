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
        if (indicator.canExcludeFromCapture() || QGuiApplication::screens().size() > 1) {
            // Windows 10 2004+ leaves the bar out of the capture, and with a
            // second monitor the bar moves there; either way it stays visible.
            QVERIFY(!indicator.requiresCaptureSafePresentation());
            QVERIFY(bar->isVisible());
            return;
        }
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

    void togglesBorderVisibilityWithoutChangingRecordingControls()
    {
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        auto *visibility = indicator.findChild<QToolButton *>(QStringLiteral("recordingBorderVisibilityButton"));
        QVERIFY(visibility);
        QWidget *border = nullptr;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (widget && widget->objectName() == QStringLiteral("recordingBorderOverlay")) {
                border = widget;
                break;
            }
        }
        QVERIFY(border);
        QVERIFY(border->isVisible());

        QTest::mouseClick(visibility, Qt::LeftButton);
        QVERIFY(!border->isVisible());
        QVERIFY(indicator.hasControlBar());
        indicator.setPaused(true);
        indicator.setPaused(false);
        QVERIFY(!border->isVisible());
        QTest::mouseClick(visibility, Qt::LeftButton);
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
        indicator.setBorderVisible(false);
        indicator.setBorderVisible(true);
        QVERIFY(!border->isVisible());
    }

    void finalizingDisablesStopPauseAndCancel()
    {
        TranslationManager::setLanguage(TranslationManager::English, false);
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true,
                                     RecordingIndicatorMode::Video);
        auto *status = indicator.findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
        auto *pause = indicator.findChild<QToolButton *>(QStringLiteral("recordingPauseButton"));
        auto *stop = indicator.findChild<QPushButton *>(QStringLiteral("recordingStopButton"));
        auto *cancel = indicator.findChild<QToolButton *>(QStringLiteral("recordingCancelButton"));
        QVERIFY(status && pause && stop && cancel);

        indicator.setFinalizing(true);
        QVERIFY(indicator.isFinalizing());
        QVERIFY(!pause->isEnabled());
        QVERIFY(!stop->isEnabled());
        QVERIFY(!cancel->isEnabled());
        QCOMPARE(status->property("recordingStatusPlain").toString(), QString::fromUtf8("Saving…"));

        QSignalSpy stopSpy(&indicator, &RecordingIndicator::stopRequested);
        QSignalSpy cancelSpy(&indicator, &RecordingIndicator::cancelRequested);
        QSignalSpy pauseSpy(&indicator, &RecordingIndicator::pauseRequested);
        stop->click();
        cancel->click();
        pause->click();
        QCOMPARE(stopSpy.count(), 0);
        QCOMPARE(cancelSpy.count(), 0);
        QCOMPARE(pauseSpy.count(), 0);
    }

    void gifFinalizingSaysConverting()
    {
        TranslationManager::setLanguage(TranslationManager::English, false);
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true,
                                     RecordingIndicatorMode::Gif);
        auto *status = indicator.findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
        QVERIFY(status);
        indicator.setFinalizing(true);
        QCOMPARE(status->property("recordingStatusPlain").toString(),
                 QString::fromUtf8("Converting GIF…"));
    }

    void showsTheTimeLimitAndWarnsNearTheEnd()
    {
        TranslationManager::setLanguage(TranslationManager::English, false);
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true,
                                     RecordingIndicatorMode::Gif);
        auto *status = indicator.findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
        QVERIFY(status);

        indicator.setElapsedSeconds(12);
        QCOMPARE(status->property("recordingStatusPlain").toString(), QStringLiteral("GIF 00:12"));

        indicator.setTimeLimitSeconds(30);
        indicator.setRemainingSeconds(18);
        QCOMPARE(status->property("recordingStatusPlain").toString(),
                 QStringLiteral("GIF 00:12 / 00:30"));
        QVERIFY(!status->property("recordingTimeLimitWarning").toBool());

        indicator.setElapsedSeconds(26);
        indicator.setRemainingSeconds(4);
        QVERIFY(status->property("recordingTimeLimitWarning").toBool());
        QVERIFY(status->text().contains(QStringLiteral("#FFB224")));
    }

    void translatesTheModeLabel()
    {
        TranslationManager::setLanguage(TranslationManager::Turkish, false);
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true,
                                     RecordingIndicatorMode::Video);
        auto *status = indicator.findChild<QLabel *>(QStringLiteral("recordingStatusLabel"));
        QVERIFY(status);
        QCOMPARE(status->property("recordingStatusPlain").toString(),
                 QString::fromUtf8("VİDEO 00:00"));
        TranslationManager::setLanguage(TranslationManager::English, false);
    }

    void buttonsHaveAccessibleNames()
    {
        TranslationManager::setLanguage(TranslationManager::English, false);
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        auto *pause = indicator.findChild<QToolButton *>(QStringLiteral("recordingPauseButton"));
        auto *stop = indicator.findChild<QPushButton *>(QStringLiteral("recordingStopButton"));
        auto *cancel = indicator.findChild<QToolButton *>(QStringLiteral("recordingCancelButton"));
        auto *details = indicator.findChild<QToolButton *>(QStringLiteral("recordingDetailsButton"));
        auto *visibility = indicator.findChild<QToolButton *>(QStringLiteral("recordingBorderVisibilityButton"));
        QVERIFY(pause && stop && cancel && details && visibility);
        QCOMPARE(pause->accessibleName(), QStringLiteral("Pause"));
        QCOMPARE(stop->accessibleName(), QStringLiteral("Stop Recording"));
        QCOMPARE(cancel->accessibleName(), QStringLiteral("Cancel Recording"));
        QVERIFY(!details->accessibleName().isEmpty());
        QVERIFY(!visibility->accessibleName().isEmpty());
        indicator.setPaused(true);
        QCOMPARE(pause->accessibleName(), QStringLiteral("Resume"));
    }

    void keepsOutsideControlsOnTop()
    {
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        const QRect control = indicator.property("recordingControlGlobalRect").toRect();
        QVERIFY(!control.intersects(QRect(120, 120, 640, 360)));
        QVERIFY(indicator.controlStaysOnTop());
        QVERIFY(indicator.windowFlags() & Qt::WindowStaysOnTopHint);
    }

    void trayPresentationDrawsOnlyTheBorder()
    {
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true,
                                     RecordingIndicatorMode::Video,
                                     defaultRecordingCaptureBackend(),
                                     RecordingControlsPresentation::TrayMenu);
        QVERIFY(!indicator.hasControlBar());
        QVERIFY(!indicator.isVisible());
        QVERIFY(!indicator.findChild<QFrame *>(QStringLiteral("recordingControlBar")));
        QVERIFY(!indicator.findChild<QPushButton *>(QStringLiteral("recordingStopButton")));
        QVERIFY(!indicator.requiresCaptureSafePresentation());

        QWidget *border = nullptr;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (widget && widget->objectName() == QStringLiteral("recordingBorderOverlay")) {
                border = widget;
                break;
            }
        }
        QVERIFY(border);
        QVERIFY(border->isVisible());
        QVERIFY(indicator.isBorderVisible());
        QVERIFY(border->windowFlags() & Qt::WindowStaysOnTopHint);

        // The tray menu's visibility toggle.
        indicator.setBorderVisible(false);
        QVERIFY(!indicator.isBorderVisible());
        QVERIFY(!border->isVisible());
        indicator.setPaused(true);
        indicator.setPaused(false);
        QVERIFY(!border->isVisible());
        indicator.setBorderVisible(true);
        QVERIFY(border->isVisible());
        QVERIFY(border->windowFlags() & Qt::WindowStaysOnTopHint);

        // Pause and finalizing keep the border and never create controls.
        indicator.setPaused(true);
        indicator.setFinalizing(true);
        QVERIFY(border->isVisible());
        QVERIFY(!indicator.hasControlBar());
        indicator.stop();
        indicator.setBorderVisible(true);
        QVERIFY(!border->isVisible());
    }

    void controlBarFallbackKeepsItsVisibilityButtonInSync()
    {
        RecordingIndicator indicator(QRect(120, 120, 640, 360), nullptr, 2, true);
        QVERIFY(indicator.hasControlBar());
        auto *visibility = indicator.findChild<QToolButton *>(QStringLiteral("recordingBorderVisibilityButton"));
        QVERIFY(visibility);
        QVERIFY(visibility->isChecked());
        indicator.setBorderVisible(false);
        QVERIFY(!visibility->isChecked());
        QTest::mouseClick(visibility, Qt::LeftButton);
        QVERIFY(indicator.isBorderVisible());
    }

};

QTEST_MAIN(RecordingIndicatorTests)
#include "RecordingIndicatorTests.moc"
