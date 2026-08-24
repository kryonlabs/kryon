include_guard(GLOBAL)

function(kryon_generate_kry_sources out_sources out_include_dir)
    set(options NO_MAIN)
    set(one_value_args KRYON_DIR ROOT OUT_DIR TARGET)
    set(multi_value_args SOURCES)
    cmake_parse_arguments(KRYGEN "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT KRYGEN_KRYON_DIR)
        message(FATAL_ERROR "kryon_generate_kry_sources requires KRYON_DIR")
    endif()
    if(NOT KRYGEN_ROOT)
        message(FATAL_ERROR "kryon_generate_kry_sources requires ROOT")
    endif()
    if(NOT KRYGEN_OUT_DIR)
        message(FATAL_ERROR "kryon_generate_kry_sources requires OUT_DIR")
    endif()
    if(NOT KRYGEN_TARGET)
        message(FATAL_ERROR "kryon_generate_kry_sources requires TARGET")
    endif()
    if(NOT KRYGEN_SOURCES)
        message(FATAL_ERROR "kryon_generate_kry_sources requires SOURCES")
    endif()

    file(GLOB KRYGEN_K2C_SOURCES CONFIGURE_DEPENDS "${KRYGEN_KRYON_DIR}/cmd/k2c/*.c")
    list(APPEND KRYGEN_K2C_SOURCES
        "${KRYGEN_KRYON_DIR}/cmd/kir/kir.c"
        "${KRYGEN_KRYON_DIR}/cmd/kir/kir_parse.c")
    find_program(KRYGEN_HOST_CC NAMES cc clang gcc REQUIRED)
    set(KRYGEN_K2C "${CMAKE_BINARY_DIR}/kryon-host-tools/k2c")

    set(krygen_generated_sources)
    set(krygen_generated_headers)
    set(krygen_command_sources)
    foreach(kry_source ${KRYGEN_SOURCES})
        file(RELATIVE_PATH kry_rel "${KRYGEN_ROOT}" "${kry_source}")
        list(APPEND krygen_command_sources "${kry_rel}")
        get_filename_component(kry_rel_dir "${kry_rel}" DIRECTORY)
        get_filename_component(kry_name "${kry_rel}" NAME_WE)
        list(APPEND krygen_generated_sources "${KRYGEN_OUT_DIR}/${kry_rel_dir}/${kry_name}.c")
        list(APPEND krygen_generated_headers "${KRYGEN_OUT_DIR}/${kry_rel_dir}/${kry_name}.h")
    endforeach()

    set(krygen_flags)
    if(KRYGEN_NO_MAIN)
        list(APPEND krygen_flags --no-main)
    endif()

    add_custom_command(
        OUTPUT
            ${krygen_generated_sources}
            ${krygen_generated_headers}
            "${KRYGEN_OUT_DIR}/kryon_project.c"
            "${KRYGEN_OUT_DIR}/kryon_project.h"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${KRYGEN_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${KRYGEN_OUT_DIR}"
        COMMAND "${KRYGEN_K2C}" ${krygen_flags} --root "${KRYGEN_ROOT}" -o "${KRYGEN_OUT_DIR}" ${krygen_command_sources}
        DEPENDS ${KRYGEN_SOURCES} "${KRYGEN_K2C}"
        WORKING_DIRECTORY "${KRYGEN_ROOT}"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${KRYGEN_K2C}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/kryon-host-tools"
        COMMAND "${KRYGEN_HOST_CC}" -Wall -Wextra -O2
            -I${KRYGEN_KRYON_DIR}/cmd/kir
            -I${KRYGEN_KRYON_DIR}/cmd/k2c
            -o "${KRYGEN_K2C}"
            ${KRYGEN_K2C_SOURCES}
        DEPENDS ${KRYGEN_K2C_SOURCES}
        VERBATIM
    )

    add_custom_target("${KRYGEN_TARGET}" DEPENDS
        ${krygen_generated_sources}
        ${krygen_generated_headers}
        "${KRYGEN_OUT_DIR}/kryon_project.c"
        "${KRYGEN_OUT_DIR}/kryon_project.h"
    )

    set(${out_sources} ${krygen_generated_sources} PARENT_SCOPE)
    set(${out_include_dir} "${KRYGEN_OUT_DIR}" PARENT_SCOPE)
endfunction()
