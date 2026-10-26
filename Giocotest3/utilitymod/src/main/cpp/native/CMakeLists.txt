cmake_minimum_required(VERSION 3.20)
project(utilitymod_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/minhook
    ${CMAKE_CURRENT_SOURCE_DIR}/hooks
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
)

# ImGui sources
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
        imgui/backends/imgui_impl_metal.cpp
    )
endif()

# MinHook sources
set(MINHOOK_SOURCES
    minhook/buffer.c
    minhook/hook.c
    minhook/trampoline.c
)

# UtilityMod sources
set(UTILITYMOD_SOURCES
    jni/jni_bridge.cpp
    hooks/minecraft_hooks.cpp
    native_client.cpp
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
        user32
        gdi32
        dwmapi
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

# Compiler warnings
target_compile_options(utilitymod_native PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

# Position independent code
set_target_properties(utilitymod_native PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Version info
set_target_properties(utilitymod_native PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)