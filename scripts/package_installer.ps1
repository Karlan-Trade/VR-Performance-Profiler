param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build-release",
    [string]$OutputDir = "dist",
    [switch]$SkipBuild,
    [switch]$SkipInstallerExe
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    $scriptDir = Split-Path -Parent $PSCommandPath
    return (Resolve-Path (Join-Path $scriptDir "..")).Path
}

function Get-ProjectVersion {
    param([string]$RepoRoot)

    $cmake = Get-Content -Path (Join-Path $RepoRoot "CMakeLists.txt") -Raw
    if ($cmake -match "project\(VRPerfProfiler\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)") {
        return $Matches[1]
    }

    throw "Unable to read project version from CMakeLists.txt"
}

function Assert-Command {
    param([string]$Name)

    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' was not found in PATH."
    }
}

function Find-InnoSetupCompiler {
    $command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 5\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Copy-RequiredFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path $Source)) {
        throw "Required file missing: $Source"
    }

    Copy-Item -Path $Source -Destination $Destination -Force
}

function Escape-InnoString {
    param([string]$Value)
    return $Value.Replace('"', '""')
}

$repoRoot = Resolve-RepoRoot
$version = Get-ProjectVersion -RepoRoot $repoRoot
$buildPath = Join-Path $repoRoot $BuildDir
$outputPath = Join-Path $repoRoot $OutputDir
$packageRoot = Join-Path $outputPath "VRPerformanceProfiler-$version"
$payloadRoot = Join-Path $packageRoot "payload"
$appPayload = Join-Path $payloadRoot "app"
$installerWork = Join-Path $packageRoot "installer"
$installerName = "VRPerformanceProfiler-$version-Setup.exe"
$installerPath = Join-Path $outputPath $installerName
$portableZip = Join-Path $outputPath "VRPerformanceProfiler-$version-portable.zip"

Assert-Command "cmake"
Assert-Command "dotnet"

if (-not $SkipBuild) {
    $configureArgs = @(
        "-S", $repoRoot,
        "-B", $buildPath,
        "-G", "NMake Makefiles",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DBUILD_TESTS=OFF"
    )

    $cachedOpenVr = Join-Path $repoRoot "build-proxy\_deps\openvr-src"
    if (Test-Path $cachedOpenVr) {
        $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_OPENVR=$cachedOpenVr"
    }

    $cachedJson = Join-Path $repoRoot "build-proxy\_deps\nlohmann_json-src"
    if (Test-Path $cachedJson) {
        $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=$cachedJson"
    }

    Invoke-Native -FilePath "cmake" -Arguments $configureArgs
    Invoke-Native -FilePath "cmake" -Arguments @("--build", $buildPath)
}

