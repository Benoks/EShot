#include <QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QStandardPaths>
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

    void quotesForwardingCaptureCommand()
    {
        const QString command = LinuxGnomeShortcutInstaller::forwardingCaptureCommand(
            QStringLiteral("/home/user/My Apps/EShot.AppImage"));
        QVERIFY(command.startsWith(QStringLiteral("/bin/sh -c '")));
        QVERIFY(command.endsWith(QStringLiteral("' eshot-capture '/home/user/My Apps/EShot.AppImage'")));
        // The script is embedded in single quotes, so it must not contain any.
        QVERIFY(!LinuxGnomeShortcutInstaller::captureForwarderScript().contains(QLatin1Char('\'')));
        QVERIFY(LinuxGnomeShortcutInstaller::forwardingCaptureCommand({}).isEmpty());
    }

    void forwarderSendsCaptureToTheRunningInstance()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("python3")).isEmpty()
            && QStandardPaths::findExecutable(QStringLiteral("socat")).isEmpty()) {
            QSKIP("Neither python3 nor socat is available");
        }
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        QLocalServer server;
        QVERIFY(server.listen(runtime.filePath(QStringLiteral("EShot.SingleInstance"))));
        const QString marker = runtime.filePath(QStringLiteral("started"));

        QProcess forwarder;
        startForwarder(forwarder, runtime, fallbackExecutable(runtime, marker));

        QByteArray received;
        QElapsedTimer timer;
        timer.start();
        QLocalSocket *socket = nullptr;
        while (timer.elapsed() < 10000 && !received.contains('\n')) {
            if (!socket && server.waitForNewConnection(50))
                socket = server.nextPendingConnection();
            if (socket) {
                socket->waitForReadyRead(50);
                received += socket->readAll();
            }
        }
        QVERIFY(forwarder.waitForFinished(10000));
        QCOMPARE(forwarder.exitCode(), 0);
        QCOMPARE(received, QByteArrayLiteral("capture\n"));
        QVERIFY(!QFileInfo::exists(marker));
    }

    void forwarderStartsEShotWhenNoInstanceListens()
    {
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        const QString marker = runtime.filePath(QStringLiteral("started"));

        QProcess forwarder;
        startForwarder(forwarder, runtime, fallbackExecutable(runtime, marker));
        QVERIFY(forwarder.waitForFinished(10000));
        QCOMPARE(forwarder.exitCode(), 0);
        QFile started(marker);
        QVERIFY(started.open(QIODevice::ReadOnly));
        QCOMPARE(started.readAll().trimmed(), QByteArrayLiteral("--capture"));
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

    void recognizesEmptyGSettingsArrays()
    {
        QVERIFY(LinuxGnomeShortcutInstaller::isEmptyBindingList(QStringLiteral("@as []")));
        QVERIFY(LinuxGnomeShortcutInstaller::isEmptyBindingList(QStringLiteral(" [] ")));
        QVERIFY(!LinuxGnomeShortcutInstaller::isEmptyBindingList(QStringLiteral("['Print']")));
        QVERIFY(!LinuxGnomeShortcutInstaller::isEmptyBindingList(QString()));
    }

    void removalResetsAnEmptyScreenshotShortcutWithoutStoredValue()
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
            "  'get org.gnome.shell.keybindings show-screenshot-ui') printf '@as []\\n' ;;\n"
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

        const auto result = LinuxGnomeShortcutInstaller::restoreDesktopScreenshotShortcut();
        QVERIFY2(result.success, qPrintable(result.error));

        QFile log(logPath);
        QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString calls = QString::fromUtf8(log.readAll());
        QVERIFY(calls.contains(QStringLiteral(
            "set org.gnome.settings-daemon.plugins.media-keys custom-keybindings []")));
        QVERIFY(calls.contains(QStringLiteral(
            "reset org.gnome.shell.keybindings show-screenshot-ui")));
        qputenv("PATH", previousPath);
        qunsetenv("ESHOT_GSETTINGS_LOG");
    }

    void removalLeavesGnomeAloneWhenEshotNeverChangedIt()
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
            "    printf \"['/other/']\\n\" ;;\n"
            "  'get org.gnome.shell.keybindings show-screenshot-ui') printf '@as []\\n' ;;\n"
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

        const auto result = LinuxGnomeShortcutInstaller::restoreDesktopScreenshotShortcut();
        QVERIFY2(result.success, qPrintable(result.error));

        QFile log(logPath);
        QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString calls = QString::fromUtf8(log.readAll());
        QVERIFY(!calls.contains(QStringLiteral("set ")));
        QVERIFY(!calls.contains(QStringLiteral("reset ")));
        qputenv("PATH", previousPath);
        qunsetenv("ESHOT_GSETTINGS_LOG");
    }

private:
    // Runs the command the way GNOME does: the shell-quoted command line is
    // split into arguments (sh follows the same quoting rules here).
    static void startForwarder(QProcess &process, const QTemporaryDir &runtime,
                               const QString &executable)
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime.path());
        process.setProcessEnvironment(environment);
        process.start(QStringLiteral("/bin/sh"),
                      {QStringLiteral("-c"),
                       LinuxGnomeShortcutInstaller::forwardingCaptureCommand(executable)});
        QVERIFY(process.waitForStarted(5000));
    }

    // Stand-in for EShot: records its arguments so a test can tell whether
    // the forwarder fell back to starting it.
    static QString fallbackExecutable(const QTemporaryDir &runtime, const QString &marker)
    {
        const QString path = runtime.filePath(QStringLiteral("fake eshot"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$*\" > '%1'\n").arg(marker).toUtf8());
        file.close();
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        return path;
    }
};

QTEST_GUILESS_MAIN(LinuxGnomeShortcutInstallerTests)
#include "LinuxGnomeShortcutInstallerTests.moc"
