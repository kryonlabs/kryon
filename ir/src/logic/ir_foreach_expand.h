#ifndef IR_FOREACH_EXPAND_H
#define IR_FOREACH_EXPAND_H

#include <stdint.h>
#include <stdbool.h>
#include "../logic/ir_foreach.h"

// Forward declarations
struct IRComponent;
struct cJSON;

// ============================================================================
// ForEach Expansion Module
// ============================================================================
// Handles runtime expansion of ForEach templates into concrete component trees.
// Replaces the hardcoded expansion logic from ir_serialization.c with a
// modular, binding-based approach.

// ============================================================================
// Expansion Context
// ============================================================================

// Context for expansion - provides access to iteration data
typedef struct IRForEachContext {
    struct cJSON* data_array;   // Array of items to iterate
    int current_index;          // Current iteration index (0-based)
    struct cJSON* current_item; // Current item data
    const char* item_name;      // Loop variable name for binding resolution
    const char* index_name;     // Index variable name for binding resolution
} IRForEachContext;

// ============================================================================
// Main Expansion API
// ============================================================================

// Expand all ForEach components in a tree (entry point)
// This recursively walks the tree and expands ForEach nodes in-place
void ir_foreach_expand_tree(struct IRComponent* root);

// Expand a single ForEach component, returning array of expanded children
// Returns number of children created, stores them in *out_children
// Caller is responsible for integrating children into parent
uint32_t ir_foreach_expand_single(struct IRComponent* foreach_comp,
                                   struct IRComponent*** out_children);

// ============================================================================
// Binding Resolution
// ============================================================================

// Resolve a binding expression to a string value given iteration context
// Returns strdup'd string, caller must free
// Handles: "item.field", "index", "item.nested.field"
char* ir_foreach_resolve_binding(const char* expression,
                                  IRForEachContext* ctx);

// Apply all bindings from a ForEach definition to a component copy
void ir_foreach_apply_bindings(struct IRComponent* component,
                                IRForEachDef* def,
                                IRForEachContext* ctx);

// ============================================================================
// Component Deep Copy
// ============================================================================

// Deep copy a component tree (utility moved from ir_serialization.c)
// Creates complete copy with all children, styles, events, etc.
// Does NOT copy layout_state (expanded copies need fresh layout)
struct IRComponent* ir_component_deep_copy(struct IRComponent* src);

// ============================================================================
// Tree Manipulation
// ============================================================================

// Replace a ForEach component in its parent with expanded children
// The ForEach component itself is removed (layout-transparent)
void ir_foreach_replace_in_parent(struct IRComponent* parent,
                                   int foreach_index,
                                   struct IRComponent** expanded_children,
                                   uint32_t expanded_count);

// ============================================================================
// Property Setters (for binding application)
// ============================================================================

// Set a component property by path (e.g., "text_content", "style.opacity")
// Returns true if property was set successfully
bool ir_foreach_set_property(struct IRComponent* component,
                              const char* property_path,
                              const char* value);

#endif // IR_FOREACH_EXPAND_H
