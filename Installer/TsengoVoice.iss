#define AppName    "Tsengo Voice"
#define AppVersion "5.0.0"
#define Publisher  "Tsengo"

[Setup]
AppId={{9E4C1F82-3A77-4B21-9D6E-TSENGOVOICE01}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputDir=Output
OutputBaseFilename=TsengoVoice-Setup-{#AppVersion}
LicenseFile=LICENSE.txt
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayName={#AppName} {#AppVersion}
SourceDir=..

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[Components]
Name: "synth"; Description: "Plugin VST3 - Synth (FL Studio)"; Types: full compact custom; Flags: checkablealone
Name: "fx"; Description: "Plugin VST3 - Effect (Reaper, Ableton, Cubase)"; Types: full custom
Name: "app"; Description: "Application autonome (loopMIDI)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Creer un raccourci sur le Bureau"; Components: app; Flags: unchecked

[Files]
Source: "artifact\Tsengo Voice Synth.vst3\*"; DestDir: "{code:GetVST3Dir}\Tsengo Voice Synth.vst3"; Components: synth; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "artifact\Tsengo Voice FX.vst3\*"; DestDir: "{code:GetVST3Dir}\Tsengo Voice FX.vst3"; Components: fx; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "artifact\Tsengo Voice.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Tsengo Voice.exe"; Components: app
Name: "{group}\Desinstaller {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Tsengo Voice.exe"; Components: app; Tasks: desktopicon

[Run]
Filename: "{app}\Tsengo Voice.exe"; Description: "Lancer {#AppName}"; Flags: nowait postinstall skipifsilent; Components: app

[Code]
var VstPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  VstPage := CreateInputDirPage(wpSelectComponents,
    'Dossier des plugins VST3',
    'Ou faut-il installer les plugins VST3 ?',
    'Le dossier standard convient a la plupart des DAW.',
    False, '');
  VstPage.Add('');
  VstPage.Values[0] := ExpandConstant('{commoncf64}\VST3');
end;

function GetVST3Dir(Param: String): String;
begin
  Result := VstPage.Values[0];
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = VstPage.ID then
    Result := not (IsComponentSelected('synth') or IsComponentSelected('fx'));
end;
