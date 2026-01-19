#ifndef IR_MODULE_REFS_H
#define IR_MODULE_REFS_H

#include <stdint.h>
#include <stdbool.h>

#include "ir_core.h"

// Forward declaration for cJSON (used in module reference functions)
typedef struct cJSON cJSON;

// ============================================================================
// Module Reference Management (for cross-file component references)
// ============================================================================

void ir_set_component_module_ref(IRComponent* component, const char* module_ref, const char* export_name);

// Recursively clear module_ref for a component tree (for serialization)
// Returns a cJSON array with the old values for each component that had module_ref set
cJSON* ir_clear_tree_module_refs(IRComponent* component);

// Restore module_ref from the refs array returned by ir_clear_tree_module_refs
void ir_restore_tree_module_refs(IRComponent* component, cJSON* refs_array);

// JSON string wrapper for ir_clear_tree_module_refs (for FFI compatibility)
// Returns a JSON string that must be freed by the caller
char* ir_clear_tree_module_refs_json(IRComponent* component);

// JSON string wrapper for ir_restore_tree_module_refs (for FFI compatibility)
void ir_restore_tree_module_refs_json(IRComponent* component, const char* json_str);

// Temporarily clear module_ref for serialization (returns old values for restoration)
// DEPRECATED: Use ir_clear_tree_module_refs instead
char* ir_clear_component_module_ref(IRComponent* component);

// Restore module_ref from the JSON string returned by ir_clear_component_module_ref
// DEPRECATED: Use ir_restore_tree_module_refs instead
void ir_restore_component_module_ref(IRComponent* component, const char* json_str);

#endif // IR_MODULE_REFS_H
