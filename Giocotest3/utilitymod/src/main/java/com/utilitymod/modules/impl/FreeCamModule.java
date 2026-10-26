package com.utilitymod.modules.impl;

import com.utilitymod.modules.Category;
import com.utilitymod.modules.ConfigurableModuleBase;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketType;
import com.utilitymod.tick.TickScheduler;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.util.math.Vec3d;

/**
 * FreeCam - allows camera to move independently from player.
 * 
 * <p>This is implemented by sending movement packets that move the player
 * while keeping the camera separate. In practice, free cam is usually
 * a client-side render modification, but we can simulate it by
 * manipulating the player position packets.
 */
public class FreeCamModule extends ConfigurableModuleBase {
    private Vec3d savedPosition;
    private float savedYaw;
    private float savedPitch;
    private boolean wasFlying;

    public FreeCamModule() {
        super("freecam", "FreeCam", "Camera moves independently from player", Category.PLAYER);
        setParameter("speed", 0.5);
        setParameter("sprintMultiplier", 2.0);
    }

    @Override
    public void onEnable() {
        MinecraftClient client = MinecraftClient.getInstance();
        ClientPlayerEntity player = client.player;
        if (player != null) {
            savedPosition = player.getPos();
            savedYaw = player.getYaw();
            savedPitch = player.getPitch();
            wasFlying = player.getAbilities().flying;
            
            // Enable creative-like flight for free cam
            player.getAbilities().flying = true;
            player.getAbilities().setFlySpeed(getParameter("speed", 0.5f));
            player.sendAbilitiesUpdate();
        }
    }

    @Override
    public void onDisable() {
        MinecraftClient client = MinecraftClient.getInstance();
        ClientPlayerEntity player = client.player;
        if (player != null && savedPosition != null) {
            // Restore position
            player.getAbilities().flying = wasFlying;
            player.sendAbilitiesUpdate();
            
            // Send position correction packet
            var action = new PacketAction(PacketType.MOVE_PLAYER_FULL);
            action.put("x", savedPosition.x);
            action.put("y", savedPosition.y);
            action.put("z", savedPosition.z);
            action.put("yaw", savedYaw);
            action.put("pitch", savedPitch);
            action.put("onGround", false);
            TickScheduler.schedule(action);
        }
    }

    @Override
    public void onTick(MinecraftClient client, ClientPlayerEntity player, ClientWorld world) {
        if (!isEnabled()) return;

        // FreeCam movement is handled by the native client's input hooks
        // This module just ensures the player state is correct
    }
}