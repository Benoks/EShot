#include <QtTest>
#include <QScreen>
#include <QSignalSpy>
#include "ui/AboutDialog.h"
#include "core/TranslationManager.h"

#include <QPushButton>

class AboutDialogTests : public QObject {
    Q_OBJECT
private slots:
    void manualCheckIsForwardedToTheApplicationUpdater()
    {
        AboutDialog dialog;
        QSignalSpy spy(&dialog, &AboutDialog::checkForUpdatesRequested);

        QVERIFY(QMetaObject::invokeMethod(&dialog, "onCheckForUpdates"));

        QCOMPARE(spy.count(), 1);
    }

    void closeButtonSaysCloseAndTheDialogCanGrow()
    {
        AboutDialog dialog;
        bool hasClose = false;
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            QVERIFY(button->text() != TranslationManager::cancel());
            hasClose = hasClose || button->text() == TranslationManager::ocrClose();
        }
        QVERIFY(hasClose);
        QVERIFY(dialog.minimumWidth() >= 380);
        QVERIFY(dialog.maximumHeight() > dialog.minimumHeight());

        // A long status must not be clipped: the dialog grows to fit it.
        dialog.setUpdateInfo(false, QString(), false, QStringLiteral("status ").repeated(60));
        dialog.adjustSize();
        // adjustSize() stops at two thirds of the screen (800x600 offscreen in CI).
        const int screenLimit = dialog.screen()->availableGeometry().height() * 2 / 3;
        QVERIFY(dialog.height() >= qMin(dialog.sizeHint().height(), screenLimit));
    }
};

QTEST_MAIN(AboutDialogTests)
#include "AboutDialogTests.moc"
