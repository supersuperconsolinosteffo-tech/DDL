# toolchain-mingw-w64.cmake
#
# Cross-compile utilitymod_native.dll for Windows x64 from Linux using MinGW-w64.
#
# Usage:
#   cmake -S . -B build-win64 \
#         -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw-w64.cmake \
#         -DUTILITYMOD_JNI_INCLUDE_DIR=/path/to/jdk/include
#   cmake --build build-win64 -j
#
# Notes:
#   * Use the POSIX threads flavour of the compiler (x86_64-w64-mingw32-g++-posix
#     on Debian/Ubuntu), because the native layer uses std::thread/std::mutex/
#     std::call_once. The win32 threads flavour cannot compile those.
#   * Override the prefix for other toolchains, e.g.
#     -DMINGW_PREFIX=x86_64-w64-mingw32.static

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX "x86_64-w64-mingw32" CACHE STRING "MinGW-w64 tool prefix")

set(CMAKE_C_COMPILER   ${MINGW_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${MINGW_PREFIX}-windres)
set(CMAKE_AR           ${MINGW_PREFIX}-ar)
set(CMAKE_RANLIB       ${MINGW_PREFIX}-ranlib)

set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
