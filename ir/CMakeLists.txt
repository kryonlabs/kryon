cmake_minimum_required(VERSION 3.22.1)

project(kryon_ir C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
    # ir_text_shaping.c  # Requires HarfBuzz - optional for basic text
    ir_bidi_stub.c
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
)

# Compiler flags
target_compile_options(kryon_ir PRIVATE
    -Wall
    -Wextra
)
