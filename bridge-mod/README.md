# UtilityClient Bridge (Fabric, Minecraft 1.21.11)

The only Java code in this project — deliberately almost empty.

`UtilityBridgeMod` registers **one** callback, `ClientTickEvents.START_CLIENT_TICK`, which calls `NativeGate.nativeOnStartTick()`. The injected C++ DLL registers that native method (found via a JVMTI loaded-class scan) and runs all module logic + the vanilla-packet flush inside it. There are no packets, no end-of-tick hooks, no mixins and no module logic in this mod.

Without the DLL, every call throws `UnsatisfiedLinkError`, which `NativeGate` handles with a 100-tick cooldown — the bridge is a harmless no-op on its own. When the DLL unloads, it calls `UnregisterNatives` first, so the bridge returns to that same no-op state instead of crashing.

Build (JDK 21 + internet on first run):

```bat
gradlew.bat build
```

Output: `build/libs/utilityclient-bridge-1.0.0.jar` → put it in `mods/` next to Fabric API. Pinned for Minecraft **1.21.11** / Loader 0.20.1 / Fabric API 0.141.6+1.21.11 (see `gradle.properties`). Version-locked on purpose; see the root README.
