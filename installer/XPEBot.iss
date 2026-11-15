; XPE Bot - instalador (Inno Setup 6)
; Se compila en CI con:
;   ISCC.exe /DAppVersion=2.1.0 /DSourceDir=<carpeta con XPEBot.exe> /DOutputDir=<salida> installer\XPEBot.iss
; Instalacion por usuario (sin admin) en %LOCALAPPDATA%\XPEBot, que es escribible y
; permite que el auto-update reemplace los archivos sin elevar permisos.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#ifndef SourceDir
  #define SourceDir "..\dist\XPEBot"
#endif

#ifndef OutputDir
  #define OutputDir "..\dist\installer"
#endif

#define AppName "XPE Bot"
#define AppPublisher "xpe.nettt"
#define AppExeName "XPEBot.exe"
#define AppUrl "https://github.com/full-proyects/Brawlhalla_Autododge_Bot"

[Setup]
AppId={{8E2A7B5C-3D64-4E7F-9C21-6A1B0F4D8E33}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
DefaultDirName={localappdata}\XPEBot
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename=XPEBot-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#AppExeName}
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
AllowNoIcons=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\logs"
Type: filesandordirs; Name: "{app}\update"
