# UtilityClient

A single-player / test-server **utility client for Minecraft 1.21.11 (Fabric)**, written as an injectable **C++ DLL**: ImGui dropdown menu, MinHook render hook, and all module logic in C++ talking to the game through **JNI/JVMTI**.

**Two hard rules shape everything in this repo:**

1. **Existing Minecraft packets only.** The DLL has a closed enum of vanilla 1.21.11 C2S packet types and constructs real packet objects through JNI. There is no custom-payload code path anywhere (checked by `tools/verify_invariants.py`).
2. **Start-of-tick only.** Nothing is ever sent between ticks or at end of tick. Every action sits in a queue until the tick gate fires at the very start of a client tick, on the client thread — the queue’s drain function literally refuses to run outside that gate.

**Version lock:** built for **1.21.11 only, forever**. All JNI names are the exact 1.21.11 intermediary mappings (generated from official Yarn data, see below) and the DLL checks the game version at attach time — on mismatch it logs an error and never sends a packet. There is nothing to “update”; if Mojang ships something new, that's simply not this client's problem.

> For your own single-player worlds and private test servers. Don’t use it on public servers — most forbid modified clients, and some modules here (Fly/NoClip/NoFall) are not legitimate gameplay anywhere but your own worlds.

---

## Quick start

You need **both** pieces:

| Piece | What it is | Where |
|---|---|---|
| `UtilityClient.dll` + `UtilityInjector.exe` | The C++ client (prebuilt in `dist/`, or build it) | `dist/` |
| `utilityclient-bridge-1.0.0.jar` | A ~2-class Fabric mod that only rings the start-of-tick bell (see “Why is there a Java mod at all?”) | build it once: `bridge-mod/gradlew.bat build` |

1. Install **Fabric Loader 0.20.1** for Minecraft **1.21.11**, plus **Fabric API 0.141.6+1.21.11**.
2. Build the bridge once (needs JDK 21 + internet the first time):
   ```bat
   cd bridge-mod
   gradlew.bat build
   ```
   Put `bridge-mod/build/libs/utilityclient-bridge-1.0.0.jar` in your `mods/` folder. (Or run `build-all.bat` at the repo root to build everything into `dist/`.)
3. Launch Minecraft 1.21.11.
4. Run `dist/UtilityInjector.exe` (it finds the Minecraft window itself). Or inject `dist/UtilityClient.dll` with any x64 LoadLibrary injector.
5. In game: **INSERT** toggles the menu (starts open), **END** cleanly unloads the DLL.
6. Logs: `%USERPROFILE%\.utilityclient\utilityclient.log`. If anything looks wrong, that file tells you exactly which stage failed.

The prebuilt DLL in `dist/` was cross-compiled (Clang, x86-64 Windows PE, system-DLL imports only — no VC++/MinGW runtime needed; SHA-256 in `dist/SHA256SUMS.txt`). `native/build.bat` reproduces it natively with Visual Studio 2022.

## Controls

| Key | Action |
|---|---|
| `INSERT` | Open / close the dropdown menu |
| `END` | Cleanly unload the DLL |
| Left-click a module row | Toggle it (takes effect at the next tick start) |
| Right-click a module row | Expand / collapse its settings |
| Search box (top bar) | Filter modules by name |

Menu and unload keys are configurable in `%USERPROFILE%\.utilityclient\utilityclient.ini` (written with defaults on first inject). Module on/off state and settings persist in `modules.ini` in the same folder.

## Menu design

The GUI is a classic dropdown/ClickGUI layout — draggable category columns (Movement / Player / Render), module rows, settings inline — restyled with strict design restraint (“taste” / impeccable rules):

- 8 px spacing grid, 26 px rows, 3 px radii, 1 px borders — no gradients, glow, blur or rainbow anything
- Neutral near-black surfaces; **one** muted green accent that means *enabled/active* and nothing else; red reserved for real errors
- Uppercase micro-labels for categories, plain text for modules, state always in the same place (`ON` accent / `OFF` faint, plus a 2 px accent bar when on)
- A slim status strip instead of a “hero” panel: game version, tick-gate state, queue depth and sent/dropped counters are always visible while the menu is open
- Segoe UI when present, clean fallback otherwise; keyboard navigation enabled; collapsible columns and search instead of endless scrolling

