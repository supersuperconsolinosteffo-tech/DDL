// imgui_manager.hpp
// ImGui integration for the native client

#pragma once

#include <imgui.h>
#include <functional>
#include <string>
#include <vector>

namespace utilitymod {

class NativeClient;

class ImGuiManager {
public:
    ImGuiManager();
    ~ImGuiManager();

    // Initialize ImGui context and backend
    bool initialize();

    // Shutdown ImGui
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return initialized_; }

    // Start a new ImGui frame
    void newFrame();

    // Render ImGui draw data
    void render();

    // Show/hide the menu
    void setMenuVisible(bool visible) { menuVisible_ = visible; }
    bool isMenuVisible() const { return menuVisible_; }
    void toggleMenu() { menuVisible_ = !menuVisible_; }

    // Set callback for rendering custom UI
    using RenderCallback = std::function<void()>;
    void setRenderCallback(RenderCallback cb) { renderCallback_ = std::move(cb); }

    // Get ImGui IO
    ImGuiIO& getIO() { return ImGui::GetIO(); }

private:
    bool initialized_ = false;
    bool menuVisible_ = true;
    RenderCallback renderCallback_;

    // Platform/renderer specific data
    void* platformWindow_ = nullptr;
    void* rendererDevice_ = nullptr;

    // Initialize platform backend (Win32/GLFW)
    bool initPlatformBackend();

    // Initialize renderer backend (D3D11/OpenGL/Metal)
    bool initRendererBackend();

    // Shutdown backends
    void shutdownBackends();

    // Default menu rendering
    void renderDefaultMenu();
};

} // namespace utilitymod