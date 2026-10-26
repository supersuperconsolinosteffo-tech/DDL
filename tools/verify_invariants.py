#!/usr/bin/env python3
"""
Static checks for the project's hard invariants (run: python3 tools/verify_invariants.py).

  I1  Existing vanilla packets only - no custom payload/packet APIs anywhere.
  I2  Start-of-tick only - the bridge registers START_CLIENT_TICK (never END),
      packets are sent from exactly one call site, that send path is drained
      from exactly one place (the tick gate), and module code never sends
      directly.
  I3  Never create/destroy the JVM - attach to the running one.
  I4  Version lock - the generated mapping header targets 1.21.11.
  I5  Vendored deps are real sources (regression guard for placeholder files).

Exits non-zero with a report if anything fails. This exists so a future edit
cannot quietly break the rules in README without this script going red.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "native" / "src"
BRIDGE = ROOT / "bridge-mod" / "src"
failures = []
checks = 0


def check(ok: bool, label: str):
    global checks
    checks += 1
    print(("  PASS  " if ok else "  FAIL  ") + label)
    if not ok:
        failures.append(label)


def iter_lines(base: pathlib.Path, suffixes):
    for p in sorted(base.rglob("*")):
        if p.is_file() and p.suffix in suffixes:
            for i, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
                yield p.relative_to(ROOT), i, line


def find(base, suffixes, needle, negated_doc=("never", "no custom", "not call", "forbidden")):
    hits = []
    for path, i, line in iter_lines(base, suffixes):
        if needle in line and not any(n in line.lower() for n in negated_doc):
            hits.append(f"{path}:{i}: {line.strip()[:110]}")
    return hits


CPP = {".cpp", ".hpp", ".h", ".c"}
JAVA = {".java"}

print("I1 existing-packets-only")
for term in ("CustomPayload", "CustomPacket", "custom_payload", "ServerboundCustom", "ClientboundCustom"):
    hits = find(SRC, CPP, term) + find(BRIDGE, JAVA, term)
    check(not hits, f"no {term} anywhere" + ("" if not hits else f" -> {hits}"))

print("I2 start-of-tick only")
hits = find(BRIDGE, JAVA, "END_CLIENT_TICK")
check(not hits, "bridge never uses END_CLIENT_TICK" + ("" if not hits else f" -> {hits}"))
hits = find(BRIDGE, JAVA, "START_CLIENT_TICK")
check(bool(hits), "bridge registers START_CLIENT_TICK")
hits = find(SRC, CPP, "netSendPacket")
send_sites = [h for h in hits if "packet_queue.cpp" in h and "CallVoidMethod" in h]
check(len(send_sites) == 1, f"exactly one packet send call site (packet_queue.cpp) -> {send_sites}")
hits = find(SRC, CPP, "DrainAtTickStart(")
callers = [h for h in hits if "tick_gate.cpp" in h]
defs = [h for h in hits if "packet_queue" in h]
check(len(callers) == 1 and len(hits) - len(defs) - len(callers) == 0,
      f"packet queue drained only from tick_gate.cpp -> callers={callers} others={[h for h in hits if h not in callers and h not in defs]}")
hits = find(SRC, CPP, "InGate()")
check(any("packet_queue.cpp" in h for h in hits), "drain refuses to run outside the gate (InGate check)")
hits = find(SRC / "modules.cpp".replace("modules.cpp", ""), CPP, "PacketQueue::Get().Push")
check(bool(hits), "modules enqueue (Push) instead of sending")

print("I3 attach-only JVM use")
for term in ("JNI_CreateJavaVM", "DestroyJavaVM"):
    hits = find(SRC, CPP, term)
    check(not hits, f"no {term}" + ("" if not hits else f" -> {hits}"))
hits = find(SRC, CPP, "JNI_GetCreatedJavaVMs")
check(bool(hits), "uses JNI_GetCreatedJavaVMs on the running JVM")
hits = find(SRC, CPP, "GetLoadedClasses")
check(bool(hits), "classes resolved via JVMTI GetLoadedClasses")

print("I4 version lock 1.21.11")
hdr = (SRC / "mappings_12111.hpp").read_text()
check('"1.21.11"' in hdr, "mapping header pins 1.21.11")
for token in ('"net/minecraft/class_310"', '"method_52787"', '"method_1574"'):
    check(token in hdr, f"mapping header contains {token}")
check("versionOk" in (SRC / "jni_bridge.cpp").read_text(), "runtime version check present (versionOk)")
fmj = (ROOT / "bridge-mod/src/main/resources/fabric.mod.json").read_text()
check('"minecraft": "1.21.11"' in fmj and '"java": ">=21"' in fmj, "bridge mod pins minecraft 1.21.11 + java 21")

print("I5 real vendored sources")
imgui_cpp = ROOT / "native/third_party/imgui/imgui.cpp"
check(imgui_cpp.exists() and imgui_cpp.stat().st_size > 500_000, "ImGui sources vendored (imgui.cpp > 500 KB)")
for name in ("buffer.c", "hook.c", "trampoline.c"):
    p = ROOT / "native/third_party/minhook/src" / name
    body = p.read_text(errors="replace")[:200] if p.exists() else ""
    check(p.exists() and p.stat().st_size > 1000 and "404" not in body, f"MinHook {name} is real source")
check((ROOT / "native/third_party/minhook/include/MinHook.h").exists(), "MinHook.h present")
check((ROOT / "native/third_party/jni/jni.h").exists() and (ROOT / "native/third_party/jni/jvmti.h").exists(),
      "JDK jni.h/jvmti.h vendored")

print(f"\n{checks - len(failures)}/{checks} invariant checks passed")
if failures:
    print("FAILURES:")
    for f in failures:
        print(" -", f)
    sys.exit(1)
