package com.utilitymod.nativeclient;

import com.utilitymod.UtilityMod;
import com.utilitymod.packet.TickActionBridge;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Map;

/**
 * Bridge to the native C++ client layer (imgui + minhook + JNI/JVMTI).
 *
 * <p>The native library is loaded from the game's configuration directory so
 * it survives across launches and can be updated independently of the mod jar.
 */
public final class NativeBridge {
    private static boolean loaded = false;
    private static Path nativeDir;

    private NativeBridge() {}

    public static boolean isLoaded() {
        return loaded;
    }

    public static Path getNativeDir() {
        if (nativeDir == null) {
            nativeDir = Paths.get(System.getProperty("user.home"), ".utilitymod", "native");
        }
        return nativeDir;
    }

    /**
     * Attempts to load the native library. If it is missing, the mod falls
     * back to Java-only mode (no imgui menu, no minhook hooks).
     */
    public static void load() {
        try {
            Path dir = getNativeDir();
            Files.createDirectories(dir);

            // Try to load from the config directory first, then from the jar resources.
            Path libPath = dir.resolve("utilitymod_native.dll");
            if (Files.exists(libPath)) {
                System.load(libPath.toString());
                loaded = true;
                UtilityMod.LOGGER.info("Loaded native library from: {}", libPath);
            } else {
                // Fall back to loading from the classpath / jar.
                System.loadLibrary("utilitymod_native");
                loaded = true;
                UtilityMod.LOGGER.info("Loaded native library from classpath.");
            }
        } catch (UnsatisfiedLinkError e) {
            UtilityMod.LOGGER.warn("Native library not found: {}", e.getMessage());
            loaded = false;
        } catch (Exception e) {
            UtilityMod.LOGGER.warn("Failed to load native library", e);
            loaded = false;
        }
    }

    // --- Native method declarations (implemented in entry_point.cpp) ---

    /**
     * Called from native code to log messages.
     */
    public static native void nativeLog(String message);

    /**
     * Called from native code to report an error.
     */
    public static native void nativeError(String message);

    /**
     * Called from native code to request a game tick packet flush.
     * The actual packet sending happens in Java on the start of the tick.
     */
    public static native void requestTickAction(int moduleId, int actionId, Map<String, String> params);

    /**
     * Gets the current player's X position.
     */
    public static native double getPlayerX();

    /**
     * Gets the current player's Y position.
     */
    public static native double getPlayerY();

    /**
     * Gets the current player's Z position.
     */
    public static native double getPlayerZ();

    /**
     * Gets the current player's yaw.
     */
    public static native float getPlayerYaw();

    /**
     * Gets the current player's pitch.
     */
    public static native float getPlayerPitch();

    /**
     * Checks if the player is alive.
     */
    public static native boolean isPlayerAlive();

    /**
     * Gets the player's entity ID.
     */
    public static native int getEntityId();

    // --- Java -> Native callbacks (called from Java) ---

    /**
     * Log a message to the native console.
     */
    public static void logToNative(String message) {
        if (loaded) {
            nativeLog(message);
        }
    }

    /**
     * Report an error to the native console.
     */
    public static void errorToNative(String message) {
        if (loaded) {
            nativeError(message);
        }
    }

    /**
     * Request a tick action from Java to native (for syncing state).
     */
    public static void requestTickActionFromJava(int moduleId, int actionId, Map<String, String> params) {
        if (loaded) {
            requestTickAction(moduleId, actionId, params);
        }
    }

    // --- Java-side implementations (called from native via JNI) ---

    /**
     * Called from native code via JNI to log messages.
     */
    public static void onNativeLog(String message) {
        UtilityMod.LOGGER.info("[Native] {}", message);
    }

    /**
     * Called from native code via JNI to report an error.
     */
    public static void onNativeError(String message) {
        UtilityMod.LOGGER.error("[Native Error] {}", message);
    }

    /**
     * Called from native code via JNI to request a tick action.
     */
    public static void onRequestTickAction(int moduleId, int actionId, Map<String, String> params) {
        TickActionBridge.getInstance().enqueue(moduleId, actionId, params);
    }

    /**
     * Called from native code via JNI to get player X position.
     */
    public static double onGetPlayerX() {
        var mc = net.minecraft.client.MinecraftClient.getInstance();
        var player = mc.player;
        return player != null ? player.getX() : 0.0;
    }

    /**
     * Called from native code via JNI to get player Y position.
     */
    public static double onGetPlayerY() {
        var mc = net.minecraft.client.MinecraftClient.getInstance();
        var player = mc.player;
        return player != null ? player.getY() : 0.0;
    }

    /**
     * Called from native code via JNI to get player Z position.
     */
    public static double onGetPlayerZ() {
        var mc = net.minecraft.client.MinecraftClient.getInstance();
        var player = mc.player;
        return player != null ? player.getZ() : 0.0;
    }

    /**
     * Called from native code via JNI to get player yaw.
     */
    public static float onGetPlayerYaw() {
        var mc = net.minecraft.client.MinecraftClient.getInstance();
        var player = mc.player;
        return player != null ? player.getYaw() : 0.0f;
    }

    /**
     * Called from native code via JNI to get player pitch.
     */
    public static float onGetPlayerPitch() {
        var mc = net.minecraft.client.MinecraftClient.getInstance();
        var player = mc.player;
        return player != null ? player.getPitch() : 0.0f;
    }

    /**
     * Called from native code via JNI to check if player is alive.
     */
    public static boolean onIsPlayerAlive() {
        var mc = net.minecraft.client.MinecraftClient.getInstance();
        var player = mc.player;
        return player != null && player.isAlive();
    }

    /**
     * Called from native code via JNI to get entity ID.
     */
    public static int onGetEntityId() {
        var mc = net.minecraft.client.MinecraftClient.getInstance();
        var player = mc.player;
        return player != null ? player.getId() : -1;
    }
}