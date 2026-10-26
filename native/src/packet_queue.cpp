#include "packet_queue.hpp"

#include "jni_bridge.hpp"
#include "log.hpp"
#include "tick_gate.hpp"

namespace uc {
namespace {

jobject HandOf(const jni::McRefs& r, int32_t hand) { return hand == 1 ? r.handOff : r.handMain; }

jobject MakeBlockPos(JNIEnv* env, const jni::McRefs& r, int32_t x, int32_t y, int32_t z) {
    if (r.ctorBlockPosInts) return env->NewObject(r.blockPos, r.ctorBlockPosInts, x, y, z);
    jobject v = env->NewObject(r.vec3i, r.ctorVec3i, x, y, z);
    if (!v) return nullptr;
    jobject bp = env->NewObject(r.blockPos, r.ctorBlockPosVec3i, v);
    env->DeleteLocalRef(v);
    return bp;
}

jobject MakeVec3d(JNIEnv* env, const jni::McRefs& r, double x, double y, double z) {
    return env->NewObject(r.vec3d, r.ctorVec3d, x, y, z);
}

// Builds exactly one vanilla 1.21.11 C2S packet object. No code path here
// can construct a custom-payload packet (closed PacketKind switch).
jobject MakePacket(JNIEnv* env, const jni::McRefs& r, const PacketAction& a, jobject world) {
    switch (a.kind) {
        case PacketKind::MoveFull:
            return env->NewObject(r.moveFull, r.ctorMoveFull, a.x, a.y, a.z, a.yaw, a.pitch,
                                   static_cast<jboolean>(a.onGround), static_cast<jboolean>(a.horizontalCollision));
        case PacketKind::MovePos:
            return env->NewObject(r.movePos, r.ctorMovePos, a.x, a.y, a.z,
                                   static_cast<jboolean>(a.onGround), static_cast<jboolean>(a.horizontalCollision));
        case PacketKind::MoveLook:
            return env->NewObject(r.moveLook, r.ctorMoveLook, a.yaw, a.pitch,
                                   static_cast<jboolean>(a.onGround), static_cast<jboolean>(a.horizontalCollision));
        case PacketKind::MoveOnGround:
            return env->NewObject(r.moveOnGround, r.ctorMoveOnGround,
                                   static_cast<jboolean>(a.onGround), static_cast<jboolean>(a.horizontalCollision));
        case PacketKind::PlayerAction: {
            if (a.action < 0 || a.action > 6 || !r.playerActions[a.action]) return nullptr;
            if (a.direction < 0 || a.direction > 5 || !r.directions[a.direction]) return nullptr;
            jobject pos = MakeBlockPos(env, r, a.hx, a.hy, a.hz);
            if (!pos) return nullptr;
            jobject pkt = env->NewObject(r.playerActionPacket, r.ctorPlayerAction, r.playerActions[a.action],
                                          pos, r.directions[a.direction], jni::NextSequence(env, world));
            env->DeleteLocalRef(pos);
            return pkt;
        }
        case PacketKind::InteractBlock: {
            if (a.direction < 0 || a.direction > 5 || !r.directions[a.direction]) return nullptr;
            jobject pos = MakeBlockPos(env, r, a.hx, a.hy, a.hz);
            jobject hitVec = MakeVec3d(env, r, a.x, a.y, a.z);
            if (!pos || !hitVec) return nullptr;
            jobject hit = env->NewObject(r.blockHitResult, r.ctorBlockHitResult, hitVec, r.directions[a.direction],
                                          pos, static_cast<jboolean>(JNI_FALSE));
            env->DeleteLocalRef(pos);
            env->DeleteLocalRef(hitVec);
            if (!hit) return nullptr;
            jobject pkt = env->NewObject(r.interactBlockPacket, r.ctorInteractBlock, HandOf(r, a.hand), hit,
                                          jni::NextSequence(env, world));
            env->DeleteLocalRef(hit);
            return pkt;
        }
        case PacketKind::InteractItem:
            return env->NewObject(r.interactItemPacket, r.ctorInteractItem, HandOf(r, a.hand),
                                   jni::NextSequence(env, world), a.yaw, a.pitch);
        case PacketKind::InteractEntity:
        case PacketKind::InteractAtEntity:
        case PacketKind::AttackEntity: {
            if (!world || a.entityId < 0) return nullptr;
            jobject entity = env->CallObjectMethod(world, r.worldGetEntityById, a.entityId);
            jni::ClearException(env, "getEntityById");
            if (!entity) return nullptr;
            const auto sneaking = static_cast<jboolean>(a.flag);
            jobject pkt = nullptr;
            if (a.kind == PacketKind::InteractEntity) {
                pkt = env->CallStaticObjectMethod(r.interactEntityPacket, r.factoryInteract, entity, sneaking, HandOf(r, a.hand));
            } else if (a.kind == PacketKind::InteractAtEntity) {
                jobject at = MakeVec3d(env, r, a.x, a.y, a.z);
                pkt = env->CallStaticObjectMethod(r.interactEntityPacket, r.factoryInteractAt, entity, sneaking, HandOf(r, a.hand), at);
                if (at) env->DeleteLocalRef(at);
            } else {
                pkt = env->CallStaticObjectMethod(r.interactEntityPacket, r.factoryAttack, entity, sneaking);
            }
            env->DeleteLocalRef(entity);
            return pkt;
        }
        case PacketKind::HandSwing:
            return env->NewObject(r.handSwingPacket, r.ctorHandSwing, HandOf(r, a.hand));
        case PacketKind::PickFromBlock: {
            if (!r.ctorPickFromBlock) return nullptr;
            jobject pos = MakeBlockPos(env, r, a.hx, a.hy, a.hz);
            if (!pos) return nullptr;
            jobject pkt = env->NewObject(r.pickFromBlockPacket, r.ctorPickFromBlock, pos, static_cast<jboolean>(a.flag));
            env->DeleteLocalRef(pos);
            return pkt;
        }
    }
    return nullptr;
}
}  // namespace

PacketQueue& PacketQueue::Get() {
    static PacketQueue q;
    return q;
}

void PacketQueue::Push(PacketAction action) {
    std::lock_guard lock(mutex_);
    queue_.push_back(action);
}

int PacketQueue::PendingCount() {
    std::lock_guard lock(mutex_);
    return static_cast<int>(queue_.size());
}

void PacketQueue::Clear() {
    std::lock_guard lock(mutex_);
    queue_.clear();
}

void PacketQueue::DrainAtTickStart(JNIEnv* env, jobject /*player*/, jobject world, jobject handler) {
    // Invariant 2: refuses to run anywhere except inside the tick gate.
    if (!tick_gate::InGate()) {
        log::Error("DrainAtTickStart called outside the tick gate; ignored (bug)");
        return;
    }
    std::deque<PacketAction> pending;
    {
        std::lock_guard lock(mutex_);
        pending.swap(queue_);
    }
    if (pending.empty()) return;

    const jni::McRefs& r = jni::Refs();
    if (!r.ready || !r.versionOk || !handler) {
        std::lock_guard lock(mutex_);
        droppedTotal_ += pending.size();
        return;
    }
    uint64_t sent = 0, dropped = 0;
    for (const PacketAction& a : pending) {
        jobject pkt = MakePacket(env, r, a, world);
        if (pkt) {
            env->CallVoidMethod(handler, r.netSendPacket, pkt);
            if (jni::ClearException(env, "sendPacket")) {
                ++dropped;
            } else {
                ++sent;
            }
            env->DeleteLocalRef(pkt);
        } else {
            jni::ClearException(env, "packet construction");
            ++dropped;
        }
    }
    std::lock_guard lock(mutex_);
    sentTotal_ += sent;
    droppedTotal_ += dropped;
}

}  // namespace uc
