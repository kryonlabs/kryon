cmake_minimum_required(VERSION 3.22.1)

project(kryon_android C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files
set(JNI_SOURCES
    kryon_jni.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../platforms/android
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../renderers/android
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../core/include
)

# Create shared library for JNI
add_library(kryon_android SHARED ${JNI_SOURCES})

# Link against platform and renderer libraries
target_link_libraries(kryon_android
    # These will be built separately and linked statically
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../platforms/android/build/${ANDROID_ABI}/libkryon_android_platform.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../renderers/android/build/${ANDROID_ABI}/libkryon_android_renderer.a

    # Android NDK libraries
    android
    log
    EGL
    GLESv3
)

# Set output directory
set_target_properties(kryon_android PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)
