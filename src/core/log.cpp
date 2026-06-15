#include "core/log.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <mutex>

namespace vrperf {

namespace {

std::mutex g_logMutex;

std::filesystem::path LogPath()
{
    wchar_t localAppData[MAX_PATH] = {};
    const auto length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        localAppData,
        MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"vrperf.log";
    }

    auto dir = std::filesystem::path(localAppData) / L"VRPerfProfiler";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / L"vrperf.log";
}

std::string Timestamp()
{
    SYSTEMTIME time = {};
    GetLocalTime(&time);

    char buffer[64] = {};
    sprintf_s(
        buffer,
        "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds);
    return buffer;
}

} // namespace

void LogInfo(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream file(LogPath(), std::ios::app);
    if (!file.is_open()) {
        return;
    }

    file << Timestamp() << " " << message << '\n';
}

} // namespace vrperf
