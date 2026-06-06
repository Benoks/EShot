; ============================================================
;  EShot Setup Script — Inno Setup 6.x
;  To compile:  iscc EShot_Setup.iss
;  https://jrsoftware.org/isinfo.php
; ============================================================

#define MyAppName      "EShot"
#define MyAppVersion   "3.0.0"
#define MyAppPublisher "EShot"
#define MyAppURL       "https://github.com/Benoks/EShot"
#define MyAppExeName   "EShot.exe"
#ifndef ReleaseDir
#define ReleaseDir     "EShot_Release"
#endif

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
ExistingInstallWelcome=Mevcut kurulum bulundu. Kurulum EShot'i mevcut klasorde guncelleyecek; ayarlariniz korunacak.
OcrLangPortuguese=Portuguese OCR language
OcrLangPolish=Polish OCR language
OcrLangDutch=Dutch OCR language
OcrLangJapanese=Japanese OCR language
OcrLangKorean=Korean OCR language
OcrLangChinese=Simplified Chinese OCR language
; Each key is defined per-language; Inno Setup picks the right one at runtime.
; Built-in keys "Additional icons:" and "Optional components:" are already
; translated in Default.isl and Turkish.isl, so they are not overridden here.

turkish.CreateDesktopShortcut=Masaüstüne kısayol oluştur
turkish.AutoStartWithWindows=Başlangıçta otomatik çalıştır
turkish.Startup=Başlangıç:
turkish.OptionalComponents=Opsiyonel bileşenler:
turkish.InstallTesseract=Tesseract OCR bileşenleri
turkish.InstallFfmpeg=FFmpeg video kaydı bileşenleri
turkish.AdditionalOcrLanguages=Ek OCR dil paketleri
turkish.OcrLangEnglish=İngilizce OCR dili
turkish.OcrLangTurkish=Türkçe OCR dili
turkish.OcrLangRussian=Rusça OCR dili
turkish.OcrLangGerman=Almanca OCR dili
turkish.OcrLangFrench=Fransızca OCR dili
turkish.OcrLangSpanish=İspanyolca OCR dili
turkish.OcrLangItalian=İtalyanca OCR dili
turkish.GroupComment=Ekran görüntüsü aracı
turkish.UninstallEntry=Kaldır {#MyAppName}
turkish.LaunchAfterInstall={#MyAppName}'ı başlat
turkish.AlreadyInstalled=zaten yüklü

english.CreateDesktopShortcut=Create desktop shortcut
english.AutoStartWithWindows=Auto-start with Windows
english.Startup=Startup:
english.OptionalComponents=Optional components:
english.InstallTesseract=Tesseract OCR components
english.InstallFfmpeg=FFmpeg video recording components
english.AdditionalOcrLanguages=Additional OCR languages
english.OcrLangEnglish=English OCR language
english.OcrLangTurkish=Turkish OCR language
english.OcrLangRussian=Russian OCR language
english.OcrLangGerman=German OCR language
english.OcrLangFrench=French OCR language
english.OcrLangSpanish=Spanish OCR language
english.OcrLangItalian=Italian OCR language
english.OcrLangPortuguese=Portuguese OCR language
english.OcrLangPolish=Polish OCR language
english.OcrLangDutch=Dutch OCR language
english.OcrLangJapanese=Japanese OCR language
english.OcrLangKorean=Korean OCR language
english.OcrLangChinese=Simplified Chinese OCR language
english.GroupComment=Screenshot tool
english.UninstallEntry=Uninstall {#MyAppName}
english.LaunchAfterInstall=Launch {#MyAppName}
english.AlreadyInstalled=already installed

[Tasks]
Name: "desktopicon";  Description: "{cm:CreateDesktopShortcut}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startupicon";  Description: "{cm:AutoStartWithWindows}"; GroupDescription: "{cm:Startup}"; Flags: unchecked
Name: "ffmpeg";       Description: "{code:ComponentDescription|ffmpeg}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine";    Description: "{code:ComponentDescription|ocrengine}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_eng"; Description: "{code:ComponentDescription|lang_eng}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_tur"; Description: "{code:ComponentDescription|lang_tur}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_rus"; Description: "{code:ComponentDescription|lang_rus}"; GroupDescription: "{cm:OptionalComponents}"
Name: "ocrengine\lang_deu"; Description: "{code:ComponentDescription|lang_deu}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_fra"; Description: "{code:ComponentDescription|lang_fra}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_spa"; Description: "{code:ComponentDescription|lang_spa}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_ita"; Description: "{code:ComponentDescription|lang_ita}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_por"; Description: "{code:ComponentDescription|lang_por}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_pol"; Description: "{code:ComponentDescription|lang_pol}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_nld"; Description: "{code:ComponentDescription|lang_nld}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_jpn"; Description: "{code:ComponentDescription|lang_jpn}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_kor"; Description: "{code:ComponentDescription|lang_kor}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked
Name: "ocrengine\lang_chi_sim"; Description: "{code:ComponentDescription|lang_chi_sim}"; GroupDescription: "{cm:OptionalComponents}"; Flags: unchecked

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
Source: "{#ReleaseDir}\ffmpeg\*";             DestDir: "{app}\ffmpeg";             Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Tasks: ffmpeg; Check: ShouldInstallFfmpeg

; Tesseract OCR engine (only when ocrengine task is selected)
Source: "{#ReleaseDir}\tesseract\tesseract.exe";       DestDir: "{app}\tesseract";       Flags: ignoreversion; Tasks: ocrengine; Check: ShouldInstallOcrEngine
Source: "{#ReleaseDir}\tesseract\winpath.exe";         DestDir: "{app}\tesseract";       Flags: ignoreversion skipifsourcedoesntexist; Tasks: ocrengine; Check: ShouldInstallOcrEngine
Source: "{#ReleaseDir}\tesseract\*.dll";               DestDir: "{app}\tesseract";       Flags: ignoreversion; Tasks: ocrengine; Check: ShouldInstallOcrEngine
Source: "{#ReleaseDir}\tesseract\tessdata\eng.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_eng; Check: ShouldInstallOcrLangEng
Source: "{#ReleaseDir}\tesseract\tessdata\tur.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_tur; Check: ShouldInstallOcrLangTur
Source: "{#ReleaseDir}\tesseract\tessdata\rus.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_rus; Check: ShouldInstallOcrLangRus
Source: "{#ReleaseDir}\tesseract\tessdata\deu.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_deu; Check: ShouldInstallOcrLangDeu
Source: "{#ReleaseDir}\tesseract\tessdata\fra.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_fra; Check: ShouldInstallOcrLangFra
Source: "{#ReleaseDir}\tesseract\tessdata\spa.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_spa; Check: ShouldInstallOcrLangSpa
Source: "{#ReleaseDir}\tesseract\tessdata\ita.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion; Tasks: ocrengine\lang_ita; Check: ShouldInstallOcrLangIta
Source: "{#ReleaseDir}\tesseract\tessdata\por.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion skipifsourcedoesntexist; Tasks: ocrengine\lang_por; Check: ShouldInstallOcrLangPor
Source: "{#ReleaseDir}\tesseract\tessdata\pol.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion skipifsourcedoesntexist; Tasks: ocrengine\lang_pol; Check: ShouldInstallOcrLangPol
Source: "{#ReleaseDir}\tesseract\tessdata\nld.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion skipifsourcedoesntexist; Tasks: ocrengine\lang_nld; Check: ShouldInstallOcrLangNld
Source: "{#ReleaseDir}\tesseract\tessdata\jpn.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion skipifsourcedoesntexist; Tasks: ocrengine\lang_jpn; Check: ShouldInstallOcrLangJpn
Source: "{#ReleaseDir}\tesseract\tessdata\kor.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion skipifsourcedoesntexist; Tasks: ocrengine\lang_kor; Check: ShouldInstallOcrLangKor
Source: "{#ReleaseDir}\tesseract\tessdata\chi_sim.traineddata"; DestDir: "{app}\tesseract\tessdata"; Flags: ignoreversion skipifsourcedoesntexist; Tasks: ocrengine\lang_chi_sim; Check: ShouldInstallOcrLangChiSim

[Icons]
Name: "{group}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"; Comment: "{cm:GroupComment}"
Name: "{group}\{cm:UninstallEntry}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";  Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command ""Unregister-ScheduledTask -TaskName '{#MyAppName}' -Confirm:$false -ErrorAction SilentlyContinue"""; Flags: runhidden; Tasks: startupicon
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command ""$User=[System.Security.Principal.WindowsIdentity]::GetCurrent().Name; $A=New-ScheduledTaskAction -Execute '{app}\{#MyAppExeName}' -Argument '--silent'; $T=New-ScheduledTaskTrigger -AtLogOn -User $User; $T.Delay='PT30S'; $P=New-ScheduledTaskPrincipal -UserId $User -LogonType Interactive -RunLevel Highest; $S=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries; Register-ScheduledTask -TaskName '{#MyAppName}' -Action $A -Trigger $T -Principal $P -Settings $S -Force | Out-Null"""; Flags: runhidden; Tasks: startupicon
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchAfterInstall}"; Flags: shellexec nowait postinstall skipifsilent

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C taskkill /F /IM {#MyAppExeName}"; Flags: runhidden; RunOnceId: "KillEShot"
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command ""Unregister-ScheduledTask -TaskName '{#MyAppName}' -Confirm:$false -ErrorAction SilentlyContinue"""; Flags: runhidden; RunOnceId: "DeleteEShotStartupTask"

[Code]
var
  GOcrPreinstalled: Boolean;
  GInstallStateFrozen: Boolean;

function IsInstalledFile(RelativePath: String): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\' + RelativePath));
end;

function ExistingInstallFound: Boolean;
var
  InstallDir: String;
begin
  Result := False;

  if RegQueryStringValue(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{E5H0T-SCAP-2024-GUID-000000000001}_is1', 'InstallLocation', InstallDir) then
    Result := FileExists(AddBackslash(InstallDir) + '{#MyAppExeName}');

  if (not Result) and RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{E5H0T-SCAP-2024-GUID-000000000001}_is1', 'InstallLocation', InstallDir) then
    Result := FileExists(AddBackslash(InstallDir) + '{#MyAppExeName}');

  if not Result then
    Result := FileExists(ExpandConstant('{autopf}\{#MyAppName}\{#MyAppExeName}'));
end;

procedure InitializeWizard;
begin
  if ExistingInstallFound then
    WizardForm.WelcomeLabel2.Caption := ExpandConstant('{cm:ExistingInstallWelcome}');
end;

function ShouldInstallOcrEngine: Boolean;
begin
  // Once installation begins, use the state frozen before any files were copied.
  // Otherwise installing tesseract.exe (an earlier [Files] entry) makes this
  // return False and the following tesseract\*.dll entry gets skipped.
  if GInstallStateFrozen then
    Result := not GOcrPreinstalled
  else
    Result := not IsInstalledFile('tesseract\tesseract.exe');
end;

function ShouldInstallFfmpeg: Boolean;
begin
  Result := not IsInstalledFile('ffmpeg\ffmpeg.exe');
end;

function ShouldInstallOcrLangEng: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\eng.traineddata');
end;

function ShouldInstallOcrLangTur: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\tur.traineddata');
end;

function ShouldInstallOcrLangRus: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\rus.traineddata');
end;

function ShouldInstallOcrLangDeu: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\deu.traineddata');
end;

function ShouldInstallOcrLangFra: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\fra.traineddata');
end;

function ShouldInstallOcrLangSpa: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\spa.traineddata');
end;

function ShouldInstallOcrLangIta: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\ita.traineddata');
end;

function ShouldInstallOcrLangPor: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\por.traineddata');
end;

function ShouldInstallOcrLangPol: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\pol.traineddata');
end;

function ShouldInstallOcrLangNld: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\nld.traineddata');
end;

function ShouldInstallOcrLangJpn: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\jpn.traineddata');
end;

function ShouldInstallOcrLangKor: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\kor.traineddata');
end;

function ShouldInstallOcrLangChiSim: Boolean;
begin
  Result := not IsInstalledFile('tesseract\tessdata\chi_sim.traineddata');
end;

function InstalledSuffix(Installed: Boolean): String;
begin
  if Installed then
    Result := ' (' + ExpandConstant('{cm:AlreadyInstalled}') + ')'
  else
    Result := '';
end;

function ComponentDescription(Param: String): String;
begin
  if Param = 'ffmpeg' then Result := ExpandConstant('{cm:InstallFfmpeg}') + InstalledSuffix(not ShouldInstallFfmpeg)
  else if Param = 'ocrengine' then Result := ExpandConstant('{cm:InstallTesseract}') + InstalledSuffix(not ShouldInstallOcrEngine)
  else if Param = 'lang_eng' then Result := ExpandConstant('{cm:OcrLangEnglish}') + InstalledSuffix(not ShouldInstallOcrLangEng)
  else if Param = 'lang_tur' then Result := ExpandConstant('{cm:OcrLangTurkish}') + InstalledSuffix(not ShouldInstallOcrLangTur)
  else if Param = 'lang_rus' then Result := ExpandConstant('{cm:OcrLangRussian}') + InstalledSuffix(not ShouldInstallOcrLangRus)
  else if Param = 'lang_deu' then Result := ExpandConstant('{cm:OcrLangGerman}') + InstalledSuffix(not ShouldInstallOcrLangDeu)
  else if Param = 'lang_fra' then Result := ExpandConstant('{cm:OcrLangFrench}') + InstalledSuffix(not ShouldInstallOcrLangFra)
  else if Param = 'lang_spa' then Result := ExpandConstant('{cm:OcrLangSpanish}') + InstalledSuffix(not ShouldInstallOcrLangSpa)
  else if Param = 'lang_ita' then Result := ExpandConstant('{cm:OcrLangItalian}') + InstalledSuffix(not ShouldInstallOcrLangIta)
  else if Param = 'lang_por' then Result := ExpandConstant('{cm:OcrLangPortuguese}') + InstalledSuffix(not ShouldInstallOcrLangPor)
  else if Param = 'lang_pol' then Result := ExpandConstant('{cm:OcrLangPolish}') + InstalledSuffix(not ShouldInstallOcrLangPol)
  else if Param = 'lang_nld' then Result := ExpandConstant('{cm:OcrLangDutch}') + InstalledSuffix(not ShouldInstallOcrLangNld)
  else if Param = 'lang_jpn' then Result := ExpandConstant('{cm:OcrLangJapanese}') + InstalledSuffix(not ShouldInstallOcrLangJpn)
  else if Param = 'lang_kor' then Result := ExpandConstant('{cm:OcrLangKorean}') + InstalledSuffix(not ShouldInstallOcrLangKor)
  else if Param = 'lang_chi_sim' then Result := ExpandConstant('{cm:OcrLangChinese}') + InstalledSuffix(not ShouldInstallOcrLangChiSim)
  else Result := Param;
end;

procedure CurPageChanged(CurPageID: Integer);
var
  I: Integer;
  InstalledText: String;
begin
  if CurPageID <> wpSelectTasks then
    Exit;

  InstalledText := ExpandConstant('{cm:AlreadyInstalled}');
  for I := 0 to WizardForm.TasksList.Items.Count - 1 do begin
    if Pos(InstalledText, WizardForm.TasksList.ItemCaption[I]) > 0 then begin
      WizardForm.TasksList.Checked[I] := True;
      WizardForm.TasksList.ItemEnabled[I] := False;
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  // Freeze the "already installed" state before any files are copied
  // (see ShouldInstallOcrEngine).
  GOcrPreinstalled := IsInstalledFile('tesseract\tesseract.exe');
  GInstallStateFrozen := True;
  // Close EShot if it is running
  Exec(ExpandConstant('{cmd}'), '/C taskkill /F /IM {#MyAppExeName}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  // Brief wait so the file lock is released
  Sleep(500);
end;
