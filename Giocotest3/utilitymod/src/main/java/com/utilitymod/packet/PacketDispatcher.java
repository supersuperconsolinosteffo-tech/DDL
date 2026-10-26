package com.utilitymod.packet;

import com.utilitymod.UtilityMod;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.client.network.ClientPlayerInteractionManager;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.network.packet.c2s.play.*;
import net.minecraft.util.Hand;
import net.minecraft.util.hit.BlockHitResult;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.Direction;
import net.minecraft.util.math.Vec3d;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.Map;

/**
 * Dispatches {@link PacketAction}s by constructing and sending the appropriate
 * existing Minecraft packets.
 *
 * <p>This class uses reflection to find constructors and factory methods so it
 * works across minor mapping changes without requiring recompilation. All
 * packets are sent through the standard client network handler.
 */
public final class PacketDispatcher {
    private static final MinecraftClient CLIENT = MinecraftClient.getInstance();

    private PacketDispatcher() {}

    /**
     * Dispatches a single action. Called at the start of a game tick only.
     */
    public static void dispatch(MinecraftClient client, PacketAction action) {
        switch (action.getType()) {
            case MOVE_PLAYER_POS -> dispatchMovePlayerPos(client, action);
            case MOVE_PLAYER_LOOK -> dispatchMovePlayerLook(client, action);
            case MOVE_PLAYER_FULL -> dispatchMovePlayerFull(client, action);
            case MOVE_PLAYER_ON_GROUND_ONLY -> dispatchMovePlayerOnGroundOnly(client, action);
            case PLAYER_ACTION -> dispatchPlayerAction(client, action);
            case INTERACT_BLOCK -> dispatchInteractBlock(client, action);
            case INTERACT_ITEM -> dispatchInteractItem(client, action);
            case INTERACT_ENTITY -> dispatchInteractEntity(client, action);
            case HAND_SWING -> dispatchHandSwing(client, action);
            case BREAK_BLOCK -> dispatchBreakBlock(client, action);
            case PICK_ITEM_FROM_BLOCK -> dispatchPickItemFromBlock(client, action);
            case CHAT_MESSAGE -> dispatchChatMessage(client, action);
        }
    }

    // --- Movement packets ---

    private static void dispatchMovePlayerPos(MinecraftClient client, PacketAction action) {
        double x = action.get("x", 0.0);
        double y = action.get("y", 0.0);
        double z = action.get("z", 0.0);
        boolean onGround = action.get("onGround", false);
        boolean horizontalCollision = action.get("horizontalCollision", false);

        // Use the standard client method which sends the correct packet
        var interactionManager = client.interactionManager;
        if (interactionManager != null) {
            // Queue a position update via the interaction manager
            var networkHandler = client.getNetworkHandler();
            if (networkHandler != null) {
                var packet = new PlayerMoveC2SPacket.PositionAndOnGround(new Vec3d(x, y, z), onGround, horizontalCollision);
                networkHandler.sendPacket(packet);
            }
        }
    }

    private static void dispatchMovePlayerLook(MinecraftClient client, PacketAction action) {
        float yaw = action.get("yaw", 0.0f);
        float pitch = action.get("pitch", 0.0f);
        boolean onGround = action.get("onGround", false);

        var networkHandler = client.getNetworkHandler();
        if (networkHandler != null) {
            var packet = new PlayerMoveC2SPacket.LookAndOnGround(yaw, pitch, onGround);
            networkHandler.sendPacket(packet);
        }
    }

    private static void dispatchMovePlayerFull(MinecraftClient client, PacketAction action) {
        double x = action.get("x", 0.0);
        double y = action.get("y", 0.0);
        double z = action.get("z", 0.0);
        float yaw = action.get("yaw", 0.0f);
        float pitch = action.get("pitch", 0.0f);
        boolean onGround = action.get("onGround", false);
        boolean horizontalCollision = action.get("horizontalCollision", false);
        boolean changePosition = action.get("changePosition", true);

        var networkHandler = client.getNetworkHandler();
        if (networkHandler != null) {
            var packet = new PlayerMoveC2SPacket.Full(x, y, z, yaw, pitch, onGround, horizontalCollision, changePosition);
            networkHandler.sendPacket(packet);
        }
    }

