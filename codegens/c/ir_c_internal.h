/**
 * C Code Generator - Internal Shared Declarations
 *
 * Common types and forward declarations shared across C codegen modules.
 */

#ifndef IR_C_INTERNAL_H
#define IR_C_INTERNAL_H

#include <stdio.h>
#include <stdbool.h>
#include "../../third_party/cJSON/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Code Generation Context
// ============================================================================

typedef struct CCodegenContext {
    FILE* output;
    int indent_level;
    cJSON* root_json;           // Full KIR JSON
    cJSON* component_tree;      // Root component
    cJSON* c_metadata;          // C metadata section
    cJSON* variables;           // Variables array
    cJSON* event_handlers;      // Event handlers array
    cJSON* helper_functions;    // Helper functions array
    cJSON* includes;            // Includes array
    cJSON* preprocessor_dirs;   // Preprocessor directives array

    // Reactive state tracking
    cJSON* reactive_manifest;   // reactive_manifest from KIR
    cJSON* reactive_vars;       // reactive_manifest.variables array
    bool has_reactive_state;    // True if any reactive variables exist
    const char* current_scope;  // Current component scope during tree traversal (e.g., "Counter#0")

    // For-loop context tracking
    const char* current_loop_var;  // Current for-loop iterator variable (e.g., "todo")

    // Pending text format info for non-reactive loop variable text templates
    // Set during property_bindings scan, used by TEXT component generation
    char* pending_text_format;     // Format string e.g., "- %s"
    char* pending_text_var;        // Variable name e.g., "todo"

    // Handler deduplication tracking
    const char* generated_handlers[256];  // Names of already-generated handlers
    int generated_handler_count;          // Number of handlers generated

    // Local variable tracking for function generation
    char local_vars[64][256];             // Names of declared local variables
    int local_var_count;                  // Number of local variables

    // Global variable names (from source_structures.const_declarations)
    char global_vars[64][256];            // Names of global variables
    int global_var_count;                 // Number of global variables
} CCodegenContext;

// ============================================================================
// Internal Code Generation Functions (ir_c_codegen.c)
// ============================================================================

// Header generation
void generate_includes(CCodegenContext* ctx);
void generate_preprocessor_directives(CCodegenContext* ctx);
void generate_variable_declarations(CCodegenContext* ctx);
void generate_helper_functions(CCodegenContext* ctx);
void generate_event_handlers(CCodegenContext* ctx);

// Array/const declarations from source_structures
void generate_array_declarations(CCodegenContext* ctx);

// Exported function generation
bool generate_exported_functions(FILE* output, cJSON* logic_block, cJSON* exports, const char* output_path);

// Universal function generation (functions with "universal" statements block, not event handlers)
// Forward declarations must be generated BEFORE array declarations
void generate_universal_function_declarations(CCodegenContext* ctx, cJSON* logic_block);
void generate_universal_functions(CCodegenContext* ctx, cJSON* logic_block);

// ============================================================================
// Variable Tracking Functions (ir_c_expression.c)
// ============================================================================

// Reset local variable tracking (call at start of each function)
void c_reset_local_vars(CCodegenContext* ctx);

// Check if a variable is a local variable (already declared in current function)
bool c_is_local_var(CCodegenContext* ctx, const char* name);

// Mark a variable as declared locally
void c_add_local_var(CCodegenContext* ctx, const char* name);

// Check if a variable is a global variable
bool c_is_global_var(CCodegenContext* ctx, const char* name);

// Initialize global variable list from source_structures
void c_init_global_vars(CCodegenContext* ctx);

// Infer C type from an expression
const char* c_infer_type(cJSON* expr);

#ifdef __cplusplus
}
#endif

#endif // IR_C_INTERNAL_H
