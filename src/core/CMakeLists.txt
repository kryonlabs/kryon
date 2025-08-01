# Core utilities - fundamental building blocks
set(CORE_SOURCES
    binary_io.c
    memory_manager.c
    arena.c
    error_system.c
    containers.c
)

add_library(kryon_core STATIC ${CORE_SOURCES})

target_include_directories(kryon_core 
    PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE 
        ${CMAKE_SOURCE_DIR}/include/internal
)

# Core has no dependencies on other Kryon modules
target_link_libraries(kryon_core)

set_target_properties(kryon_core PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)