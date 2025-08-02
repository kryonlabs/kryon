# Kryon Graphics Module

add_library(kryon_graphics OBJECT
    graphics_core.c
    color_utils.c
    image/image_loader.c
    fonts/font_manager.c
    shaders/shader_compiler.c
    textures/texture_manager.c
)

target_include_directories(kryon_graphics PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_compile_definitions(kryon_graphics PRIVATE KRYON_GRAPHICS_MODULE=1)