#include "LinuxGnomeShortcutInstaller.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr auto MediaKeysSchema = "org.gnome.settings-daemon.plugins.media-keys";
constexpr auto CustomSchema = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding";
constexpr auto CustomPath = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/eshot/";

QString shellQuote(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(value);
}

bool runGSettings(const QStringList &arguments, QString *output, QString *error)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), arguments);
    if (!process.waitForStarted(3000) || !process.waitForFinished(5000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            QString detail = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            if (detail.isEmpty())
                detail = process.errorString();
            *error = detail;
        }
        return false;
    }
    if (output)
        *output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    return true;
}

bool schemaHasKey(const QString &schema, const QString &key)
{
    QString output;
    return runGSettings({QStringLiteral("list-keys"), schema}, &output, nullptr)
        && output.split(QLatin1Char('\n'), Qt::SkipEmptyParts).contains(key);
}

QStringList parseStringArray(const QString &value)
{
    QStringList result;
    static const QRegularExpression quoted(QStringLiteral("'([^']*)'"));
    auto match = quoted.globalMatch(value);
    while (match.hasNext())
        result.append(match.next().captured(1));
    return result;
}

QString formatStringArray(const QStringList &values)
{
    QStringList quoted;
    for (QString value : values) {
        value.replace(QLatin1Char('\''), QStringLiteral("\\'"));
        quoted.append(QStringLiteral("'%1'").arg(value));
    }
    return QStringLiteral("[%1]").arg(quoted.join(QStringLiteral(", ")));
}

bool setValue(const QString &schema, const QString &key,
              const QString &value, QString *error)
{
    return runGSettings({QStringLiteral("set"), schema, key, value}, nullptr, error);
}

bool resetValue(const QString &schema, const QString &key, QString *error)
{
    return runGSettings({QStringLiteral("reset"), schema, key}, nullptr, error);
}

}

namespace LinuxGnomeShortcutInstaller {

QString captureCommand(const QString &executablePath)
{
    const QString path = executablePath.trimmed();
    return path.isEmpty() ? QString() : shellQuote(path) + QStringLiteral(" --capture");
}

QString gsettingsStringValue(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString acceleratorFromPortableSequence(const QString &portableSequence)
{
    const QStringList parts = portableSequence.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};

    QString accelerator;
    for (int i = 0; i + 1 < parts.size(); ++i) {
        const QString modifier = parts.at(i).trimmed();
        if (modifier.compare(QStringLiteral("Ctrl"), Qt::CaseInsensitive) == 0)
            accelerator += QStringLiteral("<Primary>");
        else if (modifier.compare(QStringLiteral("Alt"), Qt::CaseInsensitive) == 0)
            accelerator += QStringLiteral("<Alt>");
        else if (modifier.compare(QStringLiteral("Shift"), Qt::CaseInsensitive) == 0)
            accelerator += QStringLiteral("<Shift>");
        else if (modifier.compare(QStringLiteral("Meta"), Qt::CaseInsensitive) == 0)
            accelerator += QStringLiteral("<Super>");
        else
            return {};
    }

    QString key = parts.constLast().trimmed();
    if (key.size() == 1)
        key = key.toLower();
    return key.isEmpty() ? QString() : accelerator + key;
}

QString preferredExecutable(const QString &appImagePath,
                            const QString &applicationFilePath,
                            const QString &integratedAppImagePath)
{
    if (!appImagePath.trimmed().isEmpty() && !integratedAppImagePath.trimmed().isEmpty())
        return integratedAppImagePath.trimmed();
    if (!appImagePath.trimmed().isEmpty())
        return appImagePath.trimmed();
    return applicationFilePath.trimmed();
}

Result installCaptureShortcut(const QString &command, const QString &binding)
{
    Result result;
    if (command.trimmed().isEmpty()) {
        result.error = QStringLiteral("The EShot capture command is empty.");
        return result;
    }
    if (binding.trimmed().isEmpty()) {
        result.error = QStringLiteral("The GNOME shortcut is empty or unsupported.");
        return result;
    }
    if (QStandardPaths::findExecutable(QStringLiteral("gsettings")).isEmpty()) {
        result.error = QStringLiteral("gsettings is not installed.");
        return result;
    }

    QString error;
    QString pathsValue;
    if (!runGSettings({QStringLiteral("get"), QString::fromLatin1(MediaKeysSchema),
                       QStringLiteral("custom-keybindings")}, &pathsValue, &error)) {
        result.error = error;
        return result;
    }

    QStringList paths = parseStringArray(pathsValue);
    // Re-read the array immediately before the write to shrink the
    // read-modify-write race with other tools editing the same key.
    QString rereadValue;
    if (runGSettings({QStringLiteral("get"), QString::fromLatin1(MediaKeysSchema),
                      QStringLiteral("custom-keybindings")}, &rereadValue, &error))
        paths = parseStringArray(rereadValue);
    if (!paths.contains(QString::fromLatin1(CustomPath))) {
        paths.append(QString::fromLatin1(CustomPath));
        if (!setValue(QString::fromLatin1(MediaKeysSchema),
                      QStringLiteral("custom-keybindings"),
                      formatStringArray(paths), &error)) {
            result.error = error;
            return result;
        }
    }

    const QString custom = QStringLiteral("%1:%2")
        .arg(QString::fromLatin1(CustomSchema), QString::fromLatin1(CustomPath));
    if (!setValue(custom, QStringLiteral("name"),
                  gsettingsStringValue(QStringLiteral("EShot Capture")), &error)
        || !setValue(custom, QStringLiteral("command"),
                     gsettingsStringValue(command), &error)
        || !setValue(custom, QStringLiteral("binding"),
                     gsettingsStringValue(binding), &error)) {
        result.error = error;
        return result;
    }

    // GNOME Shell owns plain Print by default. Store its current assignment
    // before clearing only that one built-in action, so a future uninstaller
    // can restore it without touching unrelated screenshot shortcuts.
    const QString shellSchema = QStringLiteral("org.gnome.shell.keybindings");
    const QString shellKey = QStringLiteral("show-screenshot-ui");
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    const QString previousKey = QStringLiteral("linux/gnomePreviousScreenshotBinding");
    if (binding.compare(QStringLiteral("Print"), Qt::CaseInsensitive) == 0
        && schemaHasKey(shellSchema, shellKey)) {
        QString previous;
        if (runGSettings({QStringLiteral("get"), shellSchema, shellKey}, &previous, nullptr)
            && previous.contains(QStringLiteral("Print"))) {
            if (!settings.contains(previousKey))
                settings.setValue(previousKey, previous);
            if (!setValue(shellSchema, shellKey, QStringLiteral("[]"), &error)) {
                result.error = error;
                return result;
            }
        }
    } else if (settings.contains(previousKey) && schemaHasKey(shellSchema, shellKey)) {
        const QString previous = settings.value(previousKey).toString();
        if (!previous.isEmpty() && !setValue(shellSchema, shellKey, previous, &error)) {
            result.error = error;
            return result;
        }
        settings.remove(previousKey);
    }

    result.success = true;
    return result;
}

Result installPrintScreen(const QString &command)
{
    return installCaptureShortcut(command, QStringLiteral("Print"));
}

Result uninstallCaptureShortcut(bool restoreBuiltInPrintScreen)
{
    Result result;
    if (QStandardPaths::findExecutable(QStringLiteral("gsettings")).isEmpty()) {
        result.error = QStringLiteral("gsettings is not installed.");
        return result;
    }

    QString error;
    QString pathsValue;
    if (!runGSettings({QStringLiteral("get"), QString::fromLatin1(MediaKeysSchema),
                       QStringLiteral("custom-keybindings")}, &pathsValue, &error)) {
        result.error = error;
        return result;
    }

    QStringList paths = parseStringArray(pathsValue);
    // Re-read the array immediately before the write to shrink the
    // read-modify-write race with other tools editing the same key.
    QString rereadValue;
    if (runGSettings({QStringLiteral("get"), QString::fromLatin1(MediaKeysSchema),
                      QStringLiteral("custom-keybindings")}, &rereadValue, &error))
        paths = parseStringArray(rereadValue);
    if (paths.removeAll(QString::fromLatin1(CustomPath)) > 0
        && !setValue(QString::fromLatin1(MediaKeysSchema),
                     QStringLiteral("custom-keybindings"),
                     formatStringArray(paths), &error)) {
        result.error = error;
        return result;
    }

    const QString custom = QStringLiteral("%1:%2")
        .arg(QString::fromLatin1(CustomSchema), QString::fromLatin1(CustomPath));
    for (const QString &key : {QStringLiteral("binding"), QStringLiteral("command"),
                               QStringLiteral("name")}) {
        if (!resetValue(custom, key, &error)) {
            result.error = error;
            return result;
        }
    }

    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    const QString previousKey = QStringLiteral("linux/gnomePreviousScreenshotBinding");
    if (restoreBuiltInPrintScreen && settings.contains(previousKey)) {
        const QString shellSchema = QStringLiteral("org.gnome.shell.keybindings");
        const QString shellKey = QStringLiteral("show-screenshot-ui");
        const QString previous = settings.value(previousKey).toString();
        if (schemaHasKey(shellSchema, shellKey) && !previous.isEmpty()
            && !setValue(shellSchema, shellKey, previous, &error)) {
            result.error = error;
            return result;
        }
        settings.remove(previousKey);
    }

    result.success = true;
    return result;
}

}
