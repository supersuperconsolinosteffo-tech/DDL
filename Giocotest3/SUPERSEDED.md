# Superseded — do not build this folder

This was the first draft (hybrid Fabric mod + loadable native library). It is kept for history only and is **not** buildable as-is, among other reasons:

- `utilitymod/src/main/cpp/native/minhook/*.c` contain the text `404: Not Found` (failed download) and the ImGui sources the CMake file references were never vendored.
- The native layer targeted D3D11 `Present`; Minecraft Java renders with OpenGL (LWJGL), so that hook would never fire. It also tried to `JNI_CreateJavaVM`/`DestroyJavaVM` inside the already-running game JVM.
- The Java side targets Java 17 (Minecraft 1.21.11 requires 21), its Fabric entrypoints don’t implement the required interfaces, there is no Gradle wrapper, and several dependency coordinates are invalid.

The maintained implementation is at the repository root: **`native/`** (injectable C++ DLL), **`bridge-mod/`** (tiny Fabric start-of-tick bridge) and **`dist/`** (prebuilt binaries). Start with the root `README.md`.
