# Platform Abstraction Layer

# Platform-specific source files
set(PLATFORM_SOURCES
    platform.c
)

# Add platform-specific implementations
if(KRYON_PLATFORM STREQUAL "linux")
    list(APPEND PLATFORM_SOURCES linux/platform_linux.c)
    # Find X11 libraries for Linux
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(X11 QUIET x11 xrandr)
    endif()
elseif(KRYON_PLATFORM STREQUAL "windows")
    list(APPEND PLATFORM_SOURCES windows/platform_windows.c)
    # Will add Windows-specific libraries when implemented
elseif(KRYON_PLATFORM STREQUAL "macos")
    list(APPEND PLATFORM_SOURCES macos/platform_macos.c)
    # Will add macOS-specific frameworks when implemented
elseif(KRYON_PLATFORM STREQUAL "ios")
    list(APPEND PLATFORM_SOURCES ios/platform_ios.c)
    # Will add iOS-specific frameworks when implemented
elseif(KRYON_PLATFORM STREQUAL "android")
    list(APPEND PLATFORM_SOURCES android/platform_android.c)
    # Will add Android-specific libraries when implemented
elseif(KRYON_PLATFORM STREQUAL "web")
    list(APPEND PLATFORM_SOURCES web/platform_web.c)
    # Emscripten handles the platform interface
endif()

# Create platform object library
add_library(kryon_platform OBJECT ${PLATFORM_SOURCES})

# Include directories
target_include_directories(kryon_platform PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Platform-specific compile definitions
if(KRYON_PLATFORM STREQUAL "linux")
    target_compile_definitions(kryon_platform PRIVATE KRYON_PLATFORM_LINUX=1)
    if(X11_FOUND)
        target_include_directories(kryon_platform PRIVATE ${X11_INCLUDE_DIRS})
        target_compile_options(kryon_platform PRIVATE ${X11_CFLAGS_OTHER})
    endif()
elseif(KRYON_PLATFORM STREQUAL "windows")
    target_compile_definitions(kryon_platform PRIVATE KRYON_PLATFORM_WINDOWS=1)
elseif(KRYON_PLATFORM STREQUAL "macos")
    target_compile_definitions(kryon_platform PRIVATE KRYON_PLATFORM_MACOS=1)
elseif(KRYON_PLATFORM STREQUAL "ios")
    target_compile_definitions(kryon_platform PRIVATE KRYON_PLATFORM_IOS=1)
elseif(KRYON_PLATFORM STREQUAL "android")
    target_compile_definitions(kryon_platform PRIVATE KRYON_PLATFORM_ANDROID=1)
elseif(KRYON_PLATFORM STREQUAL "web")
    target_compile_definitions(kryon_platform PRIVATE KRYON_PLATFORM_WEB=1)
endif()

# Set target properties
set_target_properties(kryon_platform PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)