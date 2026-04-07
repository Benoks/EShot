; ============================================================
;  EShot Setup Script — Inno Setup 6.x
;  Derlemek için: iscc EShot_Setup.iss
;  https://jrsoftware.org/isinfo.php
; ============================================================

#define MyAppName      "EShot"
#define MyAppVersion   "1.1.0"
#define MyAppPublisher "EShot"
#define MyAppURL       "https://github.com/your-repo/EShot"
#define MyAppExeName   "EShot.exe"
#define ReleaseDir     "EShot_Release"

[Setup]
AppId={{E5H0T-SCAP-2024-GUID-000000000001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

OutputDir=installer_output
OutputBaseFilename=EShot_Setup_v{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes

WizardStyle=modern
WizardResizable=no

MinVersion=10.0
ArchitecturesInstallIn64BitMode=x64compatible

UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}

LicenseFile=LICENSE

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";  Description: "Masaüstüne kısayol oluştur"; GroupDescription: "Ek simgeler:"
Name: "startupicon";  Description: "Başlangıçta otomatik çalıştır"; GroupDescription: "Başlangıç:"; Flags: unchecked

[Files]
; Ana uygulama
Source: "{#ReleaseDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt Runtime DLL'leri
Source: "{#ReleaseDir}\Qt6Core.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Gui.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Widgets.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Network.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Svg.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\D3Dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\opengl32sw.dll";     DestDir: "{app}"; Flags: ignoreversion

; Qt eklenti klasörleri
Source: "{#ReleaseDir}\platforms\*";          DestDir: "{app}\platforms";          Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\styles\*";             DestDir: "{app}\styles";             Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\imageformats\*";       DestDir: "{app}\imageformats";       Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\iconengines\*";        DestDir: "{app}\iconengines";        Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\generic\*";            DestDir: "{app}\generic";            Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\tls\*";                DestDir: "{app}\tls";                Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"; Comment: "Ekran goruntüsü aracı"
Name: "{group}\Kaldır {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";  Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "EShot'u simdi baslat"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: startupicon

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C taskkill /F /IM {#MyAppExeName}"; Flags: runhidden; RunOnceId: "KillEShot"
