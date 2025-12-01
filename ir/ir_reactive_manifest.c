/*
 * Kryon IR Reactive Manifest System
 * Manages reactive state for hot reload and state persistence
 */

#define _POSIX_C_SOURCE 200809L  // For strdup

#include "ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_VAR_CAPACITY 16
#define INITIAL_BINDING_CAPACITY 32
#define INITIAL_CONDITIONAL_CAPACITY 16
#define INITIAL_FOR_LOOP_CAPACITY 8

// ============================================================================
// Reactive Manifest Creation/Destruction
// ============================================================================

IRReactiveManifest* ir_reactive_manifest_create(void) {
    IRReactiveManifest* manifest = malloc(sizeof(IRReactiveManifest));
    if (!manifest) return NULL;

    // Initialize variables array
    manifest->variables = malloc(sizeof(IRReactiveVarDescriptor) * INITIAL_VAR_CAPACITY);
    manifest->variable_count = 0;
    manifest->variable_capacity = INITIAL_VAR_CAPACITY;

    // Initialize bindings array
    manifest->bindings = malloc(sizeof(IRReactiveBinding) * INITIAL_BINDING_CAPACITY);
    manifest->binding_count = 0;
    manifest->binding_capacity = INITIAL_BINDING_CAPACITY;

    // Initialize conditionals array
    manifest->conditionals = malloc(sizeof(IRReactiveConditional) * INITIAL_CONDITIONAL_CAPACITY);
    manifest->conditional_count = 0;
    manifest->conditional_capacity = INITIAL_CONDITIONAL_CAPACITY;

    // Initialize for-loops array
    manifest->for_loops = malloc(sizeof(IRReactiveForLoop) * INITIAL_FOR_LOOP_CAPACITY);
    manifest->for_loop_count = 0;
    manifest->for_loop_capacity = INITIAL_FOR_LOOP_CAPACITY;

    // Initialize metadata
    manifest->next_var_id = 1;
    manifest->format_version = 1;

    if (!manifest->variables || !manifest->bindings ||
        !manifest->conditionals || !manifest->for_loops) {
        ir_reactive_manifest_destroy(manifest);
        return NULL;
    }

    return manifest;
}

void ir_reactive_manifest_destroy(IRReactiveManifest* manifest) {
    if (!manifest) return;

    // Free variables
    if (manifest->variables) {
        for (uint32_t i = 0; i < manifest->variable_count; i++) {
            IRReactiveVarDescriptor* var = &manifest->variables[i];
            free(var->name);
            free(var->source_location);
            if (var->type == IR_REACTIVE_TYPE_STRING && var->value.as_string) {
                free(var->value.as_string);
            }
        }
        free(manifest->variables);
    }

    // Free bindings
    if (manifest->bindings) {
        for (uint32_t i = 0; i < manifest->binding_count; i++) {
            IRReactiveBinding* binding = &manifest->bindings[i];
            free(binding->expression);
            free(binding->update_code);
        }
        free(manifest->bindings);
    }

    // Free conditionals
    if (manifest->conditionals) {
        for (uint32_t i = 0; i < manifest->conditional_count; i++) {
            IRReactiveConditional* cond = &manifest->conditionals[i];
            free(cond->condition);
            free(cond->dependent_var_ids);
        }
        free(manifest->conditionals);
    }

    // Free for-loops
    if (manifest->for_loops) {
        for (uint32_t i = 0; i < manifest->for_loop_count; i++) {
            IRReactiveForLoop* loop = &manifest->for_loops[i];
            free(loop->collection_expr);
            free(loop->item_template);
            free(loop->child_component_ids);
        }
        free(manifest->for_loops);
    }

    free(manifest);
}

// ============================================================================
// Add Reactive Variables
// ============================================================================

uint32_t ir_reactive_manifest_add_var(IRReactiveManifest* manifest,
                                      const char* name,
                                      IRReactiveVarType type,
                                      IRReactiveValue value) {
    if (!manifest) return 0;

    // Expand capacity if needed
    if (manifest->variable_count >= manifest->variable_capacity) {
        manifest->variable_capacity *= 2;
        IRReactiveVarDescriptor* new_vars = realloc(manifest->variables,
            sizeof(IRReactiveVarDescriptor) * manifest->variable_capacity);
        if (!new_vars) return 0;
        manifest->variables = new_vars;
    }

    // Create new variable descriptor
    IRReactiveVarDescriptor* var = &manifest->variables[manifest->variable_count];
    var->id = manifest->next_var_id++;
    var->name = name ? strdup(name) : NULL;
    var->type = type;
    var->version = 0;
    var->source_location = NULL;
    var->binding_count = 0;

    // Copy value (deep copy for strings)
    if (type == IR_REACTIVE_TYPE_STRING && value.as_string) {
        var->value.as_string = strdup(value.as_string);
    } else {
        var->value = value;
    }

    manifest->variable_count++;
    return var->id;
}

