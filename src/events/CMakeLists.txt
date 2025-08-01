# Kryon Events Module

add_library(kryon_events OBJECT
    events.c
    # input/input_handler.c  # Disabled due to type conflicts
    # gestures/gesture_recognizer.c  # Disabled for now
    # dispatch/event_dispatcher.c  # Disabled for now
    # filters/event_filters.c  # Disabled for now
)

target_include_directories(kryon_events PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_compile_definitions(kryon_events PRIVATE KRYON_EVENTS_MODULE=1)