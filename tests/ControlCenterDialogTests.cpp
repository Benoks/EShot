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

    void staysCompactInsteadOfStretchingTheHeadingArea()
    {
        ControlCenterDialog dialog;
        dialog.adjustSize();

        QCOMPARE(dialog.minimumHeight(), dialog.maximumHeight());
        QVERIFY(dialog.height() < 300);
        QCOMPARE(dialog.width(), 300);
    }
};

QTEST_MAIN(ControlCenterDialogTests)
#include "ControlCenterDialogTests.moc"
