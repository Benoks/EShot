#include "ApplicationTheme.h"

#include <QApplication>
#include <QColor>

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
        p.setColor(QPalette::Window, QColor(53, 53, 53));
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, QColor(42, 42, 42));
        p.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
        p.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
        p.setColor(QPalette::ToolTipText, Qt::white);
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, QColor(53, 53, 53));
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(42, 130, 218));
        p.setColor(QPalette::Highlight, QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::black);
        setDisabledText(p, QColor(127, 127, 127), QColor(150, 150, 150));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
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

void applyEShotApplicationTheme(QApplication &application, bool dark, bool highContrast)
{
    application.setPalette(eshotApplicationPalette(dark, highContrast));
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
            ? MenuColors{"#2b2b2b", "#ffffff", "#4a4a4a", "#3a3a3a", "#ffffff", "#8a8a8a", "#424242", 1}
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
