# Kryon CLI Module

add_executable(kryon_cli
    main.c
    compile/compile_command.c
    run/run_command.c
    dev/dev_command.c
    debug/debug_command.c
    package/package_command.c
)

target_include_directories(kryon_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_link_libraries(kryon_cli PRIVATE 
    kryon_minimal_widgets
    kryon_minimal_compiler
    kryon_minimal_memory
    kryon_runtime
    kryon_events
    kryon_script_engines
    cjson
)

set_target_properties(kryon_cli PROPERTIES
    OUTPUT_NAME kryon
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Platform-specific libraries
if(UNIX)
    target_link_libraries(kryon_cli PRIVATE m)
endif()

# Try to find raylib for real window rendering
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(RAYLIB QUIET raylib)
endif()

# Fallback: Try to find raylib manually (useful in Nix environments)
if(NOT RAYLIB_FOUND)
    find_path(RAYLIB_INCLUDE_DIR raylib.h
        PATHS
        $ENV{NIX_CFLAGS_COMPILE}
        /usr/include
        /usr/local/include
        /opt/local/include
        ${CMAKE_PREFIX_PATH}/include
    )
    
    find_library(RAYLIB_LIBRARY raylib
        PATHS
        $ENV{NIX_LDFLAGS}
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        ${CMAKE_PREFIX_PATH}/lib
    )
    
    if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
        set(RAYLIB_FOUND TRUE)
        set(RAYLIB_LIBRARIES ${RAYLIB_LIBRARY})
        set(RAYLIB_INCLUDE_DIRS ${RAYLIB_INCLUDE_DIR})
    endif()
endif()

# Link raylib if available - skip header check in Nix environments
if(RAYLIB_FOUND)
    message(STATUS "Raylib found via pkg-config - enabling raylib renderer")
    
    # Add raylib renderer source
    target_sources(kryon_cli PRIVATE
        ${CMAKE_SOURCE_DIR}/src/renderers/raylib/raylib_renderer.c
    )
    
    target_include_directories(kryon_cli PRIVATE ${RAYLIB_INCLUDE_DIRS})
    target_compile_definitions(kryon_cli PRIVATE HAVE_RAYLIB=1)
    set(RAYLIB_HEADER_FOUND TRUE)
endif()

if(RAYLIB_FOUND AND RAYLIB_HEADER_FOUND)
    
    # Link using the full library path if available from pkg-config
    if(pkgcfg_lib_RAYLIB_raylib)
        target_link_libraries(kryon_cli PRIVATE ${pkgcfg_lib_RAYLIB_raylib})
    else()
        target_link_libraries(kryon_cli PRIVATE ${RAYLIB_LIBRARIES})
    endif()
    
    # Add additional system libraries that raylib needs
    if(UNIX)
        find_library(GL_LIB GL)
        find_library(X11_LIB X11)
        find_library(XRANDR_LIB Xrandr)
        find_library(XINERAMA_LIB Xinerama)
        find_library(XI_LIB Xi)
        find_library(XCURSOR_LIB Xcursor)
        
        if(GL_LIB)
            target_link_libraries(kryon_cli PRIVATE ${GL_LIB})
        endif()
        if(X11_LIB)
            target_link_libraries(kryon_cli PRIVATE ${X11_LIB})
        endif()
        if(XRANDR_LIB)
            target_link_libraries(kryon_cli PRIVATE ${XRANDR_LIB})
        endif()
        if(XINERAMA_LIB)
            target_link_libraries(kryon_cli PRIVATE ${XINERAMA_LIB})
        endif()
        if(XI_LIB)
            target_link_libraries(kryon_cli PRIVATE ${XI_LIB})
        endif()
        if(XCURSOR_LIB)
            target_link_libraries(kryon_cli PRIVATE ${XCURSOR_LIB})
        endif()
        
        target_link_libraries(kryon_cli PRIVATE pthread dl)
    endif()
    
    message(STATUS "Raylib found for kryon CLI - real window rendering enabled")
else()
    message(STATUS "Raylib not found for kryon CLI - using stub renderers")
    # Don't define HAVE_RAYLIB at all when raylib is not available
    # This way #ifdef HAVE_RAYLIB will correctly exclude raylib code
endif()