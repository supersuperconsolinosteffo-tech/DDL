#!/usr/bin/env python3
"""
Generate native/src/mappings_12111.hpp from a Fabric Yarn 1.21.11 checkout.

Why: the injected DLL runs inside a *production* Fabric process where every
Minecraft class/member is named by its intermediary name (class_310,
method_1574, field_1724...). Those names are pinned to Minecraft 1.21.11 and
are never updated by this project. This script extracts exactly the members
the DLL needs from the official Yarn 1.21.11 enigma mappings
(https://github.com/FabricMC/yarn, branch 1.21.11), where each line is
<intermediary-name> <yarn-name> <intermediary-descriptor>.

Enum constants are not renamed by intermediary (verified against
FabricMC/intermediary mappings/1.21.11.tiny: no FIELD entries for them),
so they are emitted with their source names (MAIN_HAND, NORTH, ...).

Usage:
    python3 tools/gen_mappings.py <path-to-yarn-repo-1.21.11> [out-header]

Fails loudly if any wanted member is missing (except entries marked probe,
which the DLL resolves with a runtime fallback chain; see jni_bridge.cpp).
"""
import pathlib
import sys

# (cpp section, yarn class) -> intermediary class name is read from mappings
CLASSES = [
    ("MinecraftClient", "net/minecraft/client/MinecraftClient"),
    ("ClientPlayerEntity", "net/minecraft/client/network/ClientPlayerEntity"),
    ("ClientWorld", "net/minecraft/client/world/ClientWorld"),
    ("ClientPlayNetworkHandler", "net/minecraft/client/network/ClientPlayNetworkHandler"),
    ("ClientCommonNetworkHandler", "net/minecraft/client/network/ClientCommonNetworkHandler"),
    ("PendingUpdateManager", "net/minecraft/client/network/PendingUpdateManager"),
    ("Entity", "net/minecraft/entity/Entity"),
    ("LivingEntity", "net/minecraft/entity/LivingEntity"),
    ("PlayerEntity", "net/minecraft/entity/player/PlayerEntity"),
    ("PlayerAbilities", "net/minecraft/entity/player/PlayerAbilities"),
    ("HungerManager", "net/minecraft/entity/player/HungerManager"),
    ("PlayerInventory", "net/minecraft/entity/player/PlayerInventory"),
    ("Inventory", "net/minecraft/inventory/Inventory"),
    ("ItemStack", "net/minecraft/item/ItemStack"),
    ("UseAction", "net/minecraft/item/consume/UseAction"),
    ("GameOptions", "net/minecraft/client/option/GameOptions"),
    ("KeyBinding", "net/minecraft/client/option/KeyBinding"),
    ("SimpleOption", "net/minecraft/client/option/SimpleOption"),
    ("Mouse", "net/minecraft/client/Mouse"),
    ("World", "net/minecraft/world/World"),
    ("MoveFull", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$Full"),
    ("MovePos", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$PositionAndOnGround"),
    ("MoveLook", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$LookAndOnGround"),
    ("MoveOnGround", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$OnGroundOnly"),
    ("PlayerActionPacket", "net/minecraft/network/packet/c2s/play/PlayerActionC2SPacket"),
    ("PlayerActionEnum", "net/minecraft/network/packet/c2s/play/PlayerActionC2SPacket$Action"),
    ("InteractBlockPacket", "net/minecraft/network/packet/c2s/play/PlayerInteractBlockC2SPacket"),
    ("InteractItemPacket", "net/minecraft/network/packet/c2s/play/PlayerInteractItemC2SPacket"),
    ("InteractEntityPacket", "net/minecraft/network/packet/c2s/play/PlayerInteractEntityC2SPacket"),
    ("HandSwingPacket", "net/minecraft/network/packet/c2s/play/HandSwingC2SPacket"),
    ("PickFromBlockPacket", "net/minecraft/network/packet/c2s/play/PickItemFromBlockC2SPacket"),
    ("Hand", "net/minecraft/util/Hand"),
    ("BlockPos", "net/minecraft/util/math/BlockPos"),
    ("Vec3i", "net/minecraft/util/math/Vec3i"),
    ("Vec3d", "net/minecraft/util/math/Vec3d"),
    ("Direction", "net/minecraft/util/math/Direction"),
    ("BlockHitResult", "net/minecraft/util/hit/BlockHitResult"),
]

# (cpp const name, owner yarn class, kind, yarn member name, intermediary desc or None)
# kind: F field, M method, CTOR constructor (member name = desc), ENUM enum constant (name as-is)
# CTOR entries with name starting "?" are probes: mapping files omit them
# (record canonical ctors / ctors shared with a superclass) -> resolved at runtime.
MEMBERS = [
    ("McGetInstance", "MinecraftClient", "M", "getInstance", None),
    ("McGetGameVersion", "MinecraftClient", "M", "getGameVersion", None),
    ("McTick", "MinecraftClient", "M", "tick", None),
    ("McPlayer", "MinecraftClient", "F", "player", None),
    ("McWorld", "MinecraftClient", "F", "world", None),
    ("McOptions", "MinecraftClient", "F", "options", None),
    ("McMouse", "MinecraftClient", "F", "mouse", None),
    ("McCurrentScreen", "MinecraftClient", "F", "currentScreen", None),
    ("CpeNetworkHandler", "ClientPlayerEntity", "F", "networkHandler", None),
    ("CpeRequestRespawn", "ClientPlayerEntity", "M", "requestRespawn", None),
    ("NetSendPacket", "ClientCommonNetworkHandler", "M", "sendPacket", None),
    ("WorldGetPendingUpdates", "ClientWorld", "M", "getPendingUpdateManager", None),
    ("WorldGetEntityById", "World", "M", "getEntityById", None),
    ("PumIncrementSequence", "PendingUpdateManager", "M", "incrementSequence", None),
    ("PumGetSequence", "PendingUpdateManager", "M", "getSequence", None),
    ("EntGetX", "Entity", "M", "getX", None),
    ("EntGetY", "Entity", "M", "getY", None),
    ("EntGetZ", "Entity", "M", "getZ", None),
    ("EntGetYaw", "Entity", "M", "getYaw", "()F"),
    ("EntGetPitch", "Entity", "M", "getPitch", "()F"),
    ("EntIsOnGround", "Entity", "M", "isOnGround", None),
    ("EntGetVelocity", "Entity", "M", "getVelocity", None),
    ("EntSetVelocity", "Entity", "M", "setVelocity", "(DDD)V"),
    ("EntSetSprinting", "Entity", "M", "setSprinting", None),
    ("EntIsAlive", "Entity", "M", "isAlive", None),
    ("EntFallDistance", "Entity", "F", "fallDistance", None),
    ("EntNoClip", "Entity", "F", "noClip", None),
    ("EntHorizontalCollision", "Entity", "F", "horizontalCollision", None),
    ("LivIsUsingItem", "LivingEntity", "M", "isUsingItem", None),
    ("LivGetHealth", "LivingEntity", "M", "getHealth", None),
    ("PlGetAbilities", "PlayerEntity", "M", "getAbilities", None),
    ("PlGetHungerManager", "PlayerEntity", "M", "getHungerManager", None),
    ("PlGetInventory", "PlayerEntity", "M", "getInventory", None),
    ("PlSendAbilitiesUpdate", "PlayerEntity", "M", "sendAbilitiesUpdate", None),
    ("AbFlying", "PlayerAbilities", "F", "flying", None),
    ("AbAllowFlying", "PlayerAbilities", "F", "allowFlying", None),
    ("AbSetFlySpeed", "PlayerAbilities", "M", "setFlySpeed", None),
    ("AbGetFlySpeed", "PlayerAbilities", "M", "getFlySpeed", None),
    ("HungerGetFoodLevel", "HungerManager", "M", "getFoodLevel", None),
    ("InvGetStack", "Inventory", "M", "getStack", None),
    ("InvSetSelectedSlot", "PlayerInventory", "M", "setSelectedSlot", None),
    ("InvGetSelectedSlot", "PlayerInventory", "M", "getSelectedSlot", None),
    ("StackIsEmpty", "ItemStack", "M", "isEmpty", None),
    ("StackGetUseAction", "ItemStack", "M", "getUseAction", None),
    ("UseActionSerializedName", "UseAction", "F", "name", None),
    ("OptForwardKey", "GameOptions", "F", "forwardKey", None),
    ("OptSprintKey", "GameOptions", "F", "sprintKey", None),
    ("OptJumpKey", "GameOptions", "F", "jumpKey", None),
    ("OptSneakKey", "GameOptions", "F", "sneakKey", None),
    ("OptGetGamma", "GameOptions", "M", "getGamma", None),
    ("KeySetPressed", "KeyBinding", "M", "setPressed", None),
    ("KeyIsPressed", "KeyBinding", "M", "isPressed", None),
    ("SimpleOptGetValue", "SimpleOption", "M", "getValue", None),
    ("SimpleOptSetValue", "SimpleOption", "M", "setValue", None),
    ("SimpleOptValueField", "SimpleOption", "F", "value", None),
    ("MouseLockCursor", "Mouse", "M", "lockCursor", None),
    ("MouseUnlockCursor", "Mouse", "M", "unlockCursor", None),
    ("MouseIsCursorLocked", "Mouse", "M", "isCursorLocked", None),
    ("CtorMoveFull", "MoveFull", "CTOR", "(DDDFFZZ)V", None),
    ("CtorMovePos", "MovePos", "CTOR", "(DDDZZ)V", None),
    ("CtorMoveLook", "MoveLook", "CTOR", "(FFZZ)V", None),
    ("CtorMoveOnGround", "MoveOnGround", "CTOR", "(ZZ)V", None),
    ("CtorPlayerAction", "PlayerActionPacket", "CTOR", "(Lnet/minecraft/class_2846$class_2847;Lnet/minecraft/class_2338;Lnet/minecraft/class_2350;I)V", None),
    ("CtorInteractBlock", "InteractBlockPacket", "CTOR", "(Lnet/minecraft/class_1268;Lnet/minecraft/class_3965;I)V", None),
    ("CtorInteractItem", "InteractItemPacket", "CTOR", "(Lnet/minecraft/class_1268;IFF)V", None),
    ("CtorHandSwing", "HandSwingPacket", "CTOR", "(Lnet/minecraft/class_1268;)V", None),
    ("CtorPickFromBlock", "PickFromBlockPacket", "CTOR", "?(Lnet/minecraft/class_2338;Z)V", None),
    ("FactoryInteractEntity", "InteractEntityPacket", "M", "interact", None),
    ("FactoryInteractAtEntity", "InteractEntityPacket", "M", "interactAt", None),
    ("FactoryAttackEntity", "InteractEntityPacket", "M", "attack", None),
    ("CtorBlockPosInts", "BlockPos", "CTOR", "?(III)V", None),
    ("CtorBlockPosVec3i", "BlockPos", "CTOR", "(Lnet/minecraft/class_2382;)V", None),
    ("CtorVec3i", "Vec3i", "CTOR", "(III)V", None),
    ("CtorVec3d", "Vec3d", "CTOR", "(DDD)V", None),
    ("CtorBlockHitResult", "BlockHitResult", "CTOR", "(Lnet/minecraft/class_243;Lnet/minecraft/class_2350;Lnet/minecraft/class_2338;Z)V", None),
    ("Vec3dX", "Vec3d", "F", "x", None),
    ("Vec3dY", "Vec3d", "F", "y", None),
    ("Vec3dZ", "Vec3d", "F", "z", None),
    ("HandMain", "Hand", "ENUM", "MAIN_HAND", None),
    ("HandOff", "Hand", "ENUM", "OFF_HAND", None),
    ("DirDown", "Direction", "ENUM", "DOWN", None),
    ("DirUp", "Direction", "ENUM", "UP", None),
    ("DirNorth", "Direction", "ENUM", "NORTH", None),
    ("DirSouth", "Direction", "ENUM", "SOUTH", None),
    ("DirWest", "Direction", "ENUM", "WEST", None),
    ("DirEast", "Direction", "ENUM", "EAST", None),
    ("ActStartDestroy", "PlayerActionEnum", "ENUM", "START_DESTROY_BLOCK", None),
    ("ActAbortDestroy", "PlayerActionEnum", "ENUM", "ABORT_DESTROY_BLOCK", None),
    ("ActStopDestroy", "PlayerActionEnum", "ENUM", "STOP_DESTROY_BLOCK", None),
    ("ActDropAll", "PlayerActionEnum", "ENUM", "DROP_ALL_ITEMS", None),
    ("ActDropItem", "PlayerActionEnum", "ENUM", "DROP_ITEM", None),
    ("ActReleaseUse", "PlayerActionEnum", "ENUM", "RELEASE_USE_ITEM", None),
    ("ActSwapOffhand", "PlayerActionEnum", "ENUM", "SWAP_ITEM_WITH_OFFHAND", None),
]


def yarn_file_for(root: pathlib.Path, yarn_class: str) -> pathlib.Path:
    # inner classes live in their outer class file
    return root / "mappings" / (yarn_class.split("$")[0] + ".mapping")


def parse_file(path: pathlib.Path):
    classes = {}
    stack = []  # (indent, record)
    for raw in path.read_text(errors="replace").splitlines():
        if not raw.strip():
            continue
        stripped = raw.lstrip()
        if stripped.startswith(("COMMENT", "ARG")):
            continue
        indent = len(raw) - len(raw.lstrip("\t"))
        parts = stripped.split()
        kind = parts[0]
        if kind == "CLASS":
            inter = parts[1]
            yarn = parts[2] if len(parts) > 2 else parts[1].rsplit("/", 1)[-1]
            while stack and stack[-1][0] >= indent:
                stack.pop()
            if stack:
                full_inter = stack[-1][1]["inter"] + "$" + inter
                full_yarn = stack[-1][1]["yarn"] + "$" + yarn
            else:
                full_inter, full_yarn = inter, yarn
            rec = {"inter": full_inter, "yarn": full_yarn, "fields": {}, "methods": {}, "ctors": []}
            classes[full_yarn] = rec
            stack.append((indent, rec))
        elif kind == "FIELD" and stack:
            stack[-1][1]["fields"][parts[2]] = (parts[1], parts[3])
        elif kind == "METHOD" and stack:
            if parts[1] == "<init>":
                stack[-1][1]["ctors"].append(parts[2])
            elif len(parts) >= 4:
                stack[-1][1]["methods"][(parts[2], parts[3])] = parts[1]
    return classes


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    Yarn = pathlib.Path(sys.argv[1])
    out_path = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else pathlib.Path("native/src/mappings_12111.hpp")

    yarn_by_section = {section: yarn for section, yarn in CLASSES}
    records = {}
    for _, yarn in CLASSES:
        p = yarn_file_for(Yarn, yarn)
        if not p.exists():
            print(f"FATAL: mapping file not found: {p}")
            return 1
        records.update(parse_file(p))

    lines = []
    lines.append("// mappings_12111.hpp - GENERATED by tools/gen_mappings.py, DO NOT EDIT BY HAND.")
    lines.append("// Intermediary names for Minecraft 1.21.11 (Fabric), from official Yarn 1.21.11")
    lines.append("// enigma mappings (FabricMC/yarn branch 1.21.11). Production Fabric runs on")
    lines.append("// intermediary names, which is what an injected DLL sees through JNI/JVMTI.")
    lines.append("// This project is version-locked to 1.21.11: these names are correct for that")
    lines.append("// version forever and are intentionally never updated.")
    lines.append("#pragma once")
    lines.append("")
    lines.append("namespace uc::mapping {")
    lines.append("")
    lines.append('inline constexpr const char* kExpectedMcVersion = "1.21.11";')
    lines.append("")
    lines.append("struct NamedDesc { const char* name; const char* desc; };")
    lines.append("")
    lines.append("// Classes (JVMS internal names)")
    for section, yarn in CLASSES:
        rec = records.get(yarn)
        if rec is None:
            print(f"FATAL: class not found in mappings: {yarn}")
            return 1
        lines.append(f'inline constexpr const char* k{section} = "{rec["inter"]}";  // {yarn}')
    lines.append("")
    lines.append("// Members: {intermediary name, intermediary descriptor}")
    errors = 0
    for cpp_name, section, kind, member, desc_hint in MEMBERS:
        yarn = yarn_by_section[section]
        rec = records[yarn]
        inter_class = rec["inter"]
        if kind == "F":
            hit = rec["fields"].get(member)
            if hit is None:
                print(f"FATAL: field {yarn}.{member} not found")
                errors += 1
                continue
            lines.append(f'inline constexpr NamedDesc k{cpp_name} = {{"{hit[0]}", "{hit[1]}"}};  // {yarn}.{member}')
        elif kind == "M":
            cands = [(d, i) for (y, d), i in rec["methods"].items() if y == member and (desc_hint is None or d == desc_hint)]
            if len(cands) != 1:
                print(f"FATAL: method {yarn}.{member} ({desc_hint}) -> {cands}")
                errors += 1
                continue
            d, i = cands[0]
            lines.append(f'inline constexpr NamedDesc k{cpp_name} = {{"{i}", "{d}"}};  // {yarn}.{member}')
        elif kind == "CTOR":
            probe = member.startswith("?")
            desc = member[1:] if probe else member
            if not probe and desc not in rec["ctors"]:
                print(f"FATAL: ctor {yarn}.<init>{desc} not found, have {rec['ctors']}")
                errors += 1
                continue
            note = "probe (omitted from enigma; resolved at runtime with fallback)" if probe else f"{yarn}.<init>"
            lines.append(f'inline constexpr NamedDesc k{cpp_name} = {{"<init>", "{desc}"}};  // {note}')
        elif kind == "ENUM":
            lines.append(f'inline constexpr NamedDesc k{cpp_name} = {{"{member}", "L{inter_class};"}};  // enum const (not renamed by intermediary) {yarn}.{member}')
    lines.append("")
    lines.append("}  // namespace uc::mapping")
    lines.append("")
    if errors:
        print(f"{errors} mapping error(s), header NOT written")
        return 1
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines))
    print(f"wrote {out_path}: {len(CLASSES)} classes, {len(MEMBERS)} members")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
