# Kryon Layout Engine Module - Flutter-inspired Widget System
# Clean, performant widget-based layout system

add_library(kryon_layout OBJECT
    # Flutter-inspired widget system
    widget_system.c
    widget_transform.c  
    widget_layout.c
    widget_renderer.c
    
    # Text layout (kept)
    text/text_layout.c
)

target_include_directories(kryon_layout PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_compile_definitions(kryon_layout PRIVATE 
    KRYON_LAYOUT_MODULE=1
    KRYON_WIDGET_SYSTEM_ENABLED=1
)