    private static void dispatchMovePlayerOnGroundOnly(MinecraftClient client, PacketAction action) {
        boolean onGround = action.get("onGround", false);

        var networkHandler = client.getNetworkHandler();
        if (networkHandler != null) {
            var packet = new PlayerMoveC2SPacket.OnGroundOnly(onGround);
            networkHandler.sendPacket(packet);
        }
    }

    // --- PlayerActionC2SPacket (digging, dropping, etc.) ---

    private static void dispatchPlayerAction(MinecraftClient client, PacketAction action) {
        String actionName = action.get("action", "START_DESTROY_BLOCK");
        BlockPos pos = action.get("pos");
        Direction direction = action.get("direction", Direction.DOWN);

        if (pos == null) {
            UtilityMod.LOGGER.warn("PLAYER_ACTION requires 'pos' parameter");
            return;
        }

        var networkHandler = client.getNetworkHandler();
        if (networkHandler == null) return;

        try {
            Class<?> actionClass = Class.forName("net.minecraft.network.packet.c2s.play.PlayerActionC2SPacket$Action");
            Object actionEnum = Enum.valueOf((Class<Enum>) actionClass, actionName);

            Class<?> packetClass = Class.forName("net.minecraft.network.packet.c2s.play.PlayerActionC2SPacket");
            Constructor<?> constructor = packetClass.getConstructor(actionClass, BlockPos.class, Direction.class);
            PlayerActionC2SPacket packet = (PlayerActionC2SPacket) constructor.newInstance(actionEnum, pos, direction);
            networkHandler.sendPacket(packet);
        } catch (Exception e) {
            UtilityMod.LOGGER.error("Failed to construct PlayerActionC2SPacket", e);
        }
    }

    private static void dispatchBreakBlock(MinecraftClient client, PacketAction action) {
        // Break block is just a convenience wrapper around PLAYER_ACTION with
        // the START_DESTROY_BLOCK -> STOP_DESTROY_BLOCK sequence.
        BlockPos pos = action.get("pos");
        if (pos == null) return;

        // Send START_DESTROY_BLOCK
        action.put("action", "START_DESTROY_BLOCK");
        action.put("direction", Direction.DOWN);
        dispatchPlayerAction(client, action);

        // Send STOP_DESTROY_BLOCK on next tick
        TickScheduler.schedule(new PacketAction(PacketType.PLAYER_ACTION, Map.of(
            "action", "STOP_DESTROY_BLOCK",
            "pos", pos,
            "direction", Direction.DOWN
        )));
    }

    // --- Block interaction ---

    private static void dispatchInteractBlock(MinecraftClient client, PacketAction action) {
        Hand hand = action.get("hand", Hand.MAIN_HAND);
        BlockPos pos = action.get("pos");
        Direction direction = action.get("direction", Direction.UP);
        Vec3d hitPos = action.get("hitPos");
        boolean insideBlock = action.get("insideBlock", false);

        if (pos == null || hitPos == null) {
            UtilityMod.LOGGER.warn("INTERACT_BLOCK requires 'pos' and 'hitPos' parameters");
            return;
        }

        var networkHandler = client.getNetworkHandler();
        if (networkHandler == null) return;

        try {
            // Build BlockHitResult
            Class<?> hitResultClass = Class.forName("net.minecraft.util.hit.BlockHitResult");
            Constructor<?> hitResultCtor = hitResultClass.getConstructor(
                Vec3d.class, Direction.class, BlockPos.class, boolean.class
            );
            BlockHitResult hitResult = (BlockHitResult) hitResultCtor.newInstance(
                hitPos, direction, pos, insideBlock
            );

            Class<?> packetClass = Class.forName("net.minecraft.network.packet.c2s.play.PlayerInteractBlockC2SPacket");
            // Constructor: (Hand, BlockHitResult, int sequence)
            int sequence = client.player != null ? client.player.getNextSequenceId() : 0;
            Constructor<?> ctor = packetClass.getConstructor(Hand.class, BlockHitResult.class, int.class);
            PlayerInteractBlockC2SPacket packet = (PlayerInteractBlockC2SPacket) ctor.newInstance(hand, hitResult, sequence);
            networkHandler.sendPacket(packet);
        } catch (Exception e) {
            UtilityMod.LOGGER.error("Failed to construct PlayerInteractBlockC2SPacket", e);
        }
    }

    // --- Item interaction ---

