# Kryon Renderers Module

# =============================================================================
# RENDERERS MODULE CONFIGURATION
# =============================================================================

message(STATUS "Configuring Kryon Renderers module")

# =============================================================================
# COMMON RENDERER COMPONENTS
# =============================================================================

set(COMMON_SOURCES
    common/renderer_interface.c
    common/render_context.c
    common/render_stats.c
    common/viewport.c
    common/transform_stack.c
    common/color_utils.c
    common/primitive_renderer.c
)

set(COMMON_HEADERS
    common/renderer_interface.h
    common/render_context.h
    common/render_stats.h
    common/viewport.h
    common/transform_stack.h
    common/color_utils.h
    common/primitive_renderer.h
)

# =============================================================================
# SOFTWARE RENDERER
# =============================================================================

if(KRYON_RENDERER_SOFTWARE)
    set(SOFTWARE_SOURCES
        software/software_renderer.c
        software/pixel_buffer.c
        software/rasterizer.c
        software/text_renderer.c
        software/image_renderer.c
    )
    
    set(SOFTWARE_HEADERS
        software/software_renderer.h
        software/pixel_buffer.h
        software/rasterizer.h
        software/text_renderer.h
        software/image_renderer.h
    )
endif()

# =============================================================================
# WEBGL RENDERER
# =============================================================================

if(KRYON_RENDERER_WEBGL)
    set(WEBGL_SOURCES
        webgl/webgl_renderer.c
        webgl/webgl_context.c
        webgl/webgl_shaders.c
        webgl/webgl_buffers.c
        webgl/webgl_textures.c
        webgl/webgl_uniforms.c
    )
    
    set(WEBGL_HEADERS
        webgl/webgl_renderer.h
        webgl/webgl_context.h
        webgl/webgl_shaders.h
        webgl/webgl_buffers.h
        webgl/webgl_textures.h
        webgl/webgl_uniforms.h
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
# RAYLIB RENDERER
# =============================================================================

if(KRYON_RENDERER_RAYLIB)
    set(RAYLIB_SOURCES
        raylib/raylib_renderer.c
        raylib/raylib_context.c
        raylib/raylib_2d.c
        raylib/raylib_3d.c
        raylib/raylib_textures.c
    )
    
    set(RAYLIB_HEADERS
        raylib/raylib_renderer.h
        raylib/raylib_context.h
        raylib/raylib_2d.h
        raylib/raylib_3d.h
        raylib/raylib_textures.h
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
        html/html_dom.c
        html/html_css.c
        html/html_events.c
        html/html_serializer.c
    )
    
    set(HTML_HEADERS
        html/html_renderer.h
        html/html_dom.h
        html/html_css.h
        html/html_events.h
        html/html_serializer.h
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
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
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

# =============================================================================
# RENDERER DEPENDENCIES
# =============================================================================

# STB libraries for image processing
target_link_libraries(kryon_renderers PRIVATE stb)

# Raylib dependency
if(KRYON_RENDERER_RAYLIB)
    target_link_libraries(kryon_renderers PRIVATE raylib)
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