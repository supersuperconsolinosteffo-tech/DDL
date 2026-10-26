package com.utilityclient.bridge;

import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Client entrypoint. Registers exactly one thing: the start-of-tick callback
 * that hands control to the native DLL. No END tick event, no packets, no
 * mixins, no rendering here - by design (see the project README).
 */
public final class UtilityBridgeMod implements ClientModInitializer {
    private static final Logger LOGGER = LoggerFactory.getLogger("UtilityClient Bridge");

    @Override
    public void onInitializeClient() {
        // START tick only. Fabric fires this at the head of Minecraft.tick(),
        // on the client thread - the single legal moment for this client to act.
        ClientTickEvents.START_CLIENT_TICK.register(client -> NativeGate.onStartClientTick());

        // Force NativeGate to load now so the DLL's JVMTI scan can find it even
        // before the first tick runs.
        NativeGate.isNativeActive();

        LOGGER.info("UtilityClient Bridge ready (start-of-tick JNI gate; DLL does the rest)");
    }
}
