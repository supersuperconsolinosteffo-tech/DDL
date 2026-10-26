#include "hooks.hpp"

#include <windows.h>

#include <MinHook.h>
// NOTE: include order matters - imgui.h pulls in headers that expect a
// C++ include environment already set up; keep backends after MinHook.
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_win32.h>

#include "app_state.hpp"
#include "config.hpp"
#include "log.hpp"
#include "menu.hpp"
#include "module.hpp"
#include "tick_gate.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace uc::hooks {
namespace {
using WglSwapBuffersFn = BOOL(WINAPI*)(HDC);
WglSwapBuffersFn g_original = nullptr;
void* g_target = nullptr;
bool g_installed = false;
bool g_imguiReady = false;
HWND g_hwnd = nullptr;
WNDPROC g_originalWndProc = nullptr;

bool IsMouseMessage(UINT msg) {
    return (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL;
}
bool IsKeyboardMessage(UINT msg) {
    return msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP;
}

LRESULT CALLBACK WndProcDetour(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (static_cast<int>(wParam) == config::Get().menuKey) {
            const bool open = !app::menuOpen.load();
            app::menuOpen.store(open);
            tick_gate::g_cursorRequest.store(open ? 1 : 2);
            tick_gate::ApplyCursorNow(open);  // no-op once the tick gate is live
            if (!open) ModuleManager::Get().SaveState();
            return 0;
        }
        if (static_cast<int>(wParam) == config::Get().unloadKey) {
            app::unloadRequested.store(true);
            return 0;
        }
    }
    if (app::menuOpen.load() && g_imguiReady) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        const ImGuiIO& io = ImGui::GetIO();
        if (IsMouseMessage(msg)) return 0;                       // never click through into the game
        if (IsKeyboardMessage(msg) && io.WantCaptureKeyboard) return 0;
    }
    return CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam);
}

bool InitImGui(HDC hdc) {
    g_hwnd = WindowFromDC(hdc);
    if (!g_hwnd) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // panel positions are deterministic each run
    menu::LoadFonts();
    menu::ApplyStyle();
    if (!ImGui_ImplWin32_Init(g_hwnd)) {
        log::Error("ImGui Win32 backend init failed");
        ImGui::DestroyContext();
        return false;
    }
    // Minecraft 1.21.11 runs on an OpenGL 3.2+ core context (LWJGL).
    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        log::Error("ImGui OpenGL3 backend init failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    g_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProcDetour)));
    if (app::menuOpen.load()) {
        tick_gate::g_cursorRequest.store(1);
        tick_gate::ApplyCursorNow(true);  // covers the no-bridge case; gate re-applies otherwise
    }
    log::Info("ImGui overlay initialized (Win32 + OpenGL3)");
    return true;
}

void ShutdownImGui() {
    if (g_hwnd && g_originalWndProc) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_imguiReady = false;
    app::renderShutdownDone.store(true);
    log::Info("ImGui overlay shut down");
}

BOOL WINAPI DetourWglSwapBuffers(HDC hdc) {
    if (!g_imguiReady) {
        if (!app::unloadRequested.load()) g_imguiReady = InitImGui(hdc);
        return g_original(hdc);
    }
    if (app::unloadRequested.load()) {
        ShutdownImGui();
        return g_original(hdc);
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    menu::RenderFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return g_original(hdc);
}
}  // namespace

bool Installed() { return g_installed; }

bool Install() {
    if (g_installed) return true;
    if (MH_Initialize() != MH_OK) {
        log::Error("MH_Initialize failed");
        return false;
    }
    const HMODULE gl = GetModuleHandleW(L"opengl32.dll");
    if (!gl) {
        log::Warn("opengl32.dll not loaded yet");
        MH_Uninitialize();
        return false;
    }
    g_target = reinterpret_cast<void*>(GetProcAddress(gl, "wglSwapBuffers"));
    if (!g_target) {
        log::Error("wglSwapBuffers not found in opengl32.dll");
        MH_Uninitialize();
        return false;
    }
    if (MH_CreateHook(g_target, reinterpret_cast<void*>(&DetourWglSwapBuffers),
                      reinterpret_cast<void**>(&g_original)) != MH_OK ||
        MH_EnableHook(g_target) != MH_OK) {
        log::Error("MinHook failed to hook wglSwapBuffers");
        MH_Uninitialize();
        return false;
    }
    g_installed = true;
    log::Info("MinHook: wglSwapBuffers hooked");
    return true;
}

void Uninstall() {
    if (!g_installed) return;
    MH_DisableHook(g_target);
    MH_RemoveHook(g_target);
    MH_Uninitialize();
    g_installed = false;
    log::Info("MinHook: hook removed");
}

}  // namespace uc::hooks
