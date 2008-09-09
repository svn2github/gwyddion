[Setup]
AppName=Gwyddion Threshold-Example
AppVersion=2.0
AppVerName=Gwyddion Threshold-Example 2.0
AppPublisher=Trific soft.
AppPublisherURL=http://gwyddion.net/
AppCopyright=Copyright (C) 2003,2004 David Necas (Yeti)
DefaultDirName={code:GetGwyPath}\modules\process
DisableStartupPrompt=yes
WindowShowCaption=yes
WindowVisible=no
LicenseFile=COPYING
Compression=bzip/9
OutputBaseFilename=Gwyddion-Threshold-Example-2.0
; XXX!
Uninstallable=no

[Files]
Source: "threshold-example.dll"; DestDir: "{app}"
; XXX: COPYING is installed with gwyddion, but ...

[Code]
var
  Exists: Boolean;
  GwyPath: String;

function GetGwyInstalled (): Boolean;
begin
  Exists := RegQueryStringValue (HKLM, 'Software\Gwyddion\1.0', 'Path', GwyPath);
  if not Exists then begin
    Exists := RegQueryStringValue (HKCU, 'Software\GwyPath\1.0', 'Path', GwyPath);
  end;
  Result := Exists
end;

function GetGwyPath (S: String): String;
begin
    Result := GwyPath;
end;

function InitializeSetup(): Boolean;
begin
  Result := GetGwyInstalled ();
  if not Result then begin
    MsgBox ('Please install Gwyddion before installing modules.  You can obtain Gwyddion from http://gwyddion.net/', mbError, MB_OK);
  end;
end;