// ============================================================================
// Add Bindings
// ============================================================================

void ir_reactive_manifest_add_binding(IRReactiveManifest* manifest,
                                      uint32_t component_id,
                                      uint32_t reactive_var_id,
                                      IRBindingType binding_type,
                                      const char* expression) {
    if (!manifest) return;

    // Expand capacity if needed
    if (manifest->binding_count >= manifest->binding_capacity) {
        manifest->binding_capacity *= 2;
        IRReactiveBinding* new_bindings = realloc(manifest->bindings,
            sizeof(IRReactiveBinding) * manifest->binding_capacity);
        if (!new_bindings) return;
        manifest->bindings = new_bindings;
    }

    // Create new binding
    IRReactiveBinding* binding = &manifest->bindings[manifest->binding_count];
    binding->component_id = component_id;
    binding->reactive_var_id = reactive_var_id;
    binding->binding_type = binding_type;
    binding->expression = expression ? strdup(expression) : NULL;
    binding->update_code = NULL;

    manifest->binding_count++;

    // Update binding count in variable descriptor
    IRReactiveVarDescriptor* var = ir_reactive_manifest_get_var(manifest, reactive_var_id);
    if (var) {
        var->binding_count++;
    }
}

// ============================================================================
// Add Conditionals
// ============================================================================

void ir_reactive_manifest_add_conditional(IRReactiveManifest* manifest,
                                         uint32_t component_id,
                                         const char* condition,
                                         const uint32_t* dependent_var_ids,
                                         uint32_t dependent_var_count) {
    if (!manifest) return;

    // Expand capacity if needed
    if (manifest->conditional_count >= manifest->conditional_capacity) {
        manifest->conditional_capacity *= 2;
        IRReactiveConditional* new_conds = realloc(manifest->conditionals,
            sizeof(IRReactiveConditional) * manifest->conditional_capacity);
        if (!new_conds) return;
        manifest->conditionals = new_conds;
    }

    // Create new conditional
    IRReactiveConditional* cond = &manifest->conditionals[manifest->conditional_count];
    cond->component_id = component_id;
    cond->condition = condition ? strdup(condition) : NULL;
    cond->last_eval_result = false;
    cond->suspended = false;

    // Copy dependent var IDs
    if (dependent_var_count > 0 && dependent_var_ids) {
        cond->dependent_var_ids = malloc(sizeof(uint32_t) * dependent_var_count);
        if (cond->dependent_var_ids) {
            memcpy(cond->dependent_var_ids, dependent_var_ids,
                   sizeof(uint32_t) * dependent_var_count);
            cond->dependent_var_count = dependent_var_count;
        } else {
            cond->dependent_var_count = 0;
        }
    } else {
        cond->dependent_var_ids = NULL;
        cond->dependent_var_count = 0;
    }

    manifest->conditional_count++;
}

// ============================================================================
// Add For-Loops
// ============================================================================

void ir_reactive_manifest_add_for_loop(IRReactiveManifest* manifest,
                                      uint32_t parent_component_id,
                                      const char* collection_expr,
                                      uint32_t collection_var_id) {
    if (!manifest) return;

    // Expand capacity if needed
    if (manifest->for_loop_count >= manifest->for_loop_capacity) {
        manifest->for_loop_capacity *= 2;
        IRReactiveForLoop* new_loops = realloc(manifest->for_loops,
            sizeof(IRReactiveForLoop) * manifest->for_loop_capacity);
        if (!new_loops) return;
        manifest->for_loops = new_loops;
    }

    // Create new for-loop
    IRReactiveForLoop* loop = &manifest->for_loops[manifest->for_loop_count];
    loop->parent_component_id = parent_component_id;
    loop->collection_expr = collection_expr ? strdup(collection_expr) : NULL;
    loop->collection_var_id = collection_var_id;
    loop->item_template = NULL;
    loop->child_component_ids = NULL;
    loop->child_count = 0;

    manifest->for_loop_count++;
}

// ============================================================================
// Query Functions
// ============================================================================

IRReactiveVarDescriptor* ir_reactive_manifest_find_var(IRReactiveManifest* manifest,
                                                       const char* name) {
    if (!manifest || !name) return NULL;

    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        if (var->name && strcmp(var->name, name) == 0) {
            return var;
        }
    }

    return NULL;
}

