// minecraft_hooks.hpp
// MinHook-based hooks for Minecraft client functions

#pragma once

#include <MinHook.h>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <string>
#include <mutex>

namespace utilitymod {

// Forward declaration
class NativeClient;

// Hook manager for Minecraft client functions
class MinecraftHooks {
public:
    MinecraftHooks();
    ~MinecraftHooks();

    // Initialize hooks
    bool initialize();

    // Shutdown hooks
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return initialized_; }

    // Called every tick
    void onTick();

    // Hook a function
    template<typename T>
    bool hookFunction(void* target, void* detour, T** original) {
        if (!initialized_) return false;

        MH_STATUS status = MH_CreateHook(target, detour, reinterpret_cast<void**>(original));
        if (status != MH_OK) {
            log("MH_CreateHook failed: " + std::to_string(status));
            return false;
        }

        status = MH_EnableHook(target);
        if (status != MH_OK) {
            log("MH_EnableHook failed: " + std::to_string(status));
            return false;
        }

        return true;
    }

    // Unhook a function
    bool unhookFunction(void* target) {
        if (!initialized_) return false;

        MH_STATUS status = MH_DisableHook(target);
        if (status != MH_OK) return false;

        status = MH_RemoveHook(target);
        return status == MH_OK;
    }

    // Specific hook functions
    bool hookRender();
    bool hookTick();
    bool hookPacketSend();
    bool hookInput();

    // Callbacks for when hooks are triggered
    using RenderCallback = std::function<void()>;
    using TickCallback = std::function<void()>;
    using PacketSendCallback = std::function<bool(void* packet)>; // Return true to allow, false to block
    using InputCallback = std::function<void(int key, int action)>;

    void setRenderCallback(RenderCallback cb) { renderCallback_ = std::move(cb); }
    void setTickCallback(TickCallback cb) { tickCallback_ = std::move(cb); }
    void setPacketSendCallback(PacketSendCallback cb) { packetSendCallback_ = std::move(cb); }
    void setInputCallback(InputCallback cb) { inputCallback_ = std::move(cb); }

private:
    bool initialized_ = false;

    // Original function pointers
    void* originalPresent_ = nullptr;
    void* originalTick_ = nullptr;
    void* originalSendPacket_ = nullptr;
    void* originalKeyCallback_ = nullptr;

    // Callbacks
    RenderCallback renderCallback_;
    TickCallback tickCallback_;
    PacketSendCallback packetSendCallback_;
    InputCallback inputCallback_;

    // Pattern scanning for finding functions
    uintptr_t findPattern(const std::string& moduleName, const std::string& pattern);
    uintptr_t findPattern(const std::string& pattern);

    void log(const std::string& message);
    void error(const std::string& message);

    // Detour functions (static)
    static void* __stdcall detourPresent(void* device, void* swapChain, uint32_t syncInterval, uint32_t flags);
    static void __stdcall detourTick(void* minecraftClient);
    static bool __stdcall detourSendPacket(void* networkHandler, void* packet);
    static void __stdcall detourKeyCallback(void* window, int key, int scancode, int action, int mods);
};

} // namespace utilitymod