## Modules

Everything below runs at tick start, in C++. “Queued packet” means it goes through the one queue and leaves as that vanilla packet in the same tick-start window. “Vanilla-driven” means the module adjusts client state at tick start and lets the game’s own code send its normal packet (no duplicate movement packets fighting `sendMovementPackets` — that’s the consistent, non-breaking design).

| Module | Category | How it works |
|---|---|---|
| **AutoWalk** | Movement | Vanilla-driven: holds the forward `KeyBinding` down each tick |
| **AutoSprint** | Movement | Vanilla-driven: holds sprint while forward is held (option to require forward) |
| **Fly** | Movement | Vanilla-driven: sets `PlayerAbilities` (allowFlying/flying/speed), vanilla sends its abilities packet; originals restored on disable |
| **NoClip** | Movement | Client flag: `Entity.noClip` (use with Fly; the integrated server tolerates this in single-player, public servers won’t) |
| **NoFall** | Movement | Queued packet: vanilla `PlayerMoveC2SPacket.OnGroundOnly` while falling + local fall-distance reset. *Best effort* — vanilla still sends its own move packet later in the tick; full no-fall needs packet rewriting, which this client deliberately does not do |
| **AutoEat** | Player | Queued packet: finds hotbar food via `ItemStack.getUseAction()` (`"eat"`/optionally `"drink"`), selects the slot, queues vanilla `PlayerInteractItemC2SPacket` |
| **AutoRespawn** | Player | Vanilla-driven: `ClientPlayerEntity.requestRespawn()` after a configurable delay |
| **FullBright** | Render | Client-only: writes the gamma option’s value beyond the slider range; restored on disable. No packets |
| **HUD** | Render | FPS/coordinates/active-module overlay. No packets, no game access from the render thread (reads a per-tick snapshot) |

The packet queue API (`native/src/packet_queue.hpp`) also covers the rest of the vanilla set for extending modules: all four `PlayerMove` variants, `PlayerAction`, `PlayerInteractBlock`, `PlayerInteractItem`, `PlayerInteractEntity` (interact/interactAt/attack), `HandSwing`, `PickItemFromBlock` — with vanilla sequence numbers from `PendingUpdateManager`, exactly like the game’s own interaction manager produces them.

### Adding a module

1. Subclass `Module` in `native/src/modules.cpp` (or a new `.cpp` added to `native/CMakeLists.txt`), register it in `ModuleManager::RegisterAll()`.
2. Do work only in `OnEnable`/`OnDisable`/`OnTickStart` — these run inside the tick gate with a live `TickContext` (player state + JNI refs).
3. Send packets only with `PacketQueue::Get().Push(...)`. That single habit keeps both hard rules intact; the invariant script will fail the build if a second send site ever appears.

## Why is there a Java mod at all?

Short version: **the client is fully C++ except for one bell.**

- All module logic, the menu, the packet construction, the settings — C++, in the DLL.
- But “start of a game tick” is a *Java bytecode* moment (`Minecraft.tick()`). You can’t MinHook JIT-compiled Java. The reliable, supported way to stand at exactly that moment on the client thread is Fabric’s `ClientTickEvents.START_CLIENT_TICK`.
- So the bridge mod is two tiny classes with zero logic: at each start-tick it calls one JNI method. The DLL finds that loaded class through JVMTI (a normal `FindClass` can’t see Fabric’s class loader from a native thread) and `RegisterNatives` on it. Without the DLL, the bridge is a no-op; without the bridge, the DLL renders its menu but **sends nothing** — the status strip says so.

There’s also an experimental bridge-free gate (`jvmti_tick_gate=true` in the ini): a JVMTI breakpoint on `MinecraftClient.tick()` entry running the same gate code. It works, but HotSpot can deoptimize that method, so the bridge remains the supported path.

