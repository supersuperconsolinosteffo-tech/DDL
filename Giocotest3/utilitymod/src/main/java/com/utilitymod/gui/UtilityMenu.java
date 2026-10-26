package com.utilitymod.gui;

import com.utilitymod.modules.Category;
import com.utilitymod.modules.Module;
import com.utilitymod.modules.ModuleManager;
import com.utilitymod.UtilityMod;
import com.utilitymod.client.UtilityModClient;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.client.gui.widget.ButtonWidget;
import net.minecraft.text.Text;
import net.minecraft.util.Colors;
import net.minecraft.util.math.MathHelper;

import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/**
 * Main utility menu - a clean, dropdown-style GUI.
 *
 * <p>Design principles (inspired by tasteskill/impeccable):
 * <ul>
 *   <li>High contrast, readable typography</li>
 *   <li>Consistent 8px spacing grid</li>
 *   <li>Clear visual hierarchy: category > module > state</li>
 *   <li>Subtle motion on hover/focus (no excessive animation)</li>
 *   <li>Keyboard navigable</li>
 *   <li>Responsive to window resize</li>
 * </ul>
 */
public class UtilityMenu extends Screen {
    // Design tokens (tasteskill: consistent spacing/typography)
    private static final int PANEL_WIDTH = 320;
    private static final int PANEL_PADDING = 16;
    private static final int ITEM_HEIGHT = 28;
    private static final int CATEGORY_HEIGHT = 32;
    private static final int SPACING = 8;
    private static final int CORNER_RADIUS = 4;

    // Colors (impeccable: semantic, not decorative)
    private static final int BG_COLOR = 0xF01E1E2E;        // Near-black with warmth
    private static final int PANEL_COLOR = 0xF0252536;     // Elevated surface
    private static final int BORDER_COLOR = 0x803D3D5C;    // Subtle border
    private static final int TEXT_PRIMARY = 0xFFCDD6F4;    // High contrast text
    private static final int TEXT_SECONDARY = 0xFF9399B2;  // Muted text
    private static final int TEXT_MUTED = 0xFF6C7086;      // Disabled text
    private static final int ACCENT_COLOR = 0xFF89B4FA;    // Primary accent (blue)
    private static final int ACCENT_HOVER = 0xFFB4BEFE;    // Hover state
    private static final int SUCCESS_COLOR = 0xFFA6E3A1;   // Enabled state
    private static final int DANGER_COLOR = 0xFFF38BA8;    // Error/warning
    private static final int HOVER_BG = 0x603D3D5C;        // Subtle hover

    private final MinecraftClient client;
    private int panelX;
    private int panelY;
    private int panelHeight;
    private boolean open = false;
    private float openProgress = 0.0f;
    private int selectedCategoryIndex = 0;
    private int hoveredItemIndex = -1;
    private Category[] categories;
    private Map<Category, List<Module>> modulesByCategory;

    public UtilityMenu(MinecraftClient client) {
        super(Text.translatable("gui.utilitymod.title"));
        this.client = client;
    }

    @Override
    protected void init() {
        super.init();
        rebuildCategoryMap();
        calculateLayout();
    }

    private void rebuildCategoryMap() {
        var allModules = ModuleManager.getAll();
        modulesByCategory = allModules.stream()
            .collect(Collectors.groupingBy(Module::getCategory));

        // Sort categories by display name
        categories = modulesByCategory.keySet().stream()
            .sorted(Comparator.comparing(Category::getDisplayName))
            .toArray(Category[]::new);

        // Sort modules within each category
        modulesByCategory.values().forEach(list ->
            list.sort(Comparator.comparing(Module::getName))
        );
    }

    private void calculateLayout() {
        int screenWidth = this.width;
        int screenHeight = this.height;

        // Panel centered horizontally, slightly above center vertically
        panelX = (screenWidth - PANEL_WIDTH) / 2;
        int totalItems = modulesByCategory.values().stream().mapToInt(List::size).sum();
        int totalCategories = categories.length;
        panelHeight = PANEL_PADDING * 2
            + totalCategories * CATEGORY_HEIGHT
            + totalItems * ITEM_HEIGHT
            + totalCategories * SPACING;

        panelY = (screenHeight - panelHeight) / 2;
    }

