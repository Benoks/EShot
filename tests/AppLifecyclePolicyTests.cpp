#include <QtTest>

#include "core/AppLifecyclePolicy.h"

using namespace AppLifecyclePolicy;

class AppLifecyclePolicyTests : public QObject {
    Q_OBJECT

private slots:
    void captureHotkeyNeverDiscardsUserWork()
    {
        QCOMPARE(captureDialogAction(CaptureDialogKind::Settings),
                 CaptureDialogAction::HideUntilCaptureEnds);
        QCOMPARE(captureDialogAction(CaptureDialogKind::SettingsChild),
                 CaptureDialogAction::KeepAndSkipCapture);
        QCOMPARE(captureDialogAction(CaptureDialogKind::CaptureSession),
                 CaptureDialogAction::KeepAndSkipCapture);
        QCOMPARE(captureDialogAction(CaptureDialogKind::Installer),
                 CaptureDialogAction::KeepAndSkipCapture);
        QCOMPARE(captureDialogAction(CaptureDialogKind::Wizard),
                 CaptureDialogAction::Ignore);
        QCOMPARE(captureDialogAction(CaptureDialogKind::Other),
                 CaptureDialogAction::Close);
    }

    void settingsReturnsOnlyWhenCaptureIsOver()
    {
        QVERIFY(settingsMayReturnAfterCapture(false, false, false));
        QVERIFY(!settingsMayReturnAfterCapture(true, false, false));
        QVERIFY(!settingsMayReturnAfterCapture(false, true, false));
        QVERIFY(!settingsMayReturnAfterCapture(false, false, true));
    }
};

QTEST_APPLESS_MAIN(AppLifecyclePolicyTests)
#include "AppLifecyclePolicyTests.moc"
