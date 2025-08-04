# Minimal Kryon-C Build for Widget System Only
cmake_minimum_required(VERSION 3.10)

project(Kryon-Minimal 
    VERSION 1.0.0
    DESCRIPTION "Kryon UI System - Minimal Widget System"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/internal)

# =============================================================================
# THIRD-PARTY DEPENDENCIES
# =============================================================================

add_subdirectory(third-party)

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG)
else()
    add_compile_options(-O2 -DNDEBUG)
endif()

# Add warnings but make them non-fatal
add_compile_options(-Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable)

# Platform-specific compiler flags
if(UNIX)
    add_compile_options(-fPIC)
    add_compile_definitions(_GNU_SOURCE)
endif()

# =============================================================================
# SHARED MODULES
# =============================================================================

add_library(kryon_shared STATIC
    src/shared/kryon_mappings.c
    src/shared/krb_schema.c
)

target_include_directories(kryon_shared PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
    ${CMAKE_SOURCE_DIR}/src
)

# =============================================================================
# MINIMAL MEMORY MANAGEMENT
# =============================================================================

add_library(kryon_minimal_memory STATIC
    src/core/memory_manager.c
    src/core/binary_io.c
    src/core/arena.c
)

target_include_directories(kryon_minimal_memory PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

# =============================================================================
# RUNTIME MODULE
# =============================================================================

add_subdirectory(src/runtime)

# =============================================================================
# EVENTS MODULE
# =============================================================================

add_subdirectory(src/events)

# =============================================================================
# GRAPHICS MODULE (for color utils)
# =============================================================================

add_subdirectory(src/graphics)

# =============================================================================
# COMPILER MODULE (for lexer and parser)
# =============================================================================

add_subdirectory(src/compiler)

# =============================================================================
# SCRIPT ENGINES MODULE (for Lua support)
# =============================================================================

add_subdirectory(src/script-engines)

add_library(kryon_minimal_compiler STATIC
    $<TARGET_OBJECTS:kryon_compiler>
    $<TARGET_OBJECTS:kryon_graphics>
    src/core/error_system.c
)

target_include_directories(kryon_minimal_compiler PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_link_libraries(kryon_minimal_compiler kryon_minimal_memory kryon_shared cjson)

# =============================================================================
# WIDGET SYSTEM (Core only)
# =============================================================================

add_library(kryon_minimal_widgets STATIC
    src/layout/widget_system.c
    src/layout/widget_transform.c
    src/layout/widget_layout.c
)

target_include_directories(kryon_minimal_widgets PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_link_libraries(kryon_minimal_widgets kryon_minimal_memory)

# =============================================================================
# KRYON CLI (Modular Version)
# =============================================================================

add_subdirectory(src/cli)

# Raylib detection will be handled by the CLI module itself

# =============================================================================
# SUMMARY
# =============================================================================

message(STATUS "")
message(STATUS "=== Kryon Minimal Build ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Target: kryon (modular CLI)")
message(STATUS "===========================")
message(STATUS "")