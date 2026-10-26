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
   - Provides MSVC, the Windows SDK, D3D11 and `d3dcompiler.lib`
2. **CMake** 3.20 or newer
3. **JDK 17 or newer** — provides the JNI/JVMTI headers (`jni.h`, `jvmti.h`)
   - Set `JAVA_HOME`, or pass `-DUTILITYMOD_JNI_INCLUDE_DIR=<jdk>\include`
   - The JDK headers are **not** vendored in this repository

ImGui (docking branch) and MinHook are **vendored** in the repository, so no
download step is required:

```
utilitymod/src/main/cpp/native/
├── imgui/                  ← vendored ImGui (core + win32/dx11 backends)
└── minhook/                ← vendored MinHook
    ├── include/MinHook.h
    └── src/{buffer,hook,trampoline}.c, src/hde/*
```

### Alternative: MinGW-w64 (native Windows or cross-compile from Linux)
- MinGW-w64 GCC with the **POSIX threads** flavour
  (Debian/Ubuntu: `g++-mingw-w64-x86-64-posix`), because the native layer uses
  `std::thread`, `std::mutex` and `std::call_once`
- CMake 3.20+
- JDK 17+ headers (see above)

## Build Steps (Visual Studio / MSVC)

```powershell
cd utilitymod\src\main\cpp\native
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Or simply run `build.bat` from the project root, which performs the same steps
and copies the result to `%APPDATA%\.utilitymod\native`.

Output (multi-config generator adds the configuration directory):

```
build\bin\Release\utilitymod_native.dll
```

## Build Steps (MinGW-w64)

### Native Windows

```powershell
cd utilitymod\src\main\cpp\native
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# Output: build\bin\utilitymod_native.dll
```

### Cross-compile from Linux (produces the same x64 Windows DLL)

```bash
cd utilitymod/src/main/cpp/native
cmake -S . -B build-win64 \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchain-mingw-w64.cmake" \
      -DUTILITYMOD_JNI_INCLUDE_DIR=/path/to/jdk/include \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-win64 -j"$(nproc)"
# Output: build-win64/bin/utilitymod_native.dll
```

Notes for cross-compiling:

- `toolchain-mingw-w64.cmake` targets `x86_64-w64-mingw32`; override the prefix
  with `-DMINGW_PREFIX=...` for other distributions.
- The DLL is linked against the static C/C++ runtime (`-static-libgcc
  -static-libstdc++`), so it does not require `libstdc++-6.dll`,
  `libgcc_s_seh-1.dll` or `libwinpthread-1.dll` at runtime. It imports only
  Windows system DLLs (`KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `dwmapi`,
  `d3dcompiler_47`, plus the Universal CRT `api-ms-win-crt-*`), i.e. it
  requires **Windows 10 or newer**.
- `d3dcompiler_47.dll` is loaded at runtime; it ships with Windows 10/11.
  Some MinGW distributions lack `libd3dcompiler.a`; if the link fails with
  `undefined symbol: D3DCompile`, generate the import library from a `.def`
  file:
  ```bash
  printf 'LIBRARY d3dcompiler_47.dll\nEXPORTS\nD3DCompile\n' > d3dcompiler.def
  x86_64-w64-mingw32-dlltool -m i386:x86-64 -d d3dcompiler.def -l libd3dcompiler.a
  # then add -L<directory holding that .a> to CMAKE_SHARED_LINKER_FLAGS
  ```
  (MSVC builds link the Windows SDK `d3dcompiler.lib` and need nothing extra.)

## Install the DLL

```powershell
$nativeDir = Join-Path $env:APPDATA ".utilitymod\native"
if (-not (Test-Path $nativeDir)) { mkdir $nativeDir }
Copy-Item -Path ".\build\bin\Release\utilitymod_native.dll" -Destination $nativeDir
```

## Launch Minecraft
- Run Minecraft with Fabric 1.21.11
- The native library auto-loads on mod initialization
- Press **INSERT** key to toggle the ImGui menu
- Press **O** key to toggle the Java dropdown menu

## CMake Configuration Reference

