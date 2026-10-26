#include "jni_bridge.hpp"

#include <windows.h>

#include <cstring>
#include <vector>

#include "log.hpp"
#include "mappings_12111.hpp"

namespace uc::jni {
namespace {
JavaVM* g_vm = nullptr;
jvmtiEnv* g_jvmti = nullptr;
McRefs g_refs;

jmethodID Method(JNIEnv* env, jclass cls, const mapping::NamedDesc& nd, bool isStatic, const char* label) {
    if (!cls) return nullptr;
    jmethodID mid = isStatic ? env->GetStaticMethodID(cls, nd.name, nd.desc)
                             : env->GetMethodID(cls, nd.name, nd.desc);
    if (!mid) {
        ClearException(env, label);
        log::Error(std::string("Mapping missing (method): ") + label + " " + nd.name + " " + nd.desc);
    }
    return mid;
}

jfieldID Field(JNIEnv* env, jclass cls, const mapping::NamedDesc& nd, const char* label, bool isStatic = false) {
    if (!cls) return nullptr;
    jfieldID fid = isStatic ? env->GetStaticFieldID(cls, nd.name, nd.desc)
                            : env->GetFieldID(cls, nd.name, nd.desc);
    if (!fid) {
        ClearException(env, label);
        log::Error(std::string("Mapping missing (field): ") + label + " " + nd.name + " " + nd.desc);
    }
    return fid;
}

jobject EnumConstant(JNIEnv* env, jclass cls, const mapping::NamedDesc& nd, const char* label) {
    jfieldID fid = Field(env, cls, nd, label, true);
    if (!fid) return nullptr;
    jobject v = env->GetStaticObjectField(cls, fid);
    if (!v) { ClearException(env, label); return nullptr; }
    jobject global = env->NewGlobalRef(v);
    env->DeleteLocalRef(v);
    return global;
}

bool Present(const void* p) { return p != nullptr; }
}  // namespace

JavaVM*& Vm() { return g_vm; }
jvmtiEnv*& Jvmti() { return g_jvmti; }
McRefs& Refs() { return g_refs; }

bool AcquireJvm() {
    if (g_vm) return true;
    const HMODULE jvmDll = GetModuleHandleW(L"jvm.dll");
    if (!jvmDll) {
        log::Warn("jvm.dll not loaded in this process (not a Java process?)");
        return false;
    }
    using GetCreatedVMsFn = jint(JNICALL*)(JavaVM**, jsize, jsize*);
    const auto fn = reinterpret_cast<GetCreatedVMsFn>(GetProcAddress(jvmDll, "JNI_GetCreatedJavaVMs"));
    if (!fn) {
        log::Error("JNI_GetCreatedJavaVMs not exported by jvm.dll");
        return false;
    }
    jsize count = 0;
    if (fn(&g_vm, 1, &count) != JNI_OK || count < 1 || !g_vm) {
        log::Warn("No created JavaVM yet");
        g_vm = nullptr;
        return false;
    }
    JNIEnv* env = Env();
    if (!env) {
        log::Error("Failed to attach bootstrap thread to JVM");
        g_vm = nullptr;
        return false;
    }
    if (g_vm->GetEnv(reinterpret_cast<void**>(&g_jvmti), JVMTI_VERSION_1_2) != JNI_OK || !g_jvmti) {
        log::Error("JVMTI 1.2 env unavailable; class resolution + tick gate need JVMTI");
        g_jvmti = nullptr;
        return false;
    }
    log::Info("Attached to JVM; JVMTI env acquired");
    return true;
}

JNIEnv* Env() {
    if (!g_vm) return nullptr;
    JNIEnv* env = nullptr;
    const jint r = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (r == JNI_OK) return env;
    if (r == JNI_EDETACHED) {
        JavaVMAttachArgs args{JNI_VERSION_1_8, const_cast<char*>("UtilityClient"), nullptr};
        if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), &args) == JNI_OK) return env;
    }
    return nullptr;
}

void DetachIfNeeded() {
    if (g_vm) g_vm->DetachCurrentThread();
}

