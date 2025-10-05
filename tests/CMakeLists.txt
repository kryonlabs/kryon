# Kryon @for Directive Tests

# Only build tests if raylib is properly linked
option(BUILD_KRYON_TESTS "Build Kryon tests (requires raylib)" OFF)

if(BUILD_KRYON_TESTS)
    # @for directive unit tests
    add_executable(test_for_directive unit/runtime/test_for_directive.c)
target_include_directories(test_for_directive PRIVATE ../include)
target_link_libraries(test_for_directive 
    kryon_runtime
    kryon_minimal_memory
    kryon_events
    kryon_shared
)

# Add raylib dependency if available
if(TARGET raylib)
    target_link_libraries(test_for_directive raylib)
elseif(PKG_CONFIG_FOUND)
    pkg_check_modules(RAYLIB raylib)
    if(RAYLIB_FOUND)
        target_link_libraries(test_for_directive ${RAYLIB_LIBRARIES})
        target_include_directories(test_for_directive PRIVATE ${RAYLIB_INCLUDE_DIRS})
    endif()
endif()

# Platform-specific libraries
if(UNIX)
    target_link_libraries(test_for_directive m)
endif()

set_target_properties(test_for_directive PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    C_STANDARD 99
)

# Add to CTest
add_test(NAME test_for_directive COMMAND test_for_directive)

# Add @for directive compilation test script
add_test(NAME test_for_compilation 
         COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_for_compilation.sh
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # Integration tests (if any exist)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration)
        add_subdirectory(integration)
    endif()

    # Performance tests (if any exist)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/performance)
        add_subdirectory(performance)
    endif()
endif() # BUILD_KRYON_TESTS
