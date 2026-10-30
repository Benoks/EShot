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
    void highContrastUsesVisibleBordersFocusAndScalableFonts();
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

void OverlayPanelStyleTests::highContrastUsesVisibleBordersFocusAndScalableFonts()
{
    const QString regular = overlayPanelStyleSheet(false);
    const QString highContrast = overlayPanelStyleSheet(true);

    QVERIFY(!regular.contains(QStringLiteral("#ffff00")));
    QVERIFY(highContrast.startsWith(regular));
    const QString overrides = highContrast.mid(regular.size());
    QVERIFY(overrides.contains(QStringLiteral("border: 2px solid #ffffff")));
    QVERIFY(overrides.contains(QStringLiteral(":focus")));
    QVERIFY(overrides.contains(QStringLiteral("border-color: #ffff00")));

    // Every font in the overrides is in points (follows system scaling) and
    // at least 9pt, i.e. 12px at 96 DPI.
    static const QRegularExpression fontSize(QStringLiteral("font-size:\\s*(\\d+)(px|pt)"));
    int fonts = 0;
    auto it = fontSize.globalMatch(overrides);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        QCOMPARE(match.captured(2), QStringLiteral("pt"));
        QVERIFY(match.captured(1).toInt() >= 9);
        ++fonts;
    }
    QVERIFY(fonts > 0);
}

QTEST_MAIN(OverlayPanelStyleTests)

#include "OverlayPanelStyleTests.moc"
