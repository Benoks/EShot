#include <QtTest>

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include "core/LinuxGnomeShortcutInstaller.h"

class LinuxGnomeShortcutInstallerTests : public QObject {
    Q_OBJECT

private slots:
    void quotesCaptureExecutable()
    {
        QCOMPARE(LinuxGnomeShortcutInstaller::captureCommand(
                     QStringLiteral("/home/user/EShot AppImage")),
                 QStringLiteral("'/home/user/EShot AppImage' --capture"));
        QCOMPARE(LinuxGnomeShortcutInstaller::captureCommand(
                     QStringLiteral("/tmp/user's/EShot")),
                 QStringLiteral("'/tmp/user'\\''s/EShot' --capture"));
        QVERIFY(LinuxGnomeShortcutInstaller::captureCommand({}).isEmpty());
    }

    void prefersStableIntegratedAppImagePath()
    {
        QCOMPARE(LinuxGnomeShortcutInstaller::preferredExecutable(
                     QStringLiteral("/tmp/EShot.AppImage"),
                     QStringLiteral("/tmp/.mount/usr/bin/EShot"),
                     QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage")),
                 QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage"));
        QCOMPARE(LinuxGnomeShortcutInstaller::preferredExecutable(
                     {}, QStringLiteral("/usr/bin/EShot"), {}),
                 QStringLiteral("/usr/bin/EShot"));
    }

    void convertsQtPortableShortcutsToGnomeAccelerators()
    {
        QCOMPARE(LinuxGnomeShortcutInstaller::acceleratorFromPortableSequence(
                     QStringLiteral("Ctrl+Alt+P")),
                 QStringLiteral("<Primary><Alt>p"));
        QCOMPARE(LinuxGnomeShortcutInstaller::acceleratorFromPortableSequence(
                     QStringLiteral("Meta+Shift+F12")),
                 QStringLiteral("<Super><Shift>F12"));
        QCOMPARE(LinuxGnomeShortcutInstaller::acceleratorFromPortableSequence(
                     QStringLiteral("Print")), QStringLiteral("Print"));
        QCOMPARE(LinuxGnomeShortcutInstaller::acceleratorFromPortableSequence(
                     QStringLiteral("Scroll_Lock")), QStringLiteral("Scroll_Lock"));
        QVERIFY(LinuxGnomeShortcutInstaller::acceleratorFromPortableSequence(
                    QStringLiteral("Unknown+P")).isEmpty());
    }

    void convertsPortalTriggersToGnomeAccelerators()
    {
        QCOMPARE(LinuxGnomeShortcutInstaller::acceleratorFromPortalTrigger(
                     QStringLiteral("CTRL+SHIFT+s")),
                 QStringLiteral("<Primary><Shift>s"));
        QCOMPARE(LinuxGnomeShortcutInstaller::acceleratorFromPortalTrigger(
                     QStringLiteral("LOGO+ALT+F12")),
                 QStringLiteral("<Super><Alt>F12"));
        QCOMPARE(LinuxGnomeShortcutInstaller::acceleratorFromPortalTrigger(
                     QStringLiteral("Print")), QStringLiteral("Print"));
        QVERIFY(LinuxGnomeShortcutInstaller::acceleratorFromPortalTrigger(
                    QStringLiteral("NUM+p")).isEmpty());
    }

    void serializesShellCommandsAsGSettingsStringValues()
    {
        QCOMPARE(LinuxGnomeShortcutInstaller::gsettingsStringValue(
                     QStringLiteral("'/home/user/EShot AppImage' --capture")),
                 QStringLiteral("\"'/home/user/EShot AppImage' --capture\""));
        QCOMPARE(LinuxGnomeShortcutInstaller::gsettingsStringValue(
                     QStringLiteral("a\\b\"c")),
                 QStringLiteral("\"a\\\\b\\\"c\""));
    }

    void removesOnlyTheLegacyEshotShortcutAndRestoresPrintScreen()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QString logPath = temporary.filePath(QStringLiteral("gsettings.log"));
        QFile fake(temporary.filePath(QStringLiteral("gsettings")));
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Text));
        fake.write(
            "#!/bin/sh\n"
            "printf '%s\\n' \"$*\" >> \"$ESHOT_GSETTINGS_LOG\"\n"
            "case \"$*\" in\n"
            "  'get org.gnome.settings-daemon.plugins.media-keys custom-keybindings')\n"
            "    printf \"['/other/', '/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/eshot/']\\n\" ;;\n"
            "  'list-keys org.gnome.shell.keybindings') printf 'show-screenshot-ui\\n' ;;\n"
            "esac\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        const QByteArray previousPath = qgetenv("PATH");
        qputenv("PATH", temporary.path().toUtf8() + ':' + previousPath);
        qputenv("ESHOT_GSETTINGS_LOG", logPath.toUtf8());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           temporary.filePath(QStringLiteral("settings")));
        {
            QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
            settings.setValue(QStringLiteral("linux/gnomePreviousScreenshotBinding"),
                              QStringLiteral("['Print']"));
            settings.sync();
        }

