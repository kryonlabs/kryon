cmake_minimum_required(VERSION 3.22.1)

project(kryon_android_backend C)

# Android platform detection
if(ANDROID)
    add_definitions(-D__ANDROID__)
endif()

# Source files
set(BACKEND_SOURCES
    ir_android_renderer.c
    android_rendering.c
    android_layout.c
)

# Create static library
add_library(kryon_android_backend STATIC ${BACKEND_SOURCES})

# Include directories
target_include_directories(kryon_android_backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ir
    ${CMAKE_CURRENT_SOURCE_DIR}/../../renderers/android
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/android
)

# Link with IR library
# Note: renderer and platform libraries will be linked by the parent CMakeLists.txt
target_link_libraries(kryon_android_backend
    kryon_ir
)

# Compiler flags
target_compile_options(kryon_android_backend PRIVATE
    -Wall
    -Wextra
    -std=c11
)
