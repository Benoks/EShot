#include "OverlayPanelStyle.h"

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

QString overlayPanelStyleSheet()
{
    return QStringLiteral(R"(
        QWidget#toolSettingsDrawer,
        QWidget#recordingDrawer {
            background: rgba(37, 37, 37, 250);
            border: 1px solid rgba(255, 255, 255, 55);
            border-radius: 10px;
        }
        QWidget#toolSettingsDrawer {
            border-top-left-radius: 0px;
            border-bottom-left-radius: 0px;
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }
        QLabel {
            color: #d6d6d6;
            font-size: 11px;
            border: none;
            background: transparent;
        }
        QLabel[panelRole="title"] {
            color: #f5f5f5;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel[panelRole="section"] {
            color: #eeeeee;
            font-size: 11px;
            font-weight: 700;
        }
        QLabel[panelRole="value"] {
            color: #f4f4f4;
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
            min-height: 32px;
            max-height: 32px;
            background: #303030;
            border: 1px solid #505050;
            border-radius: 6px;
            color: #f2f2f2;
            selection-background-color: #4a4a4a;
        }
        QComboBox {
            padding: 0 8px;
        }
        QSpinBox {
            padding: 0;
        }
        QSpinBox:hover,
        QComboBox:hover {
            border-color: #686868;
            background: #383838;
        }
        QSpinBox:focus,
        QComboBox:focus {
            border-color: #168de2;
        }
        QSpinBox:disabled,
        QComboBox:disabled {
            color: #858585;
            background: #2a2a2a;
            border-color: #414141;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #4b4d52;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background: #34363a;
        }
        QComboBox::drop-down:disabled {
            background: #2d2d2d;
            border-left-color: #3d3d3d;
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
            background: #34363a;
            border-left: 1px solid #4b4d52;
        }
        QSpinBox::up-button {
            subcontrol-position: top right;
            border-top-right-radius: 6px;
        }
        QSpinBox::down-button {
            subcontrol-position: bottom right;
            border-bottom-right-radius: 6px;
        }
        QSpinBox::up-button:hover,
        QSpinBox::down-button:hover {
            background: #414141;
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
            color: #dedede;
            font-size: 11px;
            spacing: 7px;
            border: none;
            background: transparent;
        }
        QCheckBox:disabled {
            color: #858585;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid #606060;
            background: #303030;
        }
        QCheckBox::indicator:hover {
            border-color: #777777;
            background: #383838;
        }
        QCheckBox::indicator:checked {
            background: #0078d4;
            border-color: #2497eb;
            image: url(:/icons/check.svg);
        }
        QSlider::groove:horizontal {
            background: #484848;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #e8e8e8;
            border: 1px solid #9a9a9a;
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
            background: #0078d4;
            border: 1px solid #2497eb;
            border-radius: 8px;
            color: white;
            font-weight: 700;
        }
        QPushButton[panelAction="primary"]:hover {
            background: #1686dc;
        }
        QPushButton[panelAction="primary"]:pressed {
            background: #006cbe;
        }
        QPushButton[panelAction="primary"]:focus {
            border-color: #70bdff;
        }
        QPushButton[panelAction="primary"]:disabled {
            color: #8f8f8f;
            background: #3a3a3a;
            border-color: #484848;
        }
        QPushButton[panelAction="secondary"] {
            min-height: 30px;
            max-height: 30px;
            background: transparent;
            border: 1px solid #505050;
            border-radius: 8px;
            color: #d6d6d6;
        }
        QPushButton[panelAction="secondary"]:hover {
            background: #343434;
            border-color: #686868;
        }
        QPushButton[panelAction="secondary"]:pressed {
            background: #2a2a2a;
        }
        QPushButton[panelAction="secondary"]:focus {
            border-color: #168de2;
        }
        QPushButton[panelAction="secondary"]:disabled {
            color: #777777;
            border-color: #404040;
        }
    )");
}
