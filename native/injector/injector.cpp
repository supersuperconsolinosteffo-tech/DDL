// injector.cpp - minimal LoadLibrary injector for UtilityClient.dll.
//
// Usage:
//   UtilityInjector.exe [path-to-UtilityClient.dll] [pid]
// With no arguments it looks for a Minecraft window (javaw.exe/java.exe with
// a window titled "Minecraft*") and injects UtilityClient.dll from next to
// this exe (or the current directory).
//
// This is a plain, non-stealth injector for your own singleplayer game:
// OpenProcess + VirtualAllocEx + CreateRemoteThread(LoadLibraryW).
// Run it as the same user/architecture (x64) as the game; no admin needed
// for your own processes in most setups.
#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

bool IsJavaProcess(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const bool ok = QueryFullProcessImageNameW(h, 0, path, &size) != FALSE;
    CloseHandle(h);
    if (!ok) return false;
    const std::wstring p(path);
    const auto slash = p.find_last_of(L"\\/");
    const std::wstring name = (slash == std::wstring::npos) ? p : p.substr(slash + 1);
    return _wcsicmp(name.c_str(), L"javaw.exe") == 0 || _wcsicmp(name.c_str(), L"java.exe") == 0;
}

struct FindResult { DWORD pid = 0; };

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* result = reinterpret_cast<FindResult*>(lParam);
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[512] = {};
    if (GetWindowTextW(hwnd, title, 512) == 0) return TRUE;
    if (wcsstr(title, L"Minecraft") == nullptr) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid && IsJavaProcess(pid)) {
        result->pid = pid;
        return FALSE;
    }
    return TRUE;
}

DWORD FindMinecraftPid() {
    FindResult result{};
    EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&result));
    return result.pid;
}

bool AlreadyLoaded(DWORD pid, const std::wstring& dllName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Module32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, dllName.c_str()) == 0) { found = true; break; }
        } while (Module32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return found;
}

std::wstring FullPathOf(const std::wstring& path) {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH, buf, nullptr);
    return (n > 0 && n < MAX_PATH) ? buf : path;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring dllPath;
    DWORD pid = 0;
    if (argc >= 2) dllPath = argv[1];
    if (argc >= 3) pid = static_cast<DWORD>(_wtoi(argv[2]));

    if (dllPath.empty()) {
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring dir(exe);
        dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
        dllPath = dir + L"UtilityClient.dll";
        if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) dllPath = L"UtilityClient.dll";
    }
    dllPath = FullPathOf(dllPath);
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wprintf(L"DLL not found: %s\n", dllPath.c_str());
        return 1;
    }
    if (!pid) pid = FindMinecraftPid();
    if (!pid) {
        wprintf(L"No Minecraft (javaw) window found. Start Minecraft 1.21.11 first, or pass a PID.\n");
        return 1;
    }
    const auto slash = dllPath.find_last_of(L"\\/");
    const std::wstring dllName = dllPath.substr(slash + 1);
    if (AlreadyLoaded(pid, dllName)) {
        wprintf(L"%s is already loaded in PID %lu (press END in game to unload first).\n", dllName.c_str(), pid);
        return 1;
    }

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                  PROCESS_VM_WRITE | PROCESS_VM_READ,
                              FALSE, pid);
    if (!proc) {
        wprintf(L"OpenProcess(%lu) failed (error %lu). Same user? x64 game?\n", pid, GetLastError());
        return 1;
    }
    const SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { wprintf(L"VirtualAllocEx failed (%lu)\n", GetLastError()); CloseHandle(proc); return 1; }
    if (!WriteProcessMemory(proc, remote, dllPath.c_str(), bytes, nullptr)) {
        wprintf(L"WriteProcessMemory failed (%lu)\n", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }
    const auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLib, remote, 0, nullptr);
    if (!thread) {
        wprintf(L"CreateRemoteThread failed (%lu)\n", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return 1;
    }
    WaitForSingleObject(thread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (exitCode == 0) {
        wprintf(L"LoadLibrary failed in target. Check bitness (needs x64) and see %%USERPROFILE%%\\.utilityclient\\utilityclient.log\n");
        return 1;
    }
    wprintf(L"Injected %s into Minecraft (PID %lu). In game: INSERT = menu, END = unload.\n", dllName.c_str(), pid);
    return 0;
}
