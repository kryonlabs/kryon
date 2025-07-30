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
    core/application.c
    core/event_loop.c
    core/initialization.c
    core/shutdown.c
)

set(CORE_HEADERS
    core/runtime.h
    core/application.h
    core/event_loop.h
    core/initialization.h
    core/shutdown.h
)

# =============================================================================
# ELEMENT MANAGEMENT COMPONENT
# =============================================================================

set(ELEMENTS_SOURCES
    elements/element_manager.c
    elements/element_tree.c
    elements/element_factory.c
    elements/property_manager.c
    elements/template_binding.c
)

set(ELEMENTS_HEADERS
    elements/element_manager.h
    elements/element_tree.h
    elements/element_factory.h
    elements/property_manager.h
    elements/template_binding.h
)

# =============================================================================
# STATE MANAGEMENT COMPONENT
# =============================================================================

set(STATE_SOURCES
    state/state_manager.c
    state/reactive_variables.c
    state/global_stores.c
    state/watchers.c
    state/change_tracking.c
)

set(STATE_HEADERS
    state/state_manager.h
    state/reactive_variables.h
    state/global_stores.h
    state/watchers.h
    state/change_tracking.h
)

# =============================================================================
# LIFECYCLE MANAGEMENT COMPONENT
# =============================================================================

set(LIFECYCLE_SOURCES
    lifecycle/lifecycle_manager.c
    lifecycle/mount_hooks.c
    lifecycle/unmount_hooks.c
    lifecycle/update_hooks.c
    lifecycle/effect_system.c
)

set(LIFECYCLE_HEADERS
    lifecycle/lifecycle_manager.h
    lifecycle/mount_hooks.h
    lifecycle/unmount_hooks.h
    lifecycle/update_hooks.h
    lifecycle/effect_system.h
)

# =============================================================================
# MEMORY MANAGEMENT COMPONENT
# =============================================================================

set(MEMORY_SOURCES
    memory/memory_manager.c
    memory/memory_pools.c
    memory/garbage_collector.c
    memory/leak_detector.c
    memory/allocator.c
)

set(MEMORY_HEADERS
    memory/memory_manager.h
    memory/memory_pools.h
    memory/garbage_collector.h
    memory/leak_detector.h
    memory/allocator.h
)

# =============================================================================
# PROFILER COMPONENT
# =============================================================================

set(PROFILER_SOURCES
    profiler/profiler.c
    profiler/frame_timer.c
    profiler/memory_tracker.c
    profiler/performance_counter.c
    profiler/bottleneck_detector.c
)

set(PROFILER_HEADERS
    profiler/profiler.h
    profiler/frame_timer.h
    profiler/memory_tracker.h
    profiler/performance_counter.h
    profiler/bottleneck_detector.h
)

# =============================================================================
# COMBINED RUNTIME LIBRARY
# =============================================================================

set(RUNTIME_SOURCES
    ${CORE_SOURCES}
    ${ELEMENTS_SOURCES}
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
    ${CMAKE_CURRENT_SOURCE_DIR}/state
    ${CMAKE_CURRENT_SOURCE_DIR}/lifecycle
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/profiler
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
    ${CMAKE_SOURCE_DIR}/src
)

# Runtime-specific definitions
target_compile_definitions(kryon_runtime PRIVATE
    KRYON_RUNTIME_MODULE=1
    KRYON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    KRYON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    KRYON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
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