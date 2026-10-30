#include <QtTest>

#include "core/LinuxUpdateScript.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

class LinuxUpdateScriptTests : public QObject {
    Q_OBJECT

private slots:
    void waitsStagesReplacesAndRestarts()
    {
        const QString script = buildLinuxUpdateScript(
            42,
            QStringLiteral("/home/user/EShot App/EShot.AppImage"),
            QStringLiteral("/tmp/EShot Update.AppImage"));

        QVERIFY(script.startsWith(QStringLiteral("#!/bin/sh\nset -eu\n")));
        QVERIFY(script.contains(QStringLiteral("attempts=0")));
        QVERIFY(script.contains(QStringLiteral("\"$attempts\" -lt 300")));
        QVERIFY(script.contains(QStringLiteral("while kill -0 \"$pid\"")));
        QVERIFY(script.contains(QStringLiteral("cp -f -- \"$download\" \"$staged\"")));
        QVERIFY(script.contains(QStringLiteral("chmod 0755 -- \"$staged\"")));
        QVERIFY(script.contains(QStringLiteral("mv -f -- \"$staged\" \"$current\"")));
        QVERIFY(script.contains(QStringLiteral("\"$current\" --silent")));
        QVERIFY(script.contains(QStringLiteral("rm -f -- \"$download\" \"$0\"")));
    }

    void quotesSingleQuotesInPaths()
    {
        const QString script = buildLinuxUpdateScript(
            7,
            QStringLiteral("/home/o'connor/EShot.AppImage"),
            QStringLiteral("/tmp/new'build.AppImage"));

        QVERIFY(script.contains(QStringLiteral("current='/home/o'\"'\"'connor/EShot.AppImage'")));
        QVERIFY(script.contains(QStringLiteral("download='/tmp/new'\"'\"'build.AppImage'")));
    }

    void rejectsInvalidArguments()
    {
        QVERIFY(buildLinuxUpdateScript(0, QStringLiteral("/a"), QStringLiteral("/b")).isEmpty());
        QVERIFY(buildLinuxUpdateScript(1, QString(), QStringLiteral("/b")).isEmpty());
        QVERIFY(buildLinuxUpdateScript(1, QStringLiteral("/a"), QString()).isEmpty());
    }

    void verifiesDigestBeforeReplacing()
    {
        const QString digest(64, QLatin1Char('a'));
        const QString script = buildLinuxUpdateScript(
            42, QStringLiteral("/a/EShot.AppImage"), QStringLiteral("/b/new.AppImage"),
            QStringLiteral("sha256:") + digest.toUpper());
        QVERIFY(script.contains(QStringLiteral("expected=") + digest));
        QVERIFY(script.indexOf(QStringLiteral("sha256sum")) < script.indexOf(QStringLiteral("mv -f")));
        QVERIFY(!buildLinuxUpdateScript(42, QStringLiteral("/a"), QStringLiteral("/b"),
                                        QStringLiteral("not-a-digest")).contains(QStringLiteral("sha256sum")));
    }

    void keepsCurrentAppImageWhenDigestDiffers()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString current = QDir(dir.path()).filePath(QStringLiteral("EShot.AppImage"));
        const QString download = QDir(dir.path()).filePath(QStringLiteral("new.AppImage"));
        const QString scriptPath = QDir(dir.path()).filePath(QStringLiteral("apply-update.sh"));
        QFile oldFile(current);
        QVERIFY(oldFile.open(QIODevice::WriteOnly));
        oldFile.write("#!/bin/sh\nexit 0\n");
        oldFile.close();
        QFile::setPermissions(current, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QFile newFile(download);
        QVERIFY(newFile.open(QIODevice::WriteOnly));
        newFile.write("#!/bin/sh\necho tampered\n");
        newFile.close();
        QFile scriptFile(scriptPath);
        QVERIFY(scriptFile.open(QIODevice::WriteOnly));
        scriptFile.write(buildLinuxUpdateScript(2147483647, current, download,
                                                QString(64, QLatin1Char('0'))).toUtf8());
        scriptFile.close();

        QProcess process;
        process.start(QStringLiteral("/bin/sh"), {scriptPath});
        QVERIFY(process.waitForFinished(5000));
        QVERIFY(process.exitCode() != 0);
        QFile installed(current);
        QVERIFY(installed.open(QIODevice::ReadOnly));
        QCOMPARE(installed.readAll(), QByteArray("#!/bin/sh\nexit 0\n"));
        QVERIFY(!QFileInfo::exists(current + QStringLiteral(".new")));
    }

    void replacesAndRestartsAppImage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString current = QDir(dir.path()).filePath(QStringLiteral("EShot AppImage"));
        const QString download = QDir(dir.path()).filePath(QStringLiteral("downloaded AppImage"));
        const QString marker = QDir(dir.path()).filePath(QStringLiteral("restart-args"));
        const QString scriptPath = QDir(dir.path()).filePath(QStringLiteral("apply-update.sh"));

        QFile oldFile(current);
        QVERIFY(oldFile.open(QIODevice::WriteOnly));
        QCOMPARE(oldFile.write("old\n"), 4);
        oldFile.close();

        QFile newFile(download);
        QVERIFY(newFile.open(QIODevice::WriteOnly));
        const QByteArray replacement = QByteArray("#!/bin/sh\nprintf '%s' \"$*\" > ")
            + QByteArray("'") + marker.toUtf8() + QByteArray("'\n");
        QCOMPARE(newFile.write(replacement), replacement.size());
        newFile.close();

        QFile scriptFile(scriptPath);
        QVERIFY(scriptFile.open(QIODevice::WriteOnly));
        const QByteArray script = buildLinuxUpdateScript(2147483647, current, download).toUtf8();
        QCOMPARE(scriptFile.write(script), script.size());
        scriptFile.close();

        QProcess process;
        process.start(QStringLiteral("/bin/sh"), {scriptPath});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(marker), 5000);

        QFile installed(current);
        QVERIFY(installed.open(QIODevice::ReadOnly));
        QCOMPARE(installed.readAll(), replacement);
        QFile args(marker);
        QVERIFY(args.open(QIODevice::ReadOnly));
        QCOMPARE(args.readAll(), QByteArray("--silent"));
        QVERIFY(!QFileInfo::exists(download));
        QVERIFY(!QFileInfo::exists(scriptPath));
    }
};

QTEST_APPLESS_MAIN(LinuxUpdateScriptTests)
#include "LinuxUpdateScriptTests.moc"
