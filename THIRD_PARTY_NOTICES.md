# Third-party notices

All third-party code lives in `native/third_party/` with its original license
files preserved next to it. Pinned versions (this project does not track
upstream updates; versions below are what the shipped DLL was built against):

| Component | Version | License | Used for |
|---|---|---|---|
| Dear ImGui (`third_party/imgui/`) | v1.91.9b | MIT (`LICENSE.txt`) | Dropdown overlay menu (Win32 + OpenGL3 backends only) |
| MinHook (`third_party/minhook/`) | v1.3.4 | BSD 2-Clause (`LICENSE.txt`) | Hooking `wglSwapBuffers` for the overlay |
| JDK headers (`third_party/jni/`) | OpenJDK 21 | GPLv2 with Classpath Exception (file headers) | `jni.h`, `jvmti.h`, `win32/jni_md.h` interface headers only — no JDK code is linked; the game’s own `jvm.dll` provides the implementation at runtime |

Mappings provenance (generated code, `native/src/mappings_12111.hpp`):
extracted from the official **FabricMC/yarn** branch `1.21.11` enigma mappings
by `tools/gen_mappings.py`; cross-checked against **FabricMC/intermediary**
`mappings/1.21.11.tiny`. Yarn mappings are CC0.
