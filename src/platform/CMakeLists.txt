# Kryon Platform Abstraction Module

add_library(kryon_platform OBJECT
    common/platform_interface.c
    common/capabilities.c
    common/window_manager.c
)

target_include_directories(kryon_platform PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_compile_definitions(kryon_platform PRIVATE KRYON_PLATFORM_MODULE=1)