bool ClearException(JNIEnv* env, const char* where) {
    if (!env->ExceptionCheck()) return false;
    jthrowable ex = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string msg = "(unprintable)";
    if (ex) {
        jclass exCls = env->GetObjectClass(ex);
        jmethodID toString = env->GetMethodID(exCls, "toString", "()Ljava/lang/String;");
        if (toString) {
            if (auto s = static_cast<jstring>(env->CallObjectMethod(ex, toString))) {
                msg = JStringToStd(env, s);
                env->DeleteLocalRef(s);
            }
        }
        env->DeleteLocalRef(exCls);
        env->DeleteLocalRef(ex);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    log::Error(std::string("Java exception at ") + where + ": " + msg);
    return true;
}

std::string JStringToStd(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* chars = env->GetStringUTFChars(s, nullptr);
    if (!chars) return {};
    std::string out(chars);
    env->ReleaseStringUTFChars(s, chars);
    return out;
}

// One GetLoadedClasses pass for any number of wanted classes. Minecraft has
// tens of thousands of loaded classes; scanning once (inside a local frame)
// is both faster and gentler on the JNI local reference table than one scan
// per class. Returns global refs parallel to `names` (null when absent).
static std::vector<jclass> ScanClasses(JNIEnv* env, const std::vector<const char*>& names) {
    std::vector<jclass> out(names.size(), nullptr);
    if (!g_jvmti || names.empty()) return out;
    std::vector<std::string> wanted;
    wanted.reserve(names.size());
    for (const char* n : names) wanted.push_back(std::string("L") + n + ";");

    jint count = 0;
    jclass* classes = nullptr;
    if (env->PushLocalFrame(64) < 0) { ClearException(env, "ScanClasses frame"); return out; }
    if (g_jvmti->GetLoadedClasses(&count, &classes) == JVMTI_ERROR_NONE && classes) {
        size_t remaining = names.size();
        for (jint i = 0; i < count && remaining > 0; ++i) {
            char* sig = nullptr;
            if (g_jvmti->GetClassSignature(classes[i], &sig, nullptr) != JVMTI_ERROR_NONE || !sig) continue;
            for (size_t k = 0; k < wanted.size(); ++k) {
                if (!out[k] && wanted[k] == sig) {
                    out[k] = static_cast<jclass>(env->NewGlobalRef(classes[i]));
                    --remaining;
                    break;
                }
            }
            g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(sig));
        }
        g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(classes));
    }
    env->PopLocalFrame(nullptr);  // frees every local ref from the scan at once
    return out;
}

jclass FindClassBySignature(JNIEnv* env, const char* internalName) {
    return ScanClasses(env, {internalName}).front();
}

