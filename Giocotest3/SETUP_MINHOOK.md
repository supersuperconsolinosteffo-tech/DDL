# MinHook Setup for UtilityMod

MinHook is a lightweight library for hooking functions on Windows, Linux, and macOS.
This mod uses the Windows version.

## Status: vendored (no download needed)

MinHook is now checked into the repository, so the build works offline and does
not depend on third-party downloads at build time:

```
utilitymod/src/main/cpp/native/minhook/
├── include/
│   └── MinHook.h
├── src/
│   ├── buffer.c / buffer.h
│   ├── hook.c
│   ├── trampoline.c / trampoline.h
│   └── hde/
│       ├── hde32.c / hde32.h / table32.h
│       ├── hde64.c / hde64.h / table64.h
│       └── pstdint.h
└── LICENSE.txt
```

The three files the build compiles are `src/buffer.c`, `src/hook.c` and
`src/trampoline.c` (plus the HDE disassembler tables they depend on).

`CMakeLists.txt` adds `minhook/include` to the include path, which is how
`hooks/minecraft_hooks.hpp` resolves `#include <MinHook.h>`.

## Verifying the vendored copy

```bash
ls utilitymod/src/main/cpp/native/minhook/src
# buffer.c  buffer.h  hde  hook.c  trampoline.c  trampoline.h
```

If any of those files is missing, restore the checkout — do **not** re-create
them by download. Earlier versions of this project fetched them with:

```
https://raw.githubusercontent.com/TsudaK/minhook/v2.8.2/src/hook.c
```

That URL does not exist (the upstream project is `TsudaKageyu/minhook` and has
no `v2.8.2` tag), so the downloads silently saved `404: Not Found` into the
`.c` files, which then failed to compile.

## Updating MinHook

```bash
curl -L -o minhook.tar.gz https://codeload.github.com/TsudaKageyu/minhook/tar.gz/refs/heads/master
tar xzf minhook.tar.gz
cp -r minhook-master/include/MinHook.h      utilitymod/src/main/cpp/native/minhook/include/
cp    minhook-master/src/*.c minhook-master/src/*.h utilitymod/src/main/cpp/native/minhook/src/
cp -r minhook-master/src/hde/*              utilitymod/src/main/cpp/native/minhook/src/hde/
cp    minhook-master/LICENSE.txt            utilitymod/src/main/cpp/native/minhook/
```

## MinHook License

MinHook is released under the MIT License. See `minhook/LICENSE.txt` for the
full license terms.

## Cross-Platform Notes

### Windows (MSVC) - this mod's primary target
MinHook compiles as C (the target enables both the `C` and `CXX` languages).
MSVC links `psapi.lib` for `GetModuleInformation`, which the pattern scanner
uses.

### MinGW-w64
Works with both the win32 and POSIX thread flavours, but the rest of the native
layer requires the POSIX flavour (`std::thread`).
