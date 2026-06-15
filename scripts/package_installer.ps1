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

$uninstallScript = @'
$ErrorActionPreference = "Stop"

$installRoot = Split-Path -Parent $PSCommandPath
$markerPath = Join-Path $installRoot ".vrperf-installed"
$manifestPath = Join-Path $installRoot "install_manifest.txt"

if (!(Test-Path $markerPath) -or !(Test-Path $manifestPath)) {
    Write-Error "Install marker or manifest is missing. Refusing to remove '$installRoot'."
    exit 1
}

Get-Process -Name "vr_perf_profiler","VRPerfProfiler.LhmBridge" -ErrorAction SilentlyContinue |
    Stop-Process -Force

$startMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\VR Performance Profiler"
$desktopShortcut = Join-Path ([Environment]::GetFolderPath("DesktopDirectory")) "VR Performance Profiler.lnk"

Remove-Item -Path $startMenuDir -Recurse -Force
Remove-Item -Path $desktopShortcut -Force
Remove-Item -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\VRPerformanceProfiler" -Recurse -Force

$rootFullPath = [System.IO.Path]::GetFullPath($installRoot).TrimEnd('\') + '\'
$relativePaths = Get-Content -Path $manifestPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
foreach ($relativePath in $relativePaths) {
    if ([System.IO.Path]::IsPathRooted($relativePath)) {
        continue
    }

    $targetPath = [System.IO.Path]::GetFullPath((Join-Path $installRoot $relativePath))
    if (!$targetPath.StartsWith($rootFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        continue
    }

    Remove-Item -Path $targetPath -Force -ErrorAction SilentlyContinue
}

Remove-Item -Path $manifestPath -Force -ErrorAction SilentlyContinue
Remove-Item -Path $markerPath -Force -ErrorAction SilentlyContinue

Get-ChildItem -Path $installRoot -Directory -Recurse -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    ForEach-Object {
        if (!(Get-ChildItem -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue)) {
            Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
        }
    }

if (!(Get-ChildItem -LiteralPath $installRoot -Force -ErrorAction SilentlyContinue)) {
    Remove-Item -LiteralPath $installRoot -Force -ErrorAction SilentlyContinue
}
'@
Set-Content -Path (Join-Path $appPayload "uninstall.ps1") -Value $uninstallScript -Encoding UTF8

$payloadZip = Join-Path $installerWork "payload.zip"
if (Test-Path $payloadZip) {
    Remove-Item -Path $payloadZip -Force
}
Compress-Archive -Path (Join-Path $payloadRoot "*") -DestinationPath $payloadZip -Force

if (Test-Path $portableZip) {
    Remove-Item -Path $portableZip -Force
}
Compress-Archive -Path (Join-Path $appPayload "*") -DestinationPath $portableZip -Force

if (-not $SkipInstallerExe) {
    Assert-Command "csc.exe"

    $bootstrapperSource = Join-Path $installerWork "InstallerBootstrapper.cs"
    $bootstrapperCode = @'
using Microsoft.Win32;
using System.Collections.Generic;
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;

internal static class InstallerBootstrapper
{
    private const string AppName = "VR Performance Profiler";
    private const string Version = "__VERSION__";
    private const string ResourceName = "VRPerfPayloadZip";

    [STAThread]
    private static int Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        try
        {
            string installRoot = SelectInstallRoot();
            if (string.IsNullOrWhiteSpace(installRoot))
            {
                return 1;
            }
            installRoot = NormalizeInstallRoot(installRoot);

            StopProcesses();

            string extractRoot = Path.Combine(
                Path.GetTempPath(),
                "VRPerfProfilerInstall-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(extractRoot);

            List<string> installedFiles = new List<string>();
            try
            {
                string payloadZip = Path.Combine(extractRoot, "payload.zip");
                ExtractPayloadResource(payloadZip);
                ZipFile.ExtractToDirectory(payloadZip, extractRoot);

                string appSource = Path.Combine(extractRoot, "app");
                installedFiles = CopyDirectory(appSource, installRoot);
            }
            finally
            {
                TryDeleteDirectory(extractRoot);
            }

            string exePath = Path.Combine(installRoot, "vr_perf_profiler.exe");
            WriteInstallMarkerAndManifest(installRoot, installedFiles);
            CreateShortcuts(exePath, installRoot);
            RegisterUninstall(installRoot, exePath);

            if (!IsWebView2RuntimeInstalled())
            {
                MessageBox.Show(
                    "VR Performance Profiler was installed. WebView2 Runtime was not detected, so the settings window may require Microsoft Edge WebView2 Runtime.",
                    AppName,
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
            }

            Process.Start(new ProcessStartInfo
            {
                FileName = exePath,
                WorkingDirectory = installRoot,
                UseShellExecute = true
            });
            return 0;
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, AppName, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }

    private static string SelectInstallRoot()
    {
        string defaultRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Programs",
            "VR Performance Profiler");

        using (Form form = new Form())
        using (Label label = new Label())
        using (TextBox pathBox = new TextBox())
        using (Button browseButton = new Button())
        using (Button installButton = new Button())
        using (Button cancelButton = new Button())
        {
            form.Text = "Install " + AppName;
            form.StartPosition = FormStartPosition.CenterScreen;
            form.FormBorderStyle = FormBorderStyle.FixedDialog;
            form.MaximizeBox = false;
            form.MinimizeBox = false;
            form.ClientSize = new Size(560, 145);

            label.Text = "Choose installation folder:";
            label.AutoSize = true;
            label.Location = new Point(12, 16);

            pathBox.Text = defaultRoot;
            pathBox.Location = new Point(12, 42);
            pathBox.Size = new Size(430, 23);

            browseButton.Text = "Browse...";
            browseButton.Location = new Point(454, 40);
            browseButton.Size = new Size(90, 27);
            browseButton.Click += delegate
            {
                using (FolderBrowserDialog dialog = new FolderBrowserDialog())
                {
                    dialog.Description = "Choose installation folder";
                    dialog.ShowNewFolderButton = true;
                    dialog.SelectedPath = GetExistingParentDirectory(pathBox.Text);
                    if (dialog.ShowDialog(form) == DialogResult.OK)
                    {
                        pathBox.Text = dialog.SelectedPath;
                    }
                }
            };

            installButton.Text = "Install";
            installButton.Location = new Point(354, 98);
            installButton.Size = new Size(90, 28);
            installButton.DialogResult = DialogResult.OK;

            cancelButton.Text = "Cancel";
            cancelButton.Location = new Point(454, 98);
            cancelButton.Size = new Size(90, 28);
            cancelButton.DialogResult = DialogResult.Cancel;

            form.Controls.Add(label);
            form.Controls.Add(pathBox);
            form.Controls.Add(browseButton);
            form.Controls.Add(installButton);
            form.Controls.Add(cancelButton);
            form.AcceptButton = installButton;
            form.CancelButton = cancelButton;

            if (form.ShowDialog() != DialogResult.OK)
            {
                return null;
            }

            string selectedPath = pathBox.Text.Trim();
            if (string.IsNullOrWhiteSpace(selectedPath))
            {
                throw new InvalidOperationException("Installation folder cannot be empty.");
            }

            return selectedPath;
        }
    }

    private static string NormalizeInstallRoot(string selectedPath)
    {
        string fullPath = Path.GetFullPath(Environment.ExpandEnvironmentVariables(selectedPath.Trim()));
        string root = Path.GetPathRoot(fullPath);
        string trimmed = fullPath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string leaf = Path.GetFileName(trimmed);

        if (string.IsNullOrWhiteSpace(trimmed) ||
            string.Equals(trimmed, root.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar), StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Choose a folder below a drive root.");
        }

        string windows = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
        if (IsSameOrChildOf(trimmed, windows))
        {
            throw new InvalidOperationException("Choose a folder outside the Windows system directory.");
        }

        if (!string.Equals(leaf, AppName, StringComparison.OrdinalIgnoreCase))
        {
            trimmed = Path.Combine(trimmed, AppName);
        }

        return trimmed;
    }

    private static string GetExistingParentDirectory(string path)
    {
        string candidate = path;
        while (!string.IsNullOrWhiteSpace(candidate))
        {
            if (Directory.Exists(candidate))
            {
                return candidate;
            }
            candidate = Path.GetDirectoryName(candidate);
        }

        return Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    }

    private static void ExtractPayloadResource(string destination)
    {
        using (Stream input = Assembly.GetExecutingAssembly().GetManifestResourceStream(ResourceName))
        {
            if (input == null)
            {
                throw new InvalidOperationException("Installer payload is missing.");
            }

            using (FileStream output = File.Create(destination))
            {
                input.CopyTo(output);
            }
        }
    }

    private static List<string> CopyDirectory(string source, string destination)
    {
        List<string> copiedFiles = new List<string>();
        Directory.CreateDirectory(destination);

        foreach (string directory in Directory.GetDirectories(source, "*", SearchOption.AllDirectories))
        {
            Directory.CreateDirectory(directory.Replace(source, destination));
        }

        foreach (string file in Directory.GetFiles(source, "*", SearchOption.AllDirectories))
        {
            string target = file.Replace(source, destination);
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            File.Copy(file, target, true);

            string relativePath = file.Substring(source.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar).Length + 1);
            copiedFiles.Add(relativePath);
        }

        return copiedFiles;
    }

    private static void WriteInstallMarkerAndManifest(string installRoot, List<string> installedFiles)
    {
        File.WriteAllText(Path.Combine(installRoot, ".vrperf-installed"), Version);

        using (StreamWriter writer = new StreamWriter(Path.Combine(installRoot, "install_manifest.txt"), false))
        {
            foreach (string relativePath in installedFiles)
            {
                if (string.IsNullOrWhiteSpace(relativePath) || Path.IsPathRooted(relativePath))
                {
                    continue;
                }

                writer.WriteLine(relativePath);
            }

            writer.WriteLine(".vrperf-installed");
        }
    }

    private static bool IsSameOrChildOf(string candidate, string parent)
    {
        if (string.IsNullOrWhiteSpace(candidate) || string.IsNullOrWhiteSpace(parent))
        {
            return false;
        }

        string candidateFullPath = Path.GetFullPath(candidate).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
        string parentFullPath = Path.GetFullPath(parent).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
        return candidateFullPath.StartsWith(parentFullPath, StringComparison.OrdinalIgnoreCase);
    }

    private static void StopProcesses()
    {
        foreach (string processName in new[] { "vr_perf_profiler", "VRPerfProfiler.LhmBridge" })
        {
            foreach (Process process in Process.GetProcessesByName(processName))
            {
                try
                {
                    process.Kill();
                    process.WaitForExit(3000);
                }
                catch
                {
                }
            }
        }
    }

    private static void CreateShortcuts(string exePath, string installRoot)
    {
        string startMenuDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "Microsoft",
            "Windows",
            "Start Menu",
            "Programs",
            AppName);
        Directory.CreateDirectory(startMenuDir);

        CreateShortcut(Path.Combine(startMenuDir, AppName + ".lnk"), exePath, installRoot);
        CreateShortcut(
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), AppName + ".lnk"),
            exePath,
            installRoot);
    }

    private static void CreateShortcut(string shortcutPath, string targetPath, string workingDirectory)
    {
        Type shellType = Type.GetTypeFromProgID("WScript.Shell");
        dynamic shell = Activator.CreateInstance(shellType);
        dynamic shortcut = shell.CreateShortcut(shortcutPath);
        shortcut.TargetPath = targetPath;
        shortcut.WorkingDirectory = workingDirectory;
        shortcut.IconLocation = targetPath;
        shortcut.Save();
    }

    private static void RegisterUninstall(string installRoot, string exePath)
    {
        using (RegistryKey key = Registry.CurrentUser.CreateSubKey(
            @"Software\Microsoft\Windows\CurrentVersion\Uninstall\VRPerformanceProfiler"))
        {
            key.SetValue("DisplayName", AppName);
            key.SetValue("DisplayVersion", Version);
            key.SetValue("Publisher", AppName);
            key.SetValue("InstallLocation", installRoot);
            key.SetValue("DisplayIcon", exePath);
            key.SetValue(
                "UninstallString",
                "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" +
                Path.Combine(installRoot, "uninstall.ps1") +
                "\"");
            key.SetValue("NoModify", 1, RegistryValueKind.DWord);
            key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
        }
    }

    private static bool IsWebView2RuntimeInstalled()
    {
        string[] subKeys =
        {
            @"SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
            @"SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
        };

        foreach (RegistryHive hive in new[] { RegistryHive.LocalMachine, RegistryHive.CurrentUser })
        {
            foreach (string subKey in subKeys)
            {
                using (RegistryKey baseKey = RegistryKey.OpenBaseKey(hive, RegistryView.Registry64))
                using (RegistryKey key = baseKey.OpenSubKey(subKey))
                {
                    if (key != null)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    private static void TryDeleteDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path))
            {
                Directory.Delete(path, true);
            }
        }
        catch
        {
        }
    }
}
'@ -replace "__VERSION__", $version

    Set-Content -Path $bootstrapperSource -Value $bootstrapperCode -Encoding UTF8
    Invoke-Native -FilePath "csc.exe" -Arguments @(
        "/nologo",
        "/target:winexe",
        "/platform:x64",
        "/optimize+",
        "/out:$installerPath",
        "/resource:$payloadZip,VRPerfPayloadZip",
        "/reference:System.IO.Compression.dll",
        "/reference:System.IO.Compression.FileSystem.dll",
        "/reference:System.Windows.Forms.dll",
        "/reference:System.Drawing.dll",
        "/reference:Microsoft.CSharp.dll",
        $bootstrapperSource
    )

    if (-not (Test-Path $installerPath)) {
        throw "Installer compiler completed without creating installer: $installerPath"
    }
}

Write-Host "Portable package: $portableZip"
if (Test-Path $installerPath) {
    Write-Host "Installer: $installerPath"
}
