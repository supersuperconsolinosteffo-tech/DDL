// packet_queue.hpp - THE single packet path of this client.
//
// Invariants enforced by construction (see README):
//   1. Existing vanilla packets only. PacketKind is a closed enum mapping
//      1:1 to vanilla 1.21.11 C2S packet classes. There is no API here that
//      can express a custom payload / custom channel packet.
//   2. Start-of-tick only. Any thread may Push(), but packets are only
//      constructed+sent inside DrainAtTickStart(), which is only ever called
//      from the tick gate (tick_gate.cpp) while the gate flag is raised.
//      Nothing is ever sent between ticks or at end of tick.
#pragma once

#include <jni.h>

#include <cstdint>
#include <deque>
#include <mutex>

namespace uc {

// PlayerActionC2SPacket.Action values used, by source-level name (the enum
// constants are not renamed by intermediary; resolved by name in jni_bridge).
enum class PlayerActionKind : int32_t {
    StartDestroyBlock = 0,
    AbortDestroyBlock = 1,
    StopDestroyBlock  = 2,
    DropAllItems      = 3,
    DropItem          = 4,
    ReleaseUseItem    = 5,
    SwapWithOffhand   = 6,
};

// Direction by source-level name, resolved to enum objects in jni_bridge.
enum class DirectionKind : int32_t { Down = 0, Up = 1, North = 2, South = 3, West = 4, East = 5 };

enum class PacketKind : int32_t {
    MoveFull,        // PlayerMoveC2SPacket.Full            (DDD FF ZZ)
    MovePos,         // PlayerMoveC2SPacket.PositionAndOnGround (DDD ZZ)
    MoveLook,        // PlayerMoveC2SPacket.LookAndOnGround (FF ZZ)
    MoveOnGround,    // PlayerMoveC2SPacket.OnGroundOnly    (ZZ)
    PlayerAction,    // PlayerActionC2SPacket               (Action, BlockPos, Direction, int)
    InteractBlock,   // PlayerInteractBlockC2SPacket        (Hand, BlockHitResult, int)
    InteractItem,    // PlayerInteractItemC2SPacket         (Hand, int, F, F)
    InteractEntity,  // PlayerInteractEntityC2SPacket.interact   (Entity, Z, Hand)
    InteractAtEntity,// PlayerInteractEntityC2SPacket.interactAt (Entity, Z, Hand, Vec3d)
    AttackEntity,    // PlayerInteractEntityC2SPacket.attack     (Entity, Z)
    HandSwing,       // HandSwingC2SPacket                   (Hand)
    PickFromBlock,   // PickItemFromBlockC2SPacket           (BlockPos, Z)
};

struct PacketAction {
    PacketKind kind = PacketKind::MoveOnGround;
    double x = 0, y = 0, z = 0;          // move pos / interact-at hit pos
    float  yaw = 0, pitch = 0;           // move look / interact-item look
    bool   onGround = false;             // move packets
    bool   horizontalCollision = false;  // move packets (1.21.11 constructors take it)
    bool   flag = false;                 // interact-entity: sneaking; pick-block: includeData
    int32_t hx = 0, hy = 0, hz = 0;      // block pos (player-action / interact-block / pick)
    int32_t entityId = -1;               // interact-entity / attack-entity
    int32_t hand = 0;                    // 0 = MAIN_HAND, 1 = OFF_HAND
    int32_t action = 0;                  // PlayerActionKind for PlayerAction
    int32_t direction = 1;               // DirectionKind for PlayerAction / InteractBlock
};

// Factory helpers (keep call sites readable; they only fill the struct).
inline PacketAction MoveOnGround(bool onGround, bool horizCollision) {
    PacketAction a; a.kind = PacketKind::MoveOnGround; a.onGround = onGround; a.horizontalCollision = horizCollision; return a;
}
inline PacketAction MoveFull(double x, double y, double z, float yaw, float pitch, bool onGround, bool hc) {
    PacketAction a; a.kind = PacketKind::MoveFull; a.x = x; a.y = y; a.z = z; a.yaw = yaw; a.pitch = pitch; a.onGround = onGround; a.horizontalCollision = hc; return a;
}
inline PacketAction InteractItem(int32_t hand, float yaw, float pitch) {
    PacketAction a; a.kind = PacketKind::InteractItem; a.hand = hand; a.yaw = yaw; a.pitch = pitch; return a;
}
inline PacketAction HandSwing(int32_t hand) {
    PacketAction a; a.kind = PacketKind::HandSwing; a.hand = hand; return a;
}

class PacketQueue {
public:
    static PacketQueue& Get();

    // Any thread. Actions sit here until the next tick gate.
    void Push(PacketAction action);

    // Tick-gate thread only (asserts the gate flag). Constructs and sends
    // every queued vanilla packet through ClientPlayNetworkHandler.sendPacket.
    // client/player/world/handler are local refs valid for this tick.
    void DrainAtTickStart(JNIEnv* env, jobject player, jobject world, jobject handler);

    int  PendingCount();
    void Clear();

    // Diagnostics for the menu/HUD.
    uint64_t SentTotal() const { return sentTotal_; }
    uint64_t DroppedTotal() const { return droppedTotal_; }

private:
    PacketQueue() = default;
    std::mutex mutex_;
    std::deque<PacketAction> queue_;
    uint64_t sentTotal_ = 0;
    uint64_t droppedTotal_ = 0;
};

}  // namespace uc
