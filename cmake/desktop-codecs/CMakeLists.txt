cmake_minimum_required(VERSION 3.22)
project(KryonDesktopCodecs C CXX)
# Codecs only: platform-native Android presentation links no renderer.
include("${CMAKE_CURRENT_LIST_DIR}/../WebP.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../AVIF.cmake")
kryon_webp_decoder("${WEBP_DIR}")
kryon_avif_decoder("${AVIF_DIR}" "${AOM_DIR}" BUILD_SHARED_BRIDGE)
include("${AUDIO_DIR}/native/AudioCodecs.cmake")
ziran_audio_codecs("${OPUS_DIR}" "${SAMPLERATE_DIR}")