    public void toggle() {
        open = !open;
        if (open) {
            rebuildCategoryMap();
            calculateLayout();
            // Grab mouse focus for keyboard navigation
            client.setScreen(this);
        } else {
            close();
        }
    }

    @Override
    public void close() {
        open = false;
        client.setScreen(null);
        UtilityModClient.closeMenu();
    }

    @Override
    public void tick(MinecraftClient client) {
        if (!open) {
            close();
            return;
        }

        // Smooth open animation (impeccable: subtle spring motion)
        float target = open ? 1.0f : 0.0f;
        openProgress = MathHelper.lerp(0.15f, openProgress, target);

        // Close if progress is essentially zero and we're closing
        if (!open && openProgress < 0.01f) {
            close();
        }
    }

    @Override
    public void render(DrawContext context, int mouseX, int mouseY, float delta) {
        if (!open && openProgress <= 0.0f) return;

        // Render background overlay
        renderOverlay(context);

        // Render animated panel
        renderPanel(context, mouseX, mouseY);

        super.render(context, mouseX, mouseY, delta);
    }

    private void renderOverlay(DrawContext context) {
        int alpha = (int) (0.6f * openProgress * 255);
        int color = (alpha << 24) | 0x0D0D12; // Dark overlay
        context.fill(0, 0, width, height, color);
    }

    private void renderPanel(DrawContext context, int mouseX, int mouseY) {
        // Calculate animated position/scale
        float scale = 0.95f + 0.05f * openProgress;
        int scaledWidth = (int) (PANEL_WIDTH * scale);
        int scaledHeight = (int) (panelHeight * scale);
        int x = (width - scaledWidth) / 2;
        int y = (height - scaledHeight) / 2;

        // Panel background with subtle shadow
        renderRoundedRect(context, x - 4, y - 4, scaledWidth + 8, scaledHeight + 8, CORNER_RADIUS + 2, 0x40000000);
        renderRoundedRect(context, x, y, scaledWidth, scaledHeight, CORNER_RADIUS, PANEL_COLOR);
        renderRoundedRectBorder(context, x, y, scaledWidth, scaledHeight, CORNER_RADIUS, BORDER_COLOR);

        // Title bar
        int titleY = y + PANEL_PADDING;
        context.drawTextWithShadow(
            client.textRenderer,
            Text.translatable("gui.utilitymod.title"),
            x + PANEL_PADDING,
            titleY,
            TEXT_PRIMARY
        );

        // Version badge
        String version = "v" + UtilityMod.mod_version;
        int versionWidth = client.textRenderer.getWidth(version);
        context.drawTextWithShadow(
            client.textRenderer,
            Text.literal(version),
            x + scaledWidth - PANEL_PADDING - versionWidth,
            titleY,
            TEXT_MUTED
        );

        // Module list
        int currentY = titleY + client.textRenderer.fontHeight + SPACING * 2;
        hoveredItemIndex = -1;

        for (int catIdx = 0; catIdx < categories.length; catIdx++) {
            Category category = categories[catIdx];
            List<Module> modules = modulesByCategory.get(category);

            // Category header
            boolean catHovered = isMouseOver(mouseX, mouseY, x + PANEL_PADDING, currentY, scaledWidth - PANEL_PADDING * 2, CATEGORY_HEIGHT);
            renderCategoryHeader(context, x, currentY, scaledWidth, category, catHovered, catIdx == selectedCategoryIndex);
            currentY += CATEGORY_HEIGHT;

            // Module items
            for (int modIdx = 0; modIdx < modules.size(); modIdx++) {
                Module module = modules.get(modIdx);
                boolean itemHovered = isMouseOver(mouseX, mouseY, x + PANEL_PADDING, currentY, scaledWidth - PANEL_PADDING * 2, ITEM_HEIGHT);
                if (itemHovered) {
                    hoveredItemIndex = modIdx;
                }
                renderModuleItem(context, x, currentY, scaledWidth, module, itemHovered);
                currentY += ITEM_HEIGHT;
            }

            currentY += SPACING;
        }

        // Footer hint
        String hint = "Press O to close • ↑/↓ to navigate • Enter to toggle";
        int hintWidth = client.textRenderer.getWidth(hint);
        context.drawTextWithShadow(
            client.textRenderer,
            Text.literal(hint),
            x + (scaledWidth - hintWidth) / 2,
            y + scaledHeight - PANEL_PADDING - client.textRenderer.fontHeight,
            TEXT_MUTED
        );
    }

