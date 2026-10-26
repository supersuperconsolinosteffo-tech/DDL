# Cross-compile UtilityClient.dll from Linux/macOS with zig (CI + sandbox
# verification). Requires `zig` on PATH (https://ziglang.org/download or
# `pip install ziglang`). Windows users should just use build.bat (MSVC).
#
#   cmake -S native -B native/build-zig -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=native/toolchain-zig-windows-x64.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build native/build-zig
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER zig)
set(CMAKE_C_COMPILER_ARG1 "cc")
set(CMAKE_CXX_COMPILER zig)
set(CMAKE_CXX_COMPILER_ARG1 "c++")

set(CMAKE_C_FLAGS_INIT "-target x86_64-windows-gnu")
set(CMAKE_CXX_FLAGS_INIT "-target x86_64-windows-gnu")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-target x86_64-windows-gnu")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-target x86_64-windows-gnu")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-target x86_64-windows-gnu")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
