# Kryon Widget System

if(KRYON_ENABLE_WIDGETS)
    add_library(kryon_widgets OBJECT
        dropdown/dropdown_widget.c
        dropdown/dropdown_renderer.c
        dropdown/dropdown_events.c
        common/widget_base.c
        common/widget_state.c
        common/widget_layout.c
        common/widget_events.c
    )

    target_include_directories(kryon_widgets PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/internal
    )

    target_compile_definitions(kryon_widgets PRIVATE KRYON_WIDGETS_MODULE=1)
endif()