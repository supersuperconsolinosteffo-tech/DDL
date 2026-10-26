// imgui_manager.cpp
// ImGui manager implementation

#include "imgui_manager.hpp"
#include "native_client.hpp"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

namespace utilitymod {

ImGuiManager::ImGuiManager() {}

ImGuiManager::~ImGuiManager() {
    shutdown();
}

bool ImGuiManager::initialize() {
    if (initialized_) return true;

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Style (impeccable: clean, readable)
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    // Colors (tasteskill: high contrast, semantic)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.80f, 0.84f, 0.96f, 1.00f);           // TEXT_PRIMARY
    colors[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.60f, 0.70f, 1.00f);    // TEXT_MUTED
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.21f, 0.94f);        // BG_COLOR
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.21f, 0.94f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.21f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.24f, 0.24f, 0.36f, 0.50f);          // BORDER_COLOR
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.28f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.36f, 0.54f);  // HOVER_BG
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.40f, 0.67f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.21f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.21f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.21f, 0.75f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.21f, 0.90f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.14f, 0.14f, 0.21f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.36f, 0.53f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.42f, 0.53f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.36f, 0.48f, 0.53f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);        // ACCENT_COLOR
    colors[ImGuiCol_SliderGrab] = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.75f, 1.00f, 1.00f); // ACCENT_HOVER
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.28f, 0.54f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.24f, 0.36f, 0.54f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.28f, 0.40f, 0.67f);
    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.24f, 0.36f, 0.31f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.36f, 0.54f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.28f, 0.40f, 0.67f);
    colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.24f, 0.36f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.40f, 0.52f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.46f, 0.46f, 0.58f, 0.78f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.24f, 0.36f, 0.31f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.30f, 0.42f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.36f, 0.36f, 0.48f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.21f, 0.86f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.24f, 0.36f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.28f, 0.84f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.14f, 0.14f, 0.21f, 0.82f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.25f, 0.82f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.14f, 0.14f, 0.21f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.26f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.24f, 0.24f, 0.36f, 0.50f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.24f, 0.24f, 0.36f, 0.25f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.24f, 0.36f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.73f);

    // Scale for high DPI
    io.FontGlobalScale = 1.0f;

    // Initialize backends
    if (!initPlatformBackend() || !initRendererBackend()) {
        shutdown();
        return false;
    }

    initialized_ = true;
    NativeClient::instance().log("ImGui manager initialized");
    return true;
}

void ImGuiManager::shutdown() {
    if (!initialized_) return;

    shutdownBackends();

    ImGui::DestroyContext();
    initialized_ = false;
    NativeClient::instance().log("ImGui manager shut down");
}

bool ImGuiManager::initPlatformBackend() {
#if defined(_WIN32)
    // For Windows, we need the window handle
    // In practice, this would be found via hooking or enumeration
    platformWindow_ = GetActiveWindow(); // Placeholder
    if (!platformWindow_) {
        NativeClient::instance().error("No window handle for ImGui Win32 backend");
        return false;
    }

    if (!ImGui_ImplWin32_Init(static_cast<HWND>(platformWindow_))) {
        NativeClient::instance().error("ImGui_ImplWin32_Init failed");
        return false;
    }
    return true;
#else
    // GLFW backend for Linux/macOS
    // platformWindow_ = glfwGetCurrentContext();
    // return ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(platformWindow_), true);
    return false;
#endif
}

bool ImGuiManager::initRendererBackend() {
#if defined(_WIN32)
    // For D3D11, we need the device and context
    // In practice, these would be obtained via hooking IDXGISwapChain::Present
    NativeClient::instance().log("ImGui D3D11 backend: device/context needed from hook");
    // return ImGui_ImplDX11_Init(device, context);
    return false; // Requires actual device from hook
#else
    // OpenGL/Metal backend
    // return ImGui_ImplOpenGL3_Init("#version 330");
    return false;
#endif
}

void ImGuiManager::shutdownBackends() {
#if defined(_WIN32)
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
#else
    // ImGui_ImplOpenGL3_Shutdown();
    // ImGui_ImplGlfw_Shutdown();
#endif
}

void ImGuiManager::newFrame() {
    if (!initialized_) return;

#if defined(_WIN32)
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
#else
    // ImGui_ImplOpenGL3_NewFrame();
    // ImGui_ImplGlfw_NewFrame();
#endif

    ImGui::NewFrame();
}

void ImGuiManager::render() {
    if (!initialized_) return;

    if (menuVisible_) {
        if (renderCallback_) {
            renderCallback_();
        } else {
            renderDefaultMenu();
        }
    }

    ImGui::Render();

#if defined(_WIN32)
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#else
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

    // Handle viewports
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiManager::renderDefaultMenu() {
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("UtilityMod", &menuVisible_, ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    menuVisible_ = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Module List", nullptr, &menuVisible_);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::Text("UtilityMod Native Client");
        ImGui::Separator();

        ImGui::Text("Status: %s", NativeClient::instance().isInitialized() ? "Connected" : "Disconnected");
        ImGui::Text("JNI Bridge: %s", NativeClient::instance().getJniBridge() && NativeClient::instance().getJniBridge()->isInitialized() ? "Ready" : "Not Ready");
        ImGui::Text("Hooks: %s", NativeClient::instance().getHooks() && NativeClient::instance().getHooks()->isInitialized() ? "Active" : "Inactive");

        ImGui::Separator();
        ImGui::Text("Modules:");

        // Module list (synced from Java)
        static const char* moduleNames[] = {
            "AutoWalk", "AutoEat", "AutoSprint", "FreeCam", "NoClip"
        };

        for (int i = 0; i < 5; ++i) {
            NativeClient::ModuleId id = static_cast<NativeClient::ModuleId>(i);
            auto& state = NativeClient::instance().getModuleState(id);

            bool enabled = state.enabled;
            if (ImGui::Checkbox(moduleNames[i], &enabled)) {
                NativeClient::instance().setModuleState(id, enabled);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Toggle %s module", moduleNames[i]);
            }
        }

        ImGui::Separator();
        ImGui::Text("Keybind: Press INSERT to toggle menu");
        ImGui::Text("All actions sent at start of game tick");
        ImGui::Text("Using existing Minecraft packets only");
    }
    ImGui::End();
}

} // namespace utilitymod