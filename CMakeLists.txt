# Kryon-C Complete Implementation
cmake_minimum_required(VERSION 3.20)

project(Kryon 
    VERSION 1.0.0
    DESCRIPTION "Kryon UI System - Complete C Implementation"
    LANGUAGES C CXX
)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Set CXX standard (for some third-party dependencies)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(KRYON_BUILD_TESTS "Build test suite" ON)
option(KRYON_BUILD_EXAMPLES "Build example applications" ON)
option(KRYON_BUILD_TOOLS "Build development tools" ON)
option(KRYON_BUILD_DOCS "Generate documentation" OFF)

# Platform detection
if(WIN32)
    set(KRYON_PLATFORM "windows")
elseif(APPLE)
    if(IOS)
        set(KRYON_PLATFORM "ios")
    else()
        set(KRYON_PLATFORM "macos")
    endif()
elseif(ANDROID)
    set(KRYON_PLATFORM "android")
elseif(EMSCRIPTEN)
    set(KRYON_PLATFORM "web")
elseif(UNIX)
    set(KRYON_PLATFORM "linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Building for platform: ${KRYON_PLATFORM}")

# Renderer backend selection
option(KRYON_RENDERER_SDL2 "Enable SDL2 renderer" ON)
option(KRYON_RENDERER_WEBGL "Enable WebGL renderer" ${EMSCRIPTEN})
option(KRYON_RENDERER_WGPU "Enable WGPU renderer" ${UNIX})
option(KRYON_RENDERER_RAYLIB "Enable Raylib renderer" ON)
option(KRYON_RENDERER_TERMINAL "Enable terminal renderer" ON)
option(KRYON_RENDERER_HTML "Enable HTML renderer" ON)
option(KRYON_RENDERER_SOFTWARE "Enable software renderer" ON)

# Script engine selection
option(KRYON_SCRIPT_LUA "Enable Lua scripting" ON)
option(KRYON_SCRIPT_JAVASCRIPT "Enable JavaScript scripting" ON)
option(KRYON_SCRIPT_PYTHON "Enable Python scripting" OFF)
option(KRYON_SCRIPT_KRYON_LISP "Enable Kryon Lisp scripting" ON)

