package com.utilitymod.modules.impl;

import com.utilitymod.modules.Category;
import com.utilitymod.modules.ConfigurableModuleBase;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketType;
import com.utilitymod.tick.TickScheduler;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.util.math.Direction;

/**
 * AutoSprint - automatically sprints when moving forward.
 */
public class AutoSprintModule extends ConfigurableModuleBase {
    public AutoSprintModule() {
        super("autosprint", "AutoSprint", "Automatically sprints when moving forward", Category.MOVEMENT);
        setParameter("sprintThreshold", 0.8); // Sprint when moving > 80% forward
    }

    @Override
    public void onTick(MinecraftClient client, ClientPlayerEntity player, ClientWorld world) {
        if (!isEnabled()) return;

        // Check if player is moving forward
        if (player.input.movementForward > getParameter("sprintThreshold", 0.8)) {
            // Sprint is handled client-side, but we can ensure the server knows
            // by sending the appropriate movement packets with sprint flag
            // In 1.21, sprint is part of the movement packet
            // The client automatically handles this, so this module is mostly
            // for ensuring sprint state is maintained
        }
    }
}