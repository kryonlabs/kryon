# Kryon's WebP codec: libwebp's decoder plus the animation demuxer, built from
# the locked `libwebp` source package into one static `kryonwebp` archive with
# no encoder, command-line tools or threads.
#
#   include("${KRYON_DIR}/cmake/WebP.cmake")
#   kryon_webp_decoder("${WEBP_DIR}")   # static target `kryonwebp`

include_guard(GLOBAL)

function(kryon_webp_decoder webp_dir)
    if(NOT EXISTS "${webp_dir}/CMakeLists.txt" OR
       NOT EXISTS "${webp_dir}/src/demux/anim_decode.c")
        message(FATAL_ERROR "No libwebp source at ${webp_dir}")
    endif()
    foreach(option ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO
            LIBWEBPMUX WEBPMUX EXTRAS FUZZTEST)
        set(WEBP_BUILD_${option} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(WEBP_LINK_STATIC ON CACHE BOOL "" FORCE)
    set(WEBP_USE_THREAD OFF CACHE BOOL "" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_subdirectory("${webp_dir}" "${CMAKE_CURRENT_BINARY_DIR}/libwebp" EXCLUDE_FROM_ALL)
    set(sources
        $<TARGET_OBJECTS:webpdecode> $<TARGET_OBJECTS:webpdspdecode>
        $<TARGET_OBJECTS:webputilsdecode>
        "${webp_dir}/src/demux/demux.c" "${webp_dir}/src/demux/anim_decode.c")
    # libwebp's own decoder links Android's cpufeatures for NEON detection;
    # keep that object inside the one archive consumers link.
    if(TARGET cpufeatures-webp)
        list(APPEND sources "${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c")
    endif()
    add_library(kryonwebp STATIC ${sources})
    target_include_directories(kryonwebp PRIVATE "${webp_dir}")
    if(TARGET cpufeatures-webp)
        target_include_directories(kryonwebp PRIVATE
            "${ANDROID_NDK}/sources/android/cpufeatures")
        target_link_libraries(kryonwebp PRIVATE dl)
    endif()
    foreach(target kryonwebp webpdecode webpdspdecode webputilsdecode)
        target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endforeach()
endfunction()
