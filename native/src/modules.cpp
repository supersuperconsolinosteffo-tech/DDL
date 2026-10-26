// modules.cpp - built-in singleplayer utility modules (all C++, JNI/JVMTI
// resolved refs only; every action either (a) is queued and sent as an
// existing vanilla packet at tick start, or (b) adjusts client state and
// lets vanilla send its own normal packet - never a custom packet).
#include "jni_bridge.hpp"
#include "log.hpp"
#include "module.hpp"
#include "packet_queue.hpp"

namespace uc {
namespace {

void Clear(JNIEnv* env, const char* where) { jni::ClearException(env, where); }

jobject KeyOf(TickContext& ctx, jfieldID fid) {
    if (!ctx.options || !fid) return nullptr;
    jobject key = ctx.env->GetObjectField(ctx.options, fid);
    Clear(ctx.env, "KeyOf");
    return key;
}

void SetKey(TickContext& ctx, jfieldID fid, bool pressed) {
    jobject key = KeyOf(ctx, fid);
    if (!key) return;
    ctx.env->CallVoidMethod(key, jni::Refs().keySetPressed, static_cast<jboolean>(pressed));
    Clear(ctx.env, "KeyBinding.setPressed");
    ctx.env->DeleteLocalRef(key);
}

bool KeyPressed(TickContext& ctx, jfieldID fid) {
    jobject key = KeyOf(ctx, fid);
    if (!key) return false;
    const bool p = ctx.env->CallBooleanMethod(key, jni::Refs().keyIsPressed) != JNI_FALSE;
    Clear(ctx.env, "KeyBinding.isPressed");
    ctx.env->DeleteLocalRef(key);
    return p;
}

jobject AbilitiesOf(TickContext& ctx) {
    jobject ab = ctx.env->CallObjectMethod(ctx.player, jni::Refs().plGetAbilities);
    Clear(ctx.env, "getAbilities");
    return ab;
}

void SendAbilitiesUpdate(TickContext& ctx) {
    // Vanilla method; it sends the vanilla PlayerAbilities packet itself.
    ctx.env->CallVoidMethod(ctx.player, jni::Refs().plSendAbilitiesUpdate);
    Clear(ctx.env, "sendAbilitiesUpdate");
}

// Holds the forward key down each tick. Vanilla input/physics/sendMovement
// packets then run completely normally later in the same tick.
class AutoWalk final : public Module {
public:
    AutoWalk() : Module("autowalk", "AutoWalk", "Holds the forward key for you.", Category::Movement) {}
    void OnDisable(TickContext& ctx) override { SetKey(ctx, jni::Refs().optForwardKey, false); }
    void OnTickStart(TickContext& ctx) override { SetKey(ctx, jni::Refs().optForwardKey, true); }
};

// Registration order matters: AutoWalk runs first, so its synthetic forward
// press is visible here through the vanilla KeyBinding state.
class AutoSprint final : public Module {
public:
    AutoSprint() : Module("autosprint", "AutoSprint", "Holds sprint while moving forward.", Category::Movement) {
        Settings().push_back(Setting::Bool("require_forward", "Require forward", true));
    }
    void OnDisable(TickContext& ctx) override { SetKey(ctx, jni::Refs().optSprintKey, false); }
    void OnTickStart(TickContext& ctx) override {
        if (GetBool("require_forward", true) && !KeyPressed(ctx, jni::Refs().optForwardKey)) return;
        SetKey(ctx, jni::Refs().optSprintKey, true);
    }
};

class Fly final : public Module {
public:
    Fly() : Module("fly", "Fly", "Creative flight via the player's abilities (vanilla abilities packet).", Category::Movement) {
        Settings().push_back(Setting::Float("speed", "Fly speed", 0.05f, 0.01f, 0.30f));
    }
    void OnEnable(TickContext& ctx) override {
        jobject ab = AbilitiesOf(ctx);
        if (!ab) return;
        const jni::McRefs& r = jni::Refs();
        savedAllow_ = ctx.env->GetBooleanField(ab, r.abAllowFlying) != JNI_FALSE;
        savedFlying_ = ctx.env->GetBooleanField(ab, r.abFlying) != JNI_FALSE;
        savedSpeed_ = ctx.env->CallFloatMethod(ab, r.abGetFlySpeed);
        haveSaved_ = true;
        ctx.env->SetBooleanField(ab, r.abAllowFlying, JNI_TRUE);
        ctx.env->SetBooleanField(ab, r.abFlying, JNI_TRUE);
        ctx.env->CallVoidMethod(ab, r.abSetFlySpeed, GetFloat("speed", 0.05f));
        Clear(ctx.env, "Fly.OnEnable");
        ctx.env->DeleteLocalRef(ab);
        SendAbilitiesUpdate(ctx);
    }
    void OnDisable(TickContext& ctx) override {
        if (!haveSaved_) return;
        jobject ab = AbilitiesOf(ctx);
        if (!ab) return;
        const jni::McRefs& r = jni::Refs();
        ctx.env->SetBooleanField(ab, r.abAllowFlying, static_cast<jboolean>(savedAllow_));
        ctx.env->SetBooleanField(ab, r.abFlying, static_cast<jboolean>(savedFlying_));
        ctx.env->CallVoidMethod(ab, r.abSetFlySpeed, savedSpeed_);
        Clear(ctx.env, "Fly.OnDisable");
        ctx.env->DeleteLocalRef(ab);
        haveSaved_ = false;
        SendAbilitiesUpdate(ctx);
    }
    void OnTickStart(TickContext& ctx) override {
        jobject ab = AbilitiesOf(ctx);
        if (!ab) return;
        const jni::McRefs& r = jni::Refs();
        const float want = GetFloat("speed", 0.05f);
        if (ctx.env->CallFloatMethod(ab, r.abGetFlySpeed) != want) {
            ctx.env->CallVoidMethod(ab, r.abSetFlySpeed, want);
            Clear(ctx.env, "Fly.OnTickStart");
        }
        ctx.env->DeleteLocalRef(ab);
    }
private:
    bool haveSaved_ = false, savedAllow_ = false, savedFlying_ = false;
    float savedSpeed_ = 0.05f;
};

class NoClip final : public Module {
public:
    NoClip() : Module("noclip", "NoClip", "Pass through blocks (client collision off). Use with Fly.", Category::Movement) {}
    void OnDisable(TickContext& ctx) override { ctx.env->SetBooleanField(ctx.player, jni::Refs().entNoClip, JNI_FALSE); }
    void OnTickStart(TickContext& ctx) override {
        ctx.env->SetBooleanField(ctx.player, jni::Refs().entNoClip, JNI_TRUE);
        Clear(ctx.env, "NoClip");
    }
};

class NoFall final : public Module {
public:
    NoFall() : Module("nofall", "NoFall", "Queues a vanilla OnGroundOnly move packet while falling (best effort; vanilla still sends its own move packet).", Category::Movement) {
        Settings().push_back(Setting::Float("threshold", "Distance", 2.5f, 0.5f, 10.0f));
    }
    void OnTickStart(TickContext& ctx) override {
        const PlayerState& s = ctx.state;
        if (s.onGround || s.velY >= 0.0) return;
        if (s.fallDistance <= GetFloat("threshold", 2.5f)) return;
        if (ctx.tick - lastTick_ < 2) return;
        lastTick_ = ctx.tick;
        PacketQueue::Get().Push(MoveOnGround(true, s.horizontalCollision));
        ctx.env->SetDoubleField(ctx.player, jni::Refs().entFallDistance, 0.0);
        Clear(ctx.env, "NoFall");
    }
private:
    uint64_t lastTick_ = 0;
};

class AutoEat final : public Module {
public:
    AutoEat() : Module("autoeat", "AutoEat", "Eats food from the hotbar when hungry (vanilla interact-item packet).", Category::Player) {
        Settings().push_back(Setting::Int("hunger", "Eat at hunger", 14, 1, 19));
        Settings().push_back(Setting::Bool("allow_drinks", "Allow drinks", false));
    }
    void OnTickStart(TickContext& ctx) override {
        if (ctx.state.foodLevel > GetInt("hunger", 14)) return;
        if (ctx.state.usingItem) return;
        if (ctx.tick - lastTick_ < 34) return;  // one eat animation
        JNIEnv* env = ctx.env;
        const jni::McRefs& r = jni::Refs();
        jobject inv = env->CallObjectMethod(ctx.player, r.plGetInventory);
        Clear(env, "AutoEat.getInventory");
        if (!inv) return;
        const bool drinks = GetBool("allow_drinks", false);
        int foodSlot = -1;
        for (int i = 0; i < 9 && foodSlot < 0; ++i) {
            jobject stack = env->CallObjectMethod(inv, r.invGetStack, i);
            if (!stack) continue;
            if (env->CallBooleanMethod(stack, r.stackIsEmpty) == JNI_FALSE) {
                jobject ua = env->CallObjectMethod(stack, r.stackGetUseAction);
                if (ua) {
                    auto named = static_cast<jstring>(env->GetObjectField(ua, r.useActionSerializedName));
                    const std::string name = jni::JStringToStd(env, named);
                    if (name == "eat" || (drinks && name == "drink")) foodSlot = i;
                    if (named) env->DeleteLocalRef(named);
                    env->DeleteLocalRef(ua);
                }
            }
            env->DeleteLocalRef(stack);
            Clear(env, "AutoEat.scan");
        }
        if (foodSlot >= 0) {
            if (env->CallIntMethod(inv, r.invGetSelectedSlot) != foodSlot) {
                env->CallVoidMethod(inv, r.invSetSelectedSlot, foodSlot);
                Clear(env, "AutoEat.setSelectedSlot");
            }
            PacketQueue::Get().Push(InteractItem(0, ctx.state.yaw, ctx.state.pitch));
            lastTick_ = ctx.tick;
        }
        env->DeleteLocalRef(inv);
    }
private:
    uint64_t lastTick_ = 0;
};

class AutoRespawn final : public Module {
public:
    AutoRespawn() : Module("autorespawn", "AutoRespawn", "Respawns automatically (vanilla client-status flow).", Category::Player) {
        Settings().push_back(Setting::Int("delay", "Delay ticks", 10, 0, 100));
    }
    void OnTickStart(TickContext& ctx) override {
        if (ctx.state.alive) { deadSince_ = 0; return; }
        if (deadSince_ == 0) deadSince_ = ctx.tick;
        if (ctx.tick - deadSince_ >= static_cast<uint64_t>(GetInt("delay", 10))) {
            ctx.env->CallVoidMethod(ctx.player, jni::Refs().cpeRequestRespawn);
            Clear(ctx.env, "AutoRespawn");
            deadSince_ = ctx.tick;
        }
    }
private:
    uint64_t deadSince_ = 0;
};

class FullBright final : public Module {
public:
    FullBright() : Module("fullbright", "FullBright", "Raises gamma beyond the video-settings range. Client-side only, no packets.", Category::Render) {
        Settings().push_back(Setting::Float("brightness", "Brightness", 10.0f, 1.0f, 15.0f));
    }
    void OnDisable(TickContext& ctx) override { if (haveSaved_) { Apply(ctx, saved_); haveSaved_ = false; } }
    void OnTickStart(TickContext& ctx) override { Apply(ctx, GetFloat("brightness", 10.0f)); }
private:
    void Apply(TickContext& ctx, double value) {
        JNIEnv* env = ctx.env;
        const jni::McRefs& r = jni::Refs();
        if (!ctx.options) return;
        jobject gamma = env->CallObjectMethod(ctx.options, r.optGetGamma);
        if (!gamma) { Clear(env, "FullBright.getGamma"); return; }
        if (!haveSaved_) {
            jobject cur = env->CallObjectMethod(gamma, r.simpleOptGetValue);
            if (cur) { saved_ = env->CallDoubleMethod(cur, r.doubleDoubleValue); env->DeleteLocalRef(cur); }
            haveSaved_ = true;
        }
        // SimpleOption.setValue would clamp to the slider range; writing the
        // value field directly is how every full-bright implementation works.
        jobject boxed = env->CallStaticObjectMethod(r.javaDouble, r.doubleValueOf, value);
        if (boxed) {
            env->SetObjectField(gamma, r.simpleOptValueField, boxed);
            env->DeleteLocalRef(boxed);
        }
        Clear(env, "FullBright.apply");
        env->DeleteLocalRef(gamma);
    }
    bool haveSaved_ = false;
    double saved_ = 0.5;
};

class Hud final : public Module {
public:
    Hud() : Module("hud", "HUD", "Shows coordinates, FPS and the active-module list. Render-only, no packets.", Category::Render) {}
    bool IsHudVisible() const override { return DesiredEnabled(); }
};

}  // namespace

void ModuleManager::RegisterAll() {
    if (registered_) return;
    registered_ = true;
    // Order is execution order inside the gate (see AutoSprint note).
    modules_.push_back(std::make_unique<AutoWalk>());
    modules_.push_back(std::make_unique<AutoSprint>());
    modules_.push_back(std::make_unique<Fly>());
    modules_.push_back(std::make_unique<NoClip>());
    modules_.push_back(std::make_unique<NoFall>());
    modules_.push_back(std::make_unique<AutoEat>());
    modules_.push_back(std::make_unique<AutoRespawn>());
    modules_.push_back(std::make_unique<FullBright>());
    modules_.push_back(std::make_unique<Hud>());
    log::Info("Registered " + std::to_string(modules_.size()) + " modules");
}

}  // namespace uc
