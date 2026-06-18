#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const wchar_t kAppName[] = L"VR Performance Profiler";
const wchar_t kAppExeName[] = L"vr_perf_profiler.exe";
const wchar_t kAppRegistryKey[] = L"Software\\VR Performance Profiler";

std::wstring TrimTrailingSlash(std::wstring value) {
    while (!value.empty() && (value.back() == L'\\' || value.back() == L'/')) {
        value.pop_back();
    }
    return value;
}

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == L'\\' || left.back() == L'/') {
        return left + right;
    }
    return left + L"\\" + right;
}

std::wstring GetFileName(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::wstring GetParentPath(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L"";
    }
    return path.substr(0, slash);
}

std::wstring GetFullPath(const std::wstring& path) {
    DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (required == 0) {
        return path;
    }

    std::wstring result(required, L'\0');
    DWORD written = GetFullPathNameW(path.c_str(), required, &result[0], nullptr);
    if (written == 0 || written >= required) {
        return path;
    }

    result.resize(written);
    return result;
}

bool StartsWithNoCase(const std::wstring& value, const std::wstring& prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    return _wcsnicmp(value.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool IsSameOrChildOf(const std::wstring& candidate, const std::wstring& parent) {
    if (candidate.empty() || parent.empty()) {
        return false;
    }

    std::wstring candidateFull = TrimTrailingSlash(GetFullPath(candidate)) + L"\\";
    std::wstring parentFull = TrimTrailingSlash(GetFullPath(parent)) + L"\\";
    return StartsWithNoCase(candidateFull, parentFull);
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return L"";
    }

    std::wstring result(required, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], required);
    return result;
}

void Log(MSIHANDLE install, const std::wstring& message) {
    PMSIHANDLE record = MsiCreateRecord(1);
    if (record) {
        MsiRecordSetStringW(record, 0, L"[1]");
        MsiRecordSetStringW(record, 1, message.c_str());
        MsiProcessMessage(install, INSTALLMESSAGE_INFO, record);
    }
}

std::wstring GetMsiProperty(MSIHANDLE install, const wchar_t* name) {
    DWORD size = 0;
    UINT result = MsiGetPropertyW(install, name, L"", &size);
    if (result != ERROR_MORE_DATA && result != ERROR_SUCCESS) {
        return L"";
    }

    DWORD bufferSize = size + 1;
    std::wstring value(bufferSize, L'\0');
    result = MsiGetPropertyW(install, name, &value[0], &bufferSize);
    if (result != ERROR_SUCCESS) {
        return L"";
    }

    value.resize(bufferSize);
    return value;
}

