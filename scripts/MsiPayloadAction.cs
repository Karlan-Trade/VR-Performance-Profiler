using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;

internal static class MsiPayloadAction
{
    private const string AppName = "VR Performance Profiler";
    private const string ResourceName = "VRPerfPayloadZip";
    private const string AppExeName = "vr_perf_profiler.exe";
    private const string AppRegistryKey = @"Software\VR Performance Profiler";

    private static int Main(string[] args)
    {
        try
        {
            if (args.Length < 2)
            {
                throw new InvalidOperationException("Usage: install|uninstall <install folder> [version]");
            }

            string command = args[0].Trim().ToLowerInvariant();
            if (command == "install")
            {
                string version = args.Length >= 3 ? args[2] : string.Empty;
                Install(args[1], version);
                return 0;
            }

            if (command == "uninstall")
            {
                Uninstall(args[1]);
                return 0;
            }

            throw new InvalidOperationException("Unknown command: " + args[0]);
        }
        catch (Exception ex)
        {
            Log("ERROR: " + ex);
            return 1;
        }
    }

    private static void Install(string selectedPath, string version)
    {
        string installRoot = NormalizeInstallRoot(selectedPath);
        Log("Installing to " + installRoot);

        StopProcesses();
        Directory.CreateDirectory(installRoot);

        string extractRoot = Path.Combine(
            Path.GetTempPath(),
            "VRPerfProfilerMsiPayload-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(extractRoot);

        List<string> installedFiles;
        try
        {
            string payloadZip = Path.Combine(extractRoot, "payload.zip");
            string payloadRoot = Path.Combine(extractRoot, "payload");
            ExtractPayloadResource(payloadZip);
            Directory.CreateDirectory(payloadRoot);
            ZipFile.ExtractToDirectory(payloadZip, payloadRoot);

            string appSubdirectory = Path.Combine(payloadRoot, "app");
            string appSource = Directory.Exists(appSubdirectory)
                ? appSubdirectory
                : payloadRoot;
            installedFiles = CopyDirectory(appSource, installRoot);
        }
        finally
        {
            TryDeleteDirectory(extractRoot);
        }

        WriteInstallMarkerAndManifest(installRoot, installedFiles, version);
        CreateShortcuts(Path.Combine(installRoot, AppExeName), installRoot);
        WriteAppRegistry(installRoot, version);
    }

    private static void Uninstall(string selectedPath)
    {
        string installRoot = ReadInstallLocation();
        if (string.IsNullOrWhiteSpace(installRoot))
        {
            installRoot = NormalizeInstallRoot(selectedPath);
        }

        Log("Uninstalling from " + installRoot);
        StopProcesses();
        RemoveShortcuts();

        string markerPath = Path.Combine(installRoot, ".vrperf-installed");
        string manifestPath = Path.Combine(installRoot, "install_manifest.txt");
        if (File.Exists(markerPath) && File.Exists(manifestPath))
        {
            RemoveManifestFiles(installRoot, manifestPath);
            TryDeleteFile(manifestPath);
            TryDeleteFile(markerPath);
            RemoveEmptyDirectories(installRoot);
        }
        else
        {
            Log("Install marker or manifest missing; only shortcuts and registry were removed.");
        }

        RemoveAppRegistry();
    }

    private static string NormalizeInstallRoot(string selectedPath)
    {
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            throw new InvalidOperationException("Installation folder cannot be empty.");
        }

        string fullPath = Path.GetFullPath(Environment.ExpandEnvironmentVariables(selectedPath.Trim()));
        string trimmed = fullPath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string root = Path.GetPathRoot(fullPath);
        string rootTrimmed = root == null
            ? string.Empty
            : root.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string leaf = Path.GetFileName(trimmed);

        if (string.IsNullOrWhiteSpace(trimmed) ||
            string.Equals(trimmed, rootTrimmed, StringComparison.OrdinalIgnoreCase))
        {
            trimmed = Path.Combine(root, AppName);
        }
        else if (!string.Equals(leaf, AppName, StringComparison.OrdinalIgnoreCase))
        {
            trimmed = Path.Combine(trimmed, AppName);
        }

        string windows = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
        if (IsSameOrChildOf(trimmed, windows))
        {
            throw new InvalidOperationException("Choose a folder outside the Windows system directory.");
        }

        return trimmed;
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
            string targetDirectory = CombineWithRelativePath(source, destination, directory);
            Directory.CreateDirectory(targetDirectory);
        }

