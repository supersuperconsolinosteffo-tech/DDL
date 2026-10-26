package com.utilityclient.bridge;

/**
 * JNI landing pad for the UtilityClient native DLL.
 *
 * <p>The DLL finds this already-loaded class with JVMTI ({@code GetLoadedClasses}),
 * because a plain {@code FindClass} from an attached native thread cannot see
 * Fabric's class loader. It then registers the two natives below with
 * {@code RegisterNatives}. This class deliberately contains <em>no</em> module
 * or packet logic: it exists so that every native action can be executed at
 * exactly one moment - the start of a client tick - on the client thread.
 *
 * <p>Before injection (and after the DLL unloads and unregisters its natives),
 * calling a native throws {@link UnsatisfiedLinkError}. That is expected and
 * handled with a short cooldown so the bridge is a no-op without the DLL.
 */
public final class NativeGate {
    /** Ticks to wait after a missing-native failure before trying again. */
    private static final int RETRY_COOLDOWN = 100; // 5 seconds at 20 TPS

    private static int cooldown = 0;

    private NativeGate() {
    }

    /** Implemented by UtilityClient.dll. Runs all module logic + packet flush. */
    public static native void nativeOnStartTick();

    /** Implemented by UtilityClient.dll. Returns {@code true} while injected. */
    public static native boolean nativeIsInjected();

    /** Called once per client tick, at its very start, from the Fabric event. */
    static void onStartClientTick() {
        if (cooldown > 0) {
            cooldown--;
            return;
        }
        try {
            nativeOnStartTick();
        } catch (UnsatisfiedLinkError e) {
            // DLL not injected (yet) or already unloaded. Stay silent and retry later.
            cooldown = RETRY_COOLDOWN;
        }
    }

    /** @return {@code true} if the native DLL is currently injected and registered. */
    public static boolean isNativeActive() {
        try {
            return nativeIsInjected();
        } catch (UnsatisfiedLinkError e) {
            return false;
        }
    }
}
