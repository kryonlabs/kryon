#ifndef IR_FOR_H
#define IR_FOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
struct IRComponent;
struct cJSON;
struct IRSourceStructures;

// ============================================================================
// For Loop Data Model (Unified for compile-time and runtime)
// ============================================================================
// Modular data structures for template-based loop rendering.
// Supports declarative property bindings instead of hardcoded field names.
// The distinction between compile-time and runtime is now a property (is_compile_time)
// rather than a different component type.

// Property binding specification - maps template properties to iteration data
typedef struct IRForBinding {
    char* target_property;      // Target property path (e.g., "text_content", "style.opacity")
    char* source_expression;    // Source expression (e.g., "item.name", "index + 1")
    bool is_computed;           // True if requires expression evaluation (e.g., ternary)
} IRForBinding;

// Template specification for For instantiation
typedef struct IRForTemplate {
    struct IRComponent* component;  // Template component tree (deep copied for each item)
    IRForBinding* bindings;         // Array of property bindings
    uint32_t binding_count;
    uint32_t binding_capacity;
} IRForTemplate;

// Loop type enum - distinguishes different loop syntaxes
typedef enum {
    IR_LOOP_TYPE_FOR_IN = 0,        // for item in collection { }
    IR_LOOP_TYPE_FOR_RANGE,         // for i in 0..10 { }
    IR_LOOP_TYPE_FOR_EACH           // for each item in collection { } (explicit runtime)
} IRLoopType;

// Data source types
typedef enum {
    FOR_SOURCE_NONE = 0,            // Not set
    FOR_SOURCE_LITERAL,             // Inline JSON array (data embedded in KIR)
    FOR_SOURCE_VARIABLE,            // Variable reference (e.g., "state.items")
    FOR_SOURCE_EXPRESSION           // Computed expression (requires runtime evaluation)
} IRForSourceType;

// Data source specification
typedef struct IRForDataSource {
    IRForSourceType type;
    char* expression;               // Variable name or expression string
    char* literal_json;             // Serialized JSON for literal data
} IRForDataSource;

// Complete For definition - handles ALL loop types (compile-time and runtime)
typedef struct IRForDef {
    IRLoopType loop_type;           // Type of loop (for_in, for_range, for_each)
    char* item_name;                // Loop variable name (e.g., "day", "item")
    char* index_name;               // Index variable name (e.g., "i", "index")
    IRForDataSource source;         // Data source specification
    IRForTemplate tmpl;             // Template with bindings (named 'tmpl' to avoid C++ keyword)

    // NEW: Unified compile-time flag
    bool is_compile_time;           // true = expand at compile, false = runtime

    // Runtime metadata (for reactive loops)
    char* reactive_var_id;          // If runtime, which reactive var? (NULL for compile-time)
} IRForDef;

// ============================================================================
// Builder API
// ============================================================================

// Create/destroy For definition
IRForDef* ir_for_def_create(const char* item_name, const char* index_name);
void ir_for_def_destroy(IRForDef* def);

// Data source configuration
void ir_for_set_source_literal(IRForDef* def, const char* json_array);
void ir_for_set_source_variable(IRForDef* def, const char* var_name);
void ir_for_set_source_expression(IRForDef* def, const char* expr);

// Template configuration
void ir_for_set_template(IRForDef* def, struct IRComponent* template_component);
void ir_for_add_binding(IRForDef* def,
                        const char* target_property,
                        const char* source_expression,
                        bool is_computed);
void ir_for_clear_bindings(IRForDef* def);

// ============================================================================
// Serialization API
// ============================================================================

// Convert to/from cJSON for KIR serialization
struct cJSON* ir_for_def_to_json(IRForDef* def);
IRForDef* ir_for_def_from_json(struct cJSON* json);

// Binding serialization helpers
struct cJSON* ir_for_binding_to_json(IRForBinding* binding);
IRForBinding* ir_for_binding_from_json(struct cJSON* json);

// ============================================================================
// Utility Functions
// ============================================================================

// Deep copy a For definition
IRForDef* ir_for_def_copy(IRForDef* src);

// Check if For needs runtime data (variable or expression source)
bool ir_for_needs_runtime_data(IRForDef* def);

// Get source data as cJSON array (parses literal_json if needed)
struct cJSON* ir_for_get_source_data(IRForDef* def);

// Resolve variable source from const declarations in source_structures
// Returns cJSON array that caller must delete with cJSON_Delete()
struct cJSON* ir_for_resolve_variable_source(IRForDef* def, struct IRSourceStructures* source_structures);

#endif // IR_FOR_H
