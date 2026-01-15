#ifndef IR_EXECUTOR_H
#define IR_EXECUTOR_H

#include "ir_core.h"
#include "ir_logic.h"
#include <stdbool.h>

// ============================================================================
// IR EXECUTOR
// ============================================================================
// The executor handles event dispatch and handler execution for .kir files.
// It bridges the gap between IR events and actual handler code execution.

// Source code storage for language-specific handlers
typedef struct {
    char* lang;    // "nim", "lua", "js", "c"
    char* code;    // Full source code for this language
} IRExecutorSource;

#define IR_EXECUTOR_MAX_SOURCES 8
#define IR_EXECUTOR_MAX_VARS 64
#define IR_EXECUTOR_MAX_CONDITIONALS 32

// ============================================================================
// VALUE TYPE SYSTEM (for mixed-type arrays and variables)
// ============================================================================

// Value types
typedef enum {
    VAR_TYPE_INT,
    VAR_TYPE_STRING,
    VAR_TYPE_ARRAY,
    VAR_TYPE_NULL,
} IRVarType;

// Forward declaration for recursive array definition
struct IRValue;

// Value container (recursive for arrays)
typedef struct IRValue {
    IRVarType type;
    union {
        int64_t int_val;
        char* string_val;
        struct {
            struct IRValue* items;  // Array of mixed-type values
            int count;
            int capacity;
        } array_val;
    };
} IRValue;

// ============================================================================
// EXECUTOR STRUCTURES
// ============================================================================

// Variable storage for runtime state (supports mixed types)
typedef struct {
    char* name;
    IRValue value;  // Changed from int64_t to support arrays and strings
    uint32_t owner_component_id;  // 0 = global, or component instance ID
    char* scope;  // Scope string (e.g., "global", "Counter#0", "Counter#1")
} IRExecutorVar;

// Runtime conditional storage for dynamic visibility
typedef struct {
    IRExpression* condition;        // Parsed condition expression
    uint32_t* then_ids;             // Component IDs to show when true
    uint32_t then_count;
    uint32_t* else_ids;             // Component IDs to show when false
    uint32_t else_count;
} IRRuntimeConditional;

// Executor context - ties together logic, sources, and component tree
typedef struct IRExecutorContext {
    IRLogicBlock* logic;              // Logic block from .kir file
    IRReactiveManifest* manifest;     // Reactive state (optional)
    IRComponent* root;                // Component tree (optional)

    // For-loop rendering state
    IRReactiveForLoop* for_loops;     // Parsed for-loops from manifest
    int for_loop_count;               // Number of for-loops

    // Embedded source code from "sources" section
    IRExecutorSource sources[IR_EXECUTOR_MAX_SOURCES];
    int source_count;

    // Runtime variable storage
    IRExecutorVar vars[IR_EXECUTOR_MAX_VARS];
    int var_count;

    // Runtime conditionals for dynamic visibility
    IRRuntimeConditional conditionals[IR_EXECUTOR_MAX_CONDITIONALS];
    int conditional_count;

    // Current execution context (which component instance we're executing for)
    uint32_t current_instance_id;
    char* current_scope;  // Current scope string for variable lookups (e.g., "Counter#0")

    // Event metadata for current handler execution
    struct {
        uint32_t component_id;
        const char* event_type;
    } current_event;

    // Built-in function registry
    struct IRBuiltinEntry* builtins;
    int builtin_count;
    int builtin_capacity;
} IRExecutorContext;

// Built-in function signature
typedef IRValue (*IRBuiltinFunc)(struct IRExecutorContext* ctx, IRValue* args, int arg_count, uint32_t instance_id);

// Registry entry
typedef struct IRBuiltinEntry {
    char* name;
    IRBuiltinFunc handler;
} IRBuiltinEntry;

// ============================================================================
// LIFECYCLE
// ============================================================================

// Create a new executor context
IRExecutorContext* ir_executor_create(void);

// Destroy executor context and free resources
void ir_executor_destroy(IRExecutorContext* ctx);

// ============================================================================
// CONFIGURATION
// ============================================================================

// Set the logic block (from .kir file)
void ir_executor_set_logic(IRExecutorContext* ctx, IRLogicBlock* logic);

// Set reactive manifest for state access
void ir_executor_set_manifest(IRExecutorContext* ctx, IRReactiveManifest* manifest);

// Set root component for tree traversal
void ir_executor_set_root(IRExecutorContext* ctx, IRComponent* root);

// Apply initial conditional visibility based on current variable values
// Call this after setting the root and loading the manifest
void ir_executor_apply_initial_conditionals(IRExecutorContext* ctx);

// Add source code for a language (from .kir "sources" section)
void ir_executor_add_source(IRExecutorContext* ctx, const char* lang, const char* code);

// Get source code for a language
const char* ir_executor_get_source(IRExecutorContext* ctx, const char* lang);

// ============================================================================
// EVENT HANDLING
// ============================================================================

// Handle an event on a component
// Returns true if handler was found and executed
bool ir_executor_handle_event(IRExecutorContext* ctx,
                               uint32_t component_id,
                               const char* event_type);

// Handle an event by logic_id directly (used when component IDs have been remapped)
bool ir_executor_handle_event_by_logic_id(IRExecutorContext* ctx,
                                           uint32_t component_id,
                                           const char* logic_id);

// Execute a handler function by name
bool ir_executor_call_handler(IRExecutorContext* ctx, const char* handler_name);

// ============================================================================
// GLOBAL STATE MANAGER (for desktop backend integration)
// ============================================================================

// Forward declaration
typedef struct IRStateManager IRStateManager;

// Get/set the global state manager
IRStateManager* ir_state_get_global(void);
void ir_state_set_global(IRStateManager* mgr);

// Get executor from global state manager
IRExecutorContext* ir_executor_get_global(void);

/**
 * Evaluate a text expression and return the string representation
 * @param expression Variable name or expression (e.g., "value", "count")
 * @param scope Scope string for variable lookup (e.g., "Counter#0", "global", or NULL for global)
 * @return Allocated string with the value (caller must free), or NULL if not found
 */
char* ir_executor_eval_text_expression(const char* expression, const char* scope);

// ============================================================================
// FILE LOADING
// ============================================================================

// Load logic block and sources from a .kir file
bool ir_executor_load_kir_file(IRExecutorContext* ctx, const char* kir_path);

// ============================================================================
// VARIABLE MANAGEMENT
// ============================================================================

// Get a variable's value (returns copy, null if not found)
IRValue ir_executor_get_var(IRExecutorContext* ctx, const char* name, uint32_t instance_id);

// Set a variable's value (creates if doesn't exist, takes ownership of value)
void ir_executor_set_var(IRExecutorContext* ctx, const char* name, IRValue value, uint32_t instance_id);

// Backwards-compatible integer API (for existing code)
int64_t ir_executor_get_var_int(IRExecutorContext* ctx, const char* name, uint32_t instance_id);
void ir_executor_set_var_int(IRExecutorContext* ctx, const char* name, int64_t value, uint32_t instance_id);

// ============================================================================
// UNIVERSAL LOGIC EXECUTION
// ============================================================================

// Execute universal statements for a handler
bool ir_executor_run_universal(IRExecutorContext* ctx, IRLogicFunction* func, uint32_t instance_id);

// Update Text components after state change (refreshes {{var}} expressions)
void ir_executor_update_text_components(IRExecutorContext* ctx);

// Sync Input component text back to bound variable (called by backend when Input changes)
void ir_executor_sync_input_to_var(IRExecutorContext* ctx, IRComponent* input_comp);

#endif // IR_EXECUTOR_H
