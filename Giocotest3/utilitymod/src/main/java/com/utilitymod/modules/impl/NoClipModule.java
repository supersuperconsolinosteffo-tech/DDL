package com.utilitymod.modules.impl;

import com.utilitymod.modules.Category;
import com.utilitymod.modules.ConfigurableModuleBase;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketType;
import com.utilitymod.tick.TickScheduler;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.util.math.BlockPos;

/**
 * NoClip - allows player to move through blocks.
 * 
 * <p>Implemented by modifying collision detection on the client side.
 * The server will still do collision checks, so this works best in
 * single-player or on servers with anti-cheat disabled.
 */
public class NoClipModule extends ConfigurableModuleBase {
    private boolean wasNoClip;

    public NoClipModule() {
        super("noclip", "NoClip", "Move through blocks", Category.MOVEMENT);
        setParameter("speed", 1.0);
    }

    @Override
    public void onEnable() {
        MinecraftClient client = MinecraftClient.getInstance();
        ClientPlayerEntity player = client.player;
        if (player != null) {
            wasNoClip = player.noClip;
            player.noClip = true;
        }
    }

    @Override
    public void onDisable() {
        MinecraftClient client = MinecraftClient.getInstance();
        ClientPlayerEntity player = client.player;
        if (player != null) {
            player.noClip = wasNoClip;
        }
    }

    @Override
    public void onTick(MinecraftClient client, ClientPlayerEntity player, ClientWorld world) {
        if (!isEnabled()) return;

        // NoClip is handled by the player.noClip flag
        // This module just ensures it stays enabled
        if (!player.noClip) {
            player.noClip = true;
        }
    }
}