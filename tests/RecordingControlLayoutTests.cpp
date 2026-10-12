#include <QtTest>

#include "recording/RecordingControlLayout.h"

class RecordingControlLayoutTests : public QObject
{
    Q_OBJECT

private slots:
    void placesControlsBelowWhenThereIsRoom()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QRect capture(500, 250, 800, 450);

        const RecordingControlLayout layout = recordingControlLayout(
            capture, screen, QSize(310, 42), QSize(214, 42));

        QCOMPARE(layout.placement, RecordingControlPlacement::Below);
        QCOMPARE(layout.controlRect.top(), capture.bottom() + 9);
        QCOMPARE(layout.controlRect.center().x(), capture.center().x());
        QVERIFY(!layout.compact);
    }

    void movesControlsAboveWhenBottomSpaceIsUnavailable()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QRect capture(560, 620, 800, 450);

        const RecordingControlLayout layout = recordingControlLayout(
            capture, screen, QSize(310, 42), QSize(214, 42));

        QCOMPARE(layout.placement, RecordingControlPlacement::Above);
        QCOMPARE(layout.controlRect.bottom(), capture.top() - 9);
    }

    void placesControlsInsideForFullscreenCapture()
    {
        const QRect screen(0, 0, 1920, 1080);

        const RecordingControlLayout layout = recordingControlLayout(
            screen, screen, QSize(310, 42), QSize(214, 42));

        QCOMPARE(layout.placement, RecordingControlPlacement::Inside);
        QCOMPARE(layout.controlRect.bottom(), screen.bottom() - 12);
        QCOMPARE(layout.controlRect.center().x(), screen.center().x());
    }

    void movesFullscreenControlsToAnUncapturedMonitor()
    {
        const QRect capturedScreen(0, 0, 1920, 1080);
        const QRect otherScreen(1920, 0, 1920, 1080);

        const RecordingControlLayout layout = recordingControlLayout(
            capturedScreen, capturedScreen, QSize(310, 42), QSize(214, 42),
            {otherScreen});

        QCOMPARE(layout.controlScreenRect, otherScreen);
        QVERIFY(otherScreen.contains(layout.controlRect));
        QVERIFY(!capturedScreen.intersects(layout.controlRect));
    }

    void usesCompactControlsOnNarrowScreens()
    {
        const QRect screen(0, 0, 250, 600);
        const QRect capture(10, 100, 230, 320);

        const RecordingControlLayout layout = recordingControlLayout(
            capture, screen, QSize(310, 42), QSize(214, 42));

        QVERIFY(layout.compact);
        QCOMPARE(layout.controlRect.width(), 214);
        QVERIFY(screen.adjusted(8, 0, -8, 0).contains(layout.controlRect.topLeft()));
        QVERIFY(screen.adjusted(8, 0, -8, 0).contains(layout.controlRect.topRight()));
    }

    void clampsControlsToTheScreenHorizontally()
    {
        const QRect screen(-1920, 0, 1920, 1080);
        const QRect capture(-1910, 220, 300, 400);

        const RecordingControlLayout layout = recordingControlLayout(
            capture, screen, QSize(310, 42), QSize(214, 42));

        QCOMPARE(layout.controlRect.left(), screen.left() + 8);
        QVERIFY(layout.controlRect.right() <= screen.right() - 8);
    }

    void hidesInsideControlsWhenThePlatformCannotExcludeThemFromCapture()
    {
        QCOMPARE(recordingOverlayVisibilityPolicy(RecordingControlPlacement::Inside, false),
                 RecordingOverlayVisibility::RevealWhilePaused);
        QCOMPARE(recordingOverlayVisibilityPolicy(RecordingControlPlacement::Inside, true),
                 RecordingOverlayVisibility::AlwaysVisible);
        QCOMPARE(recordingOverlayVisibilityPolicy(RecordingControlPlacement::Below, false),
                 RecordingOverlayVisibility::AlwaysVisible);
        QCOMPARE(recordingOverlayVisibilityPolicy(RecordingControlPlacement::Above, false),
                 RecordingOverlayVisibility::AlwaysVisible);
    }

    void trustsDisplayAffinityWithGdiOnlyWhenConfirmedOnANewBuild()
    {
        QVERIFY(!recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsGdiGrab));
        QVERIFY(!recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsGdiGrab, 22631, false));
        QVERIFY(!recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsGdiGrab, 18363, true));
        QVERIFY(recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsGdiGrab, 19041, true));
        QVERIFY(recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsGdiGrab, 26100, true));
        QVERIFY(!recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::LinuxPortal, 0, true));
        QVERIFY(recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsGraphicsCapture));
    }

    void trustsDisplayAffinityWithDesktopDuplicationLikeGdi()
    {
        QVERIFY(!recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsDdaGrab));
        QVERIFY(!recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsDdaGrab, 22631, false));
        QVERIFY(!recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsDdaGrab, 18363, true));
        QVERIFY(recordingCaptureBackendCanExcludeOverlay(
            RecordingCaptureBackend::WindowsDdaGrab, 19041, true));
    }

    void keepsControlsOnTopOnlyWhenTheyCannotEnterTheVideo()
    {
        const QRect capture(100, 100, 800, 450);
        QVERIFY(recordingControlBarStaysOnTop(QRect(300, 560, 310, 42), capture, false));
        QVERIFY(!recordingControlBarStaysOnTop(QRect(300, 500, 310, 42), capture, false));
        QVERIFY(recordingControlBarStaysOnTop(QRect(300, 500, 310, 42), capture, true));
    }

    void exclusionKeepsFullscreenControlsVisible()
    {
        const QRect screen(0, 0, 1920, 1080);
        const RecordingControlLayout layout = recordingControlLayout(
            screen, screen, QSize(310, 42), QSize(206, 42));
        QCOMPARE(layout.placement, RecordingControlPlacement::Inside);
        QCOMPARE(recordingOverlayVisibilityPolicy(layout.controlRect, screen, true),
                 RecordingOverlayVisibility::AlwaysVisible);
        QVERIFY(!recordingControlsNeedHiddenHint(RecordingOverlayVisibility::AlwaysVisible));
        QCOMPARE(recordingOverlayVisibilityPolicy(layout.controlRect, screen, false),
                 RecordingOverlayVisibility::RevealWhilePaused);
        QVERIFY(recordingControlsNeedHiddenHint(RecordingOverlayVisibility::RevealWhilePaused));
    }

    void formatsElapsedTimeWithAnOptionalLimit()
    {
        QCOMPARE(recordingStatusTimeText(0, 0), QStringLiteral("00:00"));
        QCOMPARE(recordingStatusTimeText(75, 0), QStringLiteral("01:15"));
        QCOMPARE(recordingStatusTimeText(12, 30), QStringLiteral("00:12 / 00:30"));
        QCOMPARE(recordingStatusTimeText(31, 30), QStringLiteral("00:30 / 00:30"));
        QCOMPARE(recordingStatusTimeText(-4, -1), QStringLiteral("00:00"));
    }

    void warnsInTheLastFiveSecondsOfALimitedRecording()
    {
        QVERIFY(!recordingTimeLimitWarning(100, 0));
        QVERIFY(!recordingTimeLimitWarning(24, 30));
        QVERIFY(recordingTimeLimitWarning(25, 30));
        QVERIFY(recordingTimeLimitWarning(30, 30));
        QVERIFY(!recordingTimeLimitWarning(25, 30, 6));
        QVERIFY(recordingTimeLimitWarning(20, 30, 5));
    }

    void detectsCaptureOverlapAfterTheControlsAreDragged()
    {
        const QRect capture(100, 100, 800, 450);
        QCOMPARE(recordingOverlayVisibilityPolicy(
                     QRect(300, 500, 310, 42), capture, false),
                 RecordingOverlayVisibility::RevealWhilePaused);
        QCOMPARE(recordingOverlayVisibilityPolicy(
                     QRect(300, 560, 310, 42), capture, false),
                 RecordingOverlayVisibility::AlwaysVisible);
        QCOMPARE(recordingOverlayVisibilityPolicy(
                     QRect(300, 500, 310, 42), capture, true),
                 RecordingOverlayVisibility::AlwaysVisible);
    }

    void clampsDraggedControlsToTheCurrentScreen()
    {
        const QRect screen(-1920, 0, 1920, 1080);
        const QRect current(-1500, 900, 310, 42);
        QCOMPARE(movedRecordingControlRect(current, QPoint(-2500, 1200), screen),
                 QRect(-1912, 1030, 310, 42));
    }

    void keepsOnlyLockedRecordingBorderOnTop()
    {
        QVERIFY(recordingBorderStaysOnTop(true));
        QVERIFY(!recordingBorderStaysOnTop(false));
    }
};

QTEST_APPLESS_MAIN(RecordingControlLayoutTests)
#include "RecordingControlLayoutTests.moc"
