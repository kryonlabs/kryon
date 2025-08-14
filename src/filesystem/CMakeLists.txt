# Kryon Filesystem Module

add_library(kryon_filesystem OBJECT
    common/filesystem_interface.c
    watcher/file_watcher.c
    archive/archive_support.c
    vfs/virtual_filesystem.c
)

target_include_directories(kryon_filesystem PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(kryon_filesystem PRIVATE KRYON_FILESYSTEM_MODULE=1)