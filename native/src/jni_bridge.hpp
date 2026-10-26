// jni_bridge.hpp - attachment to the already-running Minecraft JVM and
// cached 1.21.11 (intermediary) class/member refs.
//
// Injection reality this code is built around:
//   * The JVM already exists (javaw.exe). We never JNI_CreateJavaVM and
//     never DestroyJavaVM - we JNI_GetCreatedJavaVMs + AttachCurrentThread.
//   * JNIEnv::FindClass from a freshly attached native thread only sees the
//     system class loader, which cannot see Minecraft/Fabric classes. All
//     game classes are therefore found with JVMTI GetLoadedClasses (by JVMS
//     signature) and cached as global refs, exactly once.
//   * Every JNI call site clears+logs Java exceptions; a mapping mistake
//     disables packet sending, it never crashes the game.
#pragma once

#include <jni.h>
#include <jvmti.h>

#include <cstdint>
#include <string>

#include "module.hpp"  // PlayerState

namespace uc::jni {

// Cached global class refs + member IDs + enum singletons for 1.21.11.
// Filled once by ResolveAll(). `ready` gates every game-touching operation.
struct McRefs {
    bool ready = false;
    bool versionOk = false;
    std::string versionString;

    // Classes (global refs)
    jclass minecraftClient = nullptr, clientPlayer = nullptr, clientWorld = nullptr;
    jclass networkHandler = nullptr, pendingUpdateManager = nullptr;
    jclass entity = nullptr, playerEntity = nullptr, playerAbilities = nullptr;
    jclass playerInventory = nullptr, itemStack = nullptr, gameOptions = nullptr;
    jclass keyBinding = nullptr, simpleOption = nullptr, mouseCls = nullptr;
    jclass moveFull = nullptr, movePos = nullptr, moveLook = nullptr, moveOnGround = nullptr;
    jclass playerActionPacket = nullptr, interactBlockPacket = nullptr, interactItemPacket = nullptr;
    jclass interactEntityPacket = nullptr, handSwingPacket = nullptr, pickFromBlockPacket = nullptr;
    jclass blockPos = nullptr, vec3i = nullptr, vec3d = nullptr, blockHitResult = nullptr;
    jclass javaDouble = nullptr;

    // MinecraftClient
    jmethodID mcGetInstance = nullptr, mcGetGameVersion = nullptr, mcTick = nullptr;
    jfieldID mcPlayer = nullptr, mcWorld = nullptr, mcOptions = nullptr, mcMouse = nullptr, mcCurrentScreen = nullptr;
    // Player / network
    jfieldID cpeNetworkHandler = nullptr;
    jmethodID cpeRequestRespawn = nullptr, netSendPacket = nullptr;
    jmethodID worldGetPendingUpdates = nullptr, worldGetEntityById = nullptr;
    jmethodID pumIncrementSequence = nullptr, pumGetSequence = nullptr;
    // Entity / player
    jmethodID entGetX = nullptr, entGetY = nullptr, entGetZ = nullptr, entGetYaw = nullptr, entGetPitch = nullptr;
    jmethodID entIsOnGround = nullptr, entGetVelocity = nullptr, entSetVelocity = nullptr, entSetSprinting = nullptr;
    jmethodID entIsAlive = nullptr, livIsUsingItem = nullptr, livGetHealth = nullptr;
    jfieldID entFallDistance = nullptr, entNoClip = nullptr, entHorizontalCollision = nullptr;
    jmethodID plGetAbilities = nullptr, plGetHungerManager = nullptr, plGetInventory = nullptr, plSendAbilitiesUpdate = nullptr;
    jfieldID abFlying = nullptr, abAllowFlying = nullptr;
    jmethodID abSetFlySpeed = nullptr, abGetFlySpeed = nullptr, hungerGetFoodLevel = nullptr;
    jmethodID invGetStack = nullptr, invSetSelectedSlot = nullptr, invGetSelectedSlot = nullptr;
    jmethodID stackIsEmpty = nullptr, stackGetUseAction = nullptr;
    jfieldID useActionSerializedName = nullptr;
    // Options / input
    jfieldID optForwardKey = nullptr, optSprintKey = nullptr, optJumpKey = nullptr, optSneakKey = nullptr;
    jmethodID optGetGamma = nullptr, keySetPressed = nullptr, keyIsPressed = nullptr;
    jmethodID simpleOptGetValue = nullptr, simpleOptSetValue = nullptr;
    jfieldID simpleOptValueField = nullptr;
    jmethodID mouseLockCursor = nullptr, mouseUnlockCursor = nullptr, mouseIsCursorLocked = nullptr;
    // Packets / math ctors+factories
    jmethodID ctorMoveFull = nullptr, ctorMovePos = nullptr, ctorMoveLook = nullptr, ctorMoveOnGround = nullptr;
    jmethodID ctorPlayerAction = nullptr, ctorInteractBlock = nullptr, ctorInteractItem = nullptr;
    jmethodID ctorHandSwing = nullptr, ctorPickFromBlock = nullptr;
    jmethodID factoryInteract = nullptr, factoryInteractAt = nullptr, factoryAttack = nullptr;
    jmethodID ctorBlockPosInts = nullptr, ctorBlockPosVec3i = nullptr, ctorVec3i = nullptr;
    jmethodID ctorVec3d = nullptr, ctorBlockHitResult = nullptr;
    jfieldID vec3dX = nullptr, vec3dY = nullptr, vec3dZ = nullptr;
    jmethodID doubleValueOf = nullptr, doubleDoubleValue = nullptr;
    // Enum singletons (global refs)
    jobject handMain = nullptr, handOff = nullptr;
    jobject directions[6] = {};    // Down, Up, North, South, West, East
    jobject playerActions[7] = {}; // PlayerActionKind order
};

JavaVM*& Vm();
jvmtiEnv*& Jvmti();
McRefs& Refs();

// Finds jvm.dll in this process and acquires JavaVM + JVMTI env (1.2).
// Retries are handled by the caller (bootstrap loop).
bool AcquireJvm();

// Per-thread env; attaches if needed. Returns null when no VM.
JNIEnv* Env();
void DetachIfNeeded();

// Logs and clears a pending Java exception. Returns true if one was pending.
bool ClearException(JNIEnv* env, const char* where);

// JVMTI loaded-class scan. Returns a global ref or null. Thread-safe to call
// repeatedly; intended for bootstrap/gate setup, not per-tick use.
jclass FindClassBySignature(JNIEnv* env, const char* internalName);

// Resolves every class/member in McRefs + verifies the game version string
// is 1.21.11. Returns refs.ready. Safe to retry until classes are loaded.
bool ResolveAll(JNIEnv* env);

// Per-tick helpers (tick-gate thread only).
jobject ClientInstance(JNIEnv* env);  // may be null very early
bool ReadTickContext(JNIEnv* env, TickContext& ctx);  // false => no player/world
int32_t NextSequence(JNIEnv* env, jobject world);     // vanilla packet sequence source
std::string JStringToStd(JNIEnv* env, jstring s);

}  // namespace uc::jni
