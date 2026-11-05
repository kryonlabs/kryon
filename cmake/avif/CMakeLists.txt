cmake_minimum_required(VERSION 3.22)
project(KryonAVIF C CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../AVIF.cmake)
kryon_avif_decoder("${AVIF_DIR}" "${AOM_DIR}")
