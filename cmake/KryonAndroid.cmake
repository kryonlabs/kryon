# Android NDK build helpers for Kryon apps.
#
# An app's droid/app/src/main/cpp/CMakeLists.txt generates its Ziran C sources
# and then uses these helpers for the parts every Kryon Android app shares:
#
#   include("${KRYON_DIR}/cmake/KryonAndroid.cmake")
#   kryon_android_reproducible_paths("${REPO_ROOT}") # before dependency targets
#   kryon_android_codecs("${WEBP_DIR}" "${AVIF_DIR}" "${AOM_DIR}")
#   add_library(main SHARED ${APP_SOURCES})
#   kryon_android_app(main)                     # glue, size flags, system libs
#
# The helpers only set what is common; apps add their own definitions,
# include directories, and libraries to `main` as usual.

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/WebP.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/AVIF.cmake")

set(KRYON_ANDROID_NATIVE_APP_GLUE_DIR
    "${ANDROID_NDK}/sources/android/native_app_glue")

# Apply to every target in this directory and its children, including native
# dependencies. Debug paths enter the linker's build ID before Gradle strips
# the APK library; normalizing only the final app target is insufficient.
# Keep debug information and build IDs so retained crash symbols still match.
function(kryon_android_reproducible_paths source_root)
    get_filename_component(source_root "${source_root}" ABSOLUTE)
    add_compile_options(
        "-fdebug-compilation-dir=."
        "-ffile-prefix-map=${source_root}=."
        "-ffile-prefix-map=${ANDROID_NDK}=./ndk"
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=./build/android")
endfunction()

# Build the image codecs Kryon's platform-native Android host uses. Software
# presentation needs no graphics, audio, or renderer dependency.
function(kryon_android_codecs webp_dir)
    if(NOT EXISTS "${webp_dir}/CMakeLists.txt")
        message(FATAL_ERROR "No libwebp source at ${webp_dir}")
    endif()
    kryon_webp_decoder("${webp_dir}")
    set(avif_dir "${ARGV1}")
    set(aom_dir "${ARGV2}")
    if(avif_dir STREQUAL "")
        set(avif_dir "$ENV{KRYON_ANDROID_AVIF_DIR}")
    endif()
    if(aom_dir STREQUAL "")
        set(aom_dir "$ENV{KRYON_ANDROID_AOM_DIR}")
    endif()
    kryon_avif_decoder("${avif_dir}" "${aom_dir}")
    add_library(kryon_android_codecs INTERFACE)
    target_link_libraries(kryon_android_codecs INTERFACE
        kryonwebp ziran_avif::static)
endfunction()

# Turn a shared library into a Kryon NativeActivity payload: add the NDK's
# native_app_glue, optimize for speed, keep symbols hidden, garbage-collect unused
# sections, align for 16 KB page devices, route fopen() through Kryon's APK
# asset reader, retain native crash symbols, and link Android's system APIs.
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
    # The NDK's glue is compiled from a copy in the build tree. Its own path
    # carries the NDK's install location into the object and module names,
    # which ThinLTO hashes into symbols, so builds on other machines differ.
    set(glue_source "${CMAKE_CURRENT_BINARY_DIR}/kryon-native-app-glue/android_native_app_glue.c")
    configure_file("${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
        "${glue_source}" COPYONLY)
    target_sources(${target} PRIVATE
        "${thread_generated}/android_thread.c"
        "${glue_source}")
    # Only the NativeActivity-owned app thread receives this stack policy.
    # The generated configurator calls libc normally; other thread creators retain
    # their own attributes and defaults.
    set_property(SOURCE "${glue_source}"
        APPEND PROPERTY COMPILE_DEFINITIONS pthread_attr_setdetachstate=ConfigureNativeThread)
    target_include_directories(${target} PRIVATE
        "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}")
    target_compile_definitions(${target} PRIVATE
        PLATFORM_ANDROID ANDROID_BUILD=1 _DEFAULT_SOURCE _GNU_SOURCE
        _FILE_OFFSET_BITS=64)
    target_compile_options(${target} PRIVATE
        -O2 -ffunction-sections -fdata-sections -fvisibility=hidden
        -fno-omit-frame-pointer $<$<NOT:$<CONFIG:Debug>>:-g1>)
    target_link_options(${target} PRIVATE
        -Wl,--gc-sections
        -Wl,-z,max-page-size=16384
        -Wl,-z,common-page-size=16384
        # Kryon serves fopen() from APK assets through __wrap_fopen.
        -Wl,--wrap=fopen
        -Wl,--wrap=fopen64)
    # Gradle strips the packaged library. Keep this original, with line
    # information, so native startup failures can be symbolicated.
    target_link_libraries(${target} PUBLIC android log atomic dl m)
endfunction()

# Adopt Kryon's canonical Android host for a shared-library target whose
# generated sources include the application's main and android_glue modules
# (created by scripts/android_scaffold.py). KRYON_ANDROID_HOST selects
# Kryon's own platform keyboard implementation; an application that supplies
# its own JNI glue, like one with a custom SetPlatformTextKeyboardVisible,
# must not call this. The library exposes Android's entry points and the
# notification JNI callbacks that Android resolves on receiver/service startup.
function(kryon_android_host target)
    target_compile_definitions(${target} PRIVATE KRYON_ANDROID_HOST=1)
    foreach(source IN LISTS ARGN)
        get_filename_component(source_name "${source}" NAME)
        if(source_name STREQUAL "main.c")
            # Kryon's NativeActivity glue calls android_main directly. The
            # scaffold's argv-style main remains the stable application ABI.
            set_source_files_properties("${source}"
                PROPERTIES COMPILE_OPTIONS "-ffreestanding")
        elseif(source_name STREQUAL "android_glue.c" OR
               source_name MATCHES "(^|_)(android_notifications|notification_push)\\.c$")
            # The VM locates this exported entry before native methods are
            # registered through it. Notification receivers and push services
            # also locate their JNI callbacks after a cold process start.
            # Package-generated sources carry an identity prefix.
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
