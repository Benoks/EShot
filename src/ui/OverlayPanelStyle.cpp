#include "OverlayPanelStyle.h"

#include <QSettings>
#include <QSpinBox>

OverlayPanelMetrics overlayPanelMetrics()
{
    return {
        10,
        6,
        8,
        32,
        12,
        7,
        12,
        185
    };
}

void configureOverlaySpinBox(QSpinBox *spinBox)
{
    if (!spinBox)
        return;

    spinBox->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    spinBox->setAlignment(Qt::AlignCenter);
    spinBox->setReadOnly(false);
}

bool overlayHighContrastEnabled()
{
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    return settings.value(QStringLiteral("highContrast"), false).toBool();
}

QString overlayPanelStyleSheet()
{
    return overlayPanelStyleSheet(overlayHighContrastEnabled());
}

namespace {

// Appended after the regular rules, so equal selectors override them.
QString highContrastOverrides()
{
    return QStringLiteral(R"(
        QWidget#toolSettingsDrawer,
        QWidget#recordingDrawer,
        QWidget#recordingMorePanel {
            background: #000000;
            border: 2px solid #ffffff;
        }
        QLabel {
            color: #ffffff;
            font-size: 9pt;
        }
        QLabel[panelRole="title"] {
            color: #ffffff;
            font-size: 11pt;
        }
        QLabel[panelRole="section"] {
            color: #ffffff;
            font-size: 9pt;
        }
        QLabel[panelRole="value"] {
            color: #ffffff;
            background: #000000;
            border: 1px solid #ffffff;
            font-size: 9pt;
        }
        QLabel[panelRole="hint"] {
            color: #ffffff;
            font-size: 9pt;
        }
        QFrame[panelRole="separator"] {
            color: #ffffff;
            background: #ffffff;
        }
        QSpinBox,
        QComboBox {
            background: #000000;
            border: 2px solid #ffffff;
            color: #ffffff;
            selection-background-color: #ffff00;
            selection-color: #000000;
            font-size: 9pt;
        }
        QSpinBox:hover,
        QComboBox:hover {
            background: #000000;
            border-color: #ffff00;
        }
        QSpinBox:focus,
        QComboBox:focus {
            border: 2px solid #ffff00;
        }
        QSpinBox:disabled,
        QComboBox:disabled {
            color: #969696;
            background: #000000;
            border-color: #969696;
        }
        QComboBox::drop-down,
        QComboBox::drop-down:disabled,
        QSpinBox::up-button,
        QSpinBox::down-button,
        QSpinBox::up-button:hover,
        QSpinBox::down-button:hover {
            background: #000000;
            border-left: 1px solid #ffffff;
        }
        QCheckBox {
            color: #ffffff;
            font-size: 9pt;
        }
        QCheckBox:focus {
            color: #ffff00;
        }
        QCheckBox:disabled {
            color: #969696;
        }
        QCheckBox::indicator {
            border: 2px solid #ffffff;
            background: #000000;
        }
        QCheckBox::indicator:hover {
            border-color: #ffff00;
            background: #000000;
        }
        QCheckBox::indicator:checked {
            background: #0050c8;
            border-color: #ffff00;
        }
        QSlider::groove:horizontal {
            background: #ffffff;
        }
        QSlider::handle:horizontal,
        QSlider::handle:horizontal:hover {
            background: #ffff00;
            border: 2px solid #ffffff;
        }
        QPushButton[panelAction="primary"],
        QPushButton[panelAction="secondary"],
        QPushButton[panelSegment] {
            background: #000000;
            border: 2px solid #ffffff;
            color: #ffffff;
            font-size: 9pt;
        }
        QPushButton[panelAction="primary"]:hover,
        QPushButton[panelAction="secondary"]:hover,
        QPushButton[panelSegment]:hover,
        QPushButton[panelAction="primary"]:focus,
        QPushButton[panelAction="secondary"]:focus,
        QPushButton[panelSegment]:focus {
            background: #000000;
            border-color: #ffff00;
            color: #ffff00;
        }
        QPushButton[panelAction="primary"]:pressed,
        QPushButton[panelAction="secondary"]:pressed {
            background: #404247;
        }
        QPushButton[panelSegment]:checked {
            background: #ffff00;
            border-color: #ffff00;
            color: #000000;
        }
        QPushButton[panelAction="primary"]:disabled,
        QPushButton[panelAction="secondary"]:disabled {
            color: #969696;
            background: #000000;
            border-color: #969696;
        }
        QPushButton[panelAction="link"] {
            color: #00ffff;
            font-size: 9pt;
        }
        QPushButton[panelAction="link"]:hover,
        QPushButton[panelAction="link"]:focus {
            color: #ffff00;
            text-decoration: underline;
        }
    )");
}

}

