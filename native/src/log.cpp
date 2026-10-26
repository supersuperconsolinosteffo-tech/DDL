#include "log.hpp"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>

namespace uc::log {
namespace {
std::mutex g_mutex;
std::ofstream g_file;
std::wstring g_dir;

std::wstring ResolveDir() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH);
    std::wstring base = (n > 0 && n < MAX_PATH) ? buf : L".";
    return base + L"\\.utilityclient";
}

std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return buf;
}
}  // namespace

std::wstring ConfigDir() {
    std::lock_guard lock(g_mutex);
    return g_dir.empty() ? ResolveDir() : g_dir;
}

void Init() {
    std::lock_guard lock(g_mutex);
    g_dir = ResolveDir();
    CreateDirectoryW(g_dir.c_str(), nullptr);
    g_file.open(std::filesystem::path(g_dir + L"\\utilityclient.log"), std::ios::app);
    // Write() must not deadlock: stream directly here.
    const std::string line = "[" + Timestamp() + "] [INFO ] UtilityClient log opened\n";
    if (g_file) g_file << line << std::flush;
    OutputDebugStringA(line.c_str());
}

void Write(const char* level, const std::string& message) {
    const std::string line = "[" + Timestamp() + "] [" + level + "] " + message + "\n";
    {
        std::lock_guard lock(g_mutex);
        if (g_file) g_file << line << std::flush;
    }
    OutputDebugStringA(line.c_str());
}

void Shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_file) g_file.close();
}

}  // namespace uc::log