    private static void dispatchInteractItem(MinecraftClient client, PacketAction action) {
        Hand hand = action.get("hand", Hand.MAIN_HAND);

        var networkHandler = client.getNetworkHandler();
        if (networkHandler == null) return;

        try {
            Class<?> packetClass = Class.forName("net.minecraft.network.packet.c2s.play.PlayerInteractItemC2SPacket");
            // Constructor: (Hand, int sequence)
            int sequence = client.player != null ? client.player.getNextSequenceId() : 0;
            Constructor<?> ctor = packetClass.getConstructor(Hand.class, int.class);
            PlayerInteractItemC2SPacket packet = (PlayerInteractItemC2SPacket) ctor.newInstance(hand, sequence);
            networkHandler.sendPacket(packet);
        } catch (Exception e) {
            UtilityMod.LOGGER.error("Failed to construct PlayerInteractItemC2SPacket", e);
        }
    }

    // --- Entity interaction ---

    private static void dispatchInteractEntity(MinecraftClient client, PacketAction action) {
        int entityId = action.get("entityId", -1);
        Hand hand = action.get("hand", Hand.MAIN_HAND);
        boolean sneaking = action.get("sneaking", false);
        Vec3d hitPos = action.get("hitPos");

        if (entityId < 0) {
            UtilityMod.LOGGER.warn("INTERACT_ENTITY requires 'entityId' parameter");
            return;
        }

        var networkHandler = client.getNetworkHandler();
        if (networkHandler == null) return;

        try {
            Class<?> packetClass = Class.forName("net.minecraft.network.packet.c2s.play.PlayerInteractEntityC2SPacket");
            Method interactMethod;

            if (hitPos != null) {
                // interactAt(Entity, boolean, Hand, Vec3d)
                interactMethod = packetClass.getMethod("interactAt", int.class, boolean.class, Hand.class, Vec3d.class);
                PlayerInteractEntityC2SPacket packet = (PlayerInteractEntityC2SPacket) interactMethod.invoke(null, entityId, sneaking, hand, hitPos);
                networkHandler.sendPacket(packet);
            } else {
                // interact(Entity, boolean, Hand)
                interactMethod = packetClass.getMethod("interact", int.class, boolean.class, Hand.class);
                PlayerInteractEntityC2SPacket packet = (PlayerInteractEntityC2SPacket) interactMethod.invoke(null, entityId, sneaking, hand);
                networkHandler.sendPacket(packet);
            }
        } catch (Exception e) {
            UtilityMod.LOGGER.error("Failed to construct PlayerInteractEntityC2SPacket", e);
        }
    }

    // --- Hand swing ---

    private static void dispatchHandSwing(MinecraftClient client, PacketAction action) {
        Hand hand = action.get("hand", Hand.MAIN_HAND);

        var networkHandler = client.getNetworkHandler();
        if (networkHandler == null) return;

        var packet = new HandSwingC2SPacket(hand);
        networkHandler.sendPacket(packet);
    }

    // --- Pick item from block ---

    private static void dispatchPickItemFromBlock(MinecraftClient client, PacketAction action) {
        BlockPos pos = action.get("pos");
        boolean includeData = action.get("includeData", true);

        if (pos == null) {
            UtilityMod.LOGGER.warn("PICK_ITEM_FROM_BLOCK requires 'pos' parameter");
            return;
        }

        var networkHandler = client.getNetworkHandler();
        if (networkHandler == null) return;

        try {
            Class<?> packetClass = Class.forName("net.minecraft.network.packet.c2s.play.PickItemFromBlockC2SPacket");
            Constructor<?> ctor = packetClass.getConstructor(BlockPos.class, boolean.class);
            PickItemFromBlockC2SPacket packet = (PickItemFromBlockC2SPacket) ctor.newInstance(pos, includeData);
            networkHandler.sendPacket(packet);
        } catch (Exception e) {
            UtilityMod.LOGGER.error("Failed to construct PickItemFromBlockC2SPacket", e);
        }
    }

    // --- Chat message (testing) ---

    private static void dispatchChatMessage(MinecraftClient client, PacketAction action) {
        String message = action.get("message", "");
        if (message.isEmpty()) return;

        var networkHandler = client.getNetworkHandler();
        if (networkHandler == null) return;

        var packet = new ChatMessageC2SPacket(message, System.currentTimeMillis(), System.currentTimeMillis(), null, null, null, false, false);
        networkHandler.sendPacket(packet);
    }
}