    private void renderCategoryHeader(DrawContext context, int panelX, int y, int panelWidth, Category category, boolean hovered, boolean selected) {
        int x = panelX + PANEL_PADDING;
        int width = panelWidth - PANEL_PADDING * 2;

        // Background
        if (hovered || selected) {
            renderRoundedRect(context, x, y, width, CATEGORY_HEIGHT, CORNER_RADIUS, hovered ? HOVER_BG : (BORDER_COLOR & 0x00FFFFFF) | 0x40000000);
        }

        // Category icon indicator
        int indicatorColor = selected ? ACCENT_COLOR : TEXT_MUTED;
        context.fill(x + 4, y + CATEGORY_HEIGHT / 2 - 4, x + 6, y + CATEGORY_HEIGHT / 2 + 4, indicatorColor);

        // Category name
        context.drawTextWithShadow(
            client.textRenderer,
            Text.translatable("category.utilitymod." + category.name().toLowerCase()),
            x + 14,
            y + (CATEGORY_HEIGHT - client.textRenderer.fontHeight) / 2,
            hovered ? ACCENT_HOVER : TEXT_PRIMARY
        );

        // Module count
        int count = modulesByCategory.get(category).size();
        String countText = count + " module" + (count != 1 ? "s" : "");
        int countWidth = client.textRenderer.getWidth(countText);
        context.drawTextWithShadow(
            client.textRenderer,
            Text.literal(countText),
            x + width - 12 - countWidth,
            y + (CATEGORY_HEIGHT - client.textRenderer.fontHeight) / 2,
            TEXT_SECONDARY
        );
    }

    private void renderModuleItem(DrawContext context, int panelX, int y, int panelWidth, Module module, boolean hovered) {
        int x = panelX + PANEL_PADDING;
        int width = panelWidth - PANEL_PADDING * 2;

        // Background
        if (hovered) {
            renderRoundedRect(context, x, y, width, ITEM_HEIGHT, CORNER_RADIUS, HOVER_BG);
        }

        // Enabled indicator (left side)
        int indicatorX = x + 8;
        int indicatorY = y + (ITEM_HEIGHT - 12) / 2;
        int indicatorColor = module.isEnabled() ? SUCCESS_COLOR : TEXT_MUTED;
        renderRoundedRect(context, indicatorX, indicatorY, 12, 12, 2, indicatorColor | 0x30000000);
        if (module.isEnabled()) {
            renderRoundedRect(context, indicatorX + 2, indicatorY + 2, 8, 8, 1, indicatorColor);
        }

        // Module name
        int textColor = module.isEnabled() ? TEXT_PRIMARY : TEXT_SECONDARY;
        if (hovered) textColor = ACCENT_HOVER;
        context.drawTextWithShadow(
            client.textRenderer,
            Text.translatable("module.utilitymod." + module.getId()),
            indicatorX + 20,
            y + (ITEM_HEIGHT - client.textRenderer.fontHeight) / 2,
            textColor
        );

        // Keybind hint (right side)
        if (module.getKeybind() != -1) {
            String keyName = net.minecraft.client.util.InputMappings.fromCode(module.getKeybind()).getTranslatableKey();
            String keyText = "[" + keyName + "]";
            int keyWidth = client.textRenderer.getWidth(keyText);
            context.drawTextWithShadow(
                client.textRenderer,
                Text.literal(keyText),
                x + width - 12 - keyWidth,
                y + (ITEM_HEIGHT - client.textRenderer.fontHeight) / 2,
                TEXT_MUTED
            );
        }

        // Description tooltip on hover
        if (hovered && !module.getDescription().isEmpty()) {
            renderTooltip(context, module.getDescription(), mouseX, mouseY);
        }
    }

