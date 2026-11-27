#include "ApplicationTheme.h"

#include <QApplication>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QTabBar>
#include <QList>
#include <QPair>

#include "DesignTokens.h"

namespace {

// QPalette::setColor(role, colour) fills every colour group, which made
// disabled controls look exactly like enabled ones. Give the Disabled group
// its own dimmed text and a matching placeholder colour.
void setDisabledText(QPalette &p, const QColor &disabledText, const QColor &placeholder)
{
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, disabledText);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);
    p.setColor(QPalette::PlaceholderText, placeholder);
    p.setColor(QPalette::Disabled, QPalette::PlaceholderText, disabledText);
}

}

QPalette eshotApplicationPalette(bool dark, bool highContrast)
{
    QPalette p;
    if (highContrast) {
        p.setColor(QPalette::Window, Qt::black);
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, Qt::black);
        p.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
        p.setColor(QPalette::ToolTipBase, Qt::black);
        p.setColor(QPalette::ToolTipText, Qt::yellow);
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, Qt::black);
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, Qt::cyan);
        p.setColor(QPalette::Highlight, Qt::yellow);
        p.setColor(QPalette::HighlightedText, Qt::black);
        // Grey stays distinct from white while keeping 6:1 on black.
        setDisabledText(p, QColor(150, 150, 150), QColor(200, 200, 200));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(90, 90, 90));
    } else if (dark) {
        // Charcoal, the same values as the capture overlay (DesignTokens.h).
        p.setColor(QPalette::Window, QColor(DesignTokens::Panel));
        p.setColor(QPalette::WindowText, QColor(DesignTokens::Text));
        p.setColor(QPalette::Base, QColor(DesignTokens::Field));
        p.setColor(QPalette::AlternateBase, QColor(DesignTokens::Separator));
        p.setColor(QPalette::ToolTipBase, QColor(DesignTokens::Control));
        p.setColor(QPalette::ToolTipText, QColor(DesignTokens::Text));
        p.setColor(QPalette::Text, QColor(DesignTokens::Text));
        p.setColor(QPalette::Button, QColor(DesignTokens::Control));
        p.setColor(QPalette::ButtonText, QColor(DesignTokens::Text));
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(DesignTokens::Link));
        p.setColor(QPalette::Highlight, QColor(DesignTokens::Accent));
        p.setColor(QPalette::HighlightedText, QColor(DesignTokens::OnAccent));
        p.setColor(QPalette::Mid, QColor(DesignTokens::ControlBorder));
        p.setColor(QPalette::Dark, QColor(DesignTokens::Field));
        setDisabledText(p, QColor(DesignTokens::TextDisabled), QColor(150, 152, 157));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(DesignTokens::ControlBorder));
    } else {
        p.setColor(QPalette::Window, QColor(240, 240, 240));
        p.setColor(QPalette::WindowText, Qt::black);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(233, 233, 233));
        p.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
        p.setColor(QPalette::ToolTipText, Qt::black);
        p.setColor(QPalette::Text, Qt::black);
        p.setColor(QPalette::Button, QColor(240, 240, 240));
        p.setColor(QPalette::ButtonText, Qt::black);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(0, 0, 255));
        p.setColor(QPalette::Highlight, QColor(0, 120, 215));
        p.setColor(QPalette::HighlightedText, Qt::white);
        setDisabledText(p, QColor(140, 140, 140), QColor(110, 110, 110));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(190, 190, 190));
    }
    return p;
}

