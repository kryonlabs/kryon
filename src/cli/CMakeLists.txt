# Kryon CLI Module

add_executable(kryon_cli
    main.c
    compile/compile_command.c
    run/run_command.c
    dev/dev_command.c
    build/build_command.c
    debug/debug_command.c
    package/package_command.c
)

target_include_directories(kryon_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_link_libraries(kryon_cli PRIVATE 
    kryon_static
    cjson
)

set_target_properties(kryon_cli PROPERTIES
    OUTPUT_NAME kryon
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)