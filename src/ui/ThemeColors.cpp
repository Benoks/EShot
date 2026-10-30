#include "ThemeColors.h"

#include <QApplication>
#include <QEvent>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPalette>
#include <QPointer>
#include <QSet>
#include <QRegularExpression>
#include <QWidget>

namespace ThemeColors {

namespace {

constexpr const char *kTemplateProperty = "eshotThemeStyleTemplate";

struct Swatch {
    QRgb dark;
    QRgb light;
    QRgb highContrast;
};

Swatch swatch(Role role)
{
    // Light values keep at least 4.5:1 against the light window (#f0f0f0);
    // high-contrast values are pure white/yellow/green/cyan on black.
    switch (role) {
    case Role::Muted:           return {0xaaaaaa, 0x4f4f4f, 0xffffff};
    case Role::Hint:            return {0x999999, 0x595959, 0xffffff};
    case Role::Secondary:       return {0xd0d0d0, 0x2f2f2f, 0xffffff};
    case Role::Success:         return {0x4caf50, 0x276b2d, 0x00ff00};
    case Role::SuccessSoft:     return {0x8bd17c, 0x276b2d, 0x00ff00};
    case Role::Warning:         return {0xff9800, 0xa85100, 0xffff00};
    case Role::Caution:         return {0xf0c36d, 0x805600, 0xffff00};
    case Role::BannerText:      return {0xffd166, 0x5c4300, 0xffff00};
    case Role::BannerErrorText: return {0xffb3bc, 0x9b1c28, 0xff8080};
    case Role::Danger:          return {0xff6b6b, 0xc62828, 0xff6060};
    case Role::AccentText:      return {0x8ab4f8, 0x1a5fb4, 0x00ffff};
    case Role::Link:            return {0x5b9bd5, 0x0b57d0, 0x00ffff};
    case Role::ListBackground:  return {0x252525, 0xffffff, 0x000000};
    case Role::ListBorder:      return {0x3d3d3d, 0xbdbdbd, 0xffffff};
    case Role::ListHover:       return {0x303030, 0xe5f1fb, 0x333333};
    case Role::InputBackground: return {0x2a2a2a, 0xffffff, 0x000000};
    case Role::InputBorder:     return {0x555555, 0x8a8a8a, 0xffffff};
    case Role::InputText:       return {0xffffff, 0x000000, 0xffffff};
    }
    return {0xffffff, 0x000000, 0xffffff};
}

const QHash<QString, Role> &roleNames()
{
    static const QHash<QString, Role> names = {
        {QStringLiteral("muted"), Role::Muted},
        {QStringLiteral("hint"), Role::Hint},
        {QStringLiteral("secondary"), Role::Secondary},
        {QStringLiteral("success"), Role::Success},
        {QStringLiteral("successSoft"), Role::SuccessSoft},
        {QStringLiteral("warning"), Role::Warning},
        {QStringLiteral("caution"), Role::Caution},
        {QStringLiteral("bannerText"), Role::BannerText},
        {QStringLiteral("bannerErrorText"), Role::BannerErrorText},
        {QStringLiteral("danger"), Role::Danger},
        {QStringLiteral("accentText"), Role::AccentText},
        {QStringLiteral("link"), Role::Link},
        {QStringLiteral("listBackground"), Role::ListBackground},
        {QStringLiteral("listBorder"), Role::ListBorder},
        {QStringLiteral("listHover"), Role::ListHover},
        {QStringLiteral("inputBackground"), Role::InputBackground},
        {QStringLiteral("inputBorder"), Role::InputBorder},
        {QStringLiteral("inputText"), Role::InputText},
    };
    return names;
}

void reapplyTemplate(QWidget *widget)
{
    const QVariant styleTemplate = widget->property(kTemplateProperty);
    if (!styleTemplate.isValid())
        return;
    const QString resolved = resolve(styleTemplate.toString(), currentTheme());
    if (widget->styleSheet() != resolved)
        widget->setStyleSheet(resolved);
}

// Watches the application for palette changes (the live theme preview in
// Settings) and re-resolves every themed style sheet. Whether a single
// widget sees ApplicationPaletteChange depends on its place in the widget
// tree, so the filter sits on the application object, which sees them all.
class PaletteFollower final : public QObject {
public:
    explicit PaletteFollower(QObject *parent) : QObject(parent) {}

