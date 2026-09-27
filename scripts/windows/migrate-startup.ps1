param(
    [Parameter(Mandatory = $true)]
    [string]$Exe,
    [switch]$RemoveTask
)

# Older EShot installers started the app with a high-privilege scheduled task.
# Only migrate a task belonging to this account; an installer elevated with a
# different administrator account must not write into that account's profile.
$task = Get-ScheduledTask -TaskName 'EShot' -ErrorAction SilentlyContinue
if (-not $task) { exit 0 }

$identity = [System.Security.Principal.WindowsIdentity]::GetCurrent()

# Task Scheduler may report the principal as "user", "MACHINE\user" or a SID.
function Test-CurrentUser([string]$userId) {
    if (-not $userId) { return $false }
    if ($userId -ieq $identity.Name -or $userId -ieq $identity.User.Value) { return $true }
    try {
        $account = New-Object System.Security.Principal.NTAccount($userId)
        $sid = $account.Translate([System.Security.Principal.SecurityIdentifier]).Value
        return $sid -eq $identity.User.Value
    } catch {
        return $false
    }
}

$action = $task.Actions | Select-Object -First 1
if (-not $action -or -not $action.Execute) { exit 0 }
if (-not (Test-CurrentUser $task.Principal.UserId) -or
    [System.IO.Path]::GetFileName($action.Execute.Trim('"')) -ine 'EShot.exe') {
    exit 0
}

$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$command = '"' + $Exe + '" --silent'
try {
    New-Item -Path $runKey -Force -ErrorAction Stop | Out-Null
    New-ItemProperty -Path $runKey -Name 'EShot' -PropertyType String -Value $command -Force -ErrorAction Stop | Out-Null
    if ($RemoveTask) {
        Unregister-ScheduledTask -TaskName 'EShot' -Confirm:$false -ErrorAction Stop
    }
} catch {
    exit 1
}