std::wstring NormalizeInstallRoot(const std::wstring& selectedPath) {
    if (selectedPath.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        return L"";
    }

    std::wstring fullPath = TrimTrailingSlash(GetFullPath(selectedPath));
    wchar_t rootBuffer[MAX_PATH] = {};
    wcsncpy_s(rootBuffer, fullPath.c_str(), 3);
    std::wstring root = TrimTrailingSlash(rootBuffer);
    std::wstring leaf = GetFileName(fullPath);

    if (fullPath.empty() || _wcsicmp(fullPath.c_str(), root.c_str()) == 0) {
        fullPath = JoinPath(root + L"\\", kAppName);
    } else if (_wcsicmp(leaf.c_str(), kAppName) != 0) {
        fullPath = JoinPath(fullPath, kAppName);
    }

    wchar_t windowsDir[MAX_PATH] = {};
    if (GetWindowsDirectoryW(windowsDir, ARRAYSIZE(windowsDir)) > 0 &&
        IsSameOrChildOf(fullPath, windowsDir)) {
        return L"";
    }

    return fullPath;
}

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    std::wstring full = GetFullPath(path);
    if (full.empty()) {
        return false;
    }

    if (GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    std::wstring parent = GetParentPath(TrimTrailingSlash(full));
    if (!parent.empty() && parent != full) {
        EnsureDirectory(parent);
    }

    return CreateDirectoryW(full.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool DeleteFileIfExists(const std::wstring& path) {
    if (DeleteFileW(path.c_str())) {
        return true;
    }

    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

void RemoveDirectoryIfEmpty(const std::wstring& path) {
    RemoveDirectoryW(path.c_str());
}

std::vector<char> ReadBinaryData(MSIHANDLE install, const std::wstring& binaryName) {
    std::vector<char> data;
    MSIHANDLE database = MsiGetActiveDatabase(install);
    if (!database) {
        return data;
    }

    std::wstring query = L"SELECT `Data` FROM `Binary` WHERE `Name`='" + binaryName + L"'";
    PMSIHANDLE view;
    if (MsiDatabaseOpenViewW(database, query.c_str(), &view) != ERROR_SUCCESS) {
        MsiCloseHandle(database);
        return data;
    }

    if (MsiViewExecute(view, 0) != ERROR_SUCCESS) {
        MsiCloseHandle(database);
        return data;
    }

    PMSIHANDLE record;
    if (MsiViewFetch(view, &record) != ERROR_SUCCESS) {
        MsiCloseHandle(database);
        return data;
    }

    while (true) {
        char buffer[8192];
        DWORD size = sizeof(buffer);
        UINT result = MsiRecordReadStream(record, 1, buffer, &size);
        if (result != ERROR_SUCCESS || size == 0) {
            break;
        }
        data.insert(data.end(), buffer, buffer + size);
    }

    MsiCloseHandle(database);
    return data;
}

bool WriteBinaryToFile(MSIHANDLE install, const std::wstring& binaryName, const std::wstring& targetPath) {
    std::vector<char> data = ReadBinaryData(install, binaryName);
    if (data.empty()) {
        return false;
    }

    std::wstring parent = GetParentPath(targetPath);
    if (!EnsureDirectory(parent)) {
        return false;
    }

    HANDLE file = CreateFileW(
        targetPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    CloseHandle(file);
    return ok && written == data.size();
}

struct PayloadEntry {
    std::wstring binaryId;
    std::wstring relativePath;
};

std::vector<PayloadEntry> ReadManifest(MSIHANDLE install) {
    std::vector<PayloadEntry> entries;
    std::vector<char> data = ReadBinaryData(install, L"PayloadManifest");
    if (data.empty()) {
        return entries;
    }

    if (data.size() >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
        data.erase(data.begin(), data.begin() + 3);
    }

    std::stringstream stream(std::string(data.begin(), data.end()));
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }

        PayloadEntry entry;
        entry.binaryId = Utf8ToWide(line.substr(0, tab));
        entry.relativePath = Utf8ToWide(line.substr(tab + 1));
        if (!entry.binaryId.empty() && !entry.relativePath.empty() &&
            entry.relativePath.find(L"..") == std::wstring::npos) {
            entries.push_back(entry);
        }
    }

    return entries;
}

void StopProcessByName(const wchar_t* processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
                if (process) {
                    TerminateProcess(process, 0);
                    WaitForSingleObject(process, 3000);
                    CloseHandle(process);
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

void StopProcesses() {
    StopProcessByName(L"vr_perf_profiler.exe");
    StopProcessByName(L"VRPerfProfiler.LhmBridge.exe");
}

std::wstring GetStartMenuDir() {
    PWSTR appData = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        result = JoinPath(appData, L"Microsoft\\Windows\\Start Menu\\Programs");
        result = JoinPath(result, kAppName);
        CoTaskMemFree(appData);
    }
    return result;
}

void CreateShortcut(const std::wstring& shortcutPath, const std::wstring& targetPath, const std::wstring& workingDirectory) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninitialize = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        uninitialize = false;
    } else if (FAILED(hr)) {
        return;
    }

    IShellLinkW* link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) {
        link->SetPath(targetPath.c_str());
        link->SetWorkingDirectory(workingDirectory.c_str());
        link->SetIconLocation(targetPath.c_str(), 0);

        IPersistFile* file = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
            file->Save(shortcutPath.c_str(), TRUE);
            file->Release();
        }
        link->Release();
    }

    if (uninitialize) {
        CoUninitialize();
    }
}

void CreateShortcuts(const std::wstring& installRoot) {
    std::wstring startMenuDir = GetStartMenuDir();
    if (startMenuDir.empty()) {
        return;
    }
    EnsureDirectory(startMenuDir);
    CreateShortcut(
        JoinPath(startMenuDir, std::wstring(kAppName) + L".lnk"),
        JoinPath(installRoot, kAppExeName),
        installRoot);
}

void RemoveShortcuts() {
    std::wstring startMenuDir = GetStartMenuDir();
    if (startMenuDir.empty()) {
        return;
    }
    DeleteFileIfExists(JoinPath(startMenuDir, std::wstring(kAppName) + L".lnk"));
    RemoveDirectoryIfEmpty(startMenuDir);
}

