package com.utilitymod.modules;

import com.utilitymod.UtilityMod;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketType;
import com.utilitymod.tick.TickScheduler;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.util.math.BlockPos;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Manages all utility modules. Modules are self-contained features that
 * schedule packet actions through the {@link TickScheduler}.
 */
public final class ModuleManager {
    private static final Map<String, Module> MODULES = new HashMap<>();
    private static final List<Module> ENABLED_MODULES = new ArrayList<>();

    private ModuleManager() {}

    /**
     * Registers a module. Call from module constructors.
     */
    public static void register(Module module) {
        MODULES.put(module.getId(), module);
        UtilityMod.LOGGER.debug("Registered module: {}", module.getId());
    }

    /**
     * Gets a module by ID.
     */
    public static Module get(String id) {
        return MODULES.get(id);
    }

    /**
     * Returns all registered modules.
     */
    public static List<Module> getAll() {
        return new ArrayList<>(MODULES.values());
    }

    /**
     * Returns all enabled modules.
     */
    public static List<Module> getEnabled() {
        return new ArrayList<>(ENABLED_MODULES);
    }

    /**
     * Enables a module.
     */
    public static void enable(String id) {
        Module module = MODULES.get(id);
        if (module != null && !module.isEnabled()) {
            module.setEnabled(true);
            ENABLED_MODULES.add(module);
            UtilityMod.LOGGER.info("Enabled module: {}", id);
        }
    }

    /**
     * Disables a module.
     */
    public static void disable(String id) {
        Module module = MODULES.get(id);
        if (module != null && module.isEnabled()) {
            module.setEnabled(false);
            ENABLED_MODULES.remove(module);
            UtilityMod.LOGGER.info("Disabled module: {}", id);
        }
    }

    /**
     * Toggles a module.
     */
    public static void toggle(String id) {
        Module module = MODULES.get(id);
        if (module != null) {
            if (module.isEnabled()) {
                disable(id);
            } else {
                enable(id);
            }
        }
    }

    /**
     * Called at the start of every game tick. Lets enabled modules run their
     * logic and schedule packet actions.
     */
    public static void onTick(MinecraftClient client) {
        ClientPlayerEntity player = client.player;
        ClientWorld world = client.world;

        if (player == null || world == null) return;

        for (Module module : ENABLED_MODULES) {
            try {
                module.onTick(client, player, world);
            } catch (Throwable t) {
                UtilityMod.LOGGER.error("Error in module {}: {}", module.getId(), t.getMessage(), t);
            }
        }
    }

    /**
     * Called when the client disconnects. Disables all modules and clears
     * pending actions.
     */
    public static void onDisconnect() {
        for (Module module : ENABLED_MODULES) {
            module.onDisable();
        }
        ENABLED_MODULES.clear();
        TickScheduler.clear();
    }
}