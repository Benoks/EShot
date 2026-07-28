#include <QtTest>

#include "ui/ControlCenterDialog.h"

#include <QLabel>
#include <QPushButton>

class ControlCenterDialogTests : public QObject {
    Q_OBJECT

private slots:
    void usesShortBrandHeading()
    {
        ControlCenterDialog dialog;
        const auto labels = dialog.findChildren<QLabel *>();
        QVERIFY(!labels.isEmpty());
        QCOMPARE(labels.first()->text(), QStringLiteral("EShot"));
    }

    void usesNativeButtonStyling()
    {
        ControlCenterDialog dialog;
        const auto buttons = dialog.findChildren<QPushButton *>();
        QCOMPARE(buttons.size(), 4);
        for (QPushButton *button : buttons)
            QVERIFY(button->styleSheet().isEmpty());
    }
};

QTEST_MAIN(ControlCenterDialogTests)
#include "ControlCenterDialogTests.moc"
