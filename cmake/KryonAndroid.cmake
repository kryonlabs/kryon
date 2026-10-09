# Android NDK build helpers for Kryon apps.
#
# An app's droid/app/src/main/cpp/CMakeLists.txt generates its Ziran C sources
# and then uses these helpers for the parts every Kryon Android app shares:
#
#   include("${KRYON_DIR}/cmake/KryonAndroid.cmake")
#   kryon_android_raylib("${RAYLIB_DIR}")        # static target `raylib`
#   add_library(main SHARED ${APP_SOURCES})
#   kryon_android_app(main)                     # glue, size flags, system libs
#
# The helpers only set what is common; apps add their own definitions,
# include directories, and libraries to `main` as usual.

include_guard(GLOBAL)

set(KRYON_ANDROID_NATIVE_APP_GLUE_DIR
    "${ANDROID_NDK}/sources/android/native_app_glue")

# Compile raylib's Android backend with the module and file-format set Kryon
# uses: GLES2 rendering, raudio with OGG, PNG/JPG images, and TTF fonts.
function(kryon_android_raylib raylib_dir)
    if(NOT EXISTS "${raylib_dir}/raylib.h")
        message(FATAL_ERROR "No raylib source at ${raylib_dir}")
    endif()
    add_library(raylib STATIC
        "${raylib_dir}/rcore.c"
        "${raylib_dir}/rshapes.c"
        "${raylib_dir}/rtextures.c"
        "${raylib_dir}/rtext.c"
        "${raylib_dir}/raudio.c")
    target_compile_definitions(raylib PRIVATE
        PLATFORM_ANDROID
        GRAPHICS_API_OPENGL_ES2
        SUPPORT_SCREEN_CAPTURE=0
        SUPPORT_COMPRESSION_API=0
        SUPPORT_AUTOMATION_EVENTS=0
        SUPPORT_CLIPBOARD_IMAGE=0
        SUPPORT_FILEFORMAT_BMP=0
        SUPPORT_FILEFORMAT_GIF=0
        SUPPORT_FILEFORMAT_QOI=0
        SUPPORT_FILEFORMAT_DDS=0
        SUPPORT_FILEFORMAT_TTF=1
        SUPPORT_FILEFORMAT_JPG=1
        SUPPORT_FILEFORMAT_PNG=1
        SUPPORT_FILEFORMAT_OGG=1
        SUPPORT_MODULE_RAUDIO=1)
    target_compile_options(raylib PRIVATE -Oz -ffunction-sections -fdata-sections
        -fno-omit-frame-pointer $<$<NOT:$<CONFIG:Debug>>:-g1>)
    target_include_directories(raylib PUBLIC
        "${raylib_dir}" "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}")
endfunction()

# Turn a shared library into a Kryon NativeActivity payload: add the NDK's
# native_app_glue, size-optimize, keep symbols hidden, garbage-collect unused
# sections, align for 16 KB page devices, route fopen() through raylib's APK
# asset reader, retain native crash symbols, and link raylib
# with the Android system libraries it needs.
function(kryon_android_app target)
    if(NOT ZI2C_BIN)
        find_program(ZI2C_BIN NAMES zi2c REQUIRED)
    endif()
    set(thread_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/backend/android_thread.zi")
    set(thread_generated "${CMAKE_CURRENT_BINARY_DIR}/kryon-android-thread")
    add_custom_command(
        OUTPUT "${thread_generated}/android_thread.c" "${thread_generated}/android_thread.h"
        COMMAND "${ZI2C_BIN}" --no-main
            --root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/backend"
            -o "${thread_generated}" "${thread_source}"
        DEPENDS "${thread_source}" "${ZI2C_BIN}"
        VERBATIM)
    target_sources(${target} PRIVATE
        "${thread_generated}/android_thread.c"
        "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}/android_native_app_glue.c")
    # Only the NativeActivity-owned app thread receives this stack policy.
    # The generated configurator calls libc normally; other thread creators retain
    # their own attributes and defaults.
    set_property(SOURCE "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
        APPEND PROPERTY COMPILE_DEFINITIONS pthread_attr_setdetachstate=ConfigureNativeThread)
    target_include_directories(${target} PRIVATE
        "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}")
    target_compile_definitions(${target} PRIVATE
        PLATFORM_ANDROID ANDROID_BUILD=1 _DEFAULT_SOURCE _GNU_SOURCE
        _FILE_OFFSET_BITS=64)
    target_compile_options(${target} PRIVATE
        -Oz -ffunction-sections -fdata-sections -fvisibility=hidden
        -fno-omit-frame-pointer $<$<NOT:$<CONFIG:Debug>>:-g1>)
    target_link_options(${target} PRIVATE
        -Wl,--gc-sections
        -Wl,-z,max-page-size=16384
        -Wl,-z,common-page-size=16384
        # raylib's Android backend serves fopen() from APK assets through
        # __wrap_fopen; the linker must redirect every call site to it.
        -Wl,--wrap=fopen)
    # Gradle strips the packaged library. Keep this original, with line
    # information, so native startup failures can be symbolicated.
    target_link_libraries(${target}
        raylib android log OpenSLES EGL GLESv2 atomic dl m)
endfunction()

# Adopt Kryon's canonical Android host for a shared-library target whose
# generated sources include the application's main and android_glue modules
# (created by scripts/android_scaffold.py). KRYON_ANDROID_HOST selects
# Kryon's own platform keyboard implementation; an application that supplies
# its own JNI glue, like one with a custom SetPlatformTextKeyboardVisible,
# must not call this. The library exposes Android's entry points and the
# raylib CORE data symbol required by AndroidSurface synchronization.
function(kryon_android_host target)
    target_compile_definitions(${target} PRIVATE KRYON_ANDROID_HOST=1)
    foreach(source IN LISTS ARGN)
        get_filename_component(source_name "${source}" NAME)
        if(source_name STREQUAL "main.c")
            # raylib's android_main calls this shared-library entry itself;
            # its Zi argv is byte pointers, so hosted C's spelling rule for
            # main does not apply.
            set_source_files_properties("${source}"
                PROPERTIES COMPILE_OPTIONS "-ffreestanding")
        elseif(source_name STREQUAL "android_glue.c")
            # The VM locates this exported entry before native methods are
            # registered through it.
            set_source_files_properties("${source}"
                PROPERTIES COMPILE_OPTIONS "-fvisibility=default")
        endif()
    endforeach()
    target_link_options(${target} PRIVATE
        "-Wl,--version-script,${CMAKE_CURRENT_FUNCTION_LIST_DIR}/libmain.map.txt"
        "-Wl,--no-undefined-version")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/libmain.map.txt")
endfunction()
