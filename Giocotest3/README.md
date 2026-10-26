# UtilityMod

A single-player utility mod for Minecraft 1.21.11 (Fabric) with a native C++ client layer using ImGui, MinHook, and JNI/JVMTI.

## Features

- **Pure existing packets only** — Never uses custom packets; all actions sent via standard Minecraft packet types
- **Start-of-tick execution** — All packet actions are dispatched at the **start** of a game tick only, never between ticks or at end of tick
- **Native C++ client layer** — ImGui menu, MinHook function hooks, JNI/JVMTI bridge for Java↔C++ communication
- **Single-player focused** — Designed for single-player with test server compatibility
- **Clean, modern UI** — Dropdown-style menu with tasteskill/impeccable design principles
- **Module system** — Extensible modules for movement, player, combat, world, and misc utilities
- **No mixins** — Uses Fabric API events exclusively for maximum compatibility
- **Version-locked** — Built specifically for 1.21.11; no update maintenance burden

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Minecraft Client                        │
├─────────────────────────────────────────────────────────────┤
│  Java Layer (Fabric Mod)          │  Native Layer (C++)     │
│  ─────────────────────────────────┼───────────────────────── │
│  • ModuleManager                  │  • ImGui Menu           │
│  • TickScheduler (START_TICK)     │  • MinHook Hooks        │
│  • PacketDispatcher               │  • JNI/JVMTI Bridge     │
│  • Existing Packet Types Only     │  • Pattern Scanning     │
│  • UtilityMenu (Java GUI)         │  • Input Interception   │
└───────────────────────────────────┴─────────────────────────┘
                    │
                    ▼
         ┌───────────────────────┐
         │  Server (Singleplayer │
         │  or Test Server)      │
         └───────────────────────┘
```

## Packet Types Used

Only existing Minecraft 1.21.11 packet types:

| Packet Type | Class | Purpose |
|-------------|-------|---------|
| `MOVE_PLAYER_POS` | `PlayerMoveC2SPacket.PositionAndOnGround` | Position updates |
| `MOVE_PLAYER_LOOK` | `PlayerMoveC2SPacket.LookAndOnGround` | Rotation updates |
| `MOVE_PLAYER_FULL` | `PlayerMoveC2SPacket.Full` | Full position + rotation |
| `MOVE_PLAYER_ON_GROUND_ONLY` | `PlayerMoveC2SPacket.OnGroundOnly` | Ground state only |
| `PLAYER_ACTION` | `PlayerActionC2SPacket` | Digging, dropping, etc. |
| `INTERACT_BLOCK` | `PlayerInteractBlockC2SPacket` | Block interactions |
| `INTERACT_ITEM` | `PlayerInteractItemC2SPacket` | Item use (right-click) |
| `INTERACT_ENTITY` | `PlayerInteractEntityC2SPacket` | Entity interactions |
| `HAND_SWING` | `HandSwingC2SPacket` | Hand animation |
| `PICK_ITEM_FROM_BLOCK` | `PickItemFromBlockC2SPacket` | Creative pick block |

## Built-in Modules

| Module | Category | Description |
|--------|----------|-------------|
| AutoWalk | Movement | Automatically walks forward |
| AutoSprint | Movement | Automatically sprints when moving |
| NoClip | Movement | Move through blocks (client-side) |
| AutoEat | Player | Automatically eats when hungry |
| FreeCam | Player | Camera separates from player |

## Requirements

- Java 17+
- Minecraft 1.21.11 (Fabric Loader 0.16.9+)
- Fabric API 0.100.0+
- For native layer: Windows 10/11 (D3D11), Linux (GLFW+OpenGL), or macOS (GLFW+Metal)

## Building

### Java Mod (Gradle)

```bash
# Build the mod JAR
./gradlew build

# Run development client
./gradlew runClient

# Run development server
./gradlew runServer
```

### Native Library (CMake)

```bash
# Windows (Visual Studio 2022)
cd utilitymod/src/main/cpp/native
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# Linux
cd utilitymod/src/main/cpp/native
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# macOS
cd utilitymod/src/main/cpp/native
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

The native library (`utilitymod_native.dll` / `libutilitymod_native.so` / `libutilitymod_native.dylib`) should be placed in `~/.utilitymod/native/` for automatic loading.

## Installation

1. Install Fabric Loader 0.16.9+ for Minecraft 1.21.11
2. Place `utilitymod-1.0.0.jar` in `mods/` folder
3. (Optional) Build and place native library in `~/.utilitymod/native/`
4. Launch Minecraft

## Usage

- **Open Menu**: Press `O` key
- **Navigate**: ↑/↓ arrows, Enter to toggle, Escape to close
- **Native Menu**: Press `INSERT` key (when native library loaded)
- **Keybinds**: Configurable per module in menu

## Design Principles (tasteskill / impeccable)

This mod follows strict design guidelines:

