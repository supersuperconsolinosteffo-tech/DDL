# Prebuilt binaries

## windows-x64/utilitymod_native.dll

Cross-compiled on Linux with MinGW-w64 (LLVM/Zig toolchain, clang 22.1.8) from
`utilitymod/src/main/cpp/native`.

| Property | Value |
|----------|-------|
| Architecture | x86-64 (PE32+, DLL) |
| Size | 1,966,592 bytes |
| SHA-256 | `a274c643ea13ef85987e10dd520116533bae3ece16a65adf997bfe4961eb0688` |
| Minimum Windows | Windows 10 (Universal CRT + `d3dcompiler_47.dll`) |

### Exports (18)

- `JNI_OnLoad`, `JNI_OnUnload`
- `Java_com_utilitymod_nativeclient_NativeBridge_nativeLog`
- `Java_com_utilitymod_nativeclient_NativeBridge_nativeError`
- `Java_com_utilitymod_nativeclient_NativeBridge_requestTickAction`
- `Java_com_utilitymod_nativeclient_NativeBridge_getPlayerX/Y/Z`
- `Java_com_utilitymod_nativeclient_NativeBridge_getPlayerYaw/Pitch`
- `Java_com_utilitymod_nativeclient_NativeBridge_isPlayerAlive`
- `Java_com_utilitymod_nativeclient_NativeBridge_getEntityId`
- `UtilityMod_Initialize`, `UtilityMod_Shutdown`, `UtilityMod_IsLoaded`
- `UtilityMod_SetModuleEnabled`, `UtilityMod_IsModuleEnabled`, `UtilityMod_ToggleMenu`

### Runtime dependencies

Windows system DLLs only: `KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `dwmapi`,
`d3dcompiler_47`, plus the Universal CRT (`api-ms-win-crt-*`). The MinGW C/C++
runtime is linked statically, so **no** `libstdc++-6.dll`,
`libgcc_s_seh-1.dll` or `libwinpthread-1.dll` is required.

Verify after download (PowerShell):

```powershell
Get-FileHash .\utilitymod_native.dll -Algorithm SHA256
dumpbin /exports .\utilitymod_native.dll
dumpbin /dependents .\utilitymod_native.dll
```

### Install

```powershell
$dest = Join-Path $env:APPDATA ".utilitymod\native"
if (-not (Test-Path $dest)) { mkdir $dest }
Copy-Item .\utilitymod_native.dll $dest
```

Then launch Minecraft 1.21.11 (Fabric) and press `INSERT` for the native menu,
`O` for the Java dropdown menu.

### Rebuild from source

See `BUILD_NATIVE_DLL.md`. On Windows use MSVC or MinGW; on Linux use
`toolchain-mingw-w64.cmake`:

```bash
cd utilitymod/src/main/cpp/native
cmake -S . -B build-win64 \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchain-mingw-w64.cmake" \
      -DUTILITYMOD_JNI_INCLUDE_DIR=/path/to/jdk/include \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-win64 -j
```
