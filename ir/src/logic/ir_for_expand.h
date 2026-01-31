#ifndef IR_FOR_EXPAND_H
#define IR_FOR_EXPAND_H

#include <stdint.h>
#include <stdbool.h>
#include "ir_for.h"

// Forward declarations
struct IRComponent;
struct IRSourceStructures;
struct cJSON;

// ============================================================================
// For Expansion Module
// ============================================================================
// Handles runtime expansion of For templates into concrete component trees.
// Replaces the hardcoded expansion logic from ir_serialization.c with a
// modular, binding-based approach.

// ============================================================================
// Expansion Context
// ============================================================================

// Context for expansion - provides access to iteration data
typedef struct IRForContext {
    struct cJSON* data_array;   // Array of items to iterate
    int current_index;          // Current iteration index (0-based)
    struct cJSON* current_item; // Current item data
    const char* item_name;      // Loop variable name for binding resolution
    const char* index_name;     // Index variable name for binding resolution
} IRForContext;

// ============================================================================
// Main Expansion API
// ============================================================================

// Expand all For components in a tree (entry point)
// This recursively walks the tree and expands For nodes in-place
// source_structures can be NULL for backward compatibility
void ir_for_expand_tree(struct IRComponent* root, struct IRSourceStructures* source_structures);

// Expand a single For component, returning array of expanded children
// Returns number of children created, stores them in *out_children
// Caller is responsible for integrating children into parent
// source_structures can be NULL for backward compatibility
uint32_t ir_for_expand_single(struct IRComponent* for_comp,
                               struct IRSourceStructures* source_structures,
                               struct IRComponent*** out_children);

// ============================================================================
// Binding Resolution
// ============================================================================

// Resolve a binding expression to a string value given iteration context
// Returns strdup'd string, caller must free
// Handles: "item.field", "index", "item.nested.field"
char* ir_for_resolve_binding(const char* expression,
                              IRForContext* ctx);

// Apply all bindings from a For definition to a component copy
void ir_for_apply_bindings(struct IRComponent* component,
                            IRForDef* def,
                            IRForContext* ctx);

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

// Replace a For component in its parent with expanded children
// The For component itself is removed (layout-transparent)
void ir_for_replace_in_parent(struct IRComponent* parent,
                               int for_index,
                               struct IRComponent** expanded_children,
                               uint32_t expanded_count);

// ============================================================================
// Property Setters (for binding application)
// ============================================================================

// Set a component property by path (e.g., "text_content", "style.opacity")
// Returns true if property was set successfully
bool ir_for_set_property(struct IRComponent* component,
                          const char* property_path,
                          const char* value);

#endif // IR_FOR_EXPAND_H
