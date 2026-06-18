param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build-release",
    [string]$OutputDir = "dist",
    [switch]$SkipBuild,
    [switch]$SkipInstallerExe,
    [switch]$SkipMsi
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

function Find-WixToolset {
    $command = Get-Command "wix.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $localTool = Join-Path (Resolve-RepoRoot) ".tools\wix\wix.exe"
    if (Test-Path $localTool) {
        return $localTool
    }

    return $null
}

function Find-CSharpCompiler {
    $command = Get-Command "csc.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "D:\VS\MSBuild\Current\Bin\Roslyn\csc.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\Roslyn\csc.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\Roslyn\csc.exe",
        "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
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

function Escape-WixAttribute {
    param([string]$Value)
    return $Value.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace('"', "&quot;").Replace("'", "&apos;")
}

function New-DeterministicGuid {
    param([string]$Value)

    $md5 = [System.Security.Cryptography.MD5]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value.ToLowerInvariant())
        $hash = $md5.ComputeHash($bytes)
        return (New-Object -TypeName System.Guid -ArgumentList (,$hash)).ToString().ToUpperInvariant()
    }
    finally {
        $md5.Dispose()
    }
}

function ConvertTo-LicenseRtf {
    param([string]$Text)

    $escaped = $Text.Replace("\", "\\").Replace("{", "\{").Replace("}", "\}")
    $escaped = $escaped -replace "`r?`n", "\par`r`n"
    return "{\rtf1\ansi\deff0`r`n$escaped`r`n}"
}

$repoRoot = Resolve-RepoRoot
$version = Get-ProjectVersion -RepoRoot $repoRoot
$buildPath = Join-Path $repoRoot $BuildDir
$outputPath = Join-Path $repoRoot $OutputDir
$packageRoot = Join-Path $outputPath "VRPerformanceProfiler-$version"
$payloadRoot = Join-Path $packageRoot "payload"
$appPayload = Join-Path $payloadRoot "app"
$installerWork = Join-Path $packageRoot "installer"
$installerName = "VRPerformanceProfiler-$version-Setup.msi"
$installerPath = Join-Path $outputPath $installerName
$legacyInstallerPath = Join-Path $outputPath "VRPerformanceProfiler-$version-Setup.exe"
$portableZip = Join-Path $outputPath "VRPerformanceProfiler-$version-portable.zip"

Assert-Command "cmake"

if (-not $SkipBuild) {
    $configureArgs = @(
        "-S", $repoRoot,
        "-B", $buildPath,
        "-G", "NMake Makefiles",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DBUILD_TESTS=OFF"
    )

    $openVrCacheCandidates = @(
        (Join-Path $repoRoot "build-vs18\_deps\openvr-src"),
        (Join-Path $repoRoot "build-proxy\_deps\openvr-src")
    )
    $cachedOpenVr = $openVrCacheCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($cachedOpenVr) {
        $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_OPENVR=$cachedOpenVr"
    }

    $jsonCacheCandidates = @(
        (Join-Path $repoRoot "build-codex-offset\_deps\nlohmann_json-src"),
        (Join-Path $repoRoot "build-proxy\_deps\nlohmann_json-src")
    )
    $cachedJson = $jsonCacheCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($cachedJson) {
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

if (Test-Path $portableZip) {
    Remove-Item -Path $portableZip -Force
}
Compress-Archive -Path (Join-Path $appPayload "*") -DestinationPath $portableZip -Force

if (Test-Path $legacyInstallerPath) {
    Remove-Item -Path $legacyInstallerPath -Force
}

if (-not $SkipInstallerExe -and -not $SkipMsi) {
    $wix = Find-WixToolset
    if (-not $wix) {
        throw "WiX Toolset wix.exe was not found. Install it with: dotnet tool install --tool-path .\.tools\wix wix --version 4.0.5. Portable package was still created: $portableZip"
    }

    $csc = Find-CSharpCompiler
    if (-not $csc) {
        throw "C# compiler csc.exe was not found. Install Visual Studio Build Tools or run from a developer shell. Portable package was still created: $portableZip"
    }

    $licenseRtf = Join-Path $installerWork "License.rtf"
    Set-Content -Path $licenseRtf -Value (ConvertTo-LicenseRtf (Get-Content -Path (Join-Path $repoRoot "LICENSE") -Raw)) -Encoding ASCII

    $payloadAction = Join-Path $installerWork "VRPerfPayloadAction.exe"
    Invoke-Native -FilePath $csc -Arguments @(
        "/nologo",
        "/target:winexe",
        "/platform:x64",
        "/optimize+",
        "/out:$payloadAction",
        "/resource:$portableZip,VRPerfPayloadZip",
        "/reference:System.IO.Compression.dll",
        "/reference:System.IO.Compression.FileSystem.dll",
        (Join-Path $repoRoot "scripts\MsiPayloadAction.cs")
    )

    $wixSource = Join-Path $installerWork "VRPerformanceProfiler.wxs"
    $escapedLicenseRtf = Escape-WixAttribute $licenseRtf
    $escapedIcon = Escape-WixAttribute (Join-Path $repoRoot "resources\app.ico")
    $escapedPayloadAction = Escape-WixAttribute $payloadAction
    $registrationGuid = New-DeterministicGuid "VRPerformanceProfiler:ProductRegistration"

    $wixXml = @"
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:ui="http://wixtoolset.org/schemas/v4/wxs/ui">
  <Package Name="VR Performance Profiler"
           Manufacturer="VR Performance Profiler"
           Version="$version"
           UpgradeCode="7E7F19F2-8AC4-4B64-B4B4-7D9E9C1D989B"
           Scope="perUser">
    <MajorUpgrade Schedule="afterInstallInitialize"
                  DowngradeErrorMessage="A newer version of VR Performance Profiler is already installed." />
    <MediaTemplate EmbedCab="yes" CompressionLevel="medium" />
    <Property Id="DISABLEROLLBACK" Value="1" />
    <Icon Id="AppIcon.ico" SourceFile="$escapedIcon" />
    <Property Id="ARPPRODUCTICON" Value="AppIcon.ico" />
    <Property Id="WIXUI_INSTALLDIR" Value="INSTALLFOLDER" />
    <Property Id="ARPNOREPAIR" Value="1" />
    <ui:WixUI Id="WixUI_InstallDir" />
    <WixVariable Id="WixUILicenseRtf" Value="$escapedLicenseRtf" />
    <Binary Id="PayloadActionExe" SourceFile="$escapedPayloadAction" />
    <CustomAction Id="InstallPayload"
                  BinaryRef="PayloadActionExe"
                  ExeCommand="install &quot;[INSTALLFOLDER].&quot; &quot;[ProductVersion]&quot;"
                  Execute="immediate"
                  Return="check"
                  Impersonate="yes" />
    <CustomAction Id="UninstallPayload"
                  BinaryRef="PayloadActionExe"
                  ExeCommand="uninstall &quot;[INSTALLFOLDER].&quot;"
                  Execute="immediate"
                  Return="check"
                  Impersonate="yes" />
    <InstallExecuteSequence>
      <DisableRollback Before="InstallInitialize" />
      <Custom Action="InstallPayload" After="InstallInitialize" Condition="NOT Installed" />
      <Custom Action="UninstallPayload" Before="RemoveFolders" Condition="REMOVE=&quot;ALL&quot;" />
    </InstallExecuteSequence>

    <StandardDirectory Id="LocalAppDataFolder">
      <Directory Id="LocalProgramsFolder" Name="Programs">
        <Directory Id="INSTALLFOLDER" Name="VR Performance Profiler" />
      </Directory>
    </StandardDirectory>

    <ComponentGroup Id="ProductComponents">
      <Component Id="ProductRegistration" Directory="INSTALLFOLDER" Guid="{$registrationGuid}">
        <RemoveFolder Id="InstallFolder" Directory="INSTALLFOLDER" On="uninstall" />
        <RemoveFolder Id="LocalProgramsFolder" Directory="LocalProgramsFolder" On="uninstall" />
        <RegistryValue Root="HKCU"
                       Key="Software\VR Performance Profiler"
                       Name="msiProduct"
                       Type="string"
                       Value="[ProductCode]"
                       KeyPath="yes" />
      </Component>
    </ComponentGroup>

    <Feature Id="MainFeature" Title="VR Performance Profiler" Level="1">
      <ComponentGroupRef Id="ProductComponents" />
    </Feature>
  </Package>
</Wix>
"@

    Set-Content -Path $wixSource -Value $wixXml -Encoding UTF8
    Invoke-Native -FilePath $wix -Arguments @(
        "build",
        "-arch", "x64",
        "-ext", "WixToolset.UI.wixext",
        "-pdbtype", "none",
        "-out", $installerPath,
        $wixSource
    )

    if (-not (Test-Path $installerPath)) {
        throw "WiX completed without creating installer: $installerPath"
    }
}

Write-Host "Portable package: $portableZip"
if (Test-Path $installerPath) {
    Write-Host "Installer: $installerPath"
}
