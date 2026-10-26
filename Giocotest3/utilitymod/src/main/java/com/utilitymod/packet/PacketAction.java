package com.utilitymod.packet;

import java.util.HashMap;
import java.util.Map;

/**
 * Represents a single packet action to be sent at the start of a game tick.
 *
 * <p>Only existing Minecraft packets are used. Each action maps to a specific
 * packet type and carries the parameters needed to construct it.
 */
public final class PacketAction {
    private final PacketType type;
    private final Map<String, Object> params;

    public PacketAction(PacketType type) {
        this.type = type;
        this.params = new HashMap<>();
    }

    public PacketAction(PacketType type, Map<String, Object> params) {
        this.type = type;
        this.params = new HashMap<>(params);
    }

    public PacketType getType() {
        return type;
    }

    public Map<String, Object> getParams() {
        return params;
    }

    public void put(String key, Object value) {
        params.put(key, value);
    }

    public Object get(String key) {
        return params.get(key);
    }

    @SuppressWarnings("unchecked")
    public <T> T get(String key, T defaultValue) {
        Object value = params.get(key);
        if (value == null) {
            return defaultValue;
        }
        return (T) value;
    }

    @Override
    public String toString() {
        return "PacketAction{type=" + type + ", params=" + params + "}";
    }
}