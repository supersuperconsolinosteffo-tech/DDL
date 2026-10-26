package com.utilitymod.modules;

import java.util.HashMap;
import java.util.Map;

/**
 * Base class for modules with configurable parameters.
 */
public abstract class ConfigurableModuleBase extends Module implements ConfigurableModule {
    private final Map<String, Object> parameters = new HashMap<>();

    public ConfigurableModuleBase(String id, String name, String description, Category category) {
        super(id, name, description, category);
    }

    @Override
    public void setParameter(String name, Object value) {
        parameters.put(name, value);
    }

    @Override
    public Object getParameter(String name) {
        return parameters.get(name);
    }

    @Override
    public Map<String, Object> getParameters() {
        return new HashMap<>(parameters);
    }
}