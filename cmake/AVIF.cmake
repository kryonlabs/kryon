# Platform selection only; decoder sources, flags and targets belong to avif.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ImagePackages.cmake")

# dav1d is the default AV1 decoder and libaom the explicit oracle. DAV1D_DIR
# or KRYON_DAV1D_DIR select the pinned dav1d source; otherwise it comes from
# the same locked package graph as avif.
function(kryon_avif_decoder avif_dir aom_dir)
    kryon_image_package(package avif)
    include("${package}/native/ZiranAvif.cmake")
    if(DEFINED DAV1D_DIR AND NOT DAV1D_DIR STREQUAL "")
        set(dav1d_dir "${DAV1D_DIR}")
    elseif(NOT "$ENV{KRYON_DAV1D_DIR}" STREQUAL "")
        set(dav1d_dir "$ENV{KRYON_DAV1D_DIR}")
    else()
        kryon_ziran_package_path(dav1d_dir dav1d)
    endif()
    set(shared)
    if(NOT ANDROID AND NOT WIN32 AND NOT APPLE)
        # Cairo lazily opens the same self-contained library as before.
        set(shared BUILD_SHARED_BRIDGE SHARED_TARGET_NAME kryonavif)
    endif()
    ziran_avif_add_native(AVIF_SOURCE "${avif_dir}" AOM_SOURCE "${aom_dir}" DAV1D_SOURCE "${dav1d_dir}" ${shared})
endfunction()
