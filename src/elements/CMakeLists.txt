# Kryon Element System

if(KRYON_ENABLE_ELEMENTS)
    add_library(kryon_elements OBJECT
        dropdown/dropdown_element.c
        dropdown/dropdown_renderer.c
        dropdown/dropdown_events.c
        common/element_base.c
        common/element_state.c
        common/element_layout.c
        common/element_events.c
    )

    target_include_directories(kryon_elements PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/internal
    )

    target_compile_definitions(kryon_elements PRIVATE KRYON_ELEMENTS_MODULE=1)
endif()