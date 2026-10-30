#include <QtTest>

#include <cmath>
#include <utility>

#include "ui/ApplicationTheme.h"
#include "ui/ThemeColors.h"

#include <QApplication>
#include <QLabel>

class ApplicationThemeTests : public QObject {
    Q_OBJECT
private slots:
    void createsExplicitPalettes()
    {
        QCOMPARE(eshotApplicationPalette(true, false).color(QPalette::Window), QColor(53, 53, 53));
        QCOMPARE(eshotApplicationPalette(false, false).color(QPalette::Base), QColor(Qt::white));
        QCOMPARE(eshotApplicationPalette(false, true).color(QPalette::Highlight), QColor(Qt::yellow));
    }

    void disabledTextDiffersFromEnabledText()
    {
        for (const auto &mode : {std::pair{true, false}, std::pair{false, false}, std::pair{true, true}}) {
            const QPalette p = eshotApplicationPalette(mode.first, mode.second);
            for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
                QVERIFY(p.color(QPalette::Disabled, role) != p.color(QPalette::Active, role));
                QCOMPARE(p.color(QPalette::Inactive, role), p.color(QPalette::Active, role));
            }
            const QColor placeholder = p.color(QPalette::Active, QPalette::PlaceholderText);
            QVERIFY(placeholder != p.color(QPalette::Active, QPalette::Text));
            QVERIFY(placeholder != p.color(QPalette::Active, QPalette::Base));
        }
    }

    void trayMenuFollowsTheTheme()
    {
        const QString dark = eshotTrayMenuStyleSheet(true, false);
        const QString light = eshotTrayMenuStyleSheet(false, false);
        const QString highContrast = eshotTrayMenuStyleSheet(true, true);
        QVERIFY(dark.contains(QStringLiteral("background: #2b2b2b")));
        QVERIFY(light.contains(QStringLiteral("background: #f9f9f9")));
        QVERIFY(highContrast.contains(QStringLiteral("2px solid #ffffff")));
        QVERIFY(highContrast.contains(QStringLiteral("background: #ffff00")));
        QCOMPARE(eshotTrayMenuStyleSheet(false, true), highContrast);
    }

    void themeColorsDetectThePalette()
    {
        using namespace ThemeColors;
        QCOMPARE(themeForPalette(eshotApplicationPalette(true, false)), Theme::Dark);
        QCOMPARE(themeForPalette(eshotApplicationPalette(false, false)), Theme::Light);
        QCOMPARE(themeForPalette(eshotApplicationPalette(true, true)), Theme::HighContrast);
        QCOMPARE(themeForPalette(eshotApplicationPalette(false, true)), Theme::HighContrast);
    }

    void themeColorsKeepTheDarkLookAndReadableLightText()
    {
        using namespace ThemeColors;
        // Dark values are the colours the dialogs used before.
        QCOMPARE(css(Role::Muted, Theme::Dark), QStringLiteral("#aaaaaa"));
        QCOMPARE(css(Role::Warning, Theme::Dark), QStringLiteral("#ff9800"));
        QCOMPARE(css(Role::ListBackground, Theme::Dark), QStringLiteral("#252525"));

        const auto contrast = [](const QColor &a, const QColor &b) {
            auto luminance = [](const QColor &c) {
                auto channel = [](double v) {
                    v /= 255.0;
                    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
                };
                return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green())
                    + 0.0722 * channel(c.blue());
            };
            const double la = luminance(a), lb = luminance(b);
            return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
        };
        const QColor lightWindow = eshotApplicationPalette(false, false).color(QPalette::Window);
        const QColor hcWindow = eshotApplicationPalette(true, true).color(QPalette::Window);
        for (Role role : {Role::Muted, Role::Hint, Role::Secondary, Role::Success, Role::SuccessSoft,
                          Role::Warning, Role::Caution, Role::Danger, Role::AccentText, Role::Link}) {
            QVERIFY2(contrast(color(role, Theme::Light), lightWindow) >= 4.5,
                     qPrintable(css(role, Theme::Light)));
            QVERIFY2(contrast(color(role, Theme::HighContrast), hcWindow) >= 7.0,
                     qPrintable(css(role, Theme::HighContrast)));
        }
    }

    void themeColorsResolveTemplates()
    {
        using namespace ThemeColors;
        QCOMPARE(resolve(QStringLiteral("QLabel { color: @hint; border: 1px solid @listBorder; } @nope"),
                         Theme::Light),
                 QStringLiteral("QLabel { color: #595959; border: 1px solid #bdbdbd; } @nope"));
    }

    void themedStyleSheetFollowsPaletteChanges()
    {
        int argc = 1;
        char arg0[] = "eshot_application_theme_tests";
        char *argv[] = {arg0, nullptr};
        QApplication app(argc, argv);
        applyEShotApplicationTheme(app, true, false);
        QWidget window;
        QLabel label(&window);
        ThemeColors::setStyleSheet(&label, QStringLiteral("color: @hint;"));
        QCOMPARE(label.styleSheet(), QStringLiteral("color: #999999;"));
        // Qt delivers the palette change events from the event loop.
        applyEShotApplicationTheme(app, false, false);
        QTRY_COMPARE(label.styleSheet(), QStringLiteral("color: #595959;"));
        applyEShotApplicationTheme(app, true, true);
        QTRY_COMPARE(label.styleSheet(), QStringLiteral("color: #ffffff;"));
    }
};

QTEST_APPLESS_MAIN(ApplicationThemeTests)
#include "ApplicationThemeTests.moc"
