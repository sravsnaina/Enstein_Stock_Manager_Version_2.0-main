; Inno Setup script for Enstein Stock Manager.
;
; Produces a single EnsteinStockManager-Setup-<version>.exe that installs the
; app, its Qt runtime and the PostgreSQL client libraries, and registers a
; Start Menu entry plus an uninstaller.
;
; Compiled in CI with:
;   ISCC /DAppVersion=2.0.0 /DStageDir=<deployed app dir> /DSourceRoot=<repo root> installer.iss
;
; StageDir must be the windeployqt output directory: EnsteinStockManager.exe
; with every DLL, plugin and QML module already sitting beside it.

#ifndef AppVersion
  #define AppVersion "2.0.0"
#endif
#ifndef StageDir
  #error StageDir must be defined (path to the deployed application directory)
#endif
#ifndef SourceRoot
  #error SourceRoot must be defined (path to the repository root)
#endif

#define AppName      "Enstein Stock Manager"
#define AppPublisher "Enstein Robots and Automations Pvt Limited"
#define AppExeName   "EnsteinStockManager.exe"

[Setup]
; AppId keeps upgrades in place instead of installing side by side. Never
; change this GUID once a version has shipped, or upgrades turn into duplicate
; installs that fight over the Start Menu entry.
AppId={{7C1F4A62-9D3E-4B57-8E21-5A6C0F2B9D14}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Enstein Stock Manager
DefaultGroupName=Enstein Stock Manager
DisableProgramGroupPage=yes
OutputBaseFilename=EnsteinStockManager-Setup-{#AppVersion}
SetupIconFile={#SourceRoot}\assets\app-icon.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; x64 only: the Qt runtime we ship is 64-bit, so refuse to install on a 32-bit
; Windows rather than failing at first launch with a cryptic DLL error.
; "x64" rather than the newer "x64compatible": the old identifier is accepted by
; every Inno Setup 6.x, so the build does not depend on the runner image having
; a recent one.
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; Install per-user when run without admin rights, per-machine with them.
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Qt writes the QML disk cache next to the install; leave the user's database
; (in %APPDATA%) alone, but clean up caches we created.
Type: filesandordirs; Name: "{app}\qmlcache"
