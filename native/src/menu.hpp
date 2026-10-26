// menu.hpp - ImGui dropdown (ClickGUI-style) interface.
// Design follows restrained design-taste principles documented in README:
// 8px spacing grid, one semantic accent, no gradients/glow/blur, small radii,
// uppercase micro-labels, clear Category > Module > State hierarchy.
#pragma once

namespace uc::menu {

void LoadFonts();
void ApplyStyle();

// Renders one frame of UI (status strip + category dropdown panels when the
// menu is open, HUD when enabled). Called between ImGui NewFrame/Render.
// Never calls JNI: it only reads atomics/snapshots and flips desired state.
void RenderFrame();

}  // namespace uc::menu
