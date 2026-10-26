package com.utilitymod.modules;

/**
 * Module categories for UI organization.
 */
public enum Category {
    MOVEMENT("Movement"),
    COMBAT("Combat"),
    PLAYER("Player"),
    WORLD("World"),
    MISC("Miscellaneous");

    private final String displayName;

    Category(String displayName) {
        this.displayName = displayName;
    }

    public String getDisplayName() {
        return displayName;
    }
}