package com.utilitymod.modules.impl;

import com.utilitymod.modules.Category;
import com.utilitymod.modules.Module;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketType;
import com.utilitymod.tick.TickScheduler;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.Direction;

/**
 * AutoWalk - automatically walks forward when enabled.
 *
 * <p>Schedules movement packets at the start of each tick.
 */
public class AutoWalkModule extends Module {
    private static final int WALK_SPEED = 4; // blocks per second equivalent

    public AutoWalkModule() {
        super("autowalk", "AutoWalk", "Automatically walks forward", Category.MOVEMENT);
    }

    @Override
    public void onTick(MinecraftClient client, ClientPlayerEntity player, ClientWorld world) {
        if (!isEnabled()) return;

        // Calculate forward vector based on yaw
        float yaw = player.getYaw();
        double motionX = -Math.sin(Math.toRadians(yaw));
        double motionZ = Math.cos(Math.toRadians(yaw));

        // Current position
        double x = player.getX();
        double y = player.getY();
        double z = player.getZ();

        // Target position (move forward slightly)
        double targetX = x + motionX * 0.1;
        double targetZ = z + motionZ * 0.1;

        // Check for collision at target position (simple check)
        if (!world.getBlockState(new BlockPos(targetX, y, targetZ)).getMaterial().isSolid()) {
            // Schedule movement at start of next tick
            schedulePosition(targetX, y, targetZ, player.isOnGround());
        }
    }
}