bool ResolveAll(JNIEnv* env) {
    using namespace mapping;
    McRefs& r = g_refs;
    if (r.ready) return true;

    // Single loaded-class scan for every wanted class. Slots are only
    // assigned when ALL classes are present (no leaked globals on retry).
    struct Slot { const char* name; jclass* slot; };
    const Slot slots[] = {
        {kMinecraftClient, &r.minecraftClient}, {kClientPlayerEntity, &r.clientPlayer},
        {kClientWorld, &r.clientWorld}, {kClientPlayNetworkHandler, &r.networkHandler},
        {kPendingUpdateManager, &r.pendingUpdateManager}, {kEntity, &r.entity},
        {kPlayerEntity, &r.playerEntity}, {kPlayerAbilities, &r.playerAbilities},
        {kPlayerInventory, &r.playerInventory}, {kItemStack, &r.itemStack},
        {kGameOptions, &r.gameOptions}, {kKeyBinding, &r.keyBinding},
        {kSimpleOption, &r.simpleOption}, {kMouse, &r.mouseCls},
        {kMoveFull, &r.moveFull}, {kMovePos, &r.movePos}, {kMoveLook, &r.moveLook},
        {kMoveOnGround, &r.moveOnGround}, {kPlayerActionPacket, &r.playerActionPacket},
        {kInteractBlockPacket, &r.interactBlockPacket}, {kInteractItemPacket, &r.interactItemPacket},
        {kInteractEntityPacket, &r.interactEntityPacket}, {kHandSwingPacket, &r.handSwingPacket},
        {kPickFromBlockPacket, &r.pickFromBlockPacket}, {kBlockPos, &r.blockPos},
        {kVec3i, &r.vec3i}, {kVec3d, &r.vec3d}, {kBlockHitResult, &r.blockHitResult},
        {"java/lang/Double", &r.javaDouble},
    };
    jclass handCls = nullptr, dirCls = nullptr, actCls = nullptr, useActionCls = nullptr, hungerCls = nullptr;
    const Slot extras[] = {
        {kHand, &handCls}, {kDirection, &dirCls}, {kPlayerActionEnum, &actCls},
        {kUseAction, &useActionCls}, {kHungerManager, &hungerCls},
    };
    std::vector<const char*> names;
    for (const Slot& s : slots) names.push_back(s.name);
    for (const Slot& s : extras) names.push_back(s.name);
    const std::vector<jclass> found = ScanClasses(env, names);
    bool ok = true;
    for (size_t i = 0; i < found.size(); ++i) {
        if (!found[i]) { log::Warn(std::string("Class not loaded yet: ") + names[i]); ok = false; }
    }
    if (!ok) {
        for (jclass c : found) if (c) env->DeleteGlobalRef(c);
        return false;  // game still loading; bootstrap retries
    }
    for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) *slots[i].slot = found[i];
    for (size_t i = 0; i < sizeof(extras) / sizeof(extras[0]); ++i) *extras[i].slot = found[sizeof(slots) / sizeof(slots[0]) + i];

    r.mcGetInstance = Method(env, r.minecraftClient, kMcGetInstance, true, "McGetInstance");
    r.mcGetGameVersion = Method(env, r.minecraftClient, kMcGetGameVersion, false, "McGetGameVersion");
    r.mcTick = Method(env, r.minecraftClient, kMcTick, false, "McTick");
    r.mcPlayer = Field(env, r.minecraftClient, kMcPlayer, "McPlayer");
    r.mcWorld = Field(env, r.minecraftClient, kMcWorld, "McWorld");
    r.mcOptions = Field(env, r.minecraftClient, kMcOptions, "McOptions");
    r.mcMouse = Field(env, r.minecraftClient, kMcMouse, "McMouse");
    r.mcCurrentScreen = Field(env, r.minecraftClient, kMcCurrentScreen, "McCurrentScreen");

    r.cpeNetworkHandler = Field(env, r.clientPlayer, kCpeNetworkHandler, "CpeNetworkHandler");
    r.cpeRequestRespawn = Method(env, r.clientPlayer, kCpeRequestRespawn, false, "CpeRequestRespawn");
    // sendPacket is declared on ClientCommonNetworkHandler; JNI method lookup
    // on the play handler finds the inherited method.
    r.netSendPacket = Method(env, r.networkHandler, kNetSendPacket, false, "NetSendPacket");
    r.worldGetPendingUpdates = Method(env, r.clientWorld, kWorldGetPendingUpdates, false, "WorldGetPendingUpdates");
    r.worldGetEntityById = Method(env, r.clientWorld, kWorldGetEntityById, false, "WorldGetEntityById");
    r.pumIncrementSequence = Method(env, r.pendingUpdateManager, kPumIncrementSequence, false, "PumIncrementSequence");
    r.pumGetSequence = Method(env, r.pendingUpdateManager, kPumGetSequence, false, "PumGetSequence");

    r.entGetX = Method(env, r.entity, kEntGetX, false, "EntGetX");
    r.entGetY = Method(env, r.entity, kEntGetY, false, "EntGetY");
    r.entGetZ = Method(env, r.entity, kEntGetZ, false, "EntGetZ");
    r.entGetYaw = Method(env, r.entity, kEntGetYaw, false, "EntGetYaw");
    r.entGetPitch = Method(env, r.entity, kEntGetPitch, false, "EntGetPitch");
    r.entIsOnGround = Method(env, r.entity, kEntIsOnGround, false, "EntIsOnGround");
    r.entGetVelocity = Method(env, r.entity, kEntGetVelocity, false, "EntGetVelocity");
    r.entSetVelocity = Method(env, r.entity, kEntSetVelocity, false, "EntSetVelocity");
    r.entSetSprinting = Method(env, r.entity, kEntSetSprinting, false, "EntSetSprinting");
    r.entIsAlive = Method(env, r.entity, kEntIsAlive, false, "EntIsAlive");
    r.entFallDistance = Field(env, r.entity, kEntFallDistance, "EntFallDistance");
    r.entNoClip = Field(env, r.entity, kEntNoClip, "EntNoClip");
    r.entHorizontalCollision = Field(env, r.entity, kEntHorizontalCollision, "EntHorizontalCollision");
    r.livIsUsingItem = Method(env, r.clientPlayer, kLivIsUsingItem, false, "LivIsUsingItem");
    r.livGetHealth = Method(env, r.clientPlayer, kLivGetHealth, false, "LivGetHealth");
    r.plGetAbilities = Method(env, r.playerEntity, kPlGetAbilities, false, "PlGetAbilities");
    r.plGetHungerManager = Method(env, r.playerEntity, kPlGetHungerManager, false, "PlGetHungerManager");
    r.plGetInventory = Method(env, r.playerEntity, kPlGetInventory, false, "PlGetInventory");
    r.plSendAbilitiesUpdate = Method(env, r.clientPlayer, kPlSendAbilitiesUpdate, false, "PlSendAbilitiesUpdate");
    r.abFlying = Field(env, r.playerAbilities, kAbFlying, "AbFlying");
    r.abAllowFlying = Field(env, r.playerAbilities, kAbAllowFlying, "AbAllowFlying");
    r.abSetFlySpeed = Method(env, r.playerAbilities, kAbSetFlySpeed, false, "AbSetFlySpeed");
    r.abGetFlySpeed = Method(env, r.playerAbilities, kAbGetFlySpeed, false, "AbGetFlySpeed");
    r.hungerGetFoodLevel = Method(env, hungerCls, kHungerGetFoodLevel, false, "HungerGetFoodLevel");
    r.invGetStack = Method(env, r.playerInventory, kInvGetStack, false, "InvGetStack");
    r.invSetSelectedSlot = Method(env, r.playerInventory, kInvSetSelectedSlot, false, "InvSetSelectedSlot");
    r.invGetSelectedSlot = Method(env, r.playerInventory, kInvGetSelectedSlot, false, "InvGetSelectedSlot");
    r.stackIsEmpty = Method(env, r.itemStack, kStackIsEmpty, false, "StackIsEmpty");
    r.stackGetUseAction = Method(env, r.itemStack, kStackGetUseAction, false, "StackGetUseAction");
    r.useActionSerializedName = Field(env, useActionCls, kUseActionSerializedName, "UseActionSerializedName");

    r.optForwardKey = Field(env, r.gameOptions, kOptForwardKey, "OptForwardKey");
    r.optSprintKey = Field(env, r.gameOptions, kOptSprintKey, "OptSprintKey");
    r.optJumpKey = Field(env, r.gameOptions, kOptJumpKey, "OptJumpKey");
    r.optSneakKey = Field(env, r.gameOptions, kOptSneakKey, "OptSneakKey");
    r.optGetGamma = Method(env, r.gameOptions, kOptGetGamma, false, "OptGetGamma");
    r.keySetPressed = Method(env, r.keyBinding, kKeySetPressed, false, "KeySetPressed");
    r.keyIsPressed = Method(env, r.keyBinding, kKeyIsPressed, false, "KeyIsPressed");
    r.simpleOptGetValue = Method(env, r.simpleOption, kSimpleOptGetValue, false, "SimpleOptGetValue");
    r.simpleOptSetValue = Method(env, r.simpleOption, kSimpleOptSetValue, false, "SimpleOptSetValue");
    r.simpleOptValueField = Field(env, r.simpleOption, kSimpleOptValueField, "SimpleOptValueField");
    r.mouseLockCursor = Method(env, r.mouseCls, kMouseLockCursor, false, "MouseLockCursor");
    r.mouseUnlockCursor = Method(env, r.mouseCls, kMouseUnlockCursor, false, "MouseUnlockCursor");
    r.mouseIsCursorLocked = Method(env, r.mouseCls, kMouseIsCursorLocked, false, "MouseIsCursorLocked");

    r.ctorMoveFull = Method(env, r.moveFull, kCtorMoveFull, false, "CtorMoveFull");
    r.ctorMovePos = Method(env, r.movePos, kCtorMovePos, false, "CtorMovePos");
    r.ctorMoveLook = Method(env, r.moveLook, kCtorMoveLook, false, "CtorMoveLook");
    r.ctorMoveOnGround = Method(env, r.moveOnGround, kCtorMoveOnGround, false, "CtorMoveOnGround");
    r.ctorPlayerAction = Method(env, r.playerActionPacket, kCtorPlayerAction, false, "CtorPlayerAction");
    r.ctorInteractBlock = Method(env, r.interactBlockPacket, kCtorInteractBlock, false, "CtorInteractBlock");
    r.ctorInteractItem = Method(env, r.interactItemPacket, kCtorInteractItem, false, "CtorInteractItem");
    r.ctorHandSwing = Method(env, r.handSwingPacket, kCtorHandSwing, false, "CtorHandSwing");
    r.factoryInteract = Method(env, r.interactEntityPacket, kFactoryInteractEntity, true, "FactoryInteract");
    r.factoryInteractAt = Method(env, r.interactEntityPacket, kFactoryInteractAtEntity, true, "FactoryInteractAt");
    r.factoryAttack = Method(env, r.interactEntityPacket, kFactoryAttackEntity, true, "FactoryAttack");
    r.ctorVec3i = Method(env, r.vec3i, kCtorVec3i, false, "CtorVec3i");
    r.ctorBlockPosVec3i = Method(env, r.blockPos, kCtorBlockPosVec3i, false, "CtorBlockPosVec3i");
    r.ctorVec3d = Method(env, r.vec3d, kCtorVec3d, false, "CtorVec3d");
    r.ctorBlockHitResult = Method(env, r.blockHitResult, kCtorBlockHitResult, false, "CtorBlockHitResult");
    r.vec3dX = Field(env, r.vec3d, kVec3dX, "Vec3dX");
    r.vec3dY = Field(env, r.vec3d, kVec3dY, "Vec3dY");
    r.vec3dZ = Field(env, r.vec3d, kVec3dZ, "Vec3dZ");
    {
        static const mapping::NamedDesc kDoubleValueOf{"valueOf", "(D)Ljava/lang/Double;"};
        static const mapping::NamedDesc kDoubleDoubleValue{"doubleValue", "()D"};
        r.doubleValueOf = Method(env, r.javaDouble, kDoubleValueOf, true, "DoubleValueOf");
        r.doubleDoubleValue = Method(env, r.javaDouble, kDoubleDoubleValue, false, "DoubleDoubleValue");
    }
    // Probes (absent from enigma mappings): failures are soft, with fallback.
    r.ctorBlockPosInts = env->GetMethodID(r.blockPos, kCtorBlockPosInts.name, kCtorBlockPosInts.desc);
    if (!r.ctorBlockPosInts) {
        ClearException(env, "CtorBlockPosInts probe");
        log::Info("BlockPos(III) ctor absent; using BlockPos(Vec3i) fallback");
    }
    r.ctorPickFromBlock = env->GetMethodID(r.pickFromBlockPacket, kCtorPickFromBlock.name, kCtorPickFromBlock.desc);
    if (!r.ctorPickFromBlock) {
        ClearException(env, "CtorPickFromBlock probe");
        log::Warn("PickItemFromBlock ctor probe failed; PickFromBlock actions will be dropped");
    }

    r.handMain = EnumConstant(env, handCls, kHandMain, "HandMain");
    r.handOff = EnumConstant(env, handCls, kHandOff, "HandOff");
    {
        const mapping::NamedDesc dirs[6] = {kDirDown, kDirUp, kDirNorth, kDirSouth, kDirWest, kDirEast};
        const mapping::NamedDesc acts[7] = {kActStartDestroy, kActAbortDestroy, kActStopDestroy, kActDropAll,
                                            kActDropItem, kActReleaseUse, kActSwapOffhand};
        for (int i = 0; i < 6; ++i) r.directions[i] = EnumConstant(env, dirCls, dirs[i], "Direction");
        for (int i = 0; i < 7; ++i) r.playerActions[i] = EnumConstant(env, actCls, acts[i], "PlayerAction");
    }
    env->DeleteGlobalRef(handCls);
    env->DeleteGlobalRef(dirCls);
    env->DeleteGlobalRef(actCls);
    env->DeleteGlobalRef(useActionCls);
    env->DeleteGlobalRef(hungerCls);

    const void* core[] = {r.mcGetInstance,  r.mcPlayer,     r.mcWorld,      r.netSendPacket,
                          r.ctorMoveOnGround, r.handMain,   r.ctorBlockHitResult};
    for (const void* p : core) ok &= Present(p);
    if (!ok) {
        log::Error("Core 1.21.11 mappings failed to resolve; packet sending disabled");
        return false;
    }

    // Version lock check.
    jobject client = ClientInstance(env);
    if (!client) return false;  // instance not constructed yet; retry
    auto ver = static_cast<jstring>(env->CallObjectMethod(client, r.mcGetGameVersion));
    ClearException(env, "getGameVersion");
    r.versionString = JStringToStd(env, ver);
    if (ver) env->DeleteLocalRef(ver);
    env->DeleteLocalRef(client);
    r.versionOk = r.versionString.find(kExpectedMcVersion) != std::string::npos;
    r.ready = true;
    if (r.versionOk) {
        log::Info("Resolved 1.21.11 mappings (game version: " + r.versionString + ")");
    } else {
        log::Error("VERSION MISMATCH: game reports '" + r.versionString + "', this DLL is built for " +
                   std::string(kExpectedMcVersion) + " only. Packet sending stays disabled.");
    }
    return true;
}