QString eshotDialogStyleSheet()
{
    // Graphite dialogs share the overlay's controls: flat 4 px controls, one
    // accent for the default button and focus. Scoped to QDialog so the
    // capture overlay keeps its own style sheets.
    QString css = QStringLiteral(R"(
        QDialog QPushButton {
            background: @control; color: @text; border: 1px solid @controlBorder;
            border-radius: 4px; padding: 4px 14px; min-height: 20px;
        }
        QDialog QPushButton:hover { background: @controlHover; border-color: @controlBorderHover; }
        QDialog QPushButton:pressed { background: @controlPressed; }
        QDialog QPushButton:default { background: @accent; border-color: @accent; color: @onAccent; font-weight: 600; }
        QDialog QPushButton:default:hover { background: @accentHover; border-color: @accentHover; }
        QDialog QPushButton:default:pressed { background: @accentPressed; }
        QDialog QPushButton:disabled { color: @textDisabled; background: @controlPressed; border-color: @panelBorder; }
        QDialog QLineEdit, QDialog QPlainTextEdit, QDialog QTextEdit,
        QDialog QSpinBox, QDialog QComboBox, QDialog QKeySequenceEdit {
            background: @field; color: @text; border: 1px solid @controlBorder;
            border-radius: 4px; selection-background-color: @accent; selection-color: @onAccent;
        }
        QDialog QLineEdit, QDialog QSpinBox, QDialog QComboBox { padding: 3px 6px; min-height: 20px; }
        QDialog QLineEdit:focus, QDialog QPlainTextEdit:focus, QDialog QTextEdit:focus,
        QDialog QSpinBox:focus, QDialog QComboBox:focus { border-color: @accent; }
        QDialog QLineEdit:disabled, QDialog QSpinBox:disabled, QDialog QComboBox:disabled {
            color: @textDisabled; border-color: @panelBorder;
        }
        QDialog QComboBox::drop-down {
            subcontrol-origin: padding; subcontrol-position: top right; width: 24px;
            border-left: 1px solid @controlBorder;
        }
        QDialog QComboBox::down-arrow { image: url(:/icons/chevron_down.svg); width: 12px; height: 12px; }
        QDialog QComboBox QAbstractItemView {
            background: @panel; color: @text; border: 1px solid @panelBorder;
            selection-background-color: @selected; selection-color: @text; outline: none;
        }
        QDialog QSpinBox::up-button, QDialog QSpinBox::down-button {
            subcontrol-origin: border; width: 20px; border-left: 1px solid @controlBorder; background: @control;
        }
        QDialog QSpinBox::up-button { subcontrol-position: top right; border-top-right-radius: 4px; }
        QDialog QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 4px; }
        QDialog QSpinBox::up-arrow { image: url(:/icons/chevron_up.svg); width: 10px; height: 10px; }
        QDialog QSpinBox::down-arrow { image: url(:/icons/chevron_down.svg); width: 10px; height: 10px; }
        QDialog QCheckBox::indicator {
            width: 14px; height: 14px; border-radius: 3px;
            border: 1px solid @controlBorderHover; background: @field;
        }
        QDialog QCheckBox::indicator:hover { border-color: @textMuted; }
        QDialog QCheckBox::indicator:checked { background: @accent; border-color: @accent; image: url(:/icons/check_dark.svg); }
        QDialog QRadioButton::indicator { width: 14px; height: 14px; border-radius: 8px; border: 1px solid @controlBorderHover; background: @field; }
        QDialog QRadioButton::indicator:checked { background: @accent; border: 4px solid @field; }
        QDialog QTabWidget::pane { border: 1px solid @panelBorder; border-radius: 6px; top: 6px; padding-top: 4px; }
        QDialog QTabBar::tab {
            background: @control; color: @text; padding: 5px 12px; margin-right: 4px;
            border: 1px solid @controlBorder; border-radius: 4px;
        }
        QDialog QTabBar::tab:hover { background: @controlHover; border-color: @controlBorderHover; }
        QDialog QTabBar::tab:selected { background: @selected; border-color: @controlBorderHover; font-weight: 600; }
        QDialog QCheckBox::indicator:disabled { background: @controlPressed; border-color: @panelBorder; }
        QDialog QGroupBox {
            border: 1px solid @panelBorder; border-radius: 6px; margin-top: 16px; padding: 10px 8px 8px;
        }
        QDialog QGroupBox::title {
            subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px;
            color: @text; font-weight: 600;
        }
        QDialog QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QDialog QScrollBar::handle:vertical { background: @controlBorder; border-radius: 4px; min-height: 28px; margin: 1px; }
        QDialog QScrollBar::handle:vertical:hover { background: @controlBorderHover; }
        QDialog QScrollBar::add-line:vertical, QDialog QScrollBar::sub-line:vertical { height: 0; }
        QDialog QScrollBar::add-page:vertical, QDialog QScrollBar::sub-page:vertical { background: none; }
        QToolTip { background: @control; color: @text; border: 1px solid @panelBorder; padding: 4px 6px; }
    )");
    const QList<QPair<const char *, const char *>> tokens = {
        {"@controlBorderHover", DesignTokens::ControlBorderHover},
        {"@controlBorder", DesignTokens::ControlBorder},
        {"@controlPressed", DesignTokens::ControlPressed},
        {"@controlHover", DesignTokens::ControlHover},
        {"@control", DesignTokens::Control},
        {"@accentPressed", DesignTokens::AccentPressed},
        {"@accentHover", DesignTokens::AccentHover},
        {"@accent", DesignTokens::Accent},
        {"@onAccent", DesignTokens::OnAccent},
        {"@textDisabled", DesignTokens::TextDisabled},
        {"@textMuted", DesignTokens::TextMuted},
        {"@text", DesignTokens::Text},
        {"@panelBorder", DesignTokens::PanelBorder},
        {"@panel", DesignTokens::Panel},
        {"@field", DesignTokens::Field},
        {"@selected", DesignTokens::Selected},
    };
    for (const auto &token : tokens)
        css.replace(QLatin1String(token.first), QLatin1String(token.second));
    return css;
}