    void track(QWidget *widget)
    {
        if (m_widgets.contains(widget))
            return;
        m_widgets.insert(widget);
        connect(widget, &QObject::destroyed, this,
                [this, widget]() { m_widgets.remove(widget); });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::ApplicationPaletteChange && !m_reapplying) {
            const Theme theme = currentTheme();
            if (theme != m_lastTheme) {
                m_lastTheme = theme;
                m_reapplying = true;
                const auto widgets = m_widgets;
                for (QWidget *widget : widgets)
                    reapplyTemplate(widget);
                m_reapplying = false;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QSet<QWidget *> m_widgets;
    Theme m_lastTheme = currentTheme();
    bool m_reapplying = false;
};

PaletteFollower *paletteFollower()
{
    static QPointer<PaletteFollower> follower;
    if (!follower && qApp) {
        follower = new PaletteFollower(qApp);
        qApp->installEventFilter(follower);
    }
    return follower;
}

} // namespace

Theme themeForPalette(const QPalette &palette)
{
    const QColor window = palette.color(QPalette::Active, QPalette::Window);
    const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
    if (window == QColor(Qt::black) && highlight == QColor(Qt::yellow))
        return Theme::HighContrast;
    return window.lightness() < 128 ? Theme::Dark : Theme::Light;
}

Theme currentTheme()
{
    if (!qApp)
        return Theme::Dark;
    return themeForPalette(QApplication::palette());
}

QColor color(Role role, Theme theme)
{
    const Swatch s = swatch(role);
    switch (theme) {
    case Theme::Dark: return QColor::fromRgb(s.dark);
    case Theme::Light: return QColor::fromRgb(s.light);
    case Theme::HighContrast: return QColor::fromRgb(s.highContrast);
    }
    return QColor::fromRgb(s.dark);
}

QColor color(Role role)
{
    return color(role, currentTheme());
}

QString css(Role role, Theme theme)
{
    return color(role, theme).name(QColor::HexRgb);
}

QString css(Role role)
{
    return css(role, currentTheme());
}

QString resolve(const QString &styleSheetTemplate, Theme theme)
{
    static const QRegularExpression token(QStringLiteral("@([A-Za-z]+)"));
    QString result;
    result.reserve(styleSheetTemplate.size());
    qsizetype last = 0;
    auto it = token.globalMatch(styleSheetTemplate);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result += QStringView(styleSheetTemplate).mid(last, match.capturedStart() - last);
        const auto role = roleNames().constFind(match.captured(1));
        result += role != roleNames().constEnd() ? css(role.value(), theme) : match.captured(0);
        last = match.capturedEnd();
    }
    result += QStringView(styleSheetTemplate).mid(last);
    return result;
}

void setStyleSheet(QWidget *widget, const QString &styleSheetTemplate)
{
    if (!widget)
        return;
    widget->setProperty(kTemplateProperty, styleSheetTemplate);
    widget->setStyleSheet(resolve(styleSheetTemplate, currentTheme()));
    if (PaletteFollower *follower = paletteFollower())
        follower->track(widget);
}

QIcon iconForTheme(const QIcon &icon, const QSize &size)
{
    if (icon.isNull() || currentTheme() != Theme::Light)
        return icon;
    QPixmap pixmap = icon.pixmap(size);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), QColor(0x20, 0x20, 0x20));
    painter.end();
    return QIcon(pixmap);
}

} // namespace ThemeColors
