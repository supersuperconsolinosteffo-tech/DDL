package com.utilitymod.tick;

import com.utilitymod.UtilityMod;
import com.utilitymod.modules.ModuleManager;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketDispatcher;
import net.minecraft.client.MinecraftClient;

import java.util.ArrayList;
import java.util.List;

/**
 * Schedules all packet actions to be sent at the <strong>start</strong> of a
 * game tick only.
 *
 * <p>This is the single chokepoint through which every mod action is sent to
 * the server. By queuing here and flushing at the start of the tick we
 * guarantee:
 * <ul>
 *   <li>Only existing Minecraft packets are used.</li>
 *   <li>No actions are sent between ticks or at the end of a tick.</li>
 *   <li>Actions are deterministic and reproducible.</li>
 * </ul>
 */
public final class TickScheduler {
    private static final List<PacketAction> PENDING_ACTIONS = new ArrayList<>();
    private static boolean flushScheduled = false;

    private TickScheduler() {}

    /**
     * Called by {@link ClientTickEvents.StartTick} at the very start of the
     * client tick. Flushes any queued actions.
     */
    public static void onStartTick(MinecraftClient client) {
        if (flushScheduled) {
            flushScheduled = false;
            flush(client);
        }

        // Let modules run their per-tick logic (no packet sending here).
        ModuleManager.onTick(client);
    }

    /**
     * Enqueues an action to be sent at the start of the next game tick.
     * <p>This method is safe to call from any thread. The action will be
     * dispatched on the render thread at the start of the next tick.
     */
    public static void schedule(PacketAction action) {
        synchronized (PENDING_ACTIONS) {
            PENDING_ACTIONS.add(action);
            flushScheduled = true;
        }
    }

    /**
     * Schedules a flush for the next tick. Call this from native code when you
     * want to ensure queued actions are sent as soon as possible.
     */
    public static void requestFlush() {
        flushScheduled = true;
    }

    /**
     * Flushes all pending actions immediately. Must be called from the
     * render thread at the start of a tick.
     */
    public static void flush(MinecraftClient client) {
        List<PacketAction> toSend;
        synchronized (PENDING_ACTIONS) {
            toSend = new ArrayList<>(PENDING_ACTIONS);
            PENDING_ACTIONS.clear();
        }

        if (toSend.isEmpty()) {
            return;
        }

        UtilityMod.LOGGER.debug("Flushing {} packet action(s) at start of tick", toSend.size());

        for (PacketAction action : toSend) {
            try {
                PacketDispatcher.dispatch(client, action);
            } catch (Throwable t) {
                UtilityMod.LOGGER.error("Failed to dispatch packet action: {}", action, t);
            }
        }
    }

    /**
     * Clears all pending actions. Useful when disconnecting or resetting.
     */
    public static void clear() {
        synchronized (PENDING_ACTIONS) {
            PENDING_ACTIONS.clear();
        }
        flushScheduled = false;
    }

    public static int pendingCount() {
        synchronized (PENDING_ACTIONS) {
            return PENDING_ACTIONS.size();
        }
    }
}