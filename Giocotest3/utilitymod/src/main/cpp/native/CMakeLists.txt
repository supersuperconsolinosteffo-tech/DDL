cmake_minimum_required(VERSION 3.20)
project(utilitymod_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(UTILITYMOD_WERROR "Treat compiler warnings as errors" OFF)
set(UTILITYMOD_JNI_INCLUDE_DIR "" CACHE PATH
    "Path to a JDK 'include' directory (containing jni.h and jvmti.h). \
Leave empty to use JAVA_HOME or CMake's FindJNI.")

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS 1)
    add_definitions(-DUTILITYMOD_WINDOWS)
elseif(UNIX AND APPLE)
    set(PLATFORM_MACOS 1)
    add_definitions(-DUTILITYMOD_MACOS)
elseif(UNIX)
    set(PLATFORM_LINUX 1)
    add_definitions(-DUTILITYMOD_LINUX)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# JNI / JVMTI headers (not vendored: they belong to the JDK)
# ---------------------------------------------------------------------------
set(UTILITYMOD_JNI_DIRS "")
if(UTILITYMOD_JNI_INCLUDE_DIR)
    list(APPEND UTILITYMOD_JNI_DIRS ${UTILITYMOD_JNI_INCLUDE_DIR})
    if(WIN32)
        list(APPEND UTILITYMOD_JNI_DIRS ${UTILITYMOD_JNI_INCLUDE_DIR}/win32)
    endif()
elseif(DEFINED ENV{JAVA_HOME})
    set(_java_include "$ENV{JAVA_HOME}/include")
    list(APPEND UTILITYMOD_JNI_DIRS ${_java_include})
    if(WIN32)
        list(APPEND UTILITYMOD_JNI_DIRS ${_java_include}/win32)
    endif()
else()
    find_package(JNI QUIET)
    if(JNI_INCLUDE_DIRS)
        list(APPEND UTILITYMOD_JNI_DIRS ${JNI_INCLUDE_DIRS})
    endif()
endif()

if(NOT UTILITYMOD_JNI_DIRS)
    message(FATAL_ERROR
        "JNI headers (jni.h / jvmti.h) were not found.\n"
        "Install a JDK 17+ and either set JAVA_HOME or configure with "
        "-DUTILITYMOD_JNI_INCLUDE_DIR=<jdk>/include")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hooks
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/minhook/include
    ${UTILITYMOD_JNI_DIRS}
)

# ImGui sources (docking branch: docking + viewports are used by imgui_manager)
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

# ImGui backends
if(PLATFORM_WINDOWS)
    set(IMGUI_BACKENDS
        imgui/backends/imgui_impl_win32.cpp
        imgui/backends/imgui_impl_dx11.cpp
    )
elseif(PLATFORM_LINUX)
    set(IMGUI_BACKENDS
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl3.cpp
    )
elseif(PLATFORM_MACOS)
    set(IMGUI_BACKENDS
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_metal.mm
    )
endif()

# MinHook sources (upstream layout: src/ + src/hde/)
set(MINHOOK_SOURCES
    minhook/src/buffer.c
    minhook/src/hook.c
    minhook/src/trampoline.c
    minhook/src/hde/hde32.c
    minhook/src/hde/hde64.c
)

# UtilityMod sources
set(UTILITYMOD_SOURCES
    jni/jni_bridge.cpp
    hooks/minecraft_hooks.cpp
    native_client.cpp
    imgui_manager.cpp
    entry_point.cpp
)

# Create shared library
add_library(utilitymod_native SHARED
    ${IMGUI_SOURCES}
    ${IMGUI_BACKENDS}
    ${MINHOOK_SOURCES}
    ${UTILITYMOD_SOURCES}
)

# Platform-specific libraries
if(PLATFORM_WINDOWS)
    target_link_libraries(utilitymod_native PRIVATE
        d3d11
        dxgi
        d3dcompiler
        user32
        gdi32
        dwmapi
        psapi
    )
    target_compile_definitions(utilitymod_native PRIVATE
        IMGUI_IMPL_WIN32
        IMGUI_IMPL_DX11
    )
elseif(PLATFORM_LINUX)
    target_link_libraries(utilitymod_native PRIVATE
        glfw
        GL
        dl
        pthread
    )
    target_compile_definitions(utilitymod_native PRIVATE
        IMGUI_IMPL_GLFW
        IMGUI_IMPL_OPENGL3
    )
elseif(PLATFORM_MACOS)
    target_link_libraries(utilitymod_native PRIVATE
        glfw
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Cocoa"
    )
    target_compile_definitions(utilitymod_native PRIVATE
        IMGUI_IMPL_GLFW
        IMGUI_IMPL_METAL
    )
endif()

# Compiler warnings (applied to the whole target, including vendored sources,
# so they are informational only unless UTILITYMOD_WERROR is enabled)
if(MSVC)
    target_compile_options(utilitymod_native PRIVATE /W4)
    if(UTILITYMOD_WERROR)
        target_compile_options(utilitymod_native PRIVATE /WX)
    endif()
else()
    target_compile_options(utilitymod_native PRIVATE -Wall -Wextra)
    if(UTILITYMOD_WERROR)
        target_compile_options(utilitymod_native PRIVATE -Werror)
    endif()
endif()

# Position independent code
set_target_properties(utilitymod_native PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Windows: produce "utilitymod_native.dll" (MinGW would otherwise add a "lib"
# prefix) and link the C/C++ runtime statically so the DLL does not depend on
# libgcc/libstdc++/libwinpthread being present next to Minecraft's Java runtime.
if(WIN32)
    set_target_properties(utilitymod_native PROPERTIES PREFIX "")
endif()
if(MINGW)
    # Self-contained runtime: the DLL must not require libgcc_s/libstdc++/
    # libwinpthread to be shipped next to Minecraft's Java runtime.
    target_link_options(utilitymod_native PRIVATE
        -static-libgcc
        -static-libstdc++
    )
    # Full -static also pulls in libwinpthread statically; GNU ld accepts it,
    # LLVM/lld (as used by Zig's mingw driver) rejects it for shared libraries.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(utilitymod_native PRIVATE -static)
    endif()
endif()

# Version info
set_target_properties(utilitymod_native PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
