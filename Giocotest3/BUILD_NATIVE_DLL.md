# UtilityMod Native DLL Build Instructions
# For Minecraft 1.21.11 Fabric mod injection

## Overview
Build the C++ native library (`utilitymod_native.dll`) that provides:
- ImGui dropdown menu (INSERT key)
- MinHook-based function hooks
- JNI/JVMTI bridge to Java mod
- Native module control from Java

## Prerequisites

### Windows Development
1. **Visual Studio 2022** with "Desktop development with C++" workload installed
   - Or install MSVC compiler and Windows SDK separately
2. **CMake** 3.20 or newer
3. **GLFW 3.3+** development libraries (for ImGui backend)
   - Can install via: `vcpkg install glfw3:x64-windows`
   - Or download prebuilt binaries and set include/lib paths
4. **MinHook** source files (included in project):
   - `minhook/buffer.c`, `minhook/hook.c`, `minhook/trampoline.c`
   - These should be in `utilitymod/src/main/cpp/native/minhook/`

### Alternative: MinGW
- MinGW-w64 toolchain
- CMake with MinGW generator
- GLFW and OpenGL headers

## Directory Structure
```
utilitymod/src/main/cpp/native/
├── CMakeLists.txt          ← Build configuration
├── entry_point.cpp         ← JNI exports
├── native_client.cpp       ← Main native client
├── native_client.hpp       ← Native client header
├── jni/                    ← JNI/JVMTI bridge
│   ├── jni_bridge.cpp
│   └── jni_bridge.hpp
├── hooks/                  ← MinHook implementations
│   ├── minecraft_hooks.cpp
│   └── minecraft_hooks.hpp
├── imgui/                  ← ImGui library
│   ├── imgui.cpp
│   ├── imgui_draw.cpp
│   ├── imgui_tables.cpp
│   ├── imgui_widgets.cpp
│   ├── backends/
│   │   ├── imgui_impl_win32.cpp   ← Windows backend
│   │   └── imgui_impl_dx11.cpp    ← D3D11 backend
│   └── imgui_draw.cpp
├── minhook/                ← MinHook source (REQUIRED)
│   ├── buffer.c
│   ├── hook.c
│   └── trampoline.c
└── entry_point.cpp         ← C API exports
```

## Build Steps (Visual Studio / MSVC)

### Step 1: Ensure minhook source is available
```powershell
# MinHook source files should exist in:
# C:\Users\ciaoa\Desktop\Giocotest3\utilitymod\src\main\cpp\native\minhook\
Verify these files exist:
dir "C:\Users\ciaoa\Desktop\Giocotest3\utilitymod\src\main\cpp\native\minhook"
# Expected: buffer.c, hook.c, trampoline.c
```

### Step 2: Configure CMake with Visual Studio generator
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```
This generates `utilitymod.sln` and project files.

### Step 3: Build the solution
```powershell
cmake --build . --config Release
```
Output: `Release\utilitymod_native.dll`

### Step 4: Install the DLL
```powershell
# Place in the mod's native directory
$appdata = $env:APPDATA
$nativeDir = Join-Path $appdata ".utilitymod\native"
if (-not (Test-Path $nativeDir)) { mkdir $nativeDir }
Copy-Item -Path ".\Release\utilitymod_native.dll" -Destination $nativeDir
```

### Step 5: Launch Minecraft
- Run Minecraft with Fabric 1.21.11
- The native library auto-loads on mod initialization
- Press **INSERT** key to toggle the ImGui menu
- Press **O** key to toggle the Java dropdown menu

## Build Steps (MinGW / Alternative)

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
make -j$(nproc)  # or: make -j4
# Output: utilitymod_native.dll (MinGW format)
Copy-Item -Path .\utilitymod_native.dll -Destination (Join-Path $env:APPDATA ".utilitymod\native")
```

## CMakeLists.txt Details
The project's CMakeLists.txt handles:

### Windows (MSVC)
```cmake
target_link_libraries(utilitymod_native PRIVATE
    d3d11
    dxgi
    user32
    gdi32
    dwmapi
)
target_compile_definitions(utilitymod_native PRIVATE
    IMGUI_IMPL_WIN32
    IMGUI_IMPL_DX11
)
```

### Linux
```cmake
target_link_libraries(utilitymod_native PRIVATE
    glfw
    GL
    dl
    pthread
)
target_compile_definitions(utilitymod_native PRIVATE
    IMGUI_IMPL_GLFW
    IMGUI_IMPL_OPENGL3
)
```

