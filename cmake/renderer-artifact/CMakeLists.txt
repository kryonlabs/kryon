cmake_minimum_required(VERSION 3.22)
project(KryonRendererArtifact C CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../KryonAndroid.cmake)
kryon_android_raylib("${RAYLIB_DIR}" "${WEBP_DIR}" "${AVIF_DIR}" "${AOM_DIR}")
