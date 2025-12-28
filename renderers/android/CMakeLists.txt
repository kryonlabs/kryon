cmake_minimum_required(VERSION 3.22.1)

project(kryon_android_renderer C)

# Android NDK Configuration
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror=implicit-function-declaration")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

# Platform defines
add_definitions(-DKRYON_PLATFORM_ANDROID)
add_definitions(-D__ANDROID__)

# Source files
set(ANDROID_RENDERER_SOURCES
    android_renderer.c
    android_egl.c
    android_shaders.c
    android_font.c
    android_texture.c
    android_primitives.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/android
)

# Create static library
add_library(kryon_android_renderer STATIC ${ANDROID_RENDERER_SOURCES})

# Link Android NDK libraries
target_link_libraries(kryon_android_renderer
    android
    log
    EGL
    GLESv3
)

# Set output directory
set_target_properties(kryon_android_renderer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${ANDROID_ABI}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${ANDROID_ABI}"
)

# Export include directory
target_include_directories(kryon_android_renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation
install(TARGETS kryon_android_renderer
    ARCHIVE DESTINATION lib/${ANDROID_ABI}
    LIBRARY DESTINATION lib/${ANDROID_ABI}
)

install(FILES
    android_renderer.h
    android_shaders.h
    DESTINATION include/kryon/renderers/android
)
