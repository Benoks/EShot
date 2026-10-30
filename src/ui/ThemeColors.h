#ifndef THEMECOLORS_H
#define THEMECOLORS_H

#include <QColor>
#include <QString>

class QIcon;
class QPalette;
class QSize;
class QWidget;

// Text and surface colours that are not plain palette roles (hint text,
// warnings, success, list backgrounds). Each one has a value for the dark,
// light and high-contrast application palettes, so hand-written style sheets
// stay readable in every theme. The dark values match the colours the
// dialogs used before the light theme was supported.
namespace ThemeColors {

enum class Theme { Dark, Light, HighContrast };

enum class Role {
    Muted,          // secondary descriptions
    Hint,           // small help text under a control
    Secondary,      // de-emphasised but still primary text
    Success,
    SuccessSoft,    // "installed" package state
    Warning,        // status messages that need attention
    Caution,        // "missing" package state, update available
    BannerText,     // text on the yellow notice banner
    BannerErrorText,// text on the red notice banner
    Danger,         // destructive buttons
    AccentText,     // section titles
    Link,
    ListBackground,
    ListBorder,
    ListHover,
    InputBackground,
    InputBorder,
    InputText,
};

// The theme of a palette built by eshotApplicationPalette().
Theme themeForPalette(const QPalette &palette);
// The theme of the current application palette.
Theme currentTheme();

QColor color(Role role, Theme theme);
QColor color(Role role);
// "#rrggbb" for use in style sheets.
QString css(Role role, Theme theme);
QString css(Role role);

// Replaces @muted, @hint, @warning, ... in a style sheet template with the
// colours of the given theme. Unknown names are left untouched.
QString resolve(const QString &styleSheetTemplate, Theme theme);

// Sets the resolved template as the widget's style sheet and re-resolves it
// whenever the application palette changes (the live theme preview in
// Settings), so the widget follows dark/light/high-contrast switches.
void setStyleSheet(QWidget *widget, const QString &styleSheetTemplate);

// The app's icons are white line art for dark backgrounds; on the light
// theme this returns a dark-tinted copy, otherwise the icon unchanged.
QIcon iconForTheme(const QIcon &icon, const QSize &size);

} // namespace ThemeColors

#endif