- **Typography**: System font stack, consistent hierarchy, no purple gradients
- **Spacing**: 8px base grid, consistent padding/margins
- **Color**: Semantic palette (not decorative), high contrast, WCAG AA compliant
- **Motion**: Subtle spring animations (150ms), no excessive easing
- **Layout**: Clear visual hierarchy — Category > Module > State
- **Accessibility**: Keyboard navigable, screen reader compatible, high contrast mode ready
- **Anti-slop**: No AI-default patterns (centered hero, 3-column grids, gradient blobs)

## Development

### Adding a New Module

1. Create a new class extending `Module` or `ConfigurableModuleBase`
2. Implement `onTick(client, player, world)` to schedule packet actions
3. Register in `ModuleRegistry.registerAll()`
4. Add translation keys in `en_us.json`

```java
public class MyModule extends Module {
    public MyModule() {
        super("mymodule", "My Module", "Description", Category.PLAYER);
    }

    @Override
    public void onTick(MinecraftClient client, ClientPlayerEntity player, ClientWorld world) {
        if (!isEnabled()) return;
        
        // Schedule actions for NEXT tick start
        scheduleInteractItem(); // or any helper
    }
}
```

### Scheduling Packet Actions

```java
// From a module (preferred)
scheduleMove(x, y, z, yaw, pitch, onGround);
scheduleLook(yaw, pitch, onGround);
schedulePosition(x, y, z, onGround);
scheduleInteractBlock(pos, direction, hitPos);
scheduleInteractItem();
scheduleInteractEntity(entityId);
scheduleHandSwing();
schedulePlayerAction("START_DESTROY_BLOCK", pos, direction);

// From anywhere (native bridge, etc.)
TickScheduler.schedule(new PacketAction(PacketType.MOVE_PLAYER_FULL, Map.of(
    "x", 100.0, "y", 64.0, "z", 200.0,
    "yaw", 0.0f, "pitch", 0.0f,
    "onGround", true
)));
```

### Native Module Control

From C++/ImGui:

```cpp
// Enable a module
NativeClient::instance().setModuleState(ModuleId::AutoWalk, true);

// Set parameter
NativeClient::instance().setModuleParameter(ModuleId::AutoWalk, "speed", "5.0");
```

## Project Structure

```
utilitymod/
├── src/
│   ├── main/
│   │   ├── java/com/utilitymod/
│   │   │   ├── UtilityMod.java              # Main entrypoint
│   │   │   ├── client/UtilityModClient.java # Client entrypoint
│   │   │   ├── modules/                     # Module system
│   │   │   │   ├── Module.java              # Base module class
│   │   │   │   ├── ModuleManager.java       # Module lifecycle
│   │   │   │   ├── ModuleRegistry.java      # Built-in modules
│   │   │   │   ├── Category.java            # Module categories
│   │   │   │   ├── ConfigurableModule.java  # Configurable interface
│   │   │   │   └── impl/                    # Built-in modules
│   │   │   ├── packet/                      # Packet system
│   │   │   │   ├── PacketAction.java        # Action representation
│   │   │   │   ├── PacketType.java          # Packet type enum
│   │   │   │   ├── PacketDispatcher.java    # Packet construction/sending
│   │   │   │   └── TickActionBridge.java    # Native↔Java bridge
│   │   │   ├── tick/TickScheduler.java      # Start-of-tick scheduler
│   │   │   ├── gui/UtilityMenu.java         # Java dropdown menu
│   │   │   └── nativeclient/NativeBridge.java # JNI bridge
│   │   ├── resources/
│   │   │   ├── fabric.mod.json              # Fabric mod metadata
│   │   │   ├── utilitymod.mixins.json       # Empty (no mixins)
│   │   │   └── assets/utilitymod/
│   │   │       ├── lang/en_us.json          # Translations
│   │   │       └── icon.png                 # Mod icon
│   │   └── cpp/native/                      # Native C++ layer
│   │       ├── CMakeLists.txt               # CMake build config
│   │       ├── native_client.hpp/.cpp       # Main native client
│   │       ├── jni/jni_bridge.hpp/.cpp      # JNI/JVMTI bridge
│   │       ├── hooks/minecraft_hooks.hpp/.cpp # MinHook hooks
│   │       ├── imgui_manager.hpp/.cpp       # ImGui integration
│   │       └── entry_point.cpp              # JNI exports & C API
├── build.gradle                             # Gradle build
├── gradle.properties                        # Version properties
├── settings.gradle                          # Gradle settings
└── gradle/libs.versions.toml                # Dependency versions
```

## License

MIT License — See `LICENSE` file for details.

## Disclaimer

This mod is for educational and single-player use only. Using utility mods on multiplayer servers may violate server rules and terms of service. The authors are not responsible for any consequences of misuse.

---

**UtilityMod** — Built for 1.21.11, designed to stay there.