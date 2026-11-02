# Standalone build of Kryon's WebP codec archive for native desktop hosts and
# renderer artifacts: cmake -S cmake/webp -B BUILD -DWEBP_DIR=LIBWEBP_SOURCE
cmake_minimum_required(VERSION 3.22)
project(KryonWebP C)
include(${CMAKE_CURRENT_LIST_DIR}/../WebP.cmake)
kryon_webp_decoder("${WEBP_DIR}")
