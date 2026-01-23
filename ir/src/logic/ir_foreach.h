#ifndef IR_FOREACH_H
#define IR_FOREACH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
struct IRComponent;
struct cJSON;

// ============================================================================
// ForEach Data Model
// ============================================================================
// Modular data structures for template-based dynamic list rendering.
// Supports declarative property bindings instead of hardcoded field names.

// Property binding specification - maps template properties to iteration data
typedef struct IRForEachBinding {
    char* target_property;      // Target property path (e.g., "text_content", "style.opacity")
    char* source_expression;    // Source expression (e.g., "item.name", "index + 1")
    bool is_computed;           // True if requires expression evaluation (e.g., ternary)
} IRForEachBinding;

// Template specification for ForEach instantiation
typedef struct IRForEachTemplate {
    struct IRComponent* component;  // Template component tree (deep copied for each item)
    IRForEachBinding* bindings;     // Array of property bindings
    uint32_t binding_count;
    uint32_t binding_capacity;
} IRForEachTemplate;

// Loop type enum - distinguishes different loop syntaxes
typedef enum {
    IR_LOOP_TYPE_FOR_IN = 0,        // for item in collection { }
    IR_LOOP_TYPE_FOR_RANGE,         // for i in 0..10 { }
    IR_LOOP_TYPE_FOR_EACH           // for each item in collection { } (explicit runtime)
} IRLoopType;

// Data source types
typedef enum {
    FOREACH_SOURCE_NONE = 0,        // Not set
    FOREACH_SOURCE_LITERAL,         // Inline JSON array (data embedded in KIR)
    FOREACH_SOURCE_VARIABLE,        // Variable reference (e.g., "state.items")
    FOREACH_SOURCE_EXPRESSION       // Computed expression (requires runtime evaluation)
} IRForEachSourceType;

// Data source specification
typedef struct IRForEachDataSource {
    IRForEachSourceType type;
    char* expression;               // Variable name or expression string
    char* literal_json;             // Serialized JSON for literal data
} IRForEachDataSource;

// Complete ForEach definition
typedef struct IRForEachDef {
    IRLoopType loop_type;           // Type of loop (for_in, for_range, for_each)
    char* item_name;                // Loop variable name (e.g., "day", "item")
    char* index_name;               // Index variable name (e.g., "i", "index")
    IRForEachDataSource source;     // Data source specification
    IRForEachTemplate tmpl;         // Template with bindings (named 'tmpl' to avoid C++ keyword)
} IRForEachDef;

// ============================================================================
// Builder API
// ============================================================================

// Create/destroy ForEach definition
IRForEachDef* ir_foreach_def_create(const char* item_name, const char* index_name);
void ir_foreach_def_destroy(IRForEachDef* def);

// Data source configuration
void ir_foreach_set_source_literal(IRForEachDef* def, const char* json_array);
void ir_foreach_set_source_variable(IRForEachDef* def, const char* var_name);
void ir_foreach_set_source_expression(IRForEachDef* def, const char* expr);

// Template configuration
void ir_foreach_set_template(IRForEachDef* def, struct IRComponent* template_component);
void ir_foreach_add_binding(IRForEachDef* def,
                            const char* target_property,
                            const char* source_expression,
                            bool is_computed);
void ir_foreach_clear_bindings(IRForEachDef* def);

// ============================================================================
// Serialization API
// ============================================================================

// Convert to/from cJSON for KIR serialization
struct cJSON* ir_foreach_def_to_json(IRForEachDef* def);
IRForEachDef* ir_foreach_def_from_json(struct cJSON* json);

// Binding serialization helpers
struct cJSON* ir_foreach_binding_to_json(IRForEachBinding* binding);
IRForEachBinding* ir_foreach_binding_from_json(struct cJSON* json);

// ============================================================================
// Utility Functions
// ============================================================================

// Deep copy a ForEach definition
IRForEachDef* ir_foreach_def_copy(IRForEachDef* src);

// Check if ForEach needs runtime data (variable or expression source)
bool ir_foreach_needs_runtime_data(IRForEachDef* def);

// Get source data as cJSON array (parses literal_json if needed)
struct cJSON* ir_foreach_get_source_data(IRForEachDef* def);

#endif // IR_FOREACH_H