# Feature toggles
option(KRYON_ENABLE_AUDIO "Enable audio support" ON)
option(KRYON_ENABLE_NETWORK "Enable network support" ON)
option(KRYON_ENABLE_PROFILER "Enable performance profiler" ON)
option(KRYON_ENABLE_DEBUGGER "Enable interactive debugger" ON)

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(KRYON_DEBUG=1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -Wextra -Wpedantic")
else()
    add_compile_definitions(KRYON_RELEASE=1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
endif()

# Platform-specific flags
if(EMSCRIPTEN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_FUNCTIONS='[\"_main\",\"_kryon_compile\",\"_kryon_run\"]' -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\"]'")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kryon
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal
)

# Add CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

# Find dependencies
find_package(PkgConfig QUIET)

# Third-party dependencies
add_subdirectory(third-party)

# Core libraries
add_subdirectory(src/compiler)
add_subdirectory(src/runtime)
add_subdirectory(src/platform)
add_subdirectory(src/renderers)
add_subdirectory(src/script-engines)
add_subdirectory(src/layout)
add_subdirectory(src/events)
add_subdirectory(src/filesystem)

# Optional components
if(KRYON_ENABLE_NETWORK)
    add_subdirectory(src/network)
endif()

if(KRYON_ENABLE_AUDIO)
    add_subdirectory(src/audio)
endif()

add_subdirectory(src/graphics)

# CLI and tools
add_subdirectory(src/cli)

if(KRYON_BUILD_TOOLS)
    add_subdirectory(src/tools)
endif()

# Main Kryon library (static)
add_library(kryon_static STATIC
    $<TARGET_OBJECTS:kryon_compiler>
    $<TARGET_OBJECTS:kryon_runtime>
    $<TARGET_OBJECTS:kryon_platform>
    $<TARGET_OBJECTS:kryon_renderers>
    $<TARGET_OBJECTS:kryon_script_engines>
    $<TARGET_OBJECTS:kryon_layout>
    $<TARGET_OBJECTS:kryon_events>
    $<TARGET_OBJECTS:kryon_filesystem>
    $<TARGET_OBJECTS:kryon_graphics>
    $<$<BOOL:${KRYON_ENABLE_NETWORK}>:$<TARGET_OBJECTS:kryon_network>>
    $<$<BOOL:${KRYON_ENABLE_AUDIO}>:$<TARGET_OBJECTS:kryon_audio>>
)

# Main Kryon library (shared)
add_library(kryon_shared SHARED
    $<TARGET_OBJECTS:kryon_compiler>
    $<TARGET_OBJECTS:kryon_runtime>
    $<TARGET_OBJECTS:kryon_platform>
    $<TARGET_OBJECTS:kryon_renderers>
    $<TARGET_OBJECTS:kryon_script_engines>
    $<TARGET_OBJECTS:kryon_layout>
    $<TARGET_OBJECTS:kryon_events>
    $<TARGET_OBJECTS:kryon_filesystem>
    $<TARGET_OBJECTS:kryon_graphics>
    $<$<BOOL:${KRYON_ENABLE_NETWORK}>:$<TARGET_OBJECTS:kryon_network>>
    $<$<BOOL:${KRYON_ENABLE_AUDIO}>:$<TARGET_OBJECTS:kryon_audio>>
)

# Set library properties
set_target_properties(kryon_static PROPERTIES
    OUTPUT_NAME kryon
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

set_target_properties(kryon_shared PROPERTIES
    OUTPUT_NAME kryon
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Main CLI executable
add_executable(kryon_cli src/cli/main.c)
target_link_libraries(kryon_cli kryon_static)
set_target_properties(kryon_cli PROPERTIES
    OUTPUT_NAME kryon
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Tests
if(KRYON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(KRYON_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Documentation
if(KRYON_BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        add_custom_target(docs
            ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()

# Installation
install(TARGETS kryon_cli
    DESTINATION bin
)

install(TARGETS kryon_static kryon_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/kryon/
    DESTINATION include/kryon
    FILES_MATCHING PATTERN "*.h"
)

# Package configuration
include(CPack)
set(CPACK_PACKAGE_NAME "Kryon")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "Kryon Labs")
set(CPACK_PACKAGE_CONTACT "info@kryonlabs.com")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Kryon Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${KRYON_PLATFORM}")  
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Renderers:")
message(STATUS "  SDL2: ${KRYON_RENDERER_SDL2}")
message(STATUS "  WebGL: ${KRYON_RENDERER_WEBGL}")
message(STATUS "  WGPU: ${KRYON_RENDERER_WGPU}")
message(STATUS "  Raylib: ${KRYON_RENDERER_RAYLIB}")
message(STATUS "  Terminal: ${KRYON_RENDERER_TERMINAL}")
message(STATUS "  HTML: ${KRYON_RENDERER_HTML}")
message(STATUS "  Software: ${KRYON_RENDERER_SOFTWARE}")
message(STATUS "")
message(STATUS "Script Engines:")
message(STATUS "  Lua: ${KRYON_SCRIPT_LUA}")
message(STATUS "  JavaScript: ${KRYON_SCRIPT_JAVASCRIPT}")
message(STATUS "  Python: ${KRYON_SCRIPT_PYTHON}")
message(STATUS "  Kryon Lisp: ${KRYON_SCRIPT_KRYON_LISP}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  Audio: ${KRYON_ENABLE_AUDIO}")
message(STATUS "  Network: ${KRYON_ENABLE_NETWORK}")
message(STATUS "  Profiler: ${KRYON_ENABLE_PROFILER}")
message(STATUS "  Debugger: ${KRYON_ENABLE_DEBUGGER}")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Tests: ${KRYON_BUILD_TESTS}")
message(STATUS "  Examples: ${KRYON_BUILD_EXAMPLES}")
message(STATUS "  Tools: ${KRYON_BUILD_TOOLS}")
message(STATUS "  Documentation: ${KRYON_BUILD_DOCS}")
message(STATUS "================================")
message(STATUS "")