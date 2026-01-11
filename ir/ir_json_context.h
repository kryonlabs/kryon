#ifndef IR_JSON_CONTEXT_H
#define IR_JSON_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct cJSON cJSON;

// ============================================================================
// Component Definition Entry
// ============================================================================

// Component definition entry for expansion during deserialization
typedef struct {
    char* name;
    cJSON* definition;  // Full component definition (props, state, template)
} ComponentDefEntry;

// ============================================================================
// Component Definition Context
// ============================================================================

// Context for tracking component definitions
typedef struct {
    ComponentDefEntry* entries;
    int count;
    int capacity;
} ComponentDefContext;

// ============================================================================
// Context Management
// ============================================================================

// Create a new component definition context
ComponentDefContext* ir_json_context_create(void);

// Free a component definition context
void ir_json_context_free(ComponentDefContext* ctx);

// ============================================================================
// Definition Registration and Lookup
// ============================================================================

// Add a component definition to the context
bool ir_json_context_add(ComponentDefContext* ctx, const char* name, cJSON* definition);

// Look up a component definition by name
cJSON* ir_json_context_lookup(ComponentDefContext* ctx, const char* name);

// Look up a component definition by module ID prefix
cJSON* ir_json_context_lookup_by_module(ComponentDefContext* ctx, const char* module_id);

// ============================================================================
// Global Cache Management
// ============================================================================

// Initialize the global context cache
void ir_json_context_cache_init(void);

// Shut down the global context cache
void ir_json_context_cache_shutdown(void);

// Get the global context cache
ComponentDefContext* ir_json_context_cache_get(void);

#endif // IR_JSON_CONTEXT_H
