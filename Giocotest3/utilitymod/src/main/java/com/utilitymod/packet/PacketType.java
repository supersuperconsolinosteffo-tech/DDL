package com.utilitymod.packet;

/**
 * Enum of existing Minecraft packet types that UtilityMod is allowed to use.
 *
 * <p>Each enum constant maps to a real Minecraft packet class. No custom
 * packets are ever created.
 */
public enum PacketType {
    // Movement packets (PlayerMoveC2SPacket family)
    MOVE_PLAYER_POS(
        "net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket$PositionAndOnGround"
    ),
    MOVE_PLAYER_LOOK(
        "net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket$LookAndOnGround"
    ),
    MOVE_PLAYER_FULL(
        "net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket$Full"
    ),
    MOVE_PLAYER_ON_GROUND_ONLY(
        "net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket$OnGroundOnly"
    ),

    // Action packet (PlayerActionC2SPacket)
    PLAYER_ACTION(
        "net.minecraft.network.packet.c2s.play.PlayerActionC2SPacket"
    ),

    // Block interaction
    INTERACT_BLOCK(
        "net.minecraft.network.packet.c2s.play.PlayerInteractBlockC2SPacket"
    ),

    // Item interaction
    INTERACT_ITEM(
        "net.minecraft.network.packet.c2s.play.PlayerInteractItemC2SPacket"
    ),

    // Entity interaction
    INTERACT_ENTITY(
        "net.minecraft.network.packet.c2s.play.PlayerInteractEntityC2SPacket"
    ),

    // Hand swing
    HAND_SWING(
        "net.minecraft.network.packet.c2s.play.HandSwingC2SPacket"
    ),

    // Block breaking
    BREAK_BLOCK(
        "net.minecraft.network.packet.c2s.play.PlayerActionC2SPacket"
    ),

    // Item picking
    PICK_ITEM_FROM_BLOCK(
        "net.minecraft.network.packet.c2s.play.PickItemFromBlockC2SPacket"
    ),

    // Chat message (for testing only)
    CHAT_MESSAGE(
        "net.minecraft.network.packet.c2s.play.ChatMessageC2SPacket"
    );

    private final String className;

    PacketType(String className) {
        this.className = className;
    }

    public String getClassName() {
        return className;
    }
}