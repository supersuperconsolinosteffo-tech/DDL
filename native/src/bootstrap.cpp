// bootstrap.cpp - DLL lifetime: attach to JVM, resolve mappings, install
// the render hook + tick gate, then idle until unload (END key) and tear
// everything down in an order that never calls into freed code:
//   1. stop the tick gate (Java can no longer call us / no new JNI work)
//   2. render thread destroys ImGui on its next frame, restores WndProc
//   3. MinHook hook removed
//   4. detach thread, FreeLibraryAndExitThread
#include <windows.h>

#include <cstdio>

#include "app_state.hpp"
#include "config.hpp"
#include "hooks.hpp"
#include "jni_bridge.hpp"
#include "log.hpp"
#include "module.hpp"
#include "tick_gate.hpp"

namespace uc {
namespace {

bool WaitFor(const char* what, bool (*fn)()) {
    int attempts = 0;
    while (!app::unloadRequested.load()) {
        if (fn()) return true;
        if (++attempts == 40) log::Warn(std::string("Still waiting for ") + what + " (game loading?)");
        Sleep(500);
    }
    return false;
}

}  // namespace

DWORD WINAPI BootstrapThread(LPVOID) {
    log::Init();
    log::Info("UtilityClient injecting (built for Minecraft 1.21.11, singleplayer/test servers)");
    config::Load();
    if (config::Get().debugConsole) {
        if (AllocConsole()) {
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
        }
    }

    ModuleManager::Get().RegisterAll();
    ModuleManager::Get().LoadState();

    if (!WaitFor("JVM", [] { return jni::AcquireJvm(); })) goto shutdown;
    if (!WaitFor("OpenGL hook", [] { return hooks::Install(); })) goto shutdown;
    if (!WaitFor("1.21.11 mappings", [] {
            JNIEnv* env = jni::Env();
            return env && jni::ResolveAll(env);
        })) goto shutdown;

    if (config::Get().jvmtiTickGate) {
        WaitFor("JVMTI tick gate", [] { return tick_gate::TryInstallJvmtiGate(); });
    } else {
        if (WaitFor("Fabric bridge mod", [] { return tick_gate::TryRegisterBridge(); })) {
            log::Info("Ready. INSERT = menu, END = unload.");
        }
    }

    // Main wait: bridge may still appear later (e.g. mod added after launch
    // is impossible, but a slow title screen is not). Keep trying cheaply.
    while (!app::unloadRequested.load()) {
        if (!config::Get().jvmtiTickGate && tick_gate::ActiveMode() == tick_gate::Mode::None) {
            tick_gate::TryRegisterBridge();
        }
        Sleep(250);
    }

shutdown:
    log::Info("Unloading UtilityClient...");
    ModuleManager::Get().SaveState();
    tick_gate::Shutdown();
    for (int i = 0; i < 300 && !app::renderShutdownDone.load() && hooks::Installed(); ++i) Sleep(10);
    hooks::Uninstall();
    Sleep(50);  // let any in-flight detour return
    jni::DetachIfNeeded();
    log::Info("UtilityClient unloaded cleanly");
    log::Shutdown();
    HMODULE self = app::dllModule;
    if (self) FreeLibraryAndExitThread(self, 0);
    return 0;
}

}  // namespace uc

// Verify-me export: `dumpbin /exports UtilityClient.dll` or the injector
// can confirm this is really our DLL before/after injection.
extern "C" __declspec(dllexport) const char* UtilityClientVersion() {
    return "UtilityClient 1.0.0 (Minecraft 1.21.11)";
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        uc::app::dllModule = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, &uc::BootstrapThread, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
