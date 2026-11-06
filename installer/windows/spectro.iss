; Spectro by Aron H — Windows installer (Inno Setup 6)
#define AppName "Spectro"
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\Spectro_artefacts\Release"
#endif

[Setup]
AppId={{6B2F3C1E-5A7D-4E0B-9C21-A50EC7120001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Aron H
AppPublisherURL=https://www.instagram.com/aronh.official
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=Spectro-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#AppName}

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#BuildDir}\VST3\Spectro.vst3\*"; DestDir: "{commoncf64}\VST3\Spectro.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#BuildDir}\Standalone\Spectro.exe"; DestDir: "{autopf}\Spectro"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Spectro"; Filename: "{autopf}\Spectro\Spectro.exe"; Components: standalone