if (Test-Path $packageRoot) {
    Remove-Item -Path $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $appPayload, $installerWork | Out-Null

Copy-RequiredFile -Source (Join-Path $buildPath "vr_perf_profiler.exe") -Destination $appPayload
Copy-RequiredFile -Source (Join-Path $buildPath "openvr_api.dll") -Destination $appPayload
Copy-RequiredFile -Source (Join-Path $repoRoot "LICENSE") -Destination $appPayload
Copy-RequiredFile -Source (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md") -Destination $appPayload

$webViewLoader = Join-Path $buildPath "WebView2Loader.dll"
if (Test-Path $webViewLoader) {
    Copy-Item -Path $webViewLoader -Destination $appPayload -Force
}

$lhmBridgeOutput = Join-Path $appPayload "lhm_bridge"
$previousDotnetCliHome = $env:DOTNET_CLI_HOME
$previousDotnetSkipFirstTimeExperience = $env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE
$previousNugetPackages = $env:NUGET_PACKAGES
$previousAppData = $env:APPDATA
try {
    $env:DOTNET_CLI_HOME = Join-Path $repoRoot ".dotnet-home"
    $env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = "1"
    $env:NUGET_PACKAGES = Join-Path $repoRoot ".nuget-packages"
    $env:APPDATA = Join-Path $repoRoot ".appdata"
    New-Item -ItemType Directory -Force -Path $env:DOTNET_CLI_HOME, $env:NUGET_PACKAGES, $env:APPDATA | Out-Null

    Invoke-Native -FilePath "dotnet" -Arguments @(
        "publish",
        (Join-Path $repoRoot "tools\lhm_bridge\VRPerfProfiler.LhmBridge.csproj"),
        "--configfile", (Join-Path $repoRoot "NuGet.Config"),
        "--ignore-failed-sources",
        "-c", $Configuration,
        "-r", "win-x64",
        "--self-contained", "true",
        "-p:PublishSingleFile=false",
        "-p:DebugType=None",
        "-p:DebugSymbols=false",
        "-o", $lhmBridgeOutput
    )
}
finally {
    $env:DOTNET_CLI_HOME = $previousDotnetCliHome
    $env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = $previousDotnetSkipFirstTimeExperience
    $env:NUGET_PACKAGES = $previousNugetPackages
    $env:APPDATA = $previousAppData
}

if (Test-Path $portableZip) {
    Remove-Item -Path $portableZip -Force
}
Compress-Archive -Path (Join-Path $appPayload "*") -DestinationPath $portableZip -Force

if (-not $SkipInstallerExe) {
    $iscc = Find-InnoSetupCompiler
    if (-not $iscc) {
        throw "Inno Setup compiler ISCC.exe was not found. Install Inno Setup 6, then rerun this script. Portable package was still created: $portableZip"
    }

    $setupScript = Join-Path $installerWork "VRPerformanceProfiler.iss"
    $escapedAppPayload = Escape-InnoString $appPayload
    $escapedOutputPath = Escape-InnoString $outputPath
    $escapedIcon = Escape-InnoString (Join-Path $repoRoot "resources\app.ico")
    $escapedInstallerNameWithoutExt = Escape-InnoString ([System.IO.Path]::GetFileNameWithoutExtension($installerName))

    $innoScript = @"
#define AppName "VR Performance Profiler"
#define AppVersion "$version"
#define AppPublisher "VR Performance Profiler"
#define AppExeName "vr_perf_profiler.exe"

[Setup]
AppId={{7E7F19F2-8AC4-4B64-B4B4-7D9E9C1D989B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=$escapedOutputPath
OutputBaseFilename=$escapedInstallerNameWithoutExt
SetupIconFile=$escapedIcon
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
CloseApplications=yes
RestartApplications=no
WizardStyle=modern
MinVersion=10.0

[Files]
Source: "$escapedAppPayload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
const
  ProductFolderName = 'VR Performance Profiler';

function RemoveTrailingBackslash(Value: string): string;
begin
  Result := RemoveBackslash(Value);
end;

function IsDriveRoot(Value: string): Boolean;
var
  Root: string;
begin
  Root := RemoveTrailingBackslash(ExtractFileDrive(Value) + '\');
  Result := CompareText(RemoveTrailingBackslash(Value), Root) = 0;
end;

function IsProductFolder(Value: string): Boolean;
begin
  Result := CompareText(ExtractFileName(RemoveTrailingBackslash(Value)), ProductFolderName) = 0;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  SelectedDir: string;
begin
  Result := True;
  if CurPageID = wpSelectDir then
  begin
    SelectedDir := RemoveTrailingBackslash(WizardDirValue);
    if IsDriveRoot(SelectedDir) then
    begin
      MsgBox('Choose a folder below a drive root.', mbError, MB_OK);
      Result := False;
      exit;
    end;

    if not IsProductFolder(SelectedDir) then
    begin
      WizardForm.DirEdit.Text := AddBackslash(SelectedDir) + ProductFolderName;
    end;
  end;
end;

function IsWebView2RuntimeInstalled(): Boolean;
begin
  Result :=
    RegKeyExists(HKLM64, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    RegKeyExists(HKLM32, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    RegKeyExists(HKCU64, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    RegKeyExists(HKCU32, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if not IsWebView2RuntimeInstalled() then
    begin
      MsgBox(
        'WebView2 Runtime was not detected. The settings window may require Microsoft Edge WebView2 Runtime.',
        mbInformation,
        MB_OK);
    end;
  end;
end;
"@

    Set-Content -Path $setupScript -Value $innoScript -Encoding UTF8
    Invoke-Native -FilePath $iscc -Arguments @($setupScript)

    if (-not (Test-Path $installerPath)) {
        throw "Inno Setup completed without creating installer: $installerPath"
    }
}

Write-Host "Portable package: $portableZip"
if (Test-Path $installerPath) {
    Write-Host "Installer: $installerPath"
}
