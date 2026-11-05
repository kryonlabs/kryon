# Platform selection only; decoder sources, flags and targets belong to avif.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ImagePackages.cmake")

function(kryon_avif_decoder avif_dir aom_dir)
    kryon_image_package(package avif)
    include("${package}/native/ZiranAvif.cmake")
    set(shared)
    if(NOT ANDROID AND NOT WIN32 AND NOT APPLE)
        # Cairo lazily opens the same self-contained library as before.
        set(shared BUILD_SHARED_BRIDGE SHARED_TARGET_NAME kryonavif)
    endif()
    ziran_avif_add_native(AVIF_SOURCE "${avif_dir}" AOM_SOURCE "${aom_dir}" ${shared})
endfunction()