jobject ClientInstance(JNIEnv* env) {
    McRefs& r = g_refs;
    // mcGetInstance is resolved before ready is set during ResolveAll itself.
    if (!r.mcGetInstance || !r.minecraftClient) return nullptr;
    jobject client = env->CallStaticObjectMethod(r.minecraftClient, r.mcGetInstance);
    ClearException(env, "getInstance");
    return client;
}

int32_t NextSequence(JNIEnv* env, jobject world) {
    McRefs& r = g_refs;
    if (!world || !r.worldGetPendingUpdates) return 0;
    jobject pum = env->CallObjectMethod(world, r.worldGetPendingUpdates);
    ClearException(env, "getPendingUpdateManager");
    if (!pum) return 0;
    jobject inc = env->CallObjectMethod(pum, r.pumIncrementSequence);
    ClearException(env, "incrementSequence");
    jint seq = 0;
    if (inc) {
        seq = env->CallIntMethod(inc, r.pumGetSequence);
        ClearException(env, "getSequence");
        env->DeleteLocalRef(inc);
    }
    env->DeleteLocalRef(pum);
    return seq;
}

bool ReadTickContext(JNIEnv* env, TickContext& ctx) {
    McRefs& r = g_refs;
    ctx.env = env;
    ctx.client = ClientInstance(env);
    if (!ctx.client) return false;
    ctx.player = env->GetObjectField(ctx.client, r.mcPlayer);
    ctx.world = env->GetObjectField(ctx.client, r.mcWorld);
    ctx.options = env->GetObjectField(ctx.client, r.mcOptions);
    ClearException(env, "ReadTickContext fields");
    if (!ctx.player || !ctx.world) return false;

    ctx.handler = env->GetObjectField(ctx.player, r.cpeNetworkHandler);
    ClearException(env, "ReadTickContext handler");

    PlayerState& s = ctx.state;
    s.inWorld = true;
    s.x = env->CallDoubleMethod(ctx.player, r.entGetX);
    s.y = env->CallDoubleMethod(ctx.player, r.entGetY);
    s.z = env->CallDoubleMethod(ctx.player, r.entGetZ);
    s.yaw = env->CallFloatMethod(ctx.player, r.entGetYaw);
    s.pitch = env->CallFloatMethod(ctx.player, r.entGetPitch);
    s.onGround = env->CallBooleanMethod(ctx.player, r.entIsOnGround) != JNI_FALSE;
    s.alive = env->CallBooleanMethod(ctx.player, r.entIsAlive) != JNI_FALSE;
    s.usingItem = env->CallBooleanMethod(ctx.player, r.livIsUsingItem) != JNI_FALSE;
    s.health = env->CallFloatMethod(ctx.player, r.livGetHealth);
    s.fallDistance = env->GetDoubleField(ctx.player, r.entFallDistance);
    s.horizontalCollision = env->GetBooleanField(ctx.player, r.entHorizontalCollision) != JNI_FALSE;
    ClearException(env, "ReadTickContext player");
    if (jobject vel = env->CallObjectMethod(ctx.player, r.entGetVelocity)) {
        s.velX = env->GetDoubleField(vel, r.vec3dX);
        s.velY = env->GetDoubleField(vel, r.vec3dY);
        s.velZ = env->GetDoubleField(vel, r.vec3dZ);
        env->DeleteLocalRef(vel);
        ClearException(env, "ReadTickContext velocity");
    }
    if (jobject hunger = env->CallObjectMethod(ctx.player, r.plGetHungerManager)) {
        s.foodLevel = env->CallIntMethod(hunger, r.hungerGetFoodLevel);
        env->DeleteLocalRef(hunger);
        ClearException(env, "ReadTickContext hunger");
    }
    return true;
}

}  // namespace uc::jni
