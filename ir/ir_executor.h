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

// Executor context - ties together logic, sources, and component tree
typedef struct IRExecutorContext {
    IRLogicBlock* logic;              // Logic block from .kir file
    IRReactiveManifest* manifest;     // Reactive state (optional)
    IRComponent* root;                // Component tree (optional)

    // Embedded source code from "sources" section
    IRExecutorSource sources[IR_EXECUTOR_MAX_SOURCES];
    int source_count;

    // Event metadata for current handler execution
    struct {
        uint32_t component_id;
        const char* event_type;
    } current_event;
} IRExecutorContext;

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

// Execute a handler function by name
bool ir_executor_call_handler(IRExecutorContext* ctx, const char* handler_name);

// ============================================================================
// GLOBAL EXECUTOR (for desktop backend integration)
// ============================================================================

// Get/set the global executor context (used by desktop_input.c)
IRExecutorContext* ir_executor_get_global(void);
void ir_executor_set_global(IRExecutorContext* ctx);

// ============================================================================
// FILE LOADING
// ============================================================================

// Load logic block and sources from a .kir file
bool ir_executor_load_kir_file(IRExecutorContext* ctx, const char* kir_path);

#endif // IR_EXECUTOR_H
