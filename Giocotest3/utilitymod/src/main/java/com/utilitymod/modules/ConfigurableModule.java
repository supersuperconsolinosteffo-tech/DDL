package com.utilitymod.modules;

/**
 * Interface for modules that can have parameters set at runtime.
 */
public interface ConfigurableModule {
    void setParameter(String name, Object value);
    Object getParameter(String name);
    Map<String, Object> getParameters();
}