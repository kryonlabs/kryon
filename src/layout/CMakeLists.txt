# Kryon Layout Engine Module

add_library(kryon_layout OBJECT
    flexbox/flexbox_layout.c
    grid/grid_layout.c
    constraints/constraint_solver.c
    cache/layout_cache.c
    text/text_layout.c
)

target_include_directories(kryon_layout PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_compile_definitions(kryon_layout PRIVATE KRYON_LAYOUT_MODULE=1)