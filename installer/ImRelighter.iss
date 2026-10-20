; ImRelighter installer script for Inno Setup 6.5 or newer.
; Build through scripts/build-installer.ps1 so the version and input
; directory stay in sync with the current CMake build.

#ifndef MyAppVersion
#define MyAppVersion "0.4.2"
#endif

#ifndef MyBuildDir
#define MyBuildDir "..\build\mingw-release\bin"
#endif

#define MyAppName "ImRelighter"
#define MyAppPublisher "PZA"
#define MyAppExeName "ImRelighter.exe"

[Setup]
; Keep this AppId stable: upgrades and uninstall rely on it.
AppId={{3C470D1C-79FA-4A29-9A1B-7D711779ED60}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
; Reuse the previous installation directory and task choices. This is what
; makes an in-place upgrade feel like an update instead of a second install.
UsePreviousAppDir=yes
UsePreviousTasks=yes
UsePreviousGroup=yes
UsePreviousLanguage=yes
; Per-user installation keeps the application directory writable. The app
; stores config.json next to the executable, so installing under Program Files
; would silently lose settings for non-elevated users.
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=..\dist
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-setup
SetupIconFile=..\assets\icons\ImRelight.ico
LicenseFile=..\LICENSE
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
#ifdef IncludeVCRedist
Source: "redist\vc_redist.x64.exe"; DestDir: "{tmp}"; \
    Flags: deleteafterinstall
#endif
Source: "{#MyBuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\LICENSE*"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\PATENTS*"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\assets\*"; DestDir: "{app}\assets"; \
    Flags: ignoreversion recursesubdirs createallsubdirs
; Models are user data as much as application data. Keep an existing model
; file even if the installer ships a file with the same name, add missing
; default/new model files, and never delete the models folder on uninstall.
; config.json is intentionally omitted from [Files], so it is preserved too.
Source: "{#MyBuildDir}\models\*"; DestDir: "{app}\models"; \
    Flags: onlyifdoesntexist recursesubdirs createallsubdirs \
    uninsneveruninstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; \
    Tasks: desktopicon

[Run]
#ifdef IncludeVCRedist
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "{cm:VCRedistInstalling}"; Check: NeedsVCRedist; \
    Flags: shellexec waituntilterminated
#endif
Filename: "{app}\{#MyAppExeName}"; \
    Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; \
    Flags: nowait postinstall skipifsilent

[CustomMessages]
english.VCRedistInstalling=Installing the Microsoft Visual C++ runtime...
chinesesimplified.VCRedistInstalling=正在安装 Microsoft Visual C++ 运行库…
english.VCRedistRequired=ImRelighter needs the Microsoft Visual C++ 2015-2022 runtime. Install it from https://aka.ms/vs/17/release/vc_redist.x64.exe, then start the application again.
chinesesimplified.VCRedistRequired=ImRelighter 需要 Microsoft Visual C++ 2015-2022 运行库。请从 https://aka.ms/vs/17/release/vc_redist.x64.exe 安装后重新启动应用。

; config.json is created by the application after installation. It is
; intentionally not listed in [Files] or [UninstallDelete], so uninstalling
; preserves user settings.

[Code]
function NeedsVCRedist: Boolean;
begin
  Result := not (FileExists(ExpandConstant('{sys}\vcruntime140.dll')) and
                 FileExists(ExpandConstant('{sys}\msvcp140.dll')));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssDone) and NeedsVCRedist then
    MsgBox(ExpandConstant('{cm:VCRedistRequired}'), mbInformation, MB_OK);
end;
