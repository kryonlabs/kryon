# The Android build of the `kryon:renderer-raylib` package artifact.
#
# Kryon's own Android host presents through the platform and needs no
# renderer (cmake/KryonAndroid.cmake). Applications that import the raylib
# renderer instead link the archives this project produces, which
# scripts/renderer_artifact.py builds and describes in renderer.json:
#
#   kryon_android_raylib("${RAYLIB_DIR}" "${WEBP_DIR}" "${AVIF_DIR}" "${AOM_DIR}"
#       "${AUDIO_DIR}" "${OPUS_DIR}" "${SAMPLERATE_DIR}")
#
# It defines `raylib`, `kryonwebp`, `avif_static` and, with the audio
# package, `opus`.

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/../KryonAndroid.cmake")

# Compile raylib's Android backend with the module and file-format set Kryon
# uses: GLES2 rendering, raudio with OGG, PNG/JPG images, and TTF fonts, plus
# Kryon's WebP and AVIF codecs.
function(kryon_android_raylib raylib_dir webp_dir avif_dir aom_dir)
    if(NOT EXISTS "${raylib_dir}/raylib.h")
        message(FATAL_ERROR "No raylib source at ${raylib_dir}")
    endif()
    add_library(raylib STATIC
        "${raylib_dir}/rcore.c"
        "${raylib_dir}/rshapes.c"
        "${raylib_dir}/rtextures.c"
        "${raylib_dir}/rtext.c"
        "${raylib_dir}/raudio.c")
    # rlgl's GLES2 batch holds 2048 quads, a few dozen anti-aliased rounded
    # shapes, so a phone frame flushed it many times, each upload into the
    # one vertex buffer the tiling GPU had not drawn yet. A larger batch in
    # two alternating buffers flushes far less often and waits on neither.
    target_compile_definitions(raylib PRIVATE
        PLATFORM_ANDROID
        GRAPHICS_API_OPENGL_ES2
        RL_DEFAULT_BATCH_BUFFER_ELEMENTS=8192
        RL_DEFAULT_BATCH_BUFFERS=2
        SUPPORT_SCREEN_CAPTURE=0
        SUPPORT_CUSTOM_FRAME_CONTROL=1
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
        SUPPORT_MODULE_RAUDIO=1
        # src/backend/android_fopen.zi owns the app's __wrap_fopen; raylib's
        # own APK reader keeps a distinct name so the two never collide.
        __wrap_fopen=KryonRaylibFopen)
    # rlgl's per-vertex batching runs for every mesh point each frame; size
    # optimization made it the hottest code on low-end phones.
    target_compile_options(raylib PRIVATE -O2 -ffunction-sections -fdata-sections
        -fno-omit-frame-pointer $<$<NOT:$<CONFIG:Debug>>:-g1>)
    target_include_directories(raylib PUBLIC
        "${raylib_dir}" "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}")
    kryon_android_codecs("${webp_dir}" "${avif_dir}" "${aom_dir}")
    target_link_libraries(raylib PUBLIC kryon_android_codecs)
    set(audio_dir "${ARGV4}")
    if(NOT audio_dir STREQUAL "")
        kryon_android_audio("${audio_dir}" "${ARGV5}" "${ARGV6}")
    endif()
endfunction()

# Ogg Opus assets in media_audio_raylib decode with audio/opus, which needs
# libopus, built from the audio package's pinned sources (`ziran pkg path
# audio`, `libopus`, `libsamplerate`) and linked through raylib.
function(kryon_android_audio audio_dir opus_dir samplerate_dir)
    if(NOT EXISTS "${audio_dir}/native/AudioCodecs.cmake")
        message(FATAL_ERROR "No audio package at ${audio_dir}")
    endif()
    include("${audio_dir}/native/AudioCodecs.cmake")
    ziran_audio_codecs("${opus_dir}" "${samplerate_dir}")
    # As with raylib, debug builds still need real-time decoding on slow phones.
    target_compile_options(opus PRIVATE -O2)
    target_link_libraries(raylib PUBLIC opus)
endfunction()
