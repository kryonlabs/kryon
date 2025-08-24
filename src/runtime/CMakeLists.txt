# Kryon Runtime Module

# =============================================================================
# RUNTIME MODULE CONFIGURATION
# =============================================================================

message(STATUS "Configuring Kryon Runtime module")

# =============================================================================
# CORE RUNTIME COMPONENT
# =============================================================================

set(CORE_SOURCES
    core/runtime.c
    core/krb_loader.c
    core/validation.c
    expression_evaluator.c
)

set(CORE_HEADERS
    ../include/internal/runtime.h
    core/validation.h
)

# =============================================================================
# ELEMENT MANAGEMENT COMPONENT (Consolidated Clean Architecture)
# =============================================================================

set(ELEMENTS_SOURCES
    elements.c                      # Consolidated element infrastructure
    elements/button.c
    elements/dropdown.c    
    elements/input.c
    elements/checkbox.c
    elements/slider.c
    elements/text.c
    elements/container.c            # Handles Container, Column, Row, Center
    elements/image.c
    elements/app.c
    elements/grid.c
    elements/tabbar.c
    elements/tab.c
    elements/tab_content.c
    elements/link.c
)

set(ELEMENTS_HEADERS
    # Headers moved to include/internal/
)

# =============================================================================
# NAVIGATION COMPONENT
# =============================================================================

set(NAVIGATION_SOURCES
    navigation/navigation.c
)

set(NAVIGATION_HEADERS
    navigation/navigation.h
)

# =============================================================================
# COMPILATION COMPONENT
# =============================================================================

set(COMPILATION_SOURCES
    compilation/runtime_compiler.c
)

set(COMPILATION_HEADERS
    compilation/runtime_compiler.h
)

# =============================================================================
# STATE MANAGEMENT COMPONENT
# =============================================================================

set(STATE_SOURCES
    core/state.c
)

set(STATE_HEADERS
    # Will be added when implemented
)

# =============================================================================
# LIFECYCLE MANAGEMENT COMPONENT
# =============================================================================

set(LIFECYCLE_SOURCES
    # Will be added when implemented
)

set(LIFECYCLE_HEADERS
    # Will be added when implemented
)

# =============================================================================
# MEMORY MANAGEMENT COMPONENT
# =============================================================================

set(MEMORY_SOURCES
    # Memory management moved to src/core
)

set(MEMORY_HEADERS
    # Memory headers moved to core utilities
)

# =============================================================================
# PROFILER COMPONENT
# =============================================================================

set(PROFILER_SOURCES
    # Will be added when implemented
)

set(PROFILER_HEADERS
    # Will be added when implemented
)

# =============================================================================
# COMBINED RUNTIME LIBRARY
# =============================================================================

set(RUNTIME_SOURCES
    ${CORE_SOURCES}
    ${ELEMENTS_SOURCES}
    ${NAVIGATION_SOURCES}
    ${COMPILATION_SOURCES}
    ${STATE_SOURCES}
    ${LIFECYCLE_SOURCES}
    ${MEMORY_SOURCES}
)

# Conditionally add profiler
if(KRYON_ENABLE_PROFILER)
    list(APPEND RUNTIME_SOURCES ${PROFILER_SOURCES})
    set(RUNTIME_HEADERS ${CORE_HEADERS} ${ELEMENTS_HEADERS} ${STATE_HEADERS} 
                       ${LIFECYCLE_HEADERS} ${MEMORY_HEADERS} ${PROFILER_HEADERS})
else()
    set(RUNTIME_HEADERS ${CORE_HEADERS} ${ELEMENTS_HEADERS} ${STATE_HEADERS} 
                       ${LIFECYCLE_HEADERS} ${MEMORY_HEADERS})
endif()

# Create runtime library as object library for linking
add_library(kryon_runtime OBJECT ${RUNTIME_SOURCES})

# =============================================================================
# RUNTIME CONFIGURATION
# =============================================================================

