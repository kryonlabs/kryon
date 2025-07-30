# Kryon Compiler Module

# =============================================================================
# COMPILER MODULE CONFIGURATION
# =============================================================================

message(STATUS "Configuring Kryon Compiler module")

# =============================================================================
# LEXER COMPONENT
# =============================================================================

set(LEXER_SOURCES
    lexer/lexer.c
    lexer/unicode.c
)

set(LEXER_HEADERS
    ../include/internal/lexer.h
)

# =============================================================================
# PARSER COMPONENT
# =============================================================================

set(PARSER_SOURCES
    parser/parser.c
    parser/ast_utils.c
)

set(PARSER_HEADERS
    ../include/internal/parser.h
)

# =============================================================================
# CODE GENERATION COMPONENT
# =============================================================================

set(CODEGEN_SOURCES
    codegen/codegen.c
    codegen/krb_writer.c
    codegen/krb_reader.c
    codegen/krb_file.c
)

set(CODEGEN_HEADERS
    ../include/internal/codegen.h
    ../include/internal/krb_format.h
)

# =============================================================================
# OPTIMIZER COMPONENT  
# =============================================================================

set(OPTIMIZER_SOURCES
    optimizer/optimizer.c
)

set(OPTIMIZER_HEADERS
    # Optimizer headers will be added as needed
)

# =============================================================================
# DIAGNOSTICS COMPONENT
# =============================================================================

set(DIAGNOSTICS_SOURCES
    diagnostics/diagnostics.c
)

set(DIAGNOSTICS_HEADERS
    ../include/internal/diagnostics.h
)

# =============================================================================
# MAPPINGS (Element/Property hex values)
# =============================================================================

set(MAPPINGS_SOURCES
    # Will be added when mappings are implemented
)

set(MAPPINGS_HEADERS
    # Will be added when mappings are implemented
)

# =============================================================================
# COMBINED COMPILER LIBRARY
# =============================================================================

set(COMPILER_SOURCES
    ${LEXER_SOURCES}
    ${PARSER_SOURCES}
    ${CODEGEN_SOURCES}
    ${OPTIMIZER_SOURCES}
    ${DIAGNOSTICS_SOURCES}
    ${MAPPINGS_SOURCES}
)

set(COMPILER_HEADERS
    ${LEXER_HEADERS}
    ${PARSER_HEADERS}
    ${CODEGEN_HEADERS}
    ${OPTIMIZER_HEADERS}
    ${DIAGNOSTICS_HEADERS}
    ${MAPPINGS_HEADERS}
)

# Create compiler library as object library for linking
add_library(kryon_compiler OBJECT ${COMPILER_SOURCES})

# =============================================================================
# COMPILER CONFIGURATION
# =============================================================================

target_include_directories(kryon_compiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/parser
    ${CMAKE_CURRENT_SOURCE_DIR}/codegen
    ${CMAKE_CURRENT_SOURCE_DIR}/optimizer
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

# Compiler-specific definitions
target_compile_definitions(kryon_compiler PRIVATE
    KRYON_COMPILER_MODULE=1
    KRYON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    KRYON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    KRYON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Compiler flags for this module
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(kryon_compiler PRIVATE 
        KRYON_COMPILER_DEBUG=1
        KRYON_ENABLE_COMPILER_TRACING=1
    )
endif()

# Performance optimizations for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(kryon_compiler PRIVATE
        KRYON_COMPILER_OPTIMIZED=1
        NDEBUG=1
    )
endif()

# =============================================================================
# DEPENDENCIES
# =============================================================================

# Link with cJSON for metadata processing
target_link_libraries(kryon_compiler PRIVATE cjson)

# Platform-specific dependencies
if(WIN32)
    target_link_libraries(kryon_compiler PRIVATE ws2_32)
elseif(UNIX)
    target_link_libraries(kryon_compiler PRIVATE m)
endif()

# =============================================================================
# COMPILER EXECUTABLE (for standalone use)
# =============================================================================

if(KRYON_BUILD_COMPILER_EXECUTABLE)
    add_executable(kryon_compiler_exe
        compiler_main.c
        $<TARGET_OBJECTS:kryon_compiler>
    )
    
    set_target_properties(kryon_compiler_exe PROPERTIES
        OUTPUT_NAME kryon-compile
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    target_link_libraries(kryon_compiler_exe PRIVATE cjson)
    
    if(UNIX)
        target_link_libraries(kryon_compiler_exe PRIVATE m)
    endif()
endif()

# =============================================================================
# INSTALL CONFIGURATION
# =============================================================================

# Install headers
install(FILES ${COMPILER_HEADERS}
    DESTINATION include/kryon/compiler
    COMPONENT Development
)

# Install compiler executable if built
if(KRYON_BUILD_COMPILER_EXECUTABLE)
    install(TARGETS kryon_compiler_exe
        DESTINATION bin
        COMPONENT Runtime
    )
endif()

# =============================================================================
# TESTING
# =============================================================================

if(KRYON_BUILD_TESTS)
    # Compiler tests will be defined in tests/unit/compiler/
    add_subdirectory(tests)
endif()

message(STATUS "Kryon Compiler module configured successfully")