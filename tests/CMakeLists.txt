# Kryon Unit Tests

# Test executable macro
macro(add_kryon_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} kryon_static)
    
    # Platform-specific linking
    if(KRYON_PLATFORM STREQUAL "linux" AND X11_FOUND)
        target_link_libraries(${test_name} ${X11_LIBRARIES})
    endif()
    
    # Add math library on Unix systems
    if(UNIX)
        target_link_libraries(${test_name} m)
    endif()
    
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    )
    
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro()

# Core tests
add_kryon_test(test_lexer unit/compiler/test_lexer.c)
add_kryon_test(test_parser unit/compiler/test_parser.c)
add_kryon_test(test_codegen unit/compiler/test_codegen.c)
add_kryon_test(test_runtime unit/runtime/test_runtime.c)
add_kryon_test(test_software_renderer unit/renderers/test_software_renderer.c)
add_kryon_test(test_gpu_renderer unit/renderers/test_gpu_renderer.c)
add_kryon_test(test_platform unit/platform/test_platform.c)

# New renderer interface tests
find_package(PkgConfig REQUIRED)

# Try to find SDL2 (optional for testing)
pkg_check_modules(SDL2 sdl2)
pkg_check_modules(SDL2_TTF SDL2_ttf)
pkg_check_modules(SDL2_IMAGE SDL2_image)

# Try to find Raylib (optional for testing)
find_package(raylib QUIET)

# HTML renderer test (always available - works headless)
add_executable(test_html_renderer renderers/test_renderers.c)
target_sources(test_html_renderer PRIVATE
    ../src/renderers/html/html_renderer.c
)
target_include_directories(test_html_renderer PRIVATE ../include)
target_compile_definitions(test_html_renderer PRIVATE -DHTML_ONLY)
set_target_properties(test_html_renderer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    C_STANDARD 99
)
add_test(NAME test_html_renderer COMMAND test_html_renderer --html)

# SDL2 renderer test (if SDL2 is available)
if(SDL2_FOUND AND SDL2_TTF_FOUND AND SDL2_IMAGE_FOUND)
    add_executable(test_sdl2_renderer renderers/test_renderers.c)
    target_sources(test_sdl2_renderer PRIVATE
        ../src/renderers/sdl2/sdl2_renderer.c
    )
    target_include_directories(test_sdl2_renderer PRIVATE 
        ../include
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
        ${SDL2_IMAGE_INCLUDE_DIRS}
    )
    target_link_libraries(test_sdl2_renderer 
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        m
    )
    target_compile_options(test_sdl2_renderer PRIVATE
        ${SDL2_CFLAGS_OTHER}
        ${SDL2_TTF_CFLAGS_OTHER}
        ${SDL2_IMAGE_CFLAGS_OTHER}
    )
    set_target_properties(test_sdl2_renderer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
        C_STANDARD 99
    )
    add_test(NAME test_sdl2_renderer COMMAND test_sdl2_renderer --sdl2)
    message(STATUS "SDL2 renderer test enabled")
else()
    message(STATUS "SDL2 renderer test disabled (SDL2 not found)")
endif()

# Raylib renderer test (if Raylib is available)
if(raylib_FOUND)
    add_executable(test_raylib_renderer renderers/test_renderers.c)
    target_sources(test_raylib_renderer PRIVATE
        ../src/renderers/raylib/raylib_renderer.c
    )
    target_include_directories(test_raylib_renderer PRIVATE ../include)
    target_link_libraries(test_raylib_renderer raylib m)
    set_target_properties(test_raylib_renderer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
        C_STANDARD 99
    )
    add_test(NAME test_raylib_renderer COMMAND test_raylib_renderer --raylib)
    message(STATUS "Raylib renderer test enabled")
else()
    message(STATUS "Raylib renderer test disabled (Raylib not found)")
endif()

# Integration tests (if any exist)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration)
    add_subdirectory(integration)
endif()

# Performance tests (if any exist)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/performance)
    add_subdirectory(performance)
endif()