        const auto result = LinuxGnomeShortcutInstaller::uninstallCaptureShortcut();
        QVERIFY2(result.success, qPrintable(result.error));

        QFile log(logPath);
        QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString calls = QString::fromUtf8(log.readAll());
        QVERIFY(calls.contains(QStringLiteral(
            "set org.gnome.settings-daemon.plugins.media-keys custom-keybindings ['/other/']")));
        QVERIFY(calls.contains(QStringLiteral(
            "reset org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/eshot/ binding")));
        QVERIFY(calls.contains(QStringLiteral(
            "set org.gnome.shell.keybindings show-screenshot-ui ['Print']")));

        QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
        QVERIFY(!settings.contains(QStringLiteral("linux/gnomePreviousScreenshotBinding")));
        qputenv("PATH", previousPath);
        qunsetenv("ESHOT_GSETTINGS_LOG");
    }

    void portalMigrationKeepsTheBuiltInPrintScreenDisabled()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        const QString logPath = temporary.filePath(QStringLiteral("gsettings.log"));
        QFile fake(temporary.filePath(QStringLiteral("gsettings")));
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Text));
        fake.write(
            "#!/bin/sh\n"
            "printf '%s\\n' \"$*\" >> \"$ESHOT_GSETTINGS_LOG\"\n"
            "case \"$*\" in\n"
            "  'get org.gnome.settings-daemon.plugins.media-keys custom-keybindings')\n"
            "    printf \"['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/eshot/']\\n\" ;;\n"
            "  'list-keys org.gnome.shell.keybindings') printf 'show-screenshot-ui\\n' ;;\n"
            "esac\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));

        const QByteArray previousPath = qgetenv("PATH");
        qputenv("PATH", temporary.path().toUtf8() + ':' + previousPath);
        qputenv("ESHOT_GSETTINGS_LOG", logPath.toUtf8());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           temporary.filePath(QStringLiteral("settings")));
        QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
        settings.setValue(QStringLiteral("linux/gnomePreviousScreenshotBinding"),
                          QStringLiteral("['Print']"));
        settings.sync();

        const auto result = LinuxGnomeShortcutInstaller::uninstallCaptureShortcut(false);
        QVERIFY2(result.success, qPrintable(result.error));

        QFile log(logPath);
        QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString calls = QString::fromUtf8(log.readAll());
        QVERIFY(!calls.contains(QStringLiteral(
            "set org.gnome.shell.keybindings show-screenshot-ui")));
        QVERIFY(settings.contains(QStringLiteral("linux/gnomePreviousScreenshotBinding")));
        qputenv("PATH", previousPath);
        qunsetenv("ESHOT_GSETTINGS_LOG");
    }
};

QTEST_APPLESS_MAIN(LinuxGnomeShortcutInstallerTests)
#include "LinuxGnomeShortcutInstallerTests.moc"
