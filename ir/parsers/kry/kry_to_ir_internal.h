/**
 * Kryon .kry to IR Converter - Internal Shared Declarations
 *
 * Common types, context structures, and function declarations
 * shared across kry_to_ir modules.
 */

#ifndef KRY_TO_IR_INTERNAL_H
#define KRY_TO_IR_INTERNAL_H

#include "kry_ast.h"
#include "kry_expression.h"
#include "ir_core.h"
#include "ir_logic.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Compilation Modes
// ============================================================================

typedef enum {
    IR_COMPILE_MODE_RUNTIME,  // Expand only (default, current behavior)
    IR_COMPILE_MODE_CODEGEN,  // Preserve only (templates for codegen)
    IR_COMPILE_MODE_HYBRID    // Both expand AND preserve (recommended for round-trip)
} IRCompileMode;

// ============================================================================
// Conversion Context
// ============================================================================

#define MAX_PARAMS 16

typedef struct {
    char* name;
    char* value;          // String representation of the value (for simple types)
    KryValue* kry_value;  // Full KryValue for arrays/objects (NULL for simple types)
} ParamSubstitution;

typedef struct ConversionContext {
    KryNode* ast_root;                 // Root of AST (for finding component definitions)
    KryParser* parser;                 // Parser for error reporting (NULL if unavailable)
    ParamSubstitution params[MAX_PARAMS];  // Parameter substitutions
    int param_count;
    IRReactiveManifest* manifest;      // Reactive manifest for state variables
    IRLogicBlock* logic_block;         // Logic block for event handlers
    uint32_t next_handler_id;          // Counter for generating unique handler names
    IRCompileMode compile_mode;        // Compilation mode (RUNTIME/CODEGEN/HYBRID)
    IRSourceStructures* source_structures;  // Source preservation (for round-trip codegen)
    uint32_t static_block_counter;     // Counter for generating unique static block IDs
    const char* current_static_block_id;   // Current static block context (NULL if not in static block)
    KryExprTarget target_platform;     // Target platform for expression transpilation
    char* source_file_path;            // Path of source file (for resolving imports)
    char* base_directory;              // Base directory for resolving relative imports
    bool skip_import_expansion;        // If true, don't expand imports (for multi-file KIR codegen)

    // Module import registry (for expression resolution)
    struct {
        char* import_name;             // "Math", "Colors"
        char* module_path;             // "math", "colors"
        IRModuleExport** exports;      // Cached exports from this module
        uint32_t export_count;
    }* module_imports;
    uint32_t module_import_count;
    uint32_t module_import_capacity;
} ConversionContext;

// ============================================================================
// Context Functions (kry_to_ir_context.c)
// ============================================================================

/**
 * Add a parameter substitution (string value)
 */
void kry_ctx_add_param(ConversionContext* ctx, const char* name, const char* value);

/**
 * Add a parameter substitution (KryValue for arrays/objects)
 */
void kry_ctx_add_param_value(ConversionContext* ctx, const char* name, KryValue* value);

/**
 * Substitute a parameter reference with its value
 */
const char* kry_ctx_substitute_param(ConversionContext* ctx, const char* expr);

/**
 * Check if an expression is unresolved (no parameter substitution found)
 */
bool kry_ctx_is_unresolved_expr(ConversionContext* ctx, const char* expr);

/**
 * Convert a KryValue to a JSON string representation
 * Returns a dynamically allocated string that must be freed by the caller.
 */
char* kry_value_to_json(KryValue* value);

// Note: Additional functions (resolve_value_as_string, expression converters)
// are still in kry_to_ir.c and will be extracted in future phases

#ifdef __cplusplus
}
#endif

#endif // KRY_TO_IR_INTERNAL_H
