// hooks.hpp - MinHook on wglSwapBuffers (Minecraft renders with OpenGL via
// LWJGL, so the swap-buffers call is the one stable per-frame hook point)
// plus a WndProc subclass for INSERT/END and ImGui input.
#pragma once

namespace uc::hooks {

// MH_Initialize + hook wglSwapBuffers. Callable in a retry loop until
// opengl32 is present.
bool Install();

// Disable+remove the hook and uninitialize MinHook. Bootstrap calls this
// after the render thread has torn ImGui down, right before FreeLibrary.
void Uninstall();

bool Installed();

}  // namespace uc::hooks