One terminology fix vs. common phrasing: **MinHook doesn’t inject.** Injection is the plain `LoadLibrary` injector (`native/injector/`); MinHook is what hooks `wglSwapBuffers` afterwards so ImGui can render into Minecraft’s OpenGL window (Minecraft Java is LWJGL/OpenGL — a D3D `Present` hook, as in generic game-cheat templates, would never fire).

## How the invariants are enforced (not just promised)

- `PacketKind` is a closed enum → 1:1 with vanilla packet classes → `tools/verify_invariants.py` greps the whole tree for custom-payload APIs, `END_CLIENT_TICK`, `JNI_CreateJavaVM`, extra send sites, and more (27 checks).
- One send instruction in the codebase: `packet_queue.cpp` calling `ClientPlayNetworkHandler.sendPacket` (intermediary `method_52787`) — itself only reachable from `tick_gate.cpp`, which checks an `InGate()` flag.
- Menu/render threads never touch Java objects. They flip desired state; transitions apply at the next tick start. Enable Fly at the title screen and it applies on world join; disconnect mid-session and modules re-apply cleanly on the next player.
- Mappings are **generated**, not hand-typed: `tools/gen_mappings.py` extracts exactly the 37 classes / 93 members used from official **FabricMC/yarn 1.21.11** enigma mappings (intermediary names + descriptors), cross-checked with FabricMC/intermediary `1.21.11.tiny`. Enum constants aren’t renamed by intermediary (verified against the tiny mappings), so they’re resolved by source name. Two constructors that enigma omits (`BlockPos(int,int,int)`, the `PickItemFromBlock` record ctor) are runtime probes with documented fallbacks. At attach time the DLL resolves everything in **one** JVMTI loaded-class scan, caches global refs, verifies the reported game version is `1.21.11`, and every JNI call site clears/logs Java exceptions instead of crashing your game.
- Clean unload: `END` unregisters the JNI natives (the bridge then just gets its handled `UnsatisfiedLinkError`), tears ImGui down on the render thread, restores the window procedure, removes the MinHook hook, then frees the library. No code path leaves Java calling into unloaded memory.

## Building from source

### Native DLL (primary)

**Windows (supported):** Visual Studio 2022 with “Desktop development with C++”.

```bat
native\build.bat
:: -> dist\UtilityClient.dll + dist\UtilityInjector.exe  (x64, static CRT)
```

No downloads needed: ImGui v1.91.9b, MinHook v1.3.4 and the OpenJDK 21 JNI/JVMTI headers are vendored in `native/third_party/` (see `THIRD_PARTY_NOTICES.md`). You do **not** need JDK installed to build the DLL.

**Linux/macOS cross-check (optional):** `zig` on PATH, then

```bash
cmake -S native -B native/build-zig -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/native/toolchain-zig-windows-x64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-zig
```

This is how the checked-in `dist/` binaries were produced and verified (`objdump`: PE32+ x86-64, single export `UtilityClientVersion`, system imports only).

### Bridge mod

JDK **21** (Minecraft 1.21.11 needs it — Java 17 will not work) + internet on first build:

```bat
cd bridge-mod
gradlew.bat build
:: -> build\libs\utilityclient-bridge-1.0.0.jar
```

Versions are pinned in `bridge-mod/gradle.properties` (Loader 0.20.1, Fabric API 0.141.6+1.21.11, Loom 1.19-SNAPSHOT, Mojang official mappings — the same template values as Fabric’s 1.21.11 example mod).

### Regenerating mappings (should never be necessary)

```bash
git clone --depth 1 --branch 1.21.11 https://github.com/FabricMC/yarn
python3 tools/gen_mappings.py yarn native/src/mappings_12111.hpp
python3 tools/verify_invariants.py
```

## Project layout

