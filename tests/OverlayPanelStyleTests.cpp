#include <QtTest>
#include <QSpinBox>

#include "ui/OverlayPanelStyle.h"

class OverlayPanelStyleTests : public QObject {
    Q_OBJECT

private slots:
    void usesCompactSharedMetrics();
    void definesEveryInteractiveState();
    void drawersKeepTheirShortAnimation();
    void numberInputsMatchRecordingControls();
};

void OverlayPanelStyleTests::usesCompactSharedMetrics()
{
    const OverlayPanelMetrics metrics = overlayPanelMetrics();

    QCOMPARE(metrics.panelRadius, 10);
    QCOMPARE(metrics.controlRadius, 6);
    QCOMPARE(metrics.actionRadius, 8);
    QCOMPARE(metrics.controlHeight, 32);
    QCOMPARE(metrics.contentMargin, 12);
    QCOMPARE(metrics.rowSpacing, 7);
    QCOMPARE(metrics.sectionSpacing, 12);
    QVERIFY(metrics.sectionSpacing < metrics.controlHeight);
}

void OverlayPanelStyleTests::definesEveryInteractiveState()
{
    const QString style = overlayPanelStyleSheet();

    QVERIFY(style.contains(QStringLiteral("[panelRole=\"title\"]")));
    QVERIFY(style.contains(QStringLiteral("[panelRole=\"section\"]")));
    QVERIFY(style.contains(QStringLiteral("[panelRole=\"separator\"]")));
    QVERIFY(style.contains(QStringLiteral("[panelAction=\"primary\"]")));
    QVERIFY(style.contains(QStringLiteral("[panelAction=\"secondary\"]")));
    QVERIFY(style.contains(QStringLiteral(":hover")));
    QVERIFY(style.contains(QStringLiteral(":focus")));
    QVERIFY(style.contains(QStringLiteral(":pressed")));
    QVERIFY(style.contains(QStringLiteral(":disabled")));
    QVERIFY(style.contains(QStringLiteral("border-top-left-radius: 0px")));
    QVERIFY(style.contains(QStringLiteral("border-bottom-left-radius: 0px")));
}

void OverlayPanelStyleTests::drawersKeepTheirShortAnimation()
{
    QCOMPARE(overlayPanelMetrics().drawerAnimationDurationMs, 185);
}

void OverlayPanelStyleTests::numberInputsMatchRecordingControls()
{
    QSpinBox spinBox;
    spinBox.setButtonSymbols(QAbstractSpinBox::NoButtons);
    spinBox.setAlignment(Qt::AlignLeft);

    configureOverlaySpinBox(&spinBox);

    QCOMPARE(spinBox.buttonSymbols(), QAbstractSpinBox::UpDownArrows);
    QCOMPARE(spinBox.alignment(), Qt::AlignCenter);
    QVERIFY(!spinBox.isReadOnly());
}

QTEST_MAIN(OverlayPanelStyleTests)

#include "OverlayPanelStyleTests.moc"