| Option / variable | Default | Purpose |
|-------------------|---------|---------|
| `UTILITYMOD_JNI_INCLUDE_DIR` | empty | JDK `include` directory (else `JAVA_HOME`, then `find_package(JNI)`) |
| `UTILITYMOD_WERROR` | `OFF` | Turn compiler warnings into errors (`/WX`, `-Werror`) |
| `MINGW_PREFIX` | `x86_64-w64-mingw32` | Tool prefix used by `toolchain-mingw-w64.cmake` |

Warnings are enabled (`/W4`, `-Wall -Wextra`) and also apply to the vendored
ImGui/MinHook sources, so they are informational unless `UTILITYMOD_WERROR=ON`.
The previous configuration used `-Werror` unconditionally, which no vendored
dependency survives.

### Platform-specific link dependencies

Windows:
```cmake
d3d11 dxgi d3dcompiler user32 gdi32 dwmapi psapi
```
`d3dcompiler` is required because ImGui's current D3D11 backend compiles its
shader at runtime; `psapi` is required by the pattern scanner
(`GetModuleInformation`).

## Native Library Functions Exported

### JNI exports (from entry_point.cpp)
- `JNI_OnLoad`, `JNI_OnUnload`
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

### C API exports (declared in `utilitymod_export.h`)
- `UtilityMod_Initialize()` - Initialize native client
- `UtilityMod_Shutdown()` - Shutdown native client
- `UtilityMod_IsLoaded()` - Check if initialized
- `UtilityMod_SetModuleEnabled(int moduleId, bool enabled)` - Toggle module
- `UtilityMod_IsModuleEnabled(int moduleId)` - Check module state
- `UtilityMod_ToggleMenu()` - Toggle ImGui menu

Verify a built DLL on Windows with:

```powershell
dumpbin /exports utilitymod_native.dll
dumpbin /dependents utilitymod_native.dll
```

On Linux with MinGW/wine tooling:

```bash
x86_64-w64-mingw32-objdump -p utilitymod_native.dll | grep -A25 "Ordinal/Name"
x86_64-w64-mingw32-objdump -p utilitymod_native.dll | grep "DLL Name"
```

## MinHook Hooks Implemented
The native layer hooks Minecraft client functions via MinHook:

1. **Render hooks** - IDXGISwapChain::Present for ImGui integration
2. **Tick hooks** - MinecraftClient::tick for per-tick operations
3. **Packet send hooks** - ClientPlayNetworkHandler::sendPacket for packet interception
4. **Input hooks** - Window procedure / GLFW callbacks for keybind handling

Pattern scanning (`findPattern` / `findPatternInRange`) locates those targets;
no addresses are hard-coded.

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

1. **"JNI headers (jni.h / jvmti.h) were not found"**
   - Install a JDK 17+ and set `JAVA_HOME`
   - Or configure with `-DUTILITYMOD_JNI_INCLUDE_DIR=<jdk>/include`

2. **`undefined symbol: D3DCompile` while linking (MinGW)**
   - Your MinGW distribution lacks `libd3dcompiler.a`; see the cross-compile note above

3. **`No type named 'ActionId'/'ModuleId' in 'NativeClient'` on older checkouts**
   - `ModuleId`, `ActionId` and `NativeConfig` are namespace-scope types in
     `utilitymod`; refer to them unqualified inside that namespace

4. **"Could not find JVM library"**
   - On Windows, ensure a Java JDK is installed
   - Or specify `jvmLibraryPath` in `NativeConfig`
   - `JNI_CreateJavaVM` is resolved at runtime from the loaded `jvm.dll`, so the
     library does not link against `jvm.lib`

5. **"ImGui initialization failed"**
   - Ensure D3D11 is available (Windows) or OpenGL context is current
   - The hook system needs access to the swap chain

6. **Menu not appearing**
   - Press **INSERT** key (default) to toggle native menu
   - Press **O** key to toggle Java dropdown menu
   - Verify native library is in correct location

7. **Crash on launch**
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
- **Vendored dependencies**: ImGui docking branch (1.93.1 WIP), MinHook (master)

---
*This mod is designed for single-player use with test server compatibility. Custom packet creation is avoided; only existing Minecraft 1.21.11 packets are used. All actions are scheduled at the start of game ticks only.*
