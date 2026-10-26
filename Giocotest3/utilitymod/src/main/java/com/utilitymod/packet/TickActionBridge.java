package com.utilitymod.packet;

import com.utilitymod.tick.TickScheduler;
import com.utilitymod.UtilityMod;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Bridge between native code (JNI/JVMTI) and the Java tick scheduler.
 *
 * <p>Native code calls {@link #enqueue(int, int, Map)} to request actions.
 * The actions are converted to {@link PacketAction} and scheduled for the
 * next game tick.
 */
public final class TickActionBridge {
    private static final TickActionBridge INSTANCE = new TickActionBridge();
    private final ConcurrentLinkedQueue<PendingAction> queue = new ConcurrentLinkedQueue<>();
    private final AtomicInteger sequenceId = new AtomicInteger(0);

    // Module ID -> Module Name mapping (shared with native code)
    private static final Map<Integer, String> MODULE_IDS = Map.of(
        0, "autowalk",
        1, "autoeat",
        2, "autosprint",
        3, "freecam",
        4, "noclip"
    );

    // Action IDs (shared with native code)
    private static final Map<Integer, String> ACTION_IDS = Map.of(
        0, "enable",
        1, "disable",
        2, "toggle",
        3, "set_param"
    );

    private TickActionBridge() {}

    public static TickActionBridge getInstance() {
        return INSTANCE;
    }

    /**
     * Called from native code via JNI.
     *
     * @param moduleId  the module ID (see MODULE_IDS)
     * @param actionId  the action ID (see ACTION_IDS)
     * @param params    additional parameters
     */
    public void enqueue(int moduleId, int actionId, Map<String, Object> params) {
        String moduleName = MODULE_IDS.get(moduleId);
        String actionName = ACTION_IDS.get(actionId);

        if (moduleName == null || actionName == null) {
            UtilityMod.LOGGER.warn("Unknown module/action from native: moduleId={}, actionId={}", moduleId, actionId);
            return;
        }

        queue.add(new PendingAction(moduleName, actionName, params));
    }

    /**
     * Processes queued actions. Called at the start of each tick from
     * {@link TickScheduler#onStartTick}.
     */
    public void processQueue() {
        PendingAction action;
        while ((action = queue.poll()) != null) {
            try {
                handle(action);
            } catch (Throwable t) {
                UtilityMod.LOGGER.error("Failed to process native action: {}", action, t);
            }
        }
    }

    private void handle(PendingAction action) {
        var module = ModuleManager.get(action.moduleName);
        if (module == null) {
            UtilityMod.LOGGER.warn("Module not found: {}", action.moduleName);
            return;
        }

        switch (action.actionName) {
            case "enable" -> ModuleManager.enable(action.moduleName);
            case "disable" -> ModuleManager.disable(action.moduleName);
            case "toggle" -> ModuleManager.toggle(action.moduleName);
            case "set_param" -> {
                // Handle parameter setting
                String paramName = (String) action.params.get("param");
                Object value = action.params.get("value");
                if (paramName != null && module instanceof ConfigurableModule) {
                    ((ConfigurableModule) module).setParameter(paramName, value);
                }
            }
        }
    }

    public int getQueueSize() {
        return queue.size();
    }

    private record PendingAction(String moduleName, String actionName, Map<String, Object> params) {}
}