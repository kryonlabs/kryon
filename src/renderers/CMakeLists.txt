# Kryon Renderers Module

# =============================================================================
# RENDERERS MODULE CONFIGURATION
# =============================================================================

message(STATUS "Configuring Kryon Renderers module")

# =============================================================================
# COMMON RENDERER COMPONENTS
# =============================================================================

set(COMMON_SOURCES
    # For now, just include the implemented renderers
)

set(COMMON_HEADERS
    # For now, just include the implemented renderers
)

# =============================================================================
# SOFTWARE RENDERER
# =============================================================================

if(KRYON_RENDERER_SOFTWARE)
    set(SOFTWARE_SOURCES
        software/software_renderer.c
    )
    
    set(SOFTWARE_HEADERS
        software/software_renderer.h
    )
endif()

# =============================================================================
# WEBGL RENDERER
# =============================================================================

if(KRYON_RENDERER_WEBGL)
    set(WEBGL_SOURCES
        webgl/webgl_renderer.c
        shaders/shader_compiler.c
    )
    
    set(WEBGL_HEADERS
        webgl/webgl_renderer.h
        shaders/shader_compiler.h
    )
endif()

# =============================================================================
# WGPU RENDERER (Native GPU)
# =============================================================================

if(KRYON_RENDERER_WGPU)
    set(WGPU_SOURCES
        wgpu/wgpu_renderer.c
        wgpu/wgpu_context.c
        wgpu/wgpu_pipeline.c
        wgpu/wgpu_buffers.c
        wgpu/wgpu_textures.c
        wgpu/wgpu_compute.c
    )
    
    set(WGPU_HEADERS
        wgpu/wgpu_renderer.h
        wgpu/wgpu_context.h
        wgpu/wgpu_pipeline.h
        wgpu/wgpu_buffers.h
        wgpu/wgpu_textures.h
        wgpu/wgpu_compute.h
    )
endif()

# =============================================================================
# SDL2 RENDERER
# =============================================================================

option(KRYON_RENDERER_SDL2 "Enable SDL2 renderer" ON)

if(KRYON_RENDERER_SDL2)
    set(SDL2_SOURCES
        sdl2/sdl2_renderer.c
    )
    
    set(SDL2_HEADERS
        # Headers would go here if we had public headers
    )
endif()

# =============================================================================
# RAYLIB RENDERER
# =============================================================================

if(KRYON_RENDERER_RAYLIB)
    set(RAYLIB_SOURCES
        raylib/raylib_renderer.c
    )
    
    set(RAYLIB_HEADERS
        # Headers would go here if we had public headers
    )
endif()

# =============================================================================
# TERMINAL RENDERER (TUI)
# =============================================================================

if(KRYON_RENDERER_TERMINAL)
    set(TERMINAL_SOURCES
        terminal/terminal_renderer.c
        terminal/terminal_screen.c
        terminal/terminal_colors.c
        terminal/terminal_input.c
        terminal/terminal_layout.c
    )
    
    set(TERMINAL_HEADERS
        terminal/terminal_renderer.h
        terminal/terminal_screen.h
        terminal/terminal_colors.h
        terminal/terminal_input.h
        terminal/terminal_layout.h
    )
endif()

# =============================================================================
# HTML RENDERER (DOM output)
# =============================================================================

if(KRYON_RENDERER_HTML)
    set(HTML_SOURCES
        html/html_renderer.c
    )

    set(HTML_HEADERS
        # Headers would go here if we had public headers
    )
endif()

# =============================================================================
# WEB RENDERER (HTML/CSS/JS output)
# =============================================================================

option(KRYON_RENDERER_WEB "Enable Web renderer (HTML/CSS/JS)" ON)

if(KRYON_RENDERER_WEB)
    set(WEB_SOURCES
        web/web_renderer.c
        web/css_generator.c
        web/dom_manager.c
    )

    set(WEB_HEADERS
        web/web_renderer.h
        web/css_generator.h
        web/dom_manager.h
    )
