cmake_minimum_required(VERSION 3.22.1)

project(kryon_android C)

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
set(ANDROID_PLATFORM_SOURCES
    android_platform.c
    android_storage.c
    android_input.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
)

# Create static library
add_library(kryon_android_platform STATIC ${ANDROID_PLATFORM_SOURCES})

# Link Android NDK libraries
target_link_libraries(kryon_android_platform
    android
    log
)

# Set output directory
set_target_properties(kryon_android_platform PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${ANDROID_ABI}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${ANDROID_ABI}"
)

# Export include directory
target_include_directories(kryon_android_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation
install(TARGETS kryon_android_platform
    ARCHIVE DESTINATION lib/${ANDROID_ABI}
    LIBRARY DESTINATION lib/${ANDROID_ABI}
)

install(FILES android_platform.h
    DESTINATION include/kryon/platforms/android
)
