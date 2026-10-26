// native_client.hpp
// Main native client interface for UtilityMod

#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

namespace utilitymod {

// Forward declarations
class JniBridge;
class MinecraftHooks;
class ImGuiManager;

// Module IDs (must match Java TickActionBridge.MODULE_IDS)
enum class ModuleId : int32_t {
    AutoWalk = 0,
    AutoEat = 1,
    AutoSprint = 2,
    FreeCam = 3,
    NoClip = 4,
    Count = 5
};

// Action IDs (must match Java TickActionBridge.ACTION_IDS)
enum class ActionId : int32_t {
    Enable = 0,
    Disable = 1,
    Toggle = 2,
    SetParam = 3
};

// Module state tracked by native client
struct ModuleState {
    bool enabled = false;
    std::unordered_map<std::string, std::string> parameters;
};

// Configuration for the native client
struct NativeConfig {
    bool enableImgui = true;
    bool enableHooks = true;
    bool enableJniBridge = true;
    std::string jvmLibraryPath; // Path to jvm.dll / libjvm.so
};

// Callback types for Java -> Native communication
using LogCallback = std::function<void(const std::string&)>;
using ErrorCallback = std::function<void(const std::string&)>;
using RequestTickActionCallback = std::function<void(int32_t, int32_t, const std::unordered_map<std::string, std::string>&)>;

// Native client singleton
class NativeClient {
public:
    static NativeClient& instance();

    // Initialize the native client
    bool initialize(const NativeConfig& config);

    // Shutdown the native client
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return initialized_; }

    // Get module state
    const ModuleState& getModuleState(ModuleId id) const;

    // Set module state (called from ImGui or hooks)
    void setModuleState(ModuleId id, bool enabled);

    // Set module parameter
    void setModuleParameter(ModuleId id, const std::string& key, const std::string& value);

    // Request a tick action to be sent to Java (queued for next tick)
    void requestTickAction(ModuleId moduleId, ActionId actionId, const std::unordered_map<std::string, std::string>& params = {});

    // Get the JNI bridge
    JniBridge* getJniBridge() { return jniBridge_.get(); }

    // Get the hook manager
    MinecraftHooks* getHooks() { return hooks_.get(); }

    // Get the ImGui manager
    ImGuiManager* getImGui() { return imgui_.get(); }

    // Set callbacks for Java communication
    void setLogCallback(LogCallback cb) { logCallback_ = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }
    void setRequestTickActionCallback(RequestTickActionCallback cb) { requestTickActionCallback_ = std::move(cb); }

    // Main loop tick (called from hook or separate thread)
    void onTick();

private:
    NativeClient() = default;
    ~NativeClient() = default;

    bool initialized_ = false;
    NativeConfig config_;

    std::unique_ptr<JniBridge> jniBridge_;
    std::unique_ptr<MinecraftHooks> hooks_;
    std::unique_ptr<ImGuiManager> imgui_;

    std::mutex modulesMutex_;
    std::array<ModuleState, static_cast<size_t>(ModuleId::Count)> modules_;

    LogCallback logCallback_;
    ErrorCallback errorCallback_;
    RequestTickActionCallback requestTickActionCallback_;

public:
    // Diagnostics (also used by the JNI entry points and sub-components)
    void log(const std::string& message);
    void error(const std::string& message);

private:
    // Components below forward their diagnostics through log()/error()
    friend class MinecraftHooks;
    friend class ImGuiManager;
};

} // namespace utilitymod