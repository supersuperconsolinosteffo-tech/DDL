cmake_minimum_required(VERSION 3.20)
project(UtilityClient VERSION 1.0.0 LANGUAGES C CXX)

# UtilityClient - injectable C++ DLL for Minecraft 1.21.11 (Fabric).
# Windows x64 only: Minecraft Java on Windows uses an x64 JVM + OpenGL.
if(NOT WIN32)
    message(FATAL_ERROR "UtilityClient targets Windows (injected DLL). Use a Windows or windows-gnu toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "UtilityClient must be built as x64 to match the x64 Minecraft JVM.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    # Static CRT: the DLL must inject without any VC++ redistributable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(UTILITYCLIENT_BUILD_INJECTOR "Build UtilityInjector.exe" ON)

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# --- Dear ImGui 1.91.9b (vendored) ---
add_library(imgui STATIC
    ${TP}/imgui/imgui.cpp
    ${TP}/imgui/imgui_draw.cpp
    ${TP}/imgui/imgui_tables.cpp
    ${TP}/imgui/imgui_widgets.cpp
    ${TP}/imgui/backends/imgui_impl_win32.cpp
    ${TP}/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${TP}/imgui ${TP}/imgui/backends)
target_link_libraries(imgui PUBLIC opengl32 user32 gdi32 dwmapi)

# --- MinHook 1.3.4 (vendored) ---
add_library(minhook STATIC
    ${TP}/minhook/src/buffer.c
    ${TP}/minhook/src/hook.c
    ${TP}/minhook/src/trampoline.c
    ${TP}/minhook/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${TP}/minhook/include)

if(MSVC)
    # Third-party code keeps its own warning level; our code is /W4 below.
    target_compile_options(imgui PRIVATE /W0)
    target_compile_options(minhook PRIVATE /W0)
endif()

# --- UtilityClient.dll ---
add_library(UtilityClient SHARED
    src/bootstrap.cpp
    src/config.cpp
    src/hooks.cpp
    src/jni_bridge.cpp
    src/log.cpp
    src/menu.cpp
    src/module.cpp
    src/modules.cpp
    src/packet_queue.cpp
    src/tick_gate.cpp
)
target_include_directories(UtilityClient PRIVATE
    src
    ${TP}/jni
    ${TP}/jni/win32
)
target_compile_definitions(UtilityClient PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(UtilityClient PRIVATE imgui minhook)
set_target_properties(UtilityClient PROPERTIES OUTPUT_NAME "UtilityClient" PREFIX "")

if(MSVC)
    target_compile_options(UtilityClient PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(UtilityClient PRIVATE -Wall -Wextra)
    # MinGW/zig: no libgcc/libstdc++ runtime DLL dependencies.
    target_link_options(UtilityClient PRIVATE -static-libgcc -static-libstdc++ -s)
endif()
# DllMain + exports live in bootstrap.cpp (DllMain at the bottom of that TU).

# --- UtilityInjector.exe ---
if(UTILITYCLIENT_BUILD_INJECTOR)
    add_executable(UtilityInjector injector/injector.cpp)
    target_compile_definitions(UtilityInjector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    set_target_properties(UtilityInjector PROPERTIES OUTPUT_NAME "UtilityInjector")
    if(MSVC)
        target_compile_options(UtilityInjector PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(UtilityInjector PRIVATE -Wall -Wextra)
        target_link_options(UtilityInjector PRIVATE -static-libgcc -static-libstdc++ -municode -s)
    endif()
endif()
