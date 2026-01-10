/**
 * C Source Metadata Structures
 *
 * Shared between C frontend (bindings/c/kryon.c) and IR serialization (ir_json.c)
 * for round-trip C ↔ KIR conversion
 */

#ifndef IR_C_METADATA_H
#define IR_C_METADATA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Variable declaration metadata
typedef struct {
    char* name;
    char* type;
    char* storage;          // "static", "extern", NULL for auto
    char* initial_value;
    uint32_t component_id;  // ID of component this variable references
    int line_number;
} CVariableDecl;

// Event handler metadata
typedef struct {
    char* logic_id;        // e.g., "c_click_6_0"
    char* function_name;   // e.g., "on_start_game"
    char* return_type;     // e.g., "void"
    char* parameters;      // e.g., "void" or "int x, int y"
    char* body;            // Function body source code
    int line_number;
} CEventHandlerDecl;

// Helper function metadata
typedef struct {
    char* name;
    char* return_type;
    char* parameters;
    char* body;
    int line_number;
} CHelperFunction;

// Include directive metadata
typedef struct {
    char* include_string;  // e.g., "<stdio.h>" or "\"myheader.h\""
    bool is_system;        // true for <>, false for ""
    int line_number;
} CInclude;

// Preprocessor directive metadata
typedef struct {
    char* directive_type;  // "define", "ifdef", "ifndef", "if", "elif", "else", "endif"
    char* condition;       // For ifdef/ifndef/if/elif
    char* value;           // For define
    int line_number;
} CPreprocessorDirective;

// Source file metadata
typedef struct {
    char* filename;
    char* full_path;
    char* content;
} CSourceFile;

// Complete C source metadata container
typedef struct {
    CVariableDecl* variables;
    size_t variable_count;
    size_t variable_capacity;

    CEventHandlerDecl* event_handlers;
    size_t event_handler_count;
    size_t event_handler_capacity;

    CHelperFunction* helper_functions;
    size_t helper_function_count;
    size_t helper_function_capacity;

    CInclude* includes;
    size_t include_count;
    size_t include_capacity;

    CPreprocessorDirective* preprocessor_directives;
    size_t preprocessor_directive_count;
    size_t preprocessor_directive_capacity;

    CSourceFile* source_files;
    size_t source_file_count;
    size_t source_file_capacity;

    char* main_source_file;
} CSourceMetadata;

// Global metadata instance (defined in bindings/c/kryon.c)
extern CSourceMetadata g_c_metadata;

/**
 * Clean up all C metadata, freeing allocated memory
 * Call this before loading new metadata to avoid memory leaks
 */
void ir_c_metadata_cleanup(CSourceMetadata* metadata);

#ifdef __cplusplus
}
#endif

#endif // IR_C_METADATA_H