namespace {

// Clickable controls in dialogs get a pointing hand, like the overlay panels.
class DialogPointerCursor : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Polish)
            return false;
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget || widget->testAttribute(Qt::WA_SetCursor) || !qobject_cast<QDialog *>(widget->window()))
            return false;
        if (qobject_cast<QAbstractButton *>(widget) || qobject_cast<QComboBox *>(widget)
            || qobject_cast<QTabBar *>(widget) || qobject_cast<QAbstractSlider *>(widget))
            widget->setCursor(Qt::PointingHandCursor);
        return false;
    }
};

}

void applyEShotApplicationTheme(QApplication &application, bool dark, bool highContrast)
{
    static DialogPointerCursor *pointerCursor = nullptr;
    if (!pointerCursor) {
        pointerCursor = new DialogPointerCursor(&application);
        application.installEventFilter(pointerCursor);
    }
    application.setPalette(eshotApplicationPalette(dark, highContrast));
    application.setStyleSheet(dark && !highContrast ? eshotDialogStyleSheet() : QString());
}

QString eshotTrayMenuStyleSheet(bool dark, bool highContrast)
{
    struct MenuColors {
        const char *background;
        const char *text;
        const char *border;
        const char *selectedBackground;
        const char *selectedText;
        const char *disabledText;
        const char *separator;
        int borderWidth;
    };
    const MenuColors colors = highContrast
        ? MenuColors{"#000000", "#ffffff", "#ffffff", "#ffff00", "#000000", "#969696", "#ffffff", 2}
        : dark
            ? MenuColors{DesignTokens::Panel, DesignTokens::Text, DesignTokens::PanelBorder,
                         DesignTokens::Selected, DesignTokens::Text, DesignTokens::TextDisabled,
                         DesignTokens::Separator, 1}
            : MenuColors{"#f9f9f9", "#1a1a1a", "#c4c4c4", "#dceaf8", "#000000", "#6e6e6e", "#d4d4d4", 1};
    return QStringLiteral(
               "QMenu {"
               "  background: %1;"
               "  color: %2;"
               "  border: %8px solid %3;"
               "  border-radius: 3px;"
               "  padding: 3px;"
               "}"
               "QMenu::item {"
               "  min-height: 22px;"
               "  padding: 3px 18px 3px 24px;"
               "  border-radius: 2px;"
               "}"
               "QMenu::item:selected { background: %4; color: %5; }"
               "QMenu::item:disabled { color: %6; }"
               "QMenu::icon { width: 16px; height: 16px; left: 5px; }"
               "QMenu::separator { height: 1px; background: %7; margin: 4px 5px; }")
        .arg(QLatin1String(colors.background), QLatin1String(colors.text),
             QLatin1String(colors.border), QLatin1String(colors.selectedBackground),
             QLatin1String(colors.selectedText), QLatin1String(colors.disabledText),
             QLatin1String(colors.separator))
        .arg(colors.borderWidth);
}
