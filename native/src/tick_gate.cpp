#include "tick_gate.hpp"

#include <cstring>

#include "config.hpp"
#include "jni_bridge.hpp"
#include "log.hpp"
#include "module.hpp"
#include "packet_queue.hpp"

namespace uc::tick_gate {
std::atomic<int> g_cursorRequest{0};

namespace {
Mode g_mode = Mode::None;
std::atomic<bool> g_inGate{false};
std::atomic<uint64_t> g_ticks{0};
std::atomic<bool> g_bridgeSeen{false};
jclass g_bridgeClass = nullptr;  // global ref
bool g_breakpointInstalled = false;

void ApplyCursor(JNIEnv* env, int req) {
    jni::McRefs& r = jni::Refs();
    if (!r.ready) return;
    jobject client = jni::ClientInstance(env);
    if (!client) return;
    jobject mouse = env->GetObjectField(client, r.mcMouse);
    if (mouse) {
        if (req == 1) {
            env->CallVoidMethod(mouse, r.mouseUnlockCursor);
        } else if (req == 2) {
            // Only re-grab the cursor for gameplay: player present, no screen open.
            jobject player = env->GetObjectField(client, r.mcPlayer);
            jobject screen = env->GetObjectField(client, r.mcCurrentScreen);
            if (player && !screen) env->CallVoidMethod(mouse, r.mouseLockCursor);
            if (player) env->DeleteLocalRef(player);
            if (screen) env->DeleteLocalRef(screen);
        }
        jni::ClearException(env, "cursor request");
        env->DeleteLocalRef(mouse);
    }
    env->DeleteLocalRef(client);
}

void ConsumeCursorRequest(JNIEnv* env) {
    const int req = g_cursorRequest.exchange(0);
    if (req != 0) ApplyCursor(env, req);
}
}  // namespace

void ApplyCursorNow(bool unlock) {
    if (g_ticks.load() != 0) return;  // gate is live; it applies requests itself
    JNIEnv* env = jni::Env();
    if (!env) return;
    if (!jni::Refs().ready) jni::ResolveAll(env);
    if (jni::Refs().ready) ApplyCursor(env, unlock ? 1 : 2);
}

Mode ActiveMode() { return g_mode; }
bool InGate() { return g_inGate.load(); }
uint64_t TicksObserved() { return g_ticks.load(); }
bool BridgeSeen() { return g_bridgeSeen.load(); }
jclass BridgeClassForTest() { return g_bridgeClass; }

void RunGate(JNIEnv* env) {
    if (g_inGate.exchange(true)) return;  // re-entrancy guard
    struct Guard { ~Guard() { g_inGate.store(false); } } guard;
    g_ticks.fetch_add(1);

    if (!jni::Refs().ready) jni::ResolveAll(env);
    if (!jni::Refs().ready) return;
    ConsumeCursorRequest(env);

    if (env->PushLocalFrame(128) < 0) {
        jni::ClearException(env, "PushLocalFrame");
        return;
    }
    TickContext ctx;
    ctx.tick = g_ticks.load();
    if (!jni::ReadTickContext(env, ctx)) {
        ModuleManager::Get().OnNoPlayer();
        PlayerState empty{};
        ModuleManager::Get().PublishState(empty);
        PacketQueue::Get().Clear();
        env->PopLocalFrame(nullptr);
        return;
    }
    ModuleManager::Get().PublishState(ctx.state);
    if (jni::Refs().versionOk) {
        ModuleManager::Get().OnTickGate(ctx);
        PacketQueue::Get().DrainAtTickStart(env, ctx.player, ctx.world, ctx.handler);
    } else {
        PacketQueue::Get().Clear();
    }
    env->PopLocalFrame(nullptr);
}

namespace {
extern "C" JNIEXPORT void JNICALL NativeOnStartTick(JNIEnv* env, jclass) { RunGate(env); }
extern "C" JNIEXPORT jboolean JNICALL NativeIsInjected(JNIEnv*, jclass) { return JNI_TRUE; }

void JNICALL OnBreakpoint(jvmtiEnv*, JNIEnv* env, jthread, jmethodID method, jlocation) {
    if (method == jni::Refs().mcTick) RunGate(env);
}
}  // namespace

bool TryRegisterBridge() {
    if (g_bridgeClass) return true;
    JNIEnv* env = jni::Env();
    if (!env || !jni::Jvmti()) return false;
    jclass cls = jni::FindClassBySignature(env, "com/utilityclient/bridge/NativeGate");
    if (!cls) return false;
    static const JNINativeMethod kMethods[] = {
        {const_cast<char*>("nativeOnStartTick"), const_cast<char*>("()V"),
         reinterpret_cast<void*>(&NativeOnStartTick)},
        {const_cast<char*>("nativeIsInjected"), const_cast<char*>("()Z"),
         reinterpret_cast<void*>(&NativeIsInjected)},
    };
    if (env->RegisterNatives(cls, kMethods, 2) != JNI_OK) {
        jni::ClearException(env, "RegisterNatives(NativeGate)");
        env->DeleteGlobalRef(cls);
        return false;
    }
    g_bridgeClass = cls;
    g_bridgeSeen.store(true);
    g_mode = Mode::Bridge;
    log::Info("Tick gate live: Fabric bridge (START_CLIENT_TICK)");
    return true;
}

bool TryInstallJvmtiGate() {
    if (g_breakpointInstalled) return true;
    if (g_bridgeClass) return false;  // bridge wins; never both
    JNIEnv* env = jni::Env();
    jvmtiEnv* jvmti = jni::Jvmti();
    if (!env || !jvmti) return false;
    if (!jni::Refs().ready) jni::ResolveAll(env);
    if (!jni::Refs().ready || !jni::Refs().mcTick) return false;

    jvmtiCapabilities caps{};
    caps.can_generate_breakpoint_events = 1;
    if (jvmti->AddCapabilities(&caps) != JVMTI_ERROR_NONE) {
        log::Error("JVMTI breakpoint capability refused in live phase; use the bridge mod");
        return false;
    }
    jvmtiEventCallbacks callbacks{};
    callbacks.Breakpoint = &OnBreakpoint;
    if (jvmti->SetEventCallbacks(&callbacks, static_cast<jint>(sizeof(callbacks))) != JVMTI_ERROR_NONE ||
        jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, nullptr) != JVMTI_ERROR_NONE ||
        jvmti->SetBreakpoint(jni::Refs().mcTick, 0) != JVMTI_ERROR_NONE) {
        log::Error("JVMTI breakpoint gate install failed; use the bridge mod");
        return false;
    }
    g_breakpointInstalled = true;
    g_mode = Mode::JvmtiBreakpoint;
    log::Warn("Tick gate live: JVMTI breakpoint on MinecraftClient.tick() (experimental)");
    return true;
}

void Shutdown() {
    JNIEnv* env = jni::Env();
    if (g_breakpointInstalled) {
        jvmtiEnv* jvmti = jni::Jvmti();
        if (jvmti) {
            jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_BREAKPOINT, nullptr);
            if (jni::Refs().mcTick) jvmti->ClearBreakpoint(jni::Refs().mcTick, 0);
            jvmtiEventCallbacks none{};
            jvmti->SetEventCallbacks(&none, static_cast<jint>(sizeof(none)));
        }
        g_breakpointInstalled = false;
    }
    if (g_bridgeClass && env) {
        env->UnregisterNatives(g_bridgeClass);
        jni::ClearException(env, "UnregisterNatives");
        env->DeleteGlobalRef(g_bridgeClass);
        g_bridgeClass = nullptr;
    }
    g_mode = Mode::None;
    log::Info("Tick gate shut down");
}

}  // namespace uc::tick_gate
