# Kryon Events Module

add_library(kryon_events OBJECT
    input/input_handler.c
    gestures/gesture_recognizer.c
    dispatch/event_dispatcher.c
    filters/event_filters.c
)

target_include_directories(kryon_events PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_compile_definitions(kryon_events PRIVATE KRYON_EVENTS_MODULE=1)