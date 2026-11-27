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
            background: #33312f;
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
            background: rgba(44, 43, 41, 250);
            border: 1px solid #42403d;
            border-radius: 6px;
        }
        QWidget#toolSettingsDrawer {
            border-top-left-radius: 0px;
            border-bottom-left-radius: 0px;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
        }
        QLabel {
            color: #dcd9d5;
            font-size: 12px;
            border: none;
            background: transparent;
        }
        QLabel[panelRole="title"] {
            color: #edebe8;
            font-size: 14px;
            font-weight: 600;
        }
        QLabel[panelRole="section"] {
            color: #edebe8;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel[panelRole="value"] {
            color: #edebe8;
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
            background: #3a3836;
            border: 1px solid #57534f;
            border-radius: 4px;
            color: #edebe8;
            selection-background-color: #45433f;
        }
        QComboBox {
            padding: 0 8px;
        }
        QSpinBox {
            padding: 0;
        }
        QSpinBox:hover,
        QComboBox:hover {
            border-color: #66625d;
            background: #45433f;
        }
        QSpinBox:focus,
        QComboBox:focus {
            border-color: #e6e3de;
        }
        QSpinBox:disabled,
        QComboBox:disabled {
            color: #8a8681;
            background: #22211f;
            border-color: #42403d;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #4a4743;
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
            background: #3a3836;
        }
        QComboBox::drop-down:disabled {
            background: #2c2b29;
            border-left-color: #42403d;
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
            background: #383633;
            border-left: 1px solid #4a4743;
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
            background: #42403d;
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
            color: #dcd9d5;
            font-size: 12px;
            spacing: 7px;
            border: none;
            background: transparent;
        }
        QCheckBox:disabled {
            color: #8a8681;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid #66625d;
            background: #22211f;
        }
        QCheckBox::indicator:hover {
            border-color: #8a8681;
            background: #45433f;
        }
        QCheckBox::indicator:checked {
            background: #e6e3de;
            border-color: #e6e3de;
            image: url(:/icons/check_dark.svg);
        }
        QSlider::groove:horizontal {
            background: #42403d;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #edebe8;
            border: 1px solid #b1ada8;
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
            background: #e6e3de;
            border: 1px solid #e6e3de;
            border-radius: 4px;
            color: #1c1a17;
            font-weight: 600;
        }
        QPushButton[panelAction="primary"]:hover {
            background: #f1efeb;
        }
        QPushButton[panelAction="primary"]:pressed {
            background: #d6d2cc;
        }
        QPushButton[panelAction="primary"]:focus {
            border-color: #f1efeb;
        }
        QPushButton[panelAction="primary"]:disabled {
            color: #8a8681;
            background: #3a3836;
            border-color: #42403d;
        }
        QPushButton[panelAction="secondary"] {
            min-height: 30px;
            max-height: 30px;
            background: #3a3836;
            border: 1px solid #57534f;
            border-radius: 4px;
            color: #dcd9d5;
        }
        QPushButton[panelAction="secondary"]:hover {
            background: #45433f;
            border-color: #66625d;
        }
        QPushButton[panelAction="secondary"]:pressed {
            background: #22211f;
        }
        QPushButton[panelAction="secondary"]:focus {
            border-color: #66625d;
        }
        QPushButton[panelAction="secondary"]:disabled {
            color: #8a8681;
            border-color: #42403d;
        }
        QLabel[panelRole="hint"] {
            color: #b1ada8;
            font-size: 11px;
        }
        QPushButton[panelSegment] {
            min-height: 28px;
            max-height: 28px;
            background: #22211f;
            border: 1px solid #57534f;
            color: #dcd9d5;
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
            background: #45433f;
        }
        QPushButton[panelSegment]:checked {
            background: #4a4743;
            border-color: #66625d;
            color: #edebe8;
        }
        QPushButton[panelSegment]:focus {
            border-color: #70bdff;
        }
        QPushButton[panelAction="link"] {
            background: transparent;
            border: none;
            color: #e0dcd6;
            padding: 2px 0;
            text-align: left;
        }
        QPushButton[panelAction="link"]:hover {
            color: #f1efeb;
            text-decoration: underline;
        }
        QPushButton[panelAction="link"]:focus {
            text-decoration: underline;
        }
        QLabel[panelRole="fieldLabel"] {
            color: #b1ada8;
            font-size: 12px;
        }
        QFrame[panelRole="vseparator"] {
            background: #42403d;
            border: none;
        }
        QWidget[panelRole="segmentTrack"] {
            background: #22211f;
            border: 1px solid #42403d;
            border-radius: 5px;
        }
        QPushButton[panelSegment="track"] {
            min-height: 26px;
            max-height: 26px;
            padding: 0 12px;
            background: transparent;
            border: none;
            border-radius: 3px;
            color: #b1ada8;
            font-weight: 500;
        }
        QPushButton[panelSegment="track"]:hover {
            background: transparent;
            color: #edebe8;
        }
        QPushButton[panelSegment="track"]:checked {
            background: #4a4743;
            border: none;
            color: #edebe8;
            font-weight: 600;
        }
        QWidget#recordingDrawer QComboBox {
            min-height: 30px;
            max-height: 30px;
            padding: 0 4px 0 8px;
            background: #3a3836;
            border: 1px solid #57534f;
        }
        QWidget#recordingDrawer QComboBox:hover {
            background: #45433f;
            border-color: #66625d;
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
            background: #45433f;
        }
        QWidget#recordingDrawer QToolButton:checked {
            background: #4a4743;
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