    private boolean isMouseOver(int mouseX, int mouseY, int x, int y, int width, int height) {
        return mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height;
    }

    private void renderRoundedRect(DrawContext context, int x, int y, int width, int height, int radius, int color) {
        // Simple rounded rect using multiple fills
        context.fill(x + radius, y, x + width - radius, y + height, color);
        context.fill(x, y + radius, x + width, y + height - radius, color);
        // Corners (approximate)
        for (int i = 0; i < radius; i++) {
            int alpha = (color >> 24) & 0xFF;
            int rgb = color & 0xFFFFFF;
            int c = (alpha << 24) | rgb;
            context.fill(x + radius - i, y + i, x + radius - i + 1, y + i + 1, c);
            context.fill(x + width - radius + i, y + i, x + width - radius + i + 1, y + i + 1, c);
            context.fill(x + radius - i, y + height - i - 1, x + radius - i + 1, y + height - i, c);
            context.fill(x + width - radius + i, y + height - i - 1, x + width - radius + i + 1, y + height - i, c);
        }
    }

    private void renderRoundedRectBorder(DrawContext context, int x, int y, int width, int height, int radius, int color) {
        context.drawHorizontalLine(x + radius, x + width - radius, y, color);
        context.drawHorizontalLine(x + radius, x + width - radius, y + height - 1, color);
        context.drawVerticalLine(x, y + radius, y + height - radius, color);
        context.drawVerticalLine(x + width - 1, y + radius, y + height - radius, color);
    }

    @Override
    public boolean mouseClicked(double mouseX, double mouseY, int button) {
        if (button != 0) return false;

        int currentY = panelY + PANEL_PADDING * 2 + client.textRenderer.fontHeight + SPACING * 2;

        for (Category category : categories) {
            List<Module> modules = modulesByCategory.get(category);
            currentY += CATEGORY_HEIGHT;

            for (Module module : modules) {
                if (isMouseOver((int) mouseX, (int) mouseY, panelX + PANEL_PADDING, currentY, PANEL_WIDTH - PANEL_PADDING * 2, ITEM_HEIGHT)) {
                    ModuleManager.toggle(module.getId());
                    return true;
                }
                currentY += ITEM_HEIGHT;
            }
            currentY += SPACING;
        }

        return super.mouseClicked(mouseX, mouseY, button);
    }

    @Override
    public boolean keyPressed(int keyCode, int scanCode, int modifiers) {
        if (keyCode == net.minecraft.client.option.KeyBindingHelper.getKey("key.utilitymod.open_menu").getDefaultKey().getCode()) {
            close();
            return true;
        }

        if (keyCode == 265) { // Up arrow
            navigate(-1);
            return true;
        }
        if (keyCode == 264) { // Down arrow
            navigate(1);
            return true;
        }
        if (keyCode == 257) { // Enter
            toggleSelected();
            return true;
        }
        if (keyCode == 256) { // Escape
            close();
            return true;
        }

        return super.keyPressed(keyCode, scanCode, modifiers);
    }

    private void navigate(int direction) {
        // Simple flat navigation through all modules
        List<Module> allModules = ModuleManager.getAll();
        if (allModules.isEmpty()) return;

        // Find currently hovered or first enabled
        int currentIndex = hoveredItemIndex >= 0 ? hoveredItemIndex : 0;
        int newIndex = MathHelper.clamp(currentIndex + direction, 0, allModules.size() - 1);
        hoveredItemIndex = newIndex;
    }

    private void toggleSelected() {
        List<Module> allModules = ModuleManager.getAll();
        if (hoveredItemIndex >= 0 && hoveredItemIndex < allModules.size()) {
            ModuleManager.toggle(allModules.get(hoveredItemIndex).getId());
        }
    }

    @Override
    public boolean shouldPause() {
        return false; // Don't pause game when menu is open
    }

    @Override
    public boolean shouldCloseOnEsc() {
        return true;
    }
}