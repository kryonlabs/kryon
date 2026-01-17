cmake_minimum_required(VERSION 3.22.1)

project(kryon_ir C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Required dependencies - no fallbacks, hard error if missing
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
pkg_check_modules(FRIBIDI REQUIRED fribidi)
pkg_check_modules(FREETYPE REQUIRED freetype2)

# Android platform detection
if(ANDROID)
    add_definitions(-D__ANDROID__)
endif()

# Collect all IR source files
file(GLOB IR_SOURCES
    ir_builder.c
    ir_animation.c
    ir_hot_reload.c
    ir_serialization.c
    ir_style_vars.c
    ir_executor.c
    ir_plugin.c
    ir_native_canvas.c
    ir_hashmap.c
    ir_memory.c
    ir_layout.c
    ir_json.c
    ir_logic.c
    ir_expression.c
    ir_lua_modules.c
    ir_text_shaping.c  # Requires HarfBuzz - REQUIRED, no fallbacks
    components/ir_component_registry.c
    components/ir_component_flexbox.c
    components/ir_component_text.c
    components/ir_component_button.c
    components/ir_component_checkbox.c
    components/ir_component_dropdown.c
    components/ir_component_table.c
    parsers/kry/kry_parser.c
    parsers/kry/kry_to_ir.c
    third_party/cJSON/cJSON.c
)

# Create static library
add_library(kryon_ir STATIC ${IR_SOURCES})

# Include directories
target_include_directories(kryon_ir PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON
    ${HARFBUZZ_INCLUDE_DIRS}
    ${FRIBIDI_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
)

# Link required libraries
target_link_libraries(kryon_ir PUBLIC
    ${HARFBUZZ_LIBRARIES}
    ${FRIBIDI_LIBRARIES}
    ${FREETYPE_LIBRARIES}
)

# Compiler flags
target_compile_options(kryon_ir PRIVATE
    -Wall
    -Wextra
)
