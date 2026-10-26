// tick_gate.hpp - the start-of-tick gate. This is the ONLY place module
// logic runs and the ONLY caller of PacketQueue::DrainAtTickStart.
//
// Two possible triggers (never both):
//   Bridge (supported): the tiny Fabric bridge mod calls the JNI method
//     NativeGate.nativeOnStartTick() from ClientTickEvents.START_CLIENT_TICK,
//     i.e. at the head of MinecraftClient.tick(), on the client thread.
//   JvmtiBreakpoint (experimental, config opt-in): a JVMTI breakpoint at
//     method-entry of MinecraftClient.tick() runs the same code. HotSpot may
//     deoptimize that method; use the bridge unless you have a reason.
//
// If neither gate is live, nothing is ever sent. There is deliberately no
// timer/poll/render fallback.
#pragma once

#include <jni.h>

#include <atomic>
#include <cstdint>

namespace uc::tick_gate {

enum class Mode { None, Bridge, JvmtiBreakpoint };

Mode ActiveMode();
bool InGate();                 // true only while RunGate() executes
uint64_t TicksObserved();
bool BridgeSeen();             // bridge class found at least once

// Registers NativeGate natives via a JVMTI class scan. Returns true once
// registered. Bootstrap calls this in a retry loop (game may still load).
bool TryRegisterBridge();

// Installs the JVMTI breakpoint gate (only when config asks for it).
bool TryInstallJvmtiGate();

// Removes natives registration / breakpoint. MUST run before FreeLibrary so
// Java never calls into unloaded code (bridge then gets UnsatisfiedLinkError,
// which it handles).
void Shutdown();
jclass BridgeClassForTest();  // global ref or null

// Shared gate body used by both triggers. Runs at tick start only.
void RunGate(JNIEnv* env);

// Cursor requests from the menu (executed on the client thread in RunGate):
// 0 = none, 1 = unlock (menu opened), 2 = lock (menu closed).
extern std::atomic<int> g_cursorRequest;

// UI-only fallback (no packet involved): if no tick gate has ever run (e.g.
// bridge mod missing), the WndProc - which executes on the game's own
// window thread - applies the cursor change immediately so the menu stays
// clickable even in that degraded mode.
void ApplyCursorNow(bool unlock);

}  // namespace uc::tick_gate