IRReactiveVarDescriptor* ir_reactive_manifest_get_var(IRReactiveManifest* manifest,
                                                      uint32_t var_id) {
    if (!manifest) return NULL;

    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        if (var->id == var_id) {
            return var;
        }
    }

    return NULL;
}

// ============================================================================
// Update Functions
// ============================================================================

bool ir_reactive_manifest_update_var(IRReactiveManifest* manifest,
                                     uint32_t var_id,
                                     IRReactiveValue new_value) {
    if (!manifest) return false;

    IRReactiveVarDescriptor* var = ir_reactive_manifest_get_var(manifest, var_id);
    if (!var) return false;

    // Free old string if replacing
    if (var->type == IR_REACTIVE_TYPE_STRING && var->value.as_string) {
        free(var->value.as_string);
    }

    // Update value (deep copy for strings)
    if (var->type == IR_REACTIVE_TYPE_STRING && new_value.as_string) {
        var->value.as_string = strdup(new_value.as_string);
    } else {
        var->value = new_value;
    }

    var->version++;
    return true;
}

// ============================================================================
// Debug/Print Functions
// ============================================================================

static const char* reactive_type_to_string(IRReactiveVarType type) {
    switch (type) {
        case IR_REACTIVE_TYPE_INT: return "int";
        case IR_REACTIVE_TYPE_FLOAT: return "float";
        case IR_REACTIVE_TYPE_STRING: return "string";
        case IR_REACTIVE_TYPE_BOOL: return "bool";
        case IR_REACTIVE_TYPE_CUSTOM: return "custom";
        default: return "unknown";
    }
}

static const char* binding_type_to_string(IRBindingType type) {
    switch (type) {
        case IR_BINDING_TEXT: return "text";
        case IR_BINDING_CONDITIONAL: return "conditional";
        case IR_BINDING_ATTRIBUTE: return "attribute";
        case IR_BINDING_FOR_LOOP: return "for_loop";
        case IR_BINDING_CUSTOM: return "custom";
        default: return "unknown";
    }
}

void ir_reactive_manifest_print(IRReactiveManifest* manifest) {
    if (!manifest) {
        printf("Reactive Manifest: NULL\n");
        return;
    }

    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Reactive Manifest (v%u)                                   ║\n", manifest->format_version);
    printf("╠═══════════════════════════════════════════════════════════╣\n");

    // Print variables
    printf("║  Variables: %u                                             ║\n", manifest->variable_count);
    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        printf("║    [%u] %s: %s", var->id,
               var->name ? var->name : "<unnamed>",
               reactive_type_to_string(var->type));

        // Print value
        switch (var->type) {
            case IR_REACTIVE_TYPE_INT:
                printf(" = %ld", var->value.as_int);
                break;
            case IR_REACTIVE_TYPE_FLOAT:
                printf(" = %.2f", var->value.as_float);
                break;
            case IR_REACTIVE_TYPE_STRING:
                printf(" = \"%s\"", var->value.as_string ? var->value.as_string : "");
                break;
            case IR_REACTIVE_TYPE_BOOL:
                printf(" = %s", var->value.as_bool ? "true" : "false");
                break;
            default:
                break;
        }

        printf(" (v%u, %u bindings)\n", var->version, var->binding_count);
    }

    // Print bindings
    printf("║  Bindings: %u                                              ║\n", manifest->binding_count);
    for (uint32_t i = 0; i < manifest->binding_count; i++) {
        IRReactiveBinding* binding = &manifest->bindings[i];
        printf("║    Component #%u -> Var #%u (%s)%s%s\n",
               binding->component_id,
               binding->reactive_var_id,
               binding_type_to_string(binding->binding_type),
               binding->expression ? " expr: " : "",
               binding->expression ? binding->expression : "");
    }

    // Print conditionals
    printf("║  Conditionals: %u                                          ║\n", manifest->conditional_count);
    for (uint32_t i = 0; i < manifest->conditional_count; i++) {
        IRReactiveConditional* cond = &manifest->conditionals[i];
        printf("║    Component #%u: %s (depends on %u vars)\n",
               cond->component_id,
               cond->condition ? cond->condition : "<no condition>",
               cond->dependent_var_count);
    }

    // Print for-loops
    printf("║  For-Loops: %u                                             ║\n", manifest->for_loop_count);
    for (uint32_t i = 0; i < manifest->for_loop_count; i++) {
        IRReactiveForLoop* loop = &manifest->for_loops[i];
        printf("║    Parent #%u: %s (var #%u, %u children)\n",
               loop->parent_component_id,
               loop->collection_expr ? loop->collection_expr : "<no expr>",
               loop->collection_var_id,
               loop->child_count);
    }

    printf("╚═══════════════════════════════════════════════════════════╝\n");
}
