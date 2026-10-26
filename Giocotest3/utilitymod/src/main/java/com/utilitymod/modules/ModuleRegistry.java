package com.utilitymod.modules;

import com.utilitymod.modules.impl.*;
import com.utilitymod.UtilityMod;

/**
 * Registers all built-in modules.
 * 
 * <p>Call {@link #registerAll()} during mod initialization.
 */
public final class ModuleRegistry {
    private ModuleRegistry() {}

    public static void registerAll() {
        // Movement modules
        new AutoWalkModule();
        new AutoSprintModule();
        new NoClipModule();

        // Player modules
        new AutoEatModule();
        new FreeCamModule();

        // Combat modules (placeholders for future)
        // new AutoAttackModule();
        // new CriticalsModule();

        // World modules (placeholders for future)
        // new AutoMineModule();
        // new ScaffoldModule();

        // Misc modules (placeholders for future)
        // new ChestESPModule();
        // new TracerModule();

        UtilityMod.LOGGER.info("Registered {} modules", ModuleManager.getAll().size());
    }
}