# Integration Tests for Kryon-C

# @for directive integration tests
if(BUILD_KRYON_TESTS)
    add_executable(test_for_directive_integration test_for_directive_integration.c)
target_include_directories(test_for_directive_integration PRIVATE 
    ../../include
)
target_link_libraries(test_for_directive_integration 
    kryon_runtime
    kryon_minimal_compiler
    kryon_minimal_memory
    kryon_events
    kryon_script_engines
    kryon_shared
)

# Add raylib dependency if available
if(TARGET raylib)
    target_link_libraries(test_for_directive_integration raylib)
elseif(PKG_CONFIG_FOUND)
    pkg_check_modules(RAYLIB raylib)
    if(RAYLIB_FOUND)
        target_link_libraries(test_for_directive_integration ${RAYLIB_LIBRARIES})
        target_include_directories(test_for_directive_integration PRIVATE ${RAYLIB_INCLUDE_DIRS})
    endif()
endif()

# Platform-specific libraries
if(UNIX)
    target_link_libraries(test_for_directive_integration m)
endif()

set_target_properties(test_for_directive_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
    C_STANDARD 99
)

    # Add to CTest
    add_test(NAME test_for_directive_integration 
             COMMAND test_for_directive_integration
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif() # BUILD_KRYON_TESTS