QString overlayPanelStyleSheet(bool highContrast)
{
    const QString base = QStringLiteral(R"(
        QWidget#toolSettingsDrawer,
        QWidget#recordingDrawer,
        QWidget#recordingMorePanel {
            background: rgba(59, 61, 66, 250);
            border: 1px solid #505257;
            border-radius: 6px;
        }
        QWidget#toolSettingsDrawer {
            border-top-left-radius: 0px;
            border-bottom-left-radius: 0px;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
        }
        QLabel {
            color: #dcdde0;
            font-size: 12px;
            border: none;
            background: transparent;
        }
        QLabel[panelRole="title"] {
            color: #ededee;
            font-size: 14px;
            font-weight: 600;
        }
        QLabel[panelRole="section"] {
            color: #ededee;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel[panelRole="value"] {
            color: #ededee;
            background: rgba(255, 255, 255, 18);
            border: 1px solid rgba(255, 255, 255, 38);
            border-radius: 4px;
            padding: 1px 5px;
            min-width: 24px;
            qproperty-alignment: AlignCenter;
            font-size: 10px;
            font-weight: 700;
        }
        QFrame[panelRole="separator"] {
            color: rgba(255, 255, 255, 38);
            background: rgba(255, 255, 255, 38);
            border: none;
            max-height: 1px;
        }
        QSpinBox,
        QComboBox {
            min-height: 28px;
            max-height: 28px;
            background: #46484e;
            border: 1px solid #5a5c62;
            border-radius: 4px;
            color: #ededee;
            selection-background-color: #4f5157;
        }
        QComboBox {
            padding: 0 8px;
        }
        QSpinBox {
            padding: 0;
        }
        QSpinBox:hover,
        QComboBox:hover {
            border-color: #64666c;
            background: #4f5157;
        }
        QSpinBox:focus,
        QComboBox:focus {
            border-color: #0078d7;
        }
        QSpinBox:disabled,
        QComboBox:disabled {
            color: #8a8c91;
            background: #303237;
            border-color: #505257;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #55575d;
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
            background: #46484e;
        }
        QComboBox::drop-down:disabled {
            background: #3b3d42;
            border-left-color: #505257;
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron_down.svg);
            width: 13px;
            height: 13px;
        }
        QSpinBox::up-button,
        QSpinBox::down-button {
            subcontrol-origin: border;
            width: 22px;
            background: #44464b;
            border-left: 1px solid #55575d;
        }
        QSpinBox::up-button {
            subcontrol-position: top right;
            border-top-right-radius: 4px;
        }
        QSpinBox::down-button {
            subcontrol-position: bottom right;
            border-bottom-right-radius: 4px;
        }
        QSpinBox::up-button:hover,
        QSpinBox::down-button:hover {
            background: #505257;
        }
        QSpinBox::up-arrow {
            image: url(:/icons/chevron_up.svg);
            width: 11px;
            height: 11px;
        }
        QSpinBox::down-arrow {
            image: url(:/icons/chevron_down.svg);
            width: 11px;
            height: 11px;
        }
        QCheckBox {
            color: #dcdde0;
            font-size: 12px;
            spacing: 7px;
            border: none;
            background: transparent;
        }
        QCheckBox:disabled {
            color: #8a8c91;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid #64666c;
            background: #303237;
        }
        QCheckBox::indicator:hover {
            border-color: #8a8c91;
            background: #4f5157;
        }
        QCheckBox::indicator:checked {
            background: #0078d7;
            border-color: #1a86dd;
            image: url(:/icons/check.svg);
        }
        QSlider::groove:horizontal {
            background: #505257;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #ededee;
            border: 1px solid #b3b5ba;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #ffffff;
            border-color: #b5b5b5;
        }
        QPushButton[panelAction="primary"] {
            min-height: 32px;
            max-height: 32px;
            background: #0078d7;
            border: 1px solid #1a86dd;
            border-radius: 4px;
            color: white;
            font-weight: 600;
        }
        QPushButton[panelAction="primary"]:hover {
            background: #1a86dd;
        }
        QPushButton[panelAction="primary"]:pressed {
            background: #006abf;
        }
        QPushButton[panelAction="primary"]:focus {
            border-color: #70bdff;
        }
        QPushButton[panelAction="primary"]:disabled {
            color: #8a8c91;
            background: #46484e;
            border-color: #505257;
        }
        QPushButton[panelAction="secondary"] {
            min-height: 30px;
            max-height: 30px;
            background: #46484e;
            border: 1px solid #5a5c62;
            border-radius: 4px;
            color: #dcdde0;
        }
        QPushButton[panelAction="secondary"]:hover {
            background: #4f5157;
            border-color: #64666c;
        }
        QPushButton[panelAction="secondary"]:pressed {
            background: #303237;
        }
        QPushButton[panelAction="secondary"]:focus {
            border-color: #0078d7;
        }
        QPushButton[panelAction="secondary"]:disabled {
            color: #8a8c91;
            border-color: #505257;
        }
        QLabel[panelRole="hint"] {
            color: #b3b5ba;
            font-size: 11px;
        }
        QPushButton[panelSegment] {
            min-height: 28px;
            max-height: 28px;
            background: #303237;
            border: 1px solid #5a5c62;
            color: #dcdde0;
            font-weight: 600;
        }
        QPushButton[panelSegment="first"] {
            border-top-left-radius: 4px;
            border-bottom-left-radius: 4px;
            border-right: none;
        }
        QPushButton[panelSegment="last"] {
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
        }
        QPushButton[panelSegment]:hover {
            background: #4f5157;
        }
        QPushButton[panelSegment]:checked {
            background: #55575d;
            border-color: #64666c;
            color: #ededee;
        }
        QPushButton[panelSegment]:focus {
            border-color: #70bdff;
        }
        QPushButton[panelAction="link"] {
            background: transparent;
            border: none;
            color: #7dbdf7;
            padding: 2px 0;
            text-align: left;
        }
        QPushButton[panelAction="link"]:hover {
            color: #a6d1fa;
            text-decoration: underline;
        }
        QPushButton[panelAction="link"]:focus {
            text-decoration: underline;
        }
        QLabel[panelRole="fieldLabel"] {
            color: #b3b5ba;
            font-size: 12px;
        }
        QFrame[panelRole="vseparator"] {
            background: #505257;
            border: none;
        }
        QWidget[panelRole="segmentTrack"] {
            background: #303237;
            border: 1px solid #505257;
            border-radius: 5px;
        }
        QPushButton[panelSegment="track"] {
            min-height: 26px;
            max-height: 26px;
            padding: 0 12px;
            background: transparent;
            border: none;
            border-radius: 3px;
            color: #b3b5ba;
            font-weight: 500;
        }
        QPushButton[panelSegment="track"]:hover {
            background: transparent;
            color: #ededee;
        }
        QPushButton[panelSegment="track"]:checked {
            background: #55575d;
            border: none;
            color: #ededee;
            font-weight: 600;
        }
        QWidget#recordingDrawer QComboBox {
            min-height: 30px;
            max-height: 30px;
            padding: 0 4px 0 6px;
            background: transparent;
            border: 1px solid transparent;
        }
        QWidget#recordingDrawer QComboBox:hover {
            background: #46484e;
            border-color: #5a5c62;
        }
        QWidget#recordingDrawer QComboBox::drop-down {
            width: 18px;
            background: transparent;
            border-left: none;
        }
        QWidget#recordingDrawer QToolButton,
        QPushButton[panelAction="icon"] {
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            padding: 0;
            background: transparent;
            border: none;
            border-radius: 4px;
        }
        QWidget#recordingDrawer QToolButton:hover,
        QPushButton[panelAction="icon"]:hover {
            background: #4f5157;
        }
        QWidget#recordingDrawer QToolButton:checked {
            background: #55575d;
        }
        QPushButton[panelAction="record"] {
            min-height: 30px;
            max-height: 30px;
            padding: 0 12px;
            background: #d44848;
            border: none;
            border-radius: 4px;
            color: #ffffff;
            font-weight: 600;
        }
        QPushButton[panelAction="record"]:hover {
            background: #dc5a5a;
        }
        QPushButton[panelAction="record"]:pressed {
            background: #bd3c3c;
        }
    )");
    return highContrast ? base + highContrastOverrides() : base;
}
