; Script generated by the Inno Script Studio Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "OpenKJ"
#define MyAppVersion "1.7.121"
#define MyAppPublisher "OpenKJ Project"
#define MyAppURL "https://openkj.org/"
#define MyAppExeName "OpenKJ.exe"
#define MyAppId "{474EEC43-B55A-4FCE-8E5A-4ACD90E56103}"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
LicenseFile=output\LICENSE.txt
OutputBaseFilename=OpenKJ
Compression=lzma
SolidCompression=yes
UninstallDisplayName=OpenKJ
UninstallDisplayIcon={uninstallexe}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Files]
Source: "output\OpenKJ.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "output\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Flags: nowait postinstall skipifsilent; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; Description: "MS Visual C++ Redistributable"; StatusMsg: "Installing vcredist"

[Code]
function GetUninstallString(): String;
var
  sUnInstPath: String;
  sUnInstallString: String;
begin
  // sUnInstPath := ExpandConstant('SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}_is1'); // for ver 1.4.5 and newer
  sUnInstPath := ExpandConstant('SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}'); // ver 1.4.4 -> ver 1.4.5
  if (Is64BitPowerPointFromRegisteredExe()) then begin
    RegQueryStringValue(HKLM64, sUnInstPath, 'UninstallString', sUnInstallString);
    Result := sUnInstallString;
  end
  else begin
    RegQueryStringValue(HKLM32, sUnInstPath, 'UninstallString', sUnInstallString);
    Result := sUnInstallString;
  end;
  // MsgBox(sUnInstallString, mbInformation, MB_OK);
end;
 
 
/////////////////////////////////////////////////////////////////////
function IsUpgrade(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;
 
 
/////////////////////////////////////////////////////////////////////
function UnInstallOldVersion(): Integer;
var
  sUnInstallString: String;
  iResultCode: Integer;
begin
// Return Values:
// 1 - uninstall string is empty
// 2 - error executing the UnInstallString
// 3 - successfully executed the UnInstallString
 
  // default return value
  Result := 0;
 
  // get the uninstall string of the old app
  sUnInstallString := GetUninstallString();
  if sUnInstallString <> '' then begin
    // sUnInstallString := RemoveQuotes(sUnInstallString);
    if ShellExec('', 'msiexec',  '/uninstall {D94628E7-2C6F-4A17-85BF-AD30316F61BD} /quiet', '', SW_SHOWNORMAL, ewWaitUntilTerminated, iResultCode) then // for ver 1.4.4 -> 1.4.5
    // if ShellExec('', sUnInstallString, '/SILENT /NORESTART /SUPPRESSMSGBOXES', '', SW_SHOWNORMAL, ewWaitUntilTerminated, iResultCode) then // for ver 1.4.5 and later
      Result := 3
    else
      Result := 2;
  end else
    Result := 1;
end;
 
/////////////////////////////////////////////////////////////////////
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep=ssInstall) then
  begin
    if (IsUpgrade()) then
    begin
      UnInstallOldVersion();
    end;
  end;
end;
 
function InitializeSetup(): boolean;
begin
  if (IsUpgrade()) then
  begin
    MsgBox(ExpandConstant('{cm:RemoveOld}'), mbInformation, MB_OK);
    UnInstallOldVersion();
  end;
    Result := true;
end;