endif()

# =============================================================================
# COMBINED RENDERERS LIBRARY
# =============================================================================

set(RENDERERS_SOURCES ${COMMON_SOURCES})
set(RENDERERS_HEADERS ${COMMON_HEADERS})

# Add enabled renderers
if(KRYON_RENDERER_SOFTWARE)
    list(APPEND RENDERERS_SOURCES ${SOFTWARE_SOURCES})
    list(APPEND RENDERERS_HEADERS ${SOFTWARE_HEADERS})
endif()

if(KRYON_RENDERER_SDL2)
    list(APPEND RENDERERS_SOURCES ${SDL2_SOURCES})
    list(APPEND RENDERERS_HEADERS ${SDL2_HEADERS})
endif()

if(KRYON_RENDERER_WEBGL)
    list(APPEND RENDERERS_SOURCES ${WEBGL_SOURCES})
    list(APPEND RENDERERS_HEADERS ${WEBGL_HEADERS})
endif()

if(KRYON_RENDERER_WGPU)
    list(APPEND RENDERERS_SOURCES ${WGPU_SOURCES})
    list(APPEND RENDERERS_HEADERS ${WGPU_HEADERS})
endif()

if(KRYON_RENDERER_RAYLIB)
    list(APPEND RENDERERS_SOURCES ${RAYLIB_SOURCES})
    list(APPEND RENDERERS_HEADERS ${RAYLIB_HEADERS})
endif()

if(KRYON_RENDERER_TERMINAL)
    list(APPEND RENDERERS_SOURCES ${TERMINAL_SOURCES})
    list(APPEND RENDERERS_HEADERS ${TERMINAL_HEADERS})
endif()

if(KRYON_RENDERER_HTML)
    list(APPEND RENDERERS_SOURCES ${HTML_SOURCES})
    list(APPEND RENDERERS_HEADERS ${HTML_HEADERS})
endif()

if(KRYON_RENDERER_WEB)
    list(APPEND RENDERERS_SOURCES ${WEB_SOURCES})
    list(APPEND RENDERERS_HEADERS ${WEB_HEADERS})
endif()

# Create renderers library as object library for linking
add_library(kryon_renderers OBJECT ${RENDERERS_SOURCES})

# =============================================================================
# RENDERERS CONFIGURATION
# =============================================================================

target_include_directories(kryon_renderers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/software
    ${CMAKE_CURRENT_SOURCE_DIR}/webgl
    ${CMAKE_CURRENT_SOURCE_DIR}/wgpu
    ${CMAKE_CURRENT_SOURCE_DIR}/raylib
    ${CMAKE_CURRENT_SOURCE_DIR}/terminal
    ${CMAKE_CURRENT_SOURCE_DIR}/html
    ${CMAKE_CURRENT_SOURCE_DIR}/web
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Renderer-specific definitions
target_compile_definitions(kryon_renderers PRIVATE
    KRYON_RENDERERS_MODULE=1
    KRYON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    KRYON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    KRYON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Enable specific renderers
if(KRYON_RENDERER_SOFTWARE)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_SOFTWARE=1)
endif()

if(KRYON_RENDERER_SDL2)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_SDL2=1)
endif()

if(KRYON_RENDERER_WEBGL)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_WEBGL=1)
endif()

if(KRYON_RENDERER_WGPU)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_WGPU=1)
endif()

if(KRYON_RENDERER_RAYLIB)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_RAYLIB=1)
endif()

if(KRYON_RENDERER_TERMINAL)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_TERMINAL=1)
endif()

if(KRYON_RENDERER_HTML)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_HTML=1)
endif()

if(KRYON_RENDERER_WEB)
    target_compile_definitions(kryon_renderers PRIVATE KRYON_RENDERER_WEB=1)
endif()

# =============================================================================
# RENDERER DEPENDENCIES
# =============================================================================

