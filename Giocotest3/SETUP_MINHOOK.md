# MinHook Setup for UtilityMod

MinHook is a lightweight library for hooking functions on Windows, Linux, and macOS. 
This mod uses the Windows version via MSVC, but the CMakeLists.txt is cross-platform.

## Download MinHook

### Option A: Use the official GitHub release (recommended)

```powershell
# From PowerShell in the minhook directory
cd C:\Users\ciaoa\Desktop\Giocotest3\utilitymod\src\main\cpp\native\minhook

# Download MinHook v2.8.2 source (or latest)
# You can get it from: https://github.com/TsudaK/minhook/releases

# Using curl/powershell:
iwr https://github.com/TsudaK/minhook/releases/download/v2.8.2/minhook-v2.8.2-src.zip -OutFile minhook-src.zip
Expand-Archive minhook-src.zip .
# This creates minhook/ subdirectory with:
#   buffer.c, hook.c, trampoline.c, (and optionally other files)
```

### Option B: Manual download

1. Go to: https://github.com/TsudaK/minhook/releases
2. Download the latest "src" ZIP release
3. Extract to: `C:\Users\ciaoa\Desktop\Giocotest3\utilitymod\src\main\cpp\native\minhook\`
4. Expected files after extraction:
   - `buffer.c`
   - `hook.c` 
   - `trampoline.c`
   - (Optionally: CMakeLists.txt, minhook.h, etc. - the CMakeLists.txt in the parent will use .c files)

## Verify MinHook Installation

After extraction, verify these files exist:

```powershell
dir "C:\Users\ciaoa\Desktop\Giocotest3\utilitymod\src\main\cpp\native\minhook"
```

Expected output (or similar):
```
Directory: C:\Users\ciaoa\Desktop\Giocotest3\utilitymod\src\main\cpp\native\minhook

Mode                LastWriteTime         Length Name
----                -------------         ------ ----
-a----        26/10/2026     12:46           3334 CMakeLists.txt    <-- from original project
-a----        26/10/2026     12:50           7841 entry_point.cpp
-a----        26/10/2026     12:49          10437 imgui_manager.cpp
...
```

You should see at minimum:
- `buffer.c`
- `hook.c`
- `trampoline.c`

## MinHook License

MinHook is released under the MIT License. See the MinHook repo for full license terms.

The mod's CMakeLists.txt will compile these 3 `.c` files into the shared library automatically.

## Cross-Platform Notes

### Windows (MSVC) - This mod's primary target
- MinHook compiles natively with MSVC
- Links against system APIs (kernel32, user32, etc.)
- The CMakeLists.txt defines `IMGUI_IMPL_WIN32` and `IMGUI_IMPL_DX11` for the ImGui backends

### Linux
- MinHook also has a Linux port
- CMake would use `-DWIN32=0` / `UNIX` detection
- Would link against `pthread` etc.

### macOS
- MinHook has limited macOS support
- Would use DYLD interposing or similar techniques

## For This Mod (Windows MSVC)

The CMakeLists.txt at `utilitymod/src/main/cpp/native/CMakeLists.txt` includes:

```cmake
# MinHook sources
set(MINHOOK_SOURCES
    minhook/buffer.c
    minhook/hook.c
    minhook/trampoline.c
)

# Link Windows system libraries
target_link_libraries(utilitymod_native PRIVATE
    d3d11
    dxgi
    user32
    gdi32
    dwmapi
)
```

These 3 source files + the Windows API links are all that's needed for the hooking functionality on Windows.

## Build Order Summary

1. **Download MinHook source** to `utilitymod/src/main/cpp/native/minhook/`
   - Must have: `buffer.c`, `hook.c`, `trampoline.c`

2. **Ensure imgui backends exist**:
   - `utilitymod/src/main/cpp/native/imgui/backends/imgui_impl_win32.cpp`
   - `utilitymod/src/main/cpp/native/imgui/backends/imgui_impl_dx11.cpp`

3. **Build with CMake**:
   ```powershell
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64
   cmake --build . --config Release
   ```

4. **Install DLL**:
   ```powershell
   copy .\Release\utilitymod_native.dll $env:APPDATA\.utilitymod\native
   ```

5. **Launch Minecraft** with Fabric 1.21.11
   - Press INSERT for native ImGui menu
   - Press O for Java dropdown menu

## MinHook Version Compatibility

This project has been tested with MinHook v2.8.x. 
Older versions (pre-2.0) may have API differences.
The GitHub releases page always provides source code compatible with the latest MSVC compilers.

If you encounter compilation errors related to MinHook APIs:
- Ensure you're using MinHook v2.8.2 or newer
- The `MH_Initialize()`, `MH_CreateHook()`, `MH_EnableHook()` API should be consistent
- The CMakeLists.txt uses the modern API style