void WriteRegistryString(HKEY root, const std::wstring& subkey, const std::wstring& name, const std::wstring& value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(
            key,
            name.c_str(),
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
}

std::wstring ReadRegistryString(HKEY root, const std::wstring& subkey, const std::wstring& name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return L"";
    }

    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(key);
        return L"";
    }

    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name.c_str(), nullptr, nullptr, reinterpret_cast<BYTE*>(&value[0]), &bytes) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return L"";
    }

    RegCloseKey(key);
    if (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

void WriteManifest(const std::wstring& installRoot, const std::vector<PayloadEntry>& entries, const std::wstring& version) {
    std::wofstream marker(JoinPath(installRoot, L".vrperf-installed"), std::ios::trunc);
    marker << version;
    marker.close();

    std::wofstream manifest(JoinPath(installRoot, L"install_manifest.txt"), std::ios::trunc);
    for (const PayloadEntry& entry : entries) {
        if (entry.relativePath.find(L"..") == std::wstring::npos) {
            manifest << entry.relativePath << L"\n";
        }
    }
    manifest << L".vrperf-installed\n";
}

void RemoveManifestFiles(const std::wstring& installRoot) {
    std::wstring manifestPath = JoinPath(installRoot, L"install_manifest.txt");
    std::wifstream manifest(manifestPath);
    std::wstring rootFull = TrimTrailingSlash(GetFullPath(installRoot)) + L"\\";
    std::wstring line;
    while (std::getline(manifest, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (line.empty() || line.find(L"..") != std::wstring::npos) {
            continue;
        }

        std::wstring target = GetFullPath(JoinPath(installRoot, line));
        if (StartsWithNoCase(target, rootFull)) {
            DeleteFileIfExists(target);
        }
    }
    manifest.close();

    DeleteFileIfExists(manifestPath);
    DeleteFileIfExists(JoinPath(installRoot, L".vrperf-installed"));
}

void RemoveEmptyDirectoriesDeep(const std::wstring& root) {
    std::vector<std::wstring> directories;
    std::wstring search = JoinPath(root, L"*");
    WIN32_FIND_DATAW findData = {};
    HANDLE find = FindFirstFileW(search.c_str(), &findData);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(findData.cFileName, L".") != 0 &&
                wcscmp(findData.cFileName, L"..") != 0) {
                std::wstring child = JoinPath(root, findData.cFileName);
                RemoveEmptyDirectoriesDeep(child);
                directories.push_back(child);
            }
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }

    std::sort(directories.rbegin(), directories.rend());
    for (const std::wstring& directory : directories) {
        RemoveDirectoryIfEmpty(directory);
    }
    RemoveDirectoryIfEmpty(root);
}

UINT InstallPayloadImpl(MSIHANDLE install) {
    std::wstring installRoot = NormalizeInstallRoot(GetMsiProperty(install, L"INSTALLFOLDER"));
    if (installRoot.empty()) {
        Log(install, L"InstallPayload: invalid install folder.");
        return ERROR_INSTALL_FAILURE;
    }

    std::vector<PayloadEntry> entries = ReadManifest(install);
    if (entries.empty()) {
        Log(install, L"InstallPayload: payload manifest is empty.");
        return ERROR_INSTALL_FAILURE;
    }

    StopProcesses();
    if (!EnsureDirectory(installRoot)) {
        Log(install, L"InstallPayload: unable to create install folder.");
        return ERROR_INSTALL_FAILURE;
    }

    for (const PayloadEntry& entry : entries) {
        std::wstring target = JoinPath(installRoot, entry.relativePath);
        if (!WriteBinaryToFile(install, entry.binaryId, target)) {
            Log(install, L"InstallPayload: unable to write " + entry.relativePath);
            return ERROR_INSTALL_FAILURE;
        }
    }

    WriteManifest(installRoot, entries, GetMsiProperty(install, L"ProductVersion"));
    CreateShortcuts(installRoot);
    WriteRegistryString(HKEY_CURRENT_USER, kAppRegistryKey, L"InstallLocation", installRoot);
    WriteRegistryString(HKEY_CURRENT_USER, kAppRegistryKey, L"Version", GetMsiProperty(install, L"ProductVersion"));
    Log(install, L"InstallPayload: installed to " + installRoot);
    return ERROR_SUCCESS;
}

UINT UninstallPayloadImpl(MSIHANDLE install) {
    std::wstring installRoot = ReadRegistryString(HKEY_CURRENT_USER, kAppRegistryKey, L"InstallLocation");
    if (installRoot.empty()) {
        installRoot = NormalizeInstallRoot(GetMsiProperty(install, L"INSTALLFOLDER"));
    }

    if (installRoot.empty()) {
        Log(install, L"UninstallPayload: install folder is unknown.");
        return ERROR_SUCCESS;
    }

    StopProcesses();
    RemoveShortcuts();
    RemoveManifestFiles(installRoot);
    RemoveEmptyDirectoriesDeep(installRoot);
    RegDeleteTreeW(HKEY_CURRENT_USER, kAppRegistryKey);
    Log(install, L"UninstallPayload: removed from " + installRoot);
    return ERROR_SUCCESS;
}

}  // namespace

extern "C" __declspec(dllexport) UINT __stdcall InstallPayload(MSIHANDLE install) {
    return InstallPayloadImpl(install);
}

extern "C" __declspec(dllexport) UINT __stdcall UninstallPayload(MSIHANDLE install) {
    return UninstallPayloadImpl(install);
}
