#include <QtTest>

#include "core/LinuxPortalGlobalShortcuts.h"
#include "core/LinuxPortalHostRegistry.h"
#include "core/LinuxKGlobalAccelShortcuts.h"
#include "core/LinuxPortalScreenCast.h"

class LinuxPortalShortcutTests : public QObject {
    Q_OBJECT

private slots:
    void classifiesHostPortalRegistration()
    {
        QCOMPARE(LinuxPortalHostRegistry::classifyReply(true, QString(), QString()),
                 LinuxPortalHostRegistrationState::Registered);
        QCOMPARE(LinuxPortalHostRegistry::classifyReply(
                     false, QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface"), QString()),
                 LinuxPortalHostRegistrationState::Unsupported);
        QCOMPARE(LinuxPortalHostRegistry::classifyReply(
                     false, QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"), QString()),
                 LinuxPortalHostRegistrationState::Unsupported);
        QCOMPARE(LinuxPortalHostRegistry::classifyReply(
                     false, QStringLiteral("org.freedesktop.portal.Error.NotAllowed"), QString()),
                 LinuxPortalHostRegistrationState::Failed);
        QCOMPARE(LinuxPortalHostRegistry::classifyReply(
                     false,
                     QStringLiteral("org.freedesktop.portal.Error.Failed"),
                     QStringLiteral("Connection already associated with an application ID")),
                 LinuxPortalHostRegistrationState::Registered);
        QVERIFY(LinuxPortalHostRegistry::portalMayIdentifyApp(
            LinuxPortalHostRegistrationState::Registered));
        QVERIFY(LinuxPortalHostRegistry::portalMayIdentifyApp(
            LinuxPortalHostRegistrationState::Unsupported));
        QVERIFY(!LinuxPortalHostRegistry::portalMayIdentifyApp(
            LinuxPortalHostRegistrationState::Failed));
        QCOMPARE(LinuxPortalHostRegistry::applicationId(),
                 QStringLiteral("io.github.benoks.EShot"));
    }

    void formatsPreferredTriggers()
    {
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredTrigger(0, VK_SNAPSHOT), QStringLiteral("Print"));
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredTrigger(0, VK_SCROLL), QStringLiteral("Scroll_Lock"));
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredTrigger(
                     MOD_CONTROL | MOD_ALT, 'P'),
                 QStringLiteral("CTRL+ALT+p"));
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredTrigger(MOD_SHIFT, '7'),
                 QStringLiteral("SHIFT+7"));
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredTrigger(MOD_CONTROL, VK_F12),
                 QStringLiteral("CTRL+F12"));
        QVERIFY(LinuxPortalGlobalShortcuts::preferredTrigger(0, 0).isEmpty());
        // Modifiers-only triggers (unmapped key) must not produce dangling "+" parts.
        QVERIFY(LinuxPortalGlobalShortcuts::preferredTrigger(
                    MOD_CONTROL | MOD_ALT, VK_APPS).isEmpty());
    }

    void prefersPortalThenFallsBackToX11()
    {
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredBackend(true, true),
                 LinuxHotkeyBackend::Portal);
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredBackend(true, false),
                 LinuxHotkeyBackend::Portal);
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredBackend(false, true),
                 LinuxHotkeyBackend::X11);
        QCOMPARE(LinuxPortalGlobalShortcuts::preferredBackend(false, false),
                 LinuxHotkeyBackend::Unavailable);
    }

    void usesPortalConfigurationUiWhenSupported()
    {
        QVERIFY(!LinuxPortalGlobalShortcuts::supportsConfiguration(1u));
        QVERIFY(LinuxPortalGlobalShortcuts::supportsConfiguration(2u));
        QVERIFY(LinuxPortalGlobalShortcuts::supportsConfiguration(3u));
    }

    void detectsWaylandBehindXwayland()
    {
        QVERIFY(LinuxPortalScreenCast::isWaylandSessionType(QStringLiteral("wayland"),
                                                             QStringLiteral("xcb")));
        QVERIFY(LinuxPortalScreenCast::isWaylandSessionType(QString(),
                                                             QStringLiteral("wayland")));
        QVERIFY(!LinuxPortalScreenCast::isWaylandSessionType(QStringLiteral("x11"),
                                                              QStringLiteral("xcb")));
    }

    void buildsCompatibleGnomeScreenCastOptions()
    {
        const QVariantMap modern = LinuxPortalScreenCast::sourceOptions(
            6u, 1u | 2u | 4u, QStringLiteral("restore-me"));
        QCOMPARE(modern.value(QStringLiteral("types")).toUInt(), 1u);
        QCOMPARE(modern.value(QStringLiteral("multiple")).toBool(), false);
        QCOMPARE(modern.value(QStringLiteral("cursor_mode")).toUInt(), 2u);
        QCOMPARE(modern.value(QStringLiteral("persist_mode")).toUInt(), 2u);
        QCOMPARE(modern.value(QStringLiteral("restore_token")).toString(),
                 QStringLiteral("restore-me"));

        const QVariantMap legacy = LinuxPortalScreenCast::sourceOptions(3u, 1u, {});
        QCOMPARE(legacy.value(QStringLiteral("cursor_mode")).toUInt(), 1u);
        QVERIFY(!legacy.contains(QStringLiteral("persist_mode")));
        QVERIFY(!legacy.contains(QStringLiteral("restore_token")));
    }

    void detectsKdeDesktop()
    {
        QVERIFY(LinuxKGlobalAccelShortcuts::isKdeDesktop(QStringLiteral("KDE")));
        QVERIFY(LinuxKGlobalAccelShortcuts::isKdeDesktop(QStringLiteral("plasma:KDE")));
        QVERIFY(!LinuxKGlobalAccelShortcuts::isKdeDesktop(QStringLiteral("GNOME")));
    }

    void usesStableKdeActionIds()
    {
        const QStringList id = LinuxKGlobalAccelShortcuts::actionId(1);
        QCOMPARE(id.value(0), QStringLiteral("io.github.benoks.EShot"));
        QCOMPARE(id.value(1), QStringLiteral("eshot_capture"));
        QCOMPARE(id.value(2), QStringLiteral("EShot"));
        QCOMPARE(id.value(3), QStringLiteral("Capture"));
    }

    void marksKdeShortcutsAsPresent()
    {
        QCOMPARE(LinuxKGlobalAccelShortcuts::registrationFlags(), uint(6));
        QCOMPARE(LinuxKGlobalAccelShortcuts::defaultRegistrationFlags(), uint(12));
    }

    void fallsBackToPortalWhenKdeCannotClaimShortcut()
    {
        QVERIFY(LinuxKGlobalAccelShortcuts::shouldUsePortalFallback(false, true));
        QVERIFY(!LinuxKGlobalAccelShortcuts::shouldUsePortalFallback(true, true));
        QVERIFY(!LinuxKGlobalAccelShortcuts::shouldUsePortalFallback(false, false));
    }
};

QTEST_APPLESS_MAIN(LinuxPortalShortcutTests)
#include "LinuxPortalShortcutTests.moc"
