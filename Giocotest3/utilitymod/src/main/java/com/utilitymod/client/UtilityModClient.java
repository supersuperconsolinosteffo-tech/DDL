package com.utilitymod.client;

import com.utilitymod.UtilityMod;
import com.utilitymod.modules.ModuleManager;
import com.utilitymod.modules.ModuleRegistry;
import com.utilitymod.nativeclient.NativeBridge;
import com.utilitymod.packet.TickActionBridge;
import com.utilitymod.tick.TickScheduler;
import com.utilitymod.gui.UtilityMenu;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keybinding.v1.KeyBindingHelper;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.options.KeyBinding;
import net.minecraft.client.util.InputMappings;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;

/**
 * Client-side entrypoint for UtilityMod.
 *
 * <p>Registers the menu keybinding, the start-of-tick scheduler and the
 * native client bridge. All packet actions are emitted by
 * {@link TickScheduler} at the start of a game tick only.
 */
public final class UtilityModClient {
    private static final Logger LOGGER = UtilityMod.LOGGER;

    @Nullable
    private static UtilityMenu menu;

    public static void onInitializeClient() {
        UtilityMod.init();

        // Register all built-in modules
        ModuleRegistry.registerAll();

        // Native client bridge (imgui + minhook + JNI/JVMTI).
        // Loaded lazily so the mod still works if the native library is absent.
        try {
            NativeBridge.load();
            LOGGER.info("Native bridge loaded: {}", NativeBridge.isLoaded());
        } catch (Throwable t) {
            LOGGER.warn("Native bridge not available; running Java-only mode", t);
        }

        // Register the menu keybinding (O = open menu, like many utility mods).
        var menuKey = KeyBindingHelper.registerKeyBinding(
            new KeyBinding(
                "key.utilitymod.open_menu",
                InputMappings.Type.KEYSYM,
                79, // 'O'
                "category.utilitymod.main"
            )
        );

        // Register the start-of-tick scheduler. All packet actions are emitted here.
        ClientTickEvents.START_CLIENT_TICK.register(client -> {
            // Process native -> Java action queue
            TickActionBridge.getInstance().processQueue();
            
            // Run tick scheduler (flushes packets, runs modules)
            TickScheduler.onStartTick(client);
        });

        // Handle disconnect
        ClientTickEvents.START_CLIENT_TICK.register(client -> {
            if (client.player == null || client.world == null) {
                ModuleManager.onDisconnect();
            }
        });

        // Open / close the menu on key press.
        ClientTickEvents.START_CLIENT_TICK.register(client -> {
            if (menuKey.wasPressed()) {
                if (menu == null) {
                    menu = new UtilityMenu(client);
                }
                menu.toggle();
            }
            if (menu != null) {
                menu.tick(client);
            }
        });

        LOGGER.info("UtilityMod client initialized with {} modules.", ModuleManager.getAll().size());
    }

    @Nullable
    public static UtilityMenu getMenu() {
        return menu;
    }

    public static void closeMenu() {
        if (menu != null) {
            menu.close();
            menu = null;
        }
    }
}