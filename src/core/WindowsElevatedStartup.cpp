#include "WindowsElevatedStartup.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace WindowsElevatedStartup {
namespace {
constexpr const char *TaskName = "EShot Elevated";

#ifdef Q_OS_WIN
QString psQuote(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

// Runs a PowerShell script as administrator and waits for it to finish.
bool runElevatedScript(const QString &script)
{
    const QString parameters = QStringLiteral(
        "-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand %1")
        .arg(encodedPowerShellCommand(script));
    const std::wstring params = parameters.toStdWString();
    // Full path: an elevated start must not resolve powershell.exe through a
    // user-writable PATH entry.
    wchar_t systemDir[MAX_PATH] = {};
    const UINT systemDirLength = GetSystemDirectoryW(systemDir, MAX_PATH);
    if (systemDirLength == 0 || systemDirLength >= MAX_PATH)
        return false;
    const std::wstring powershell = std::wstring(systemDir)
        + L"\\WindowsPowerShell\\v1.0\\powershell.exe";

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = powershell.c_str();
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    bool ok = false;
    // Fails with ERROR_CANCELLED when the UAC prompt is declined.
    if (ShellExecuteExW(&info) && info.hProcess) {
        if (WaitForSingleObject(info.hProcess, 60000) == WAIT_OBJECT_0) {
            DWORD exitCode = 1;
            ok = GetExitCodeProcess(info.hProcess, &exitCode) && exitCode == 0;
        }
        CloseHandle(info.hProcess);
    }
    return ok;
}
#endif
}

QString encodedPowerShellCommand(const QString &script)
{
    QByteArray utf16le;
    utf16le.reserve(script.size() * 2);
    for (const QChar ch : script) {
        const char16_t unit = ch.unicode();
        utf16le.append(char(unit & 0xff));
        utf16le.append(char((unit >> 8) & 0xff));
    }
    return QString::fromLatin1(utf16le.toBase64());
}

bool installLocationIsAdminProtected(const QString &appDirectory,
                                     const QStringList &protectedRoots)
{
    const auto normalized = [](QString path) {
        path = QDir::cleanPath(path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/')));
        while (path.endsWith(QLatin1Char('/')) && path.size() > 1)
            path.chop(1);
        return path;
    };
    const QString app = normalized(appDirectory);
    if (app.isEmpty())
        return false;
    for (const QString &root : protectedRoots) {
        const QString base = normalized(root);
        if (base.isEmpty())
            continue;
        if (app.compare(base, Qt::CaseInsensitive) == 0
            || app.startsWith(base + QLatin1Char('/'), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool currentInstallIsAdminProtected()
{
#ifdef Q_OS_WIN
    return installLocationIsAdminProtected(
        QCoreApplication::applicationDirPath(),
        {qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramW6432"),
         qEnvironmentVariable("ProgramFiles(x86)")});
#else
    return false;
#endif
}

bool isSupported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool isProcessElevated()
{
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation = {};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation,
                                              sizeof(elevation), &size)
        && elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return elevated;
#else
    return false;
#endif
}

bool configure(bool enable, bool startAtLogon)
{
#ifdef Q_OS_WIN
    QString script = QStringLiteral("$ErrorActionPreference = 'Stop'\r\n");
    if (!enable) {
        script += QStringLiteral("Unregister-ScheduledTask -TaskName %1 -Confirm:$false "
                                 "-ErrorAction SilentlyContinue\r\n").arg(psQuote(QLatin1String(TaskName)));
        return runElevatedScript(script);
    }

    // The task runs for the signed-in user; the UAC prompt only authorises
    // creating it. Read the user here, before elevation can change it.
    const QString user = qEnvironmentVariable("USERDOMAIN").isEmpty()
        ? qEnvironmentVariable("USERNAME")
        : qEnvironmentVariable("USERDOMAIN") + QLatin1Char('\\') + qEnvironmentVariable("USERNAME");
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    script += QStringLiteral("$user = %1\r\n").arg(psQuote(user));
    script += QStringLiteral("$action = New-ScheduledTaskAction -Execute %1 -Argument %2\r\n")
                  .arg(psQuote(exe),
                       psQuote(QStringLiteral("--silent ") + QLatin1String(TaskArgument)));
    script += QStringLiteral("$principal = New-ScheduledTaskPrincipal -UserId $user "
                             "-LogonType Interactive -RunLevel Highest\r\n");
    // No execution time limit: the default of three days would stop EShot.
    script += QStringLiteral("$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries "
                             "-DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero)\r\n");
    script += QStringLiteral("$task = @{ TaskName = %1; Action = $action; Principal = $principal; "
                             "Settings = $settings; Force = $true }\r\n")
                  .arg(psQuote(QLatin1String(TaskName)));
    if (startAtLogon)
        script += QStringLiteral("$task.Trigger = New-ScheduledTaskTrigger -AtLogOn -User $user\r\n");
    script += QStringLiteral("Register-ScheduledTask @task | Out-Null\r\n");
    return runElevatedScript(script);
#else
    Q_UNUSED(enable);
    Q_UNUSED(startAtLogon);
    return false;
#endif
}

bool launchElevated()
{
#ifdef Q_OS_WIN
    return QProcess::execute(QStringLiteral("schtasks"),
                             {QStringLiteral("/Run"), QStringLiteral("/TN"),
                              QLatin1String(TaskName)}) == 0;
#else
    return false;
#endif
}

}
