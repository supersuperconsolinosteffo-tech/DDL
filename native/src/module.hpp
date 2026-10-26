// module.hpp - module framework. All module logic is C++ and runs ONLY
// inside the tick gate (start of client tick, client thread). The ImGui
// thread never touches Java objects: it flips `desiredEnabled` and edits
// settings (mutex-guarded); the ModuleManager applies transitions at the
// next tick start, where JNI use is safe and correctly timed.
#pragma once

#include <jni.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace uc {

// Snapshot of the local player, refreshed once per tick inside the gate.
// Also published (mutex-guarded copy) for the HUD.
struct PlayerState {
    bool inWorld = false;
    double x = 0, y = 0, z = 0;
    double velX = 0, velY = 0, velZ = 0;
    double fallDistance = 0;
    float yaw = 0, pitch = 0, health = 0;
    int foodLevel = 20;
    bool onGround = false;
    bool horizontalCollision = false;
    bool alive = false;
    bool usingItem = false;
};

// Everything a module needs during its tick-start callback. All jobject
// members are local refs valid only for the current gate invocation.
struct TickContext {
    JNIEnv* env = nullptr;
    jobject client = nullptr;   // MinecraftClient
    jobject player = nullptr;   // ClientPlayerEntity (never null in OnTickStart)
    jobject world = nullptr;    // ClientWorld
    jobject handler = nullptr;  // ClientPlayNetworkHandler
    jobject options = nullptr;  // GameOptions
    PlayerState state{};
    uint64_t tick = 0;
};

enum class Category : int32_t { Movement, Player, Render, Misc, Count };
inline const char* CategoryName(Category c) {
    switch (c) {
        case Category::Movement: return "Movement";
        case Category::Player:   return "Player";
        case Category::Render:   return "Render";
        case Category::Misc:     return "Misc";
        default:                 return "Misc";
    }
}

struct Setting {
    enum class Type { Bool, Float, Int } type = Type::Bool;
    std::string key;
    std::string label;
    bool  boolValue = false;
    float floatValue = 0, floatMin = 0, floatMax = 1;
    int   intValue = 0, intMin = 0, intMax = 10;

    static Setting Bool(std::string key, std::string label, bool dflt) {
        Setting s; s.type = Type::Bool; s.key = std::move(key); s.label = std::move(label); s.boolValue = dflt; return s;
    }
    static Setting Float(std::string key, std::string label, float dflt, float mn, float mx) {
        Setting s; s.type = Type::Float; s.key = std::move(key); s.label = std::move(label);
        s.floatValue = dflt; s.floatMin = mn; s.floatMax = mx; return s;
    }
    static Setting Int(std::string key, std::string label, int dflt, int mn, int mx) {
        Setting s; s.type = Type::Int; s.key = std::move(key); s.label = std::move(label);
        s.intValue = dflt; s.intMin = mn; s.intMax = mx; return s;
    }
};

class Module {
public:
    Module(std::string id, std::string name, std::string description, Category category)
        : id_(std::move(id)), name_(std::move(name)), description_(std::move(description)), category_(category) {}
    virtual ~Module() = default;

    const std::string& Id() const { return id_; }
    const std::string& Name() const { return name_; }
    const std::string& Description() const { return description_; }
    Category GetCategory() const { return category_; }

    // Menu-thread facing state.
    bool DesiredEnabled() const { return desiredEnabled_.load(); }
    void SetDesiredEnabled(bool v) { desiredEnabled_.store(v); }
    // Applied state is owned by the tick gate (ModuleManager).
    bool AppliedEnabled() const { return appliedEnabled_; }

    std::vector<Setting>& Settings() { return settings_; }
    std::mutex& SettingsMutex() { return settingsMutex_; }
    bool  GetBool(const std::string& key, bool dflt = false);
    float GetFloat(const std::string& key, float dflt = 0.f);
    int   GetInt(const std::string& key, int dflt = 0);

    // Lifecycle. OnEnable/OnDisable/OnTickStart run at tick start on the
    // client thread with a live player. Queue packets with
    // PacketQueue::Get().Push(...) - they are sent later in the same gate.
    virtual void OnEnable(TickContext&) {}
    virtual void OnDisable(TickContext&) {}
    virtual void OnTickStart(TickContext&) {}

    // HUD-only modules (no world interaction) set this; render reads it.
    virtual bool IsHudVisible() const { return false; }

private:
    friend class ModuleManager;
    std::string id_, name_, description_;
    Category category_;
    std::atomic<bool> desiredEnabled_{false};
    bool appliedEnabled_ = false;  // tick-gate owned
    std::vector<Setting> settings_;
    std::mutex settingsMutex_;
};

class ModuleManager {
public:
    static ModuleManager& Get();

    void RegisterAll();  // construct built-in modules (idempotent)
    const std::vector<std::unique_ptr<Module>>& All() const { return modules_; }
    Module* Find(const std::string& id);

    // Called by the tick gate (client thread, tick start).
    // Applies pending enable/disable transitions, then ticks applied modules.
    void OnTickGate(TickContext& ctx);
    // Called by the gate when there is no player (title screen/disconnect):
    // applied state is forgotten so modules re-apply cleanly on (re)join.
    void OnNoPlayer();

    // ~/.utilityclient/modules.ini persistence (desired state + settings).
    void LoadState();
    void SaveState();

    // Published snapshot for HUD (any thread).
    PlayerState LatestState();
    void PublishState(const PlayerState& s);

private:
    ModuleManager() = default;
    std::vector<std::unique_ptr<Module>> modules_;
    bool registered_ = false;
    std::mutex stateMutex_;
    PlayerState latestState_{};
};

}  // namespace uc
