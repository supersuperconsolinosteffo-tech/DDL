package com.utilitymod;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Main entrypoint for the UtilityMod.
 *
 * <p>This mod is designed for single-player use but is also compatible with
 * test servers. It never uses custom packets — all actions are sent through
 * the existing Minecraft packet set, at the start of a game tick only.
 */
public final class UtilityMod {
    public static final String MOD_ID = "utilitymod";
    public static final String MOD_NAME = "UtilityMod";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_NAME);

    public static final String NATIVE_LIBRARY = "utilitymod_native";

    private UtilityMod() {}

    public static void init() {
        LOGGER.info("UtilityMod initialized (single-player / test-server mode)");
    }
}