### macOS
```cmake
target_link_libraries(utilitymod_native PRIVATE
    glfw
    "-framework Metal"
    "-framework QuartzCore"
    "-framework Cocoa"
)
target_compile_definitions(utilitymod_native PRIVATE
    IMGUI_IMPL_GLFW
    IMGUI_IMPL_METAL
)
```

## Native Library Functions Exported

### JNI exports (from entry_point.cpp)
- `Java_com_utilitymod_nativeclient_NativeBridge_nativeLog`
- `Java_com_utilitymod_nativeclient_NativeBridge_nativeError`
- `Java_com_utilitymod_nativeclient_NativeBridge_requestTickAction`
- `Java_com_utilitymod_nativeclient_NativeBridge_getPlayerX`
- `Java_com_utilitymod_nativeclient_NativeBridge_getPlayerY`
- `Java_com_utilitymod_nativeclient_NativeBridge_getPlayerZ`
- `Java_com_utilitymod_nativeclient_NativeBridge_getPlayerYaw`
- `Java_com_utilitymod_nativeclient_NativeBridge_getPlayerPitch`
- `Java_com_utilitymod_nativeclient_NativeBridge_isPlayerAlive`
- `Java_com_utilitymod_nativeclient_NativeBridge_getEntityId`

### C API exports (for direct use)
- `UtilityMod_Initialize()` - Initialize native client
- `UtilityMod_Shutdown()` - Shutdown native client
- `UtilityMod_IsLoaded()` - Check if initialized
- `UtilityMod_SetModuleEnabled(int moduleId, bool enabled)` - Toggle module
- `UtilityMod_IsModuleEnabled(int moduleId)` - Check module state
- `UtilityMod_ToggleMenu()` - Toggle ImGui menu

## MinHook Hooks Implemented
The native layer hooks Minecraft client functions via MinHook:

1. **Render hooks** - IDXGISwapChain::Present for ImGui integration
2. **Tick hooks** - MinecraftClient::tick for per-tick operations
3. **Packet send hooks** - ClientPlayNetworkHandler::sendPacket for packet interception
4. **Input hooks** - Window procedure / GLFW callbacks for keybind handling

## Design Tokens (tasteskill/impeccable)
The ImGui style uses:
- **Color palette**: Semantic (not decorative), WCAG AA compliant
- **Spacing**: 8px base grid consistent throughout
- **Typography**: System font stack, readable hierarchy
- **Motion**: 150ms spring animations, no excessive easing
- **Layout**: Clear hierarchy: Category > Module > State
- **Anti-slop**: No AI-default patterns (centered heroes, 3-column grids, gradient blobs)

## Troubleshooting

### Common Issues

1. **"MinHook not found" error**
   - Ensure minhook source files are in the `minhook/` subdirectory
   - The CMakeLists.txt references `MINHOOK_SOURCES` variable

2. **"Could not find JVM library"**
   - On Windows, ensure Java JDK is installed
   - Or specify `jvmLibraryPath` in NativeConfig

3. **"ImGui initialization failed"**
   - Ensure D3D11 is available (Windows) or OpenGL context is current
   - The hook system needs access to the swap chain

4. **Menu not appearing**
   - Press **INSERT** key (default) to toggle native menu
   - Press **O** key to toggle Java dropdown menu
   - Verify native library is in correct location

5. **Crash on launch**
   - Verify Minecraft version matches (1.21.11)
   - Verify Fabric API is installed
   - Check that the DLL architecture matches (x64 only)

## Post-Build Checklist
- [ ] DLL placed in `%APPDATA%\.utilitymod\native\`
- [ ] Minecraft 1.21.11 with Fabric installed
- [ ] Fabric API version compatible
- [ ] Launch Minecraft once to verify auto-load
- [ ] Press INSERT for native menu
- [ ] Press O for Java dropdown menu
- [ ] Test module toggles (AutoWalk, AutoEat, etc.)
- [ ] Verify no crashes or game instability

## Version Notes
- **Built for**: Minecraft 1.21.11 (Fabric)
- **Java version**: 17+
- **Native DLL**: x64 Windows only (Linux/macOS via CMake conditional)
- **Mod version**: 1.0.0
- **Native library version**: Will auto-detect from mod metadata

---
*This mod is designed for single-player use with test server compatibility. Custom packet creation is avoided; only existing Minecraft 1.21.11 packets are used. All actions are scheduled at the start of game ticks only.*