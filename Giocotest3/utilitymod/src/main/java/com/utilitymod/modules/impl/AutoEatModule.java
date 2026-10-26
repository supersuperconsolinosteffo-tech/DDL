package com.utilitymod.modules.impl;

import com.utilitymod.modules.Category;
import com.utilitymod.modules.Module;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketType;
import com.utilitymod.tick.TickScheduler;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.item.Items;
import net.minecraft.util.Hand;

/**
 * AutoEat - automatically eats food when hunger is low.
 *
 * <p>Schedules item interaction packets at the start of each tick.
 */
public class AutoEatModule extends Module {
    private static final float HUNGER_THRESHOLD = 6.0f; // hunger bar units
    private int eatCooldown = 0;

    public AutoEatModule() {
        super("autoeat", "AutoEat", "Automatically eats food when hunger is low", Category.PLAYER);
    }

    @Override
    public void onTick(MinecraftClient client, ClientPlayerEntity player, ClientWorld world) {
        if (!isEnabled()) return;

        if (eatCooldown > 0) {
            eatCooldown--;
            return;
        }

        // Check hunger level
        float hunger = player.getHungerManager().getFoodLevel();
        if (hunger >= HUNGER_THRESHOLD) return;

        // Find food in inventory
        int foodSlot = -1;
        for (int i = 0; i < 9; i++) {
            var stack = player.getInventory().getStack(i);
            if (stack.isFood()) {
                foodSlot = i;
                break;
            }
        }

        if (foodSlot == -1) return;

        // Switch to food slot and eat
        int previousSlot = player.getInventory().selectedSlot;
        if (foodSlot != previousSlot) {
            // Schedule a slot change (this is client-side only, but we can simulate)
            // For actual packet-based eating, we just use the current hand
        }

        // Schedule item use (right-click with food in hand)
        scheduleInteractItem();
        eatCooldown = 20; // 1 second cooldown
    }
}