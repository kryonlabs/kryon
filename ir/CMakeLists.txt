cmake_minimum_required(VERSION 3.22.1)

project(kryon_ir C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Collect all IR source files
file(GLOB IR_SOURCES
    ir_core.c
    ir_builder.c
    ir_animation.c
    ir_hot_reload.c
    ir_serialization.c
    ir_style_vars.c
    ir_executor.c
    ir_plugin.c
    ir_native_canvas.c
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
