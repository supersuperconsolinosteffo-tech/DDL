// native_client.cpp
// Main native client implementation

#include "native_client.hpp"
#include "jni/jni_bridge.hpp"
#include "hooks/minecraft_hooks.hpp"
#include "imgui_manager.hpp"

#include <iostream>
#include <chrono>

namespace utilitymod {

NativeClient& NativeClient::instance() {
    static NativeClient instance;
    return instance;
}

bool NativeClient::initialize(const NativeConfig& config) {
    if (initialized_) {
        log("NativeClient already initialized");
        return true;
    }

    config_ = config;
    log("Initializing UtilityMod native client...");

    try {
        // Initialize JNI Bridge first (needed for callbacks)
        if (config_.enableJniBridge) {
            jniBridge_ = std::make_unique<JniBridge>();
            if (!jniBridge_->initialize(config_.jvmLibraryPath)) {
                error("Failed to initialize JNI bridge");
                return false;
            }
            log("JNI bridge initialized");
        }

        // Initialize Minecraft hooks
        if (config_.enableHooks) {
            hooks_ = std::make_unique<MinecraftHooks>();
            if (!hooks_->initialize()) {
                error("Failed to initialize Minecraft hooks");
                return false;
            }
            log("Minecraft hooks initialized");
        }

        // Initialize ImGui
        if (config_.enableImgui) {
            imgui_ = std::make_unique<ImGuiManager>();
            if (!imgui_->initialize()) {
                error("Failed to initialize ImGui");
                return false;
            }
            log("ImGui initialized");
        }

        // Initialize module states
        for (auto& module : modules_) {
            module.enabled = false;
            module.parameters.clear();
        }

        initialized_ = true;
        log("UtilityMod native client initialized successfully");
        return true;

    } catch (const std::exception& e) {
        error(std::string("Initialization failed: ") + e.what());
        shutdown();
        return false;
    }
}

void NativeClient::shutdown() {
    if (!initialized_) return;

    log("Shutting down UtilityMod native client...");

    imgui_.reset();
    hooks_.reset();
    jniBridge_.reset();

    initialized_ = false;
    log("UtilityMod native client shut down");
}

const ModuleState& NativeClient::getModuleState(ModuleId id) const {
    static ModuleState empty;
    size_t index = static_cast<size_t>(id);
    if (index < modules_.size()) {
        return modules_[index];
    }
    return empty;
}

void NativeClient::setModuleState(ModuleId id, bool enabled) {
    std::lock_guard<std::mutex> lock(modulesMutex_);
    size_t index = static_cast<size_t>(id);
    if (index < modules_.size()) {
        modules_[index].enabled = enabled;
        // Request the action to be sent to Java at next tick
        requestTickAction(id, enabled ? ActionId::Enable : ActionId::Disable);
    }
}

void NativeClient::setModuleParameter(ModuleId id, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(modulesMutex_);
    size_t index = static_cast<size_t>(id);
    if (index < modules_.size()) {
        modules_[index].parameters[key] = value;
        std::unordered_map<std::string, std::string> params;
        params["param"] = key;
        params["value"] = value;
        requestTickAction(id, ActionId::SetParam, params);
    }
}

void NativeClient::requestTickAction(ModuleId moduleId, ActionId actionId, const std::unordered_map<std::string, std::string>& params) {
    if (requestTickActionCallback_) {
        requestTickActionCallback_(
            static_cast<int32_t>(moduleId),
            static_cast<int32_t>(actionId),
            params
        );
    }
}

void NativeClient::onTick() {
    if (!initialized_) return;

    // Process ImGui frame
    if (imgui_) {
        imgui_->newFrame();
        imgui_->render();
    }

    // Process hooks
    if (hooks_) {
        hooks_->onTick();
    }
}

void NativeClient::log(const std::string& message) {
    if (logCallback_) {
        logCallback_(message);
    }
    std::cout << "[UtilityMod] " << message << std::endl;
}

void NativeClient::error(const std::string& message) {
    if (errorCallback_) {
        errorCallback_(message);
    }
    std::cerr << "[UtilityMod Error] " << message << std::endl;
}

} // namespace utilitymod