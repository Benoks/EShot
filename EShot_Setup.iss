; ============================================================
;  EShot Setup Script — Inno Setup 6.x
;  To compile:  iscc EShot_Setup.iss
;  https://jrsoftware.org/isinfo.php
; ============================================================

#define MyAppName      "EShot"
#define MyAppVersion   "2.4.0"
#define MyAppPublisher "EShot"
#define MyAppURL       "https://github.com/Benoks/EShot"
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

MinVersion=10.0
ArchitecturesInstallIn64BitMode=x64compatible

UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}

LicenseFile=LICENSE

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
; Each key is defined per-language; Inno Setup picks the right one at runtime.
; Built-in keys "Additional icons:" and "Optional components:" are already
; translated in Default.isl and Turkish.isl, so they are not overridden here.

turkish.CreateDesktopShortcut=Masaüstüne kısayol oluştur
turkish.AutoStartWithWindows=Başlangıçta otomatik çalıştır
turkish.Startup=Başlangıç:
turkish.OptionalComponents=Opsiyonel bileşenler:
turkish.InstallTesseract=Tesseract OCR motorunu yükle (~170 MB)
turkish.AdditionalOcrLanguages=Ek OCR dil paketleri (eng+rus varsayılan)
turkish.OcrLangEnglish=İngilizce OCR dili
turkish.OcrLangTurkish=Türkçe OCR dili
turkish.OcrLangRussian=Rusça OCR dili
turkish.OcrLangGerman=Almanca OCR dili
turkish.OcrLangFrench=Fransızca OCR dili
turkish.OcrLangSpanish=İspanyolca OCR dili
turkish.OcrLangItalian=İtalyanca OCR dili
turkish.GroupComment=Ekran görüntüsü aracı
turkish.UninstallEntry=Kaldır {#MyAppName}

english.CreateDesktopShortcut=Create desktop shortcut
english.AutoStartWithWindows=Auto-start with Windows
english.Startup=Startup:
english.OptionalComponents=Optional components:
english.InstallTesseract=Install Tesseract OCR engine (~170 MB)
english.AdditionalOcrLanguages=Additional OCR languages (eng+rus default)
english.OcrLangEnglish=English OCR language
english.OcrLangTurkish=Turkish OCR language
english.OcrLangRussian=Russian OCR language
english.OcrLangGerman=German OCR language
english.OcrLangFrench=French OCR language
english.OcrLangSpanish=Spanish OCR language
english.OcrLangItalian=Italian OCR language
english.GroupComment=Screenshot tool
english.UninstallEntry=Uninstall {#MyAppName}

[Tasks]
Name: "desktopicon";  Description: "{cm:CreateDesktopShortcut}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startupicon";  Description: "{cm:AutoStartWithWindows}"; GroupDescription: "{cm:Startup}"; Flags: unchecked
Name: "ocrengine";    Description: "{cm:InstallTesseract}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_eng"; Description: "{cm:OcrLangEnglish}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_tur"; Description: "{cm:OcrLangTurkish}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_rus"; Description: "{cm:OcrLangRussian}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_deu"; Description: "{cm:OcrLangGerman}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_fra"; Description: "{cm:OcrLangFrench}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_spa"; Description: "{cm:OcrLangSpanish}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_ita"; Description: "{cm:OcrLangItalian}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked

[Files]
; Main application
Source: "{#ReleaseDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

; Qt runtime DLLs
Source: "{#ReleaseDir}\Qt6Core.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Gui.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Widgets.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Network.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleaseDir}\Qt6Svg.dll";         DestDir: "{app}"; Flags: ignoreversion

; MSVC and graphics runtime DLLs deployed by windeployqt / Visual Studio
Source: "{#ReleaseDir}\concrt140.dll";              DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#ReleaseDir}\msvcp140*.dll";              DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#ReleaseDir}\vccorlib140.dll";            DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#ReleaseDir}\vcruntime140*.dll";          DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#ReleaseDir}\D3Dcompiler_47.dll";         DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#ReleaseDir}\opengl32sw.dll";             DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Qt plugin folders
Source: "{#ReleaseDir}\platforms\*";          DestDir: "{app}\platforms";          Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\styles\*";             DestDir: "{app}\styles";             Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\imageformats\*";       DestDir: "{app}\imageformats";       Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\iconengines\*";        DestDir: "{app}\iconengines";        Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\generic\*";            DestDir: "{app}\generic";            Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\tls\*";                DestDir: "{app}\tls";                Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReleaseDir}\translations\*";       DestDir: "{app}\translations";       Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Tesseract OCR engine (only when ocrengine task is selected)
Source: "{#ReleaseDir}\tesseract\tesseract.exe";       DestDir: "{app}\tesseract";       Flags: ignoreversion; Tasks: ocrengine
Source: "{#ReleaseDir}\tesseract\winpath.exe";         DestDir: "{app}\tesseract";       Flags: ignoreversion; Tasks: ocrengine
Source: "{#ReleaseDir}\tesseract\*.dll";               DestDir: "{app}\tesseract";       Flags: ignoreversion; Tasks: ocrengine
Source: "{#ReleaseDir}\tesseract\tessdata\eng.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_eng
Source: "{#ReleaseDir}\tesseract\tessdata\tur.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_tur
Source: "{#ReleaseDir}\tesseract\tessdata\rus.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_rus
Source: "{#ReleaseDir}\tesseract\tessdata\deu.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_deu
Source: "{#ReleaseDir}\tesseract\tessdata\fra.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_fra
Source: "{#ReleaseDir}\tesseract\tessdata\spa.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_spa
Source: "{#ReleaseDir}\tesseract\tessdata\ita.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_ita

[Icons]
Name: "{group}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"; Comment: "{cm:GroupComment}"
Name: "{group}\{cm:UninstallEntry}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";  Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; Filename: "{app}\{#MyAppExeName}"; Description: "Launch EShot now"; Flags: nowait postinstall skipifsilent
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command ""Unregister-ScheduledTask -TaskName '{#MyAppName}' -Confirm:$false -ErrorAction SilentlyContinue"""; Flags: runhidden
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command ""$A=New-ScheduledTaskAction -Execute '{app}\{#MyAppExeName}' -Argument '--silent'; $T=New-ScheduledTaskTrigger -AtLogOn; Register-ScheduledTask -TaskName '{#MyAppName}' -Action $A -Trigger $T -RunLevel Highest -Force | Out-Null"""; Flags: runhidden; Tasks: startupicon

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C taskkill /F /IM {#MyAppExeName}"; Flags: runhidden; RunOnceId: "KillEShot"
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command ""Unregister-ScheduledTask -TaskName '{#MyAppName}' -Confirm:$false -ErrorAction SilentlyContinue"""; Flags: runhidden; RunOnceId: "DeleteEShotStartupTask"

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  // Close EShot if it is running
  Exec(ExpandConstant('{cmd}'), '/C taskkill /F /IM {#MyAppExeName}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  // Brief wait so the file lock is released
  Sleep(500);
end;