```
DDL/
├─ dist/                      prebuilt UtilityClient.dll + UtilityInjector.exe (+ SHA256SUMS)
├─ native/                    the C++ client
│  ├─ src/
│  │  ├─ bootstrap.cpp        DllMain + attach/resolve/hook/gate/unload lifecycle
│  │  ├─ hooks.cpp            MinHook wglSwapBuffers + WndProc (INSERT/END, ImGui input)
│  │  ├─ menu.cpp             ImGui dropdown GUI + design tokens + HUD
│  │  ├─ modules.cpp          the nine built-in modules
│  │  ├─ jni_bridge.cpp/.hpp  JNI_GetCreatedJavaVMs, JVMTI class scan, cached refs, version check
│  │  ├─ tick_gate.cpp/.hpp   start-of-tick gate (bridge RegisterNatives / JVMTI breakpoint)
│  │  ├─ packet_queue.cpp     the only packet path (vanilla packets, drained in-gate only)
│  │  ├─ mappings_12111.hpp   GENERATED intermediary mappings for 1.21.11
│  │  ├─ module/config/log.*  framework, ini, file logger
│  │  └─ app_state.hpp        cross-thread flags
│  ├─ injector/injector.cpp   plain x64 LoadLibrary injector (finds the Minecraft window)
│  ├─ third_party/            ImGui 1.91.9b, MinHook 1.3.4, OpenJDK 21 jni headers
│  ├─ CMakeLists.txt / build.bat / toolchain-zig-windows-x64.cmake
├─ bridge-mod/                Fabric mod: ONLY the START_CLIENT_TICK → JNI bell
├─ tools/                     gen_mappings.py, verify_invariants.py
├─ build-all.bat              one-click: DLL + bridge → dist/
└─ Giocotest3/                SUPERSEDED first draft (see Giocotest3/SUPERSEDED.md) — kept for history, do not build
```

## Troubleshooting

Check `%USERPROFILE%\.utilityclient\utilityclient.log` first — every stage logs there.

- **“no tick gate — install bridge mod (packets paused)” in the top bar** — the bridge jar isn’t in `mods/`, or Fabric API/Loader is missing. The menu working but modules doing nothing is exactly this state, by design.
- **“wrong game version (…) — packets OFF”** — you injected into something that isn’t 1.21.11 (different version, or a dev environment where names differ). This client will not guess; use a production Fabric **1.21.11** install.
- **Injector says it can’t find Minecraft** — start the game first and get past the launcher; the injector looks for a visible `Minecraft*` window owned by `javaw.exe`. You can also pass a PID: `UtilityInjector.exe UtilityClient.dll <pid>`.
- **Menu shows but mouse doesn’t move** — cursor unlock is applied at tick start (or immediately as a UI-only fallback when no gate exists). If you injected during a loading screen, wait until the world/title renders, then toggle `INSERT` once.
- **Game input while the menu is open** — the mouse is fully captured by the menu; game *keys* are polled by LWJGL, so WASD can still move you with the menu open (pause with `ESC` if you’re configuring things mid-flight).
- **Antivirus flags the injector/DLL** — unsigned `LoadLibrary` injectors trigger heuristics. Build from source (`native/build.bat`) so the binaries are yours, and/or whitelist the `dist/` folder.
- **Crash** — attach the log. JNI problems are designed to log-and-disable rather than crash; a hard crash most likely means the game isn’t actually production 1.21.11, or another overlay/injected tool is fighting over `wglSwapBuffers`/the window procedure.

## Status / verification done

- ✅ Native DLL + injector compile and link clean (Zig/Clang cross, x86-64 PE verified with `objdump`; same sources build with MSVC via `build.bat`).
- ✅ `tools/verify_invariants.py`: 27/27 checks pass.
- ✅ Bridge sources parse and follow Fabric’s current 1.21.11 template; the bridge jar itself must be built on a machine with JDK 21 + internet (this build sandbox has no route to Fabric/Mojang mavens, so the jar is not prebuilt in `dist/`).
- ⚠️ Not yet run against a live game in this environment (no Minecraft here) — first-run validation is: inject at title screen → status strip should show `tick gate: bridge` → join a single-player world → tick counter climbs → toggle AutoWalk/FullBright → `END` unloads without a crash and the log ends with “unloaded cleanly”.

## License

Project code: MIT (`Giocotest3/LICENSE` is the original license file; third-party terms in `THIRD_PARTY_NOTICES.md`). Minecraft is a trademark of Mojang/Microsoft; this is an unofficial modification, not affiliated with Mojang.
