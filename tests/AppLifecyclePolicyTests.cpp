#include <QtTest>

#include "core/AppLifecyclePolicy.h"
#include "core/WindowsElevatedStartup.h"

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

    void elevatedScriptIsEncodedAsUtf16LittleEndian()
    {
        // "a" + U+011F (g with breve) in UTF-16LE: 61 00 1F 01.
        const QString script = QStringLiteral("a") + QChar(0x011F);
        QCOMPARE(WindowsElevatedStartup::encodedPowerShellCommand(script),
                 QString::fromLatin1(QByteArray("\x61\x00\x1f\x01", 4).toBase64()));
        const QString longer = QStringLiteral("$ErrorActionPreference = 'Stop'\r\nRegister-ScheduledTask @task");
        const QByteArray decoded = QByteArray::fromBase64(
            WindowsElevatedStartup::encodedPowerShellCommand(longer).toLatin1());
        QCOMPARE(QString::fromUtf16(reinterpret_cast<const char16_t *>(decoded.constData()),
                                    decoded.size() / 2),
                 longer);
    }

    void elevatedStartupIsSafeOnlyInProgramFiles()
    {
        const QStringList roots{QStringLiteral("C:\\Program Files"),
                                QStringLiteral("C:\\Program Files (x86)"), QString()};
        QVERIFY(WindowsElevatedStartup::installLocationIsAdminProtected(
            QStringLiteral("C:/Program Files/EShot"), roots));
        QVERIFY(WindowsElevatedStartup::installLocationIsAdminProtected(
            QStringLiteral("c:\\program files (x86)\\EShot\\"), roots));
        QVERIFY(!WindowsElevatedStartup::installLocationIsAdminProtected(
            QStringLiteral("C:/Users/me/AppData/Local/Programs/EShot"), roots));
        QVERIFY(!WindowsElevatedStartup::installLocationIsAdminProtected(
            QStringLiteral("C:/Program Files Evil/EShot"), roots));
        QVERIFY(!WindowsElevatedStartup::installLocationIsAdminProtected(QString(), roots));
    }
};

QTEST_APPLESS_MAIN(AppLifecyclePolicyTests)
#include "AppLifecyclePolicyTests.moc"
