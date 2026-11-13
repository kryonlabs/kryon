cmake_minimum_required(VERSION 3.22)
project(KryonRendererArtifact C CXX)
include("${CMAKE_CURRENT_LIST_DIR}/RaylibAndroid.cmake")
kryon_android_raylib("${RAYLIB_DIR}" "${WEBP_DIR}" "${AVIF_DIR}" "${AOM_DIR}"
    "${KRYON_ANDROID_AUDIO_DIR}" "${KRYON_ANDROID_OPUS_DIR}" "${KRYON_ANDROID_SAMPLERATE_DIR}")
