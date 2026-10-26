package com.utilitymod.modules;

import com.utilitymod.UtilityMod;
import com.utilitymod.packet.PacketAction;
import com.utilitymod.packet.PacketType;
import com.utilitymod.tick.TickScheduler;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayerEntity;
import net.minecraft.client.world.ClientWorld;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.Direction;

/**
 * Base class for utility modules.
 *
 * <p>Modules should only schedule packet actions through {@link TickScheduler}.
 * They must not send packets directly or between ticks.
 */
public abstract class Module {
    private final String id;
    private final String name;
    private final String description;
    private final Category category;
    private boolean enabled = false;
    private int keybind = -1; // -1 = no keybind

    public Module(String id, String name, String description, Category category) {
        this.id = id;
        this.name = name;
        this.description = description;
        this.category = category;
        ModuleManager.register(this);
    }

    public String getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public String getDescription() {
        return description;
    }

    public Category getCategory() {
        return category;
    }

    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
        if (enabled) {
            onEnable();
        } else {
            onDisable();
        }
    }

    public int getKeybind() {
        return keybind;
    }

    public void setKeybind(int keybind) {
        this.keybind = keybind;
    }

    /**
     * Called every tick when the module is enabled. Override to implement
     * module logic. Schedule packet actions using {@link TickScheduler#schedule(PacketAction)}.
     */
    public abstract void onTick(MinecraftClient client, ClientPlayerEntity player, ClientWorld world);

    /**
     * Called when the module is enabled.
     */
    protected void onEnable() {}

    /**
     * Called when the module is disabled.
     */
    protected void onDisable() {}

    /**
     * Helper: schedule a simple movement packet.
     */
    protected void scheduleMove(double x, double y, double z, float yaw, float pitch, boolean onGround) {
        var action = new PacketAction(PacketType.MOVE_PLAYER_FULL);
        action.put("x", x);
        action.put("y", y);
        action.put("z", z);
        action.put("yaw", yaw);
        action.put("pitch", pitch);
        action.put("onGround", onGround);
        TickScheduler.schedule(action);
    }

    /**
     * Helper: schedule a look-only packet.
     */
    protected void scheduleLook(float yaw, float pitch, boolean onGround) {
        var action = new PacketAction(PacketType.MOVE_PLAYER_LOOK);
        action.put("yaw", yaw);
        action.put("pitch", pitch);
        action.put("onGround", onGround);
        TickScheduler.schedule(action);
    }

    /**
     * Helper: schedule a position-only packet.
     */
    protected void schedulePosition(double x, double y, double z, boolean onGround) {
        var action = new PacketAction(PacketType.MOVE_PLAYER_POS);
        action.put("x", x);
        action.put("y", y);
        action.put("z", z);
        action.put("onGround", onGround);
        TickScheduler.schedule(action);
    }

    /**
     * Helper: schedule a block interaction.
     */
    protected void scheduleInteractBlock(BlockPos pos, Direction direction, BlockPos hitPos) {
        var action = new PacketAction(PacketType.INTERACT_BLOCK);
        action.put("pos", pos);
        action.put("direction", direction);
        action.put("hitPos", new net.minecraft.util.math.Vec3d(
            hitPos.getX() + 0.5, hitPos.getY() + 0.5, hitPos.getZ() + 0.5
        ));
        TickScheduler.schedule(action);
    }

    /**
     * Helper: schedule an item interaction (right-click).
     */
    protected void scheduleInteractItem() {
        var action = new PacketAction(PacketType.INTERACT_ITEM);
        TickScheduler.schedule(action);
    }

    /**
     * Helper: schedule an entity interaction.
     */
    protected void scheduleInteractEntity(int entityId) {
        var action = new PacketAction(PacketType.INTERACT_ENTITY);
        action.put("entityId", entityId);
        TickScheduler.schedule(action);
    }

    /**
     * Helper: schedule a hand swing.
     */
    protected void scheduleHandSwing() {
        var action = new PacketAction(PacketType.HAND_SWING);
        TickScheduler.schedule(action);
    }

    /**
     * Helper: schedule a player action (digging, dropping, etc.)
     */
    protected void schedulePlayerAction(String actionName, BlockPos pos, Direction direction) {
        var action = new PacketAction(PacketType.PLAYER_ACTION);
        action.put("action", actionName);
        action.put("pos", pos);
        action.put("direction", direction);
        TickScheduler.schedule(action);
    }

    @Override
    public String toString() {
        return "Module{id='" + id + "', name='" + name + "', enabled=" + enabled + '}';
    }
}