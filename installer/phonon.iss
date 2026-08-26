#define MyAppName "Phonon Windows"
#define MyAppVersion "0.1.0"
#define MyAppExeName "phonon.exe"

[Setup]
AppId={{7E3F1A2B-9C4D-4E8A-B5F1-PHONON00001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\PhononWindows
DefaultGroupName=Phonon
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputDir=..\dist
OutputBaseFilename=PhononWindows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "autostart"; Description: "Lancer Phonon au démarrage de Windows"; \
    GroupDescription: "Options :"; Flags: unchecked

[Files]
Source: "..\build\Release\phonon.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\Phonon"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller Phonon"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Lancer Phonon"; \
    Flags: nowait postinstall skipifsilent

[Registry]
; Démarrage automatique (si la tâche est cochée)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "PhononWindows"; \
    ValueData: """{app}\{#MyAppExeName}"" --minimized"; \
    Tasks: autostart; Flags: uninsdeletevalue

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C taskkill /IM phonon.exe /F"; \
    RunOnceId: "KillPhonon"; Flags: runhidden

[UninstallDelete]
; Supprime le modèle téléchargé (~1,2 Go) pour une désinstallation propre.
Type: filesandordirs; Name: "{localappdata}\PhononWindows"
