# Keep the renderer's archive name while consuming the package-owned recipe.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ImagePackages.cmake")

function(kryon_webp_decoder webp_dir)
    kryon_image_package(package webp)
    include("${package}/native/WebP.cmake")
    ziran_webp_decoder(kryonwebp "${webp_dir}")
endfunction()