        foreach (string file in Directory.GetFiles(source, "*", SearchOption.AllDirectories))
        {
            if (string.Equals(Path.GetFileName(file), "payload.zip", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            string target = CombineWithRelativePath(source, destination, file);
            string parent = Path.GetDirectoryName(target);
            if (!string.IsNullOrWhiteSpace(parent))
            {
                Directory.CreateDirectory(parent);
            }

            File.Copy(file, target, true);
            copiedFiles.Add(GetRelativePath(source, file));
        }

        return copiedFiles;
    }

    private static string CombineWithRelativePath(string root, string destinationRoot, string path)
    {
        return Path.Combine(destinationRoot, GetRelativePath(root, path));
    }

    private static string GetRelativePath(string root, string path)
    {
        string normalizedRoot = Path.GetFullPath(root)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
            Path.DirectorySeparatorChar;
        string normalizedPath = Path.GetFullPath(path);
        if (!normalizedPath.StartsWith(normalizedRoot, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Path is outside payload root: " + path);
        }

        return normalizedPath.Substring(normalizedRoot.Length);
    }

    private static void WriteInstallMarkerAndManifest(
        string installRoot,
        IEnumerable<string> installedFiles,
        string version)
    {
        File.WriteAllText(Path.Combine(installRoot, ".vrperf-installed"), version ?? string.Empty);

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

    private static void RemoveManifestFiles(string installRoot, string manifestPath)
    {
        string rootFullPath = Path.GetFullPath(installRoot)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
            Path.DirectorySeparatorChar;

        foreach (string relativePath in File.ReadAllLines(manifestPath))
        {
            if (string.IsNullOrWhiteSpace(relativePath) || Path.IsPathRooted(relativePath))
            {
                continue;
            }

            string targetPath = Path.GetFullPath(Path.Combine(installRoot, relativePath));
            if (!targetPath.StartsWith(rootFullPath, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            TryDeleteFile(targetPath);
        }
    }

    private static void RemoveEmptyDirectories(string installRoot)
    {
        if (!Directory.Exists(installRoot))
        {
            return;
        }

        foreach (string directory in Directory.GetDirectories(installRoot, "*", SearchOption.AllDirectories))
        {
            RemoveDirectoryIfEmpty(directory);
        }

        RemoveDirectoryIfEmpty(installRoot);
    }

    private static void RemoveDirectoryIfEmpty(string directory)
    {
        try
        {
            if (Directory.Exists(directory) &&
                Directory.GetFileSystemEntries(directory).Length == 0)
            {
                Directory.Delete(directory, false);
            }
        }
        catch (Exception ex)
        {
            Log("Unable to remove directory '" + directory + "': " + ex.Message);
        }
    }

    private static bool IsSameOrChildOf(string candidate, string parent)
    {
        if (string.IsNullOrWhiteSpace(candidate) || string.IsNullOrWhiteSpace(parent))
        {
            return false;
        }

        string candidateFullPath = Path.GetFullPath(candidate)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
            Path.DirectorySeparatorChar;
        string parentFullPath = Path.GetFullPath(parent)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
            Path.DirectorySeparatorChar;
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
                catch (Exception ex)
                {
                    Log("Unable to stop " + processName + ": " + ex.Message);
                }
            }
        }
    }

    private static void CreateShortcuts(string exePath, string installRoot)
    {
        string startMenuDir = GetStartMenuDir();
        Directory.CreateDirectory(startMenuDir);
        CreateShortcut(Path.Combine(startMenuDir, AppName + ".lnk"), exePath, installRoot);
    }

    private static void RemoveShortcuts()
    {
        TryDeleteDirectory(GetStartMenuDir());
    }

    private static string GetStartMenuDir()
    {
        return Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "Microsoft",
            "Windows",
            "Start Menu",
            "Programs",
            AppName);
    }

    private static void CreateShortcut(string shortcutPath, string targetPath, string workingDirectory)
    {
        Type shellType = Type.GetTypeFromProgID("WScript.Shell");
        if (shellType == null)
        {
            throw new InvalidOperationException("WScript.Shell is unavailable.");
        }

        object shell = Activator.CreateInstance(shellType);
        object shortcut = shellType.InvokeMember(
            "CreateShortcut",
            System.Reflection.BindingFlags.InvokeMethod,
            null,
            shell,
            new object[] { shortcutPath });

        Type shortcutType = shortcut.GetType();
        shortcutType.InvokeMember("TargetPath", System.Reflection.BindingFlags.SetProperty, null, shortcut, new object[] { targetPath });
        shortcutType.InvokeMember("WorkingDirectory", System.Reflection.BindingFlags.SetProperty, null, shortcut, new object[] { workingDirectory });
        shortcutType.InvokeMember("IconLocation", System.Reflection.BindingFlags.SetProperty, null, shortcut, new object[] { targetPath });
        shortcutType.InvokeMember("Save", System.Reflection.BindingFlags.InvokeMethod, null, shortcut, null);
    }

    private static void WriteAppRegistry(string installRoot, string version)
    {
        using (RegistryKey key = Registry.CurrentUser.CreateSubKey(AppRegistryKey))
        {
            if (key == null)
            {
                throw new InvalidOperationException("Unable to create app registry key.");
            }

            key.SetValue("InstallLocation", installRoot);
            key.SetValue("Version", version ?? string.Empty);
        }
    }

    private static string ReadInstallLocation()
    {
        using (RegistryKey key = Registry.CurrentUser.OpenSubKey(AppRegistryKey))
        {
            return key == null ? null : key.GetValue("InstallLocation") as string;
        }
    }

    private static void RemoveAppRegistry()
    {
        try
        {
            Registry.CurrentUser.DeleteSubKeyTree(AppRegistryKey, false);
        }
        catch (Exception ex)
        {
            Log("Unable to remove app registry key: " + ex.Message);
        }
    }

    private static void TryDeleteFile(string path)
    {
        try
        {
            if (File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch (Exception ex)
        {
            Log("Unable to remove file '" + path + "': " + ex.Message);
        }
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
        catch (Exception ex)
        {
            Log("Unable to remove directory '" + path + "': " + ex.Message);
        }
    }

    private static void Log(string message)
    {
        try
        {
            File.AppendAllText(
                Path.Combine(Path.GetTempPath(), "VRPerformanceProfiler-msi-helper.log"),
                DateTime.Now.ToString("u") + " " + message + Environment.NewLine);
        }
        catch
        {
        }
    }
}