# STB libraries for image processing
target_link_libraries(kryon_renderers PRIVATE stb)

# SDL2 dependency
if(KRYON_RENDERER_SDL2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
    
    target_link_libraries(kryon_renderers PRIVATE 
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
    )
    
    target_include_directories(kryon_renderers PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
        ${SDL2_IMAGE_INCLUDE_DIRS}
    )
    
    target_compile_options(kryon_renderers PRIVATE
        ${SDL2_CFLAGS_OTHER}
        ${SDL2_TTF_CFLAGS_OTHER}
        ${SDL2_IMAGE_CFLAGS_OTHER}
    )
endif()

# Raylib dependency
if(KRYON_RENDERER_RAYLIB)
    find_package(raylib QUIET)
    if(raylib_FOUND)
        target_link_libraries(kryon_renderers PRIVATE raylib)
    else()
        message(WARNING "Raylib not found, disabling Raylib renderer")
        set(KRYON_RENDERER_RAYLIB OFF CACHE BOOL "Enable Raylib renderer" FORCE)
    endif()
endif()

# Platform-specific graphics libraries
if(KRYON_RENDERER_WGPU AND NOT EMSCRIPTEN)
    if(WIN32)
        target_link_libraries(kryon_renderers PRIVATE d3d11 dxgi)
    elseif(APPLE)
        target_link_libraries(kryon_renderers PRIVATE "-framework Metal" "-framework QuartzCore")
    elseif(UNIX)
        find_package(Vulkan)
        if(Vulkan_FOUND)
            target_link_libraries(kryon_renderers PRIVATE Vulkan::Vulkan)
        endif()
    endif()
endif()

# OpenGL dependencies for non-web platforms
if((KRYON_RENDERER_WEBGL OR KRYON_RENDERER_RAYLIB) AND NOT EMSCRIPTEN)
    find_package(OpenGL)
    if(OpenGL_FOUND)
        target_link_libraries(kryon_renderers PRIVATE OpenGL::GL)
    endif()
endif()

# Terminal dependencies
if(KRYON_RENDERER_TERMINAL)
    if(UNIX)
        target_link_libraries(kryon_renderers PRIVATE ncurses)
    endif()
endif()

# =============================================================================
# PERFORMANCE OPTIMIZATIONS
# =============================================================================

# Renderer-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(kryon_renderers PRIVATE
        KRYON_OPTIMIZE_RENDERING=1
        KRYON_BATCH_DRAW_CALLS=1
        KRYON_OPTIMIZE_SHADERS=1
    )
endif()

# SIMD optimizations if available
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(kryon_renderers PRIVATE -msse4.1 -mavx)
        target_compile_definitions(kryon_renderers PRIVATE KRYON_USE_SIMD=1)
    endif()
endif()

# =============================================================================
# SHADER COMPILATION
# =============================================================================

# Function to compile shaders (if needed)
function(compile_shaders RENDERER_NAME)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${RENDERER_NAME}/shaders)
        file(GLOB SHADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${RENDERER_NAME}/shaders/*.glsl)
        foreach(SHADER_FILE ${SHADER_FILES})
            get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
            # Add custom commands to compile shaders if needed
        endforeach()
    endif()
endfunction()

# Compile shaders for enabled renderers
if(KRYON_RENDERER_WEBGL)
    compile_shaders("webgl")
endif()

if(KRYON_RENDERER_WGPU)
    compile_shaders("wgpu")
endif()

# =============================================================================
# INSTALL CONFIGURATION
# =============================================================================

# Install headers
install(FILES ${RENDERERS_HEADERS}
    DESTINATION include/kryon/renderers
    COMPONENT Development
)

# =============================================================================
# TESTING
# =============================================================================

if(KRYON_BUILD_TESTS)
    # Renderer tests will be defined in tests/unit/renderers/
    add_subdirectory(tests)
endif()

message(STATUS "Kryon Renderers module configured successfully")