target_include_directories(kryon_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/elements
    ${CMAKE_CURRENT_SOURCE_DIR}/navigation
    ${CMAKE_CURRENT_SOURCE_DIR}/compilation
    ${CMAKE_CURRENT_SOURCE_DIR}/state
    ${CMAKE_CURRENT_SOURCE_DIR}/lifecycle
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/profiler
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Runtime-specific definitions
target_compile_definitions(kryon_runtime PRIVATE
    KRYON_RUNTIME_MODULE=1
    KRYON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    KRYON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    KRYON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    KRYON_RENDERER_RAYLIB=1
)

# Feature flags
if(KRYON_ENABLE_PROFILER)
    target_compile_definitions(kryon_runtime PRIVATE KRYON_ENABLE_PROFILER=1)
endif()

if(KRYON_ENABLE_DEBUGGER)
    target_compile_definitions(kryon_runtime PRIVATE KRYON_ENABLE_DEBUGGER=1)
endif()

# Debug/Release specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(kryon_runtime PRIVATE 
        KRYON_RUNTIME_DEBUG=1
        KRYON_ENABLE_MEMORY_DEBUGGING=1
        KRYON_ENABLE_LEAK_DETECTION=1
    )
else()
    target_compile_definitions(kryon_runtime PRIVATE
        KRYON_RUNTIME_OPTIMIZED=1
        NDEBUG=1
    )
endif()

# =============================================================================
# DEPENDENCIES
# =============================================================================

# Link with JSON library for state serialization
target_link_libraries(kryon_runtime PRIVATE cjson)

# Link with core utilities (color functions, etc.)
if(TARGET kryon_minimal_memory)
    target_link_libraries(kryon_runtime PRIVATE kryon_minimal_memory)
endif()

# Link with shared property mappings
if(TARGET kryon_shared)
    target_link_libraries(kryon_runtime PRIVATE kryon_shared)
endif()

# Link with events module
if(TARGET kryon_events)
    target_link_libraries(kryon_runtime PRIVATE kryon_events)
endif()

# Link with script engines module
if(TARGET kryon_script_engines)
    target_link_libraries(kryon_runtime PRIVATE kryon_script_engines)
endif()

# Platform-specific dependencies
if(WIN32)
    target_link_libraries(kryon_runtime PRIVATE 
        kernel32 user32 gdi32 winspool shell32 ole32 
        oleaut32 uuid comdlg32 advapi32
    )
elseif(APPLE)
    target_link_libraries(kryon_runtime PRIVATE 
        "-framework Foundation"
        "-framework CoreFoundation"
    )
elseif(UNIX)
    target_link_libraries(kryon_runtime PRIVATE m pthread dl)
endif()

# =============================================================================
# MEMORY ALLOCATION STRATEGY
# =============================================================================

# Custom memory allocator options
option(KRYON_USE_CUSTOM_ALLOCATOR "Use custom memory allocator" ON)
option(KRYON_USE_MEMORY_POOLS "Use memory pools for performance" ON)
option(KRYON_ENABLE_MEMORY_TRACKING "Enable memory usage tracking" ${KRYON_DEBUG})

if(KRYON_USE_CUSTOM_ALLOCATOR)
    target_compile_definitions(kryon_runtime PRIVATE KRYON_USE_CUSTOM_ALLOCATOR=1)
endif()

if(KRYON_USE_MEMORY_POOLS)
    target_compile_definitions(kryon_runtime PRIVATE KRYON_USE_MEMORY_POOLS=1)
endif()

if(KRYON_ENABLE_MEMORY_TRACKING)
    target_compile_definitions(kryon_runtime PRIVATE KRYON_ENABLE_MEMORY_TRACKING=1)
endif()

# =============================================================================
# PERFORMANCE TUNING
# =============================================================================

# Performance-related compile definitions
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(kryon_runtime PRIVATE
        KRYON_OPTIMIZE_ELEMENT_LOOKUPS=1
        KRYON_OPTIMIZE_STATE_UPDATES=1
        KRYON_OPTIMIZE_MEMORY_ALLOCATION=1
    )
endif()

# Platform-specific optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(kryon_runtime PRIVATE KRYON_ARCH_X64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(kryon_runtime PRIVATE KRYON_ARCH_ARM64=1)
endif()

# =============================================================================
# INSTALL CONFIGURATION
# =============================================================================

# Install headers
install(FILES ${RUNTIME_HEADERS}
    DESTINATION include/kryon/runtime
    COMPONENT Development
)

# =============================================================================
# TESTING
# =============================================================================

if(KRYON_BUILD_TESTS)
    # Runtime tests will be defined in tests/unit/runtime/
    add_subdirectory(tests)
endif()

message(STATUS "Kryon Runtime module configured successfully")