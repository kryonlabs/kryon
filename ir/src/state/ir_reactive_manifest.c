/**
 * Kryon Reactive Manifest Implementation
 * Manages reactive state for UI hot reload and serialization
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/ir_core.h"
#include "../utils/ir_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Reactive Manifest Functions
// ============================================================================

IRReactiveManifest* ir_reactive_manifest_create(void) {
    IRReactiveManifest* manifest = calloc(1, sizeof(IRReactiveManifest));
    if (!manifest) {
        fprintf(stderr, "[ReactiveManifest] Failed to allocate manifest\n");
        return NULL;
    }

    manifest->variable_capacity = 16;
    manifest->binding_capacity = 32;
    manifest->conditional_capacity = 16;
    manifest->for_loop_capacity = 16;
    manifest->component_def_capacity = 8;
    manifest->source_capacity = 4;
    manifest->computed_property_capacity = 8;
    manifest->action_capacity = 16;
    manifest->watcher_capacity = 8;

    manifest->variables = calloc(manifest->variable_capacity, sizeof(IRReactiveVarDescriptor));
    manifest->bindings = calloc(manifest->binding_capacity, sizeof(IRReactiveBinding));
    manifest->conditionals = calloc(manifest->conditional_capacity, sizeof(IRReactiveConditional));
    manifest->for_loops = calloc(manifest->for_loop_capacity, sizeof(IRReactiveForLoop));
    manifest->component_defs = calloc(manifest->component_def_capacity, sizeof(IRComponentDefinition));
    manifest->sources = calloc(manifest->source_capacity, sizeof(IRSourceEntry));
    manifest->computed_properties = calloc(manifest->computed_property_capacity, sizeof(IRComputedProperty));
    manifest->actions = calloc(manifest->action_capacity, sizeof(IRAction));
    manifest->watchers = calloc(manifest->watcher_capacity, sizeof(IRWatcher));

    if (!manifest->variables || !manifest->bindings || !manifest->conditionals ||
        !manifest->for_loops || !manifest->component_defs || !manifest->sources ||
        !manifest->computed_properties || !manifest->actions || !manifest->watchers) {
        fprintf(stderr, "[ReactiveManifest] Failed to allocate internal arrays\n");
        ir_reactive_manifest_destroy(manifest);
        return NULL;
    }

    manifest->next_var_id = 1;
    manifest->next_computed_id = 1;
    manifest->next_action_id = 1;
    manifest->next_watcher_id = 1;

    return manifest;
}

void ir_reactive_manifest_destroy(IRReactiveManifest* manifest) {
    if (!manifest) return;

    // Free variable strings
    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        free(manifest->variables[i].name);
        free(manifest->variables[i].source_location);
        free(manifest->variables[i].type_string);
        free(manifest->variables[i].initial_value_json);
        free(manifest->variables[i].scope);
        if (manifest->variables[i].type == IR_REACTIVE_TYPE_STRING) {
            free(manifest->variables[i].value.as_string);
        }
    }

    // Free binding strings
    for (uint32_t i = 0; i < manifest->binding_count; i++) {
        free(manifest->bindings[i].expression);
        free(manifest->bindings[i].update_code);
    }

    // Free conditional strings and arrays
    for (uint32_t i = 0; i < manifest->conditional_count; i++) {
        free(manifest->conditionals[i].condition);
        free(manifest->conditionals[i].dependent_var_ids);
        free(manifest->conditionals[i].then_children_ids);
        free(manifest->conditionals[i].else_children_ids);
    }

    // Free for-loop strings and arrays
    for (uint32_t i = 0; i < manifest->for_loop_count; i++) {
        free(manifest->for_loops[i].collection_expr);
        free(manifest->for_loops[i].item_template);
        free(manifest->for_loops[i].child_component_ids);
    }

    // Free component definitions
    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        IRComponentDefinition* def = &manifest->component_defs[i];
        free(def->name);
        for (uint32_t j = 0; j < def->prop_count; j++) {
            free(def->props[j].name);
            free(def->props[j].type);
            free(def->props[j].default_value);
        }
        free(def->props);
        for (uint32_t j = 0; j < def->state_var_count; j++) {
            free(def->state_vars[j].name);
            free(def->state_vars[j].type);
            free(def->state_vars[j].initial_expr);
        }
        free(def->state_vars);
        // Note: template_root is part of component tree, freed separately
    }

    // Free source entries
    for (uint32_t i = 0; i < manifest->source_count; i++) {
        free(manifest->sources[i].lang);
        free(manifest->sources[i].code);
    }

    // Free computed properties
    for (uint32_t i = 0; i < manifest->computed_property_count; i++) {
        IRComputedProperty* computed = &manifest->computed_properties[i];
        free(computed->name);
        free(computed->function_name);
        for (uint32_t j = 0; j < computed->dependency_count; j++) {
            free(computed->dependencies[j]);
        }
        free(computed->dependencies);
        if (computed->state == IR_COMPUTED_STATE_VALID &&
            computed->cached_value.as_string) {
            free(computed->cached_value.as_string);
        }
        free(computed->source_lua);
        free(computed->source_js);
    }

    // Free actions
    for (uint32_t i = 0; i < manifest->action_count; i++) {
        IRAction* action = &manifest->actions[i];
        free(action->name);
        free(action->function_name);
        for (uint32_t j = 0; j < action->watch_path_count; j++) {
            free(action->watch_paths[j]);
        }
        free(action->watch_paths);
        free(action->source_lua);
        free(action->source_js);
    }

    // Free watchers
    for (uint32_t i = 0; i < manifest->watcher_count; i++) {
        IRWatcher* watcher = &manifest->watchers[i];
        free(watcher->watched_path);
        free(watcher->handler_function);
        free(watcher->watched_var_ids);
        free(watcher->source_lua);
        free(watcher->source_js);
    }

    free(manifest->variables);
    free(manifest->bindings);
    free(manifest->conditionals);
    free(manifest->for_loops);
    free(manifest->component_defs);
    free(manifest->sources);
    free(manifest->computed_properties);
    free(manifest->actions);
    free(manifest->watchers);
    free(manifest);
}

uint32_t ir_reactive_manifest_add_var(IRReactiveManifest* manifest,
                                      const char* name,
                                      IRReactiveVarType type,
                                      IRReactiveValue value) {
    if (!manifest || !name) return 0;

    // Resize if needed
    if (manifest->variable_count >= manifest->variable_capacity) {
        if (!ir_grow_capacity(&manifest->variable_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow variables array - would overflow\n");
            return 0;
        }
        IRReactiveVarDescriptor* new_vars = realloc(manifest->variables,
                                                     manifest->variable_capacity * sizeof(IRReactiveVarDescriptor));
        if (!new_vars) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize variables array\n");
            return 0;
        }
        manifest->variables = new_vars;
    }

    uint32_t var_id = manifest->next_var_id++;
    IRReactiveVarDescriptor* var = &manifest->variables[manifest->variable_count++];

    memset(var, 0, sizeof(IRReactiveVarDescriptor));
    var->id = var_id;
    var->name = strdup(name);
    var->type = type;
    var->value = value;
    var->version = 0;

    // Copy string value if needed
    if (type == IR_REACTIVE_TYPE_STRING && value.as_string) {
        var->value.as_string = strdup(value.as_string);
    }

    return var_id;
}

void ir_reactive_manifest_set_var_metadata(IRReactiveManifest* manifest,
                                           uint32_t var_id,
                                           const char* type_string,
                                           const char* initial_value_json,
                                           const char* scope) {
    if (!manifest) return;

    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        if (manifest->variables[i].id == var_id) {
            IRReactiveVarDescriptor* var = &manifest->variables[i];

            if (type_string) {
                free(var->type_string);
                var->type_string = strdup(type_string);
            }

            if (initial_value_json) {
                free(var->initial_value_json);
                var->initial_value_json = strdup(initial_value_json);
            }

            if (scope) {
                free(var->scope);
                var->scope = strdup(scope);
            }

            return;
        }
    }

    fprintf(stderr, "[ReactiveManifest] Warning: Variable ID %u not found\n", var_id);
}

void ir_reactive_manifest_add_binding(IRReactiveManifest* manifest,
                                      uint32_t component_id,
                                      uint32_t reactive_var_id,
                                      IRBindingType binding_type,
                                      const char* expression) {
    if (!manifest) return;

    // Resize if needed
    if (manifest->binding_count >= manifest->binding_capacity) {
        if (!ir_grow_capacity(&manifest->binding_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow bindings array - would overflow\n");
            return;
        }
        IRReactiveBinding* new_bindings = realloc(manifest->bindings,
                                                   manifest->binding_capacity * sizeof(IRReactiveBinding));
        if (!new_bindings) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize bindings array\n");
            return;
        }
        manifest->bindings = new_bindings;
    }

    IRReactiveBinding* binding = &manifest->bindings[manifest->binding_count++];
    binding->component_id = component_id;
    binding->reactive_var_id = reactive_var_id;
    binding->binding_type = binding_type;
    binding->expression = expression ? strdup(expression) : NULL;
    binding->update_code = NULL;
}

void ir_reactive_manifest_add_conditional(IRReactiveManifest* manifest,
                                         uint32_t component_id,
                                         const char* condition,
                                         const uint32_t* dependent_var_ids,
                                         uint32_t dependent_var_count) {
    if (!manifest) return;

    // Resize if needed
    if (manifest->conditional_count >= manifest->conditional_capacity) {
        if (!ir_grow_capacity(&manifest->conditional_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow conditionals array - would overflow\n");
            return;
        }
        IRReactiveConditional* new_conds = realloc(manifest->conditionals,
                                                    manifest->conditional_capacity * sizeof(IRReactiveConditional));
        if (!new_conds) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize conditionals array\n");
            return;
        }
        manifest->conditionals = new_conds;
    }

    IRReactiveConditional* cond = &manifest->conditionals[manifest->conditional_count++];
    cond->component_id = component_id;
    cond->condition = condition ? strdup(condition) : NULL;
    cond->last_eval_result = false;
    cond->suspended = false;
    cond->then_children_ids = NULL;
    cond->then_children_count = 0;
    cond->else_children_ids = NULL;
    cond->else_children_count = 0;

    if (dependent_var_count > 0 && dependent_var_ids) {
        cond->dependent_var_ids = malloc(dependent_var_count * sizeof(uint32_t));
        if (cond->dependent_var_ids) {
            memcpy(cond->dependent_var_ids, dependent_var_ids, dependent_var_count * sizeof(uint32_t));
            cond->dependent_var_count = dependent_var_count;
        } else {
            cond->dependent_var_count = 0;
        }
    } else {
        cond->dependent_var_ids = NULL;
        cond->dependent_var_count = 0;
    }
}

void ir_reactive_manifest_set_conditional_branches(IRReactiveManifest* manifest,
                                                   uint32_t component_id,
                                                   const uint32_t* then_children_ids,
                                                   uint32_t then_children_count,
                                                   const uint32_t* else_children_ids,
                                                   uint32_t else_children_count) {
    if (!manifest) return;

    // Find the conditional with matching component_id
    for (uint32_t i = 0; i < manifest->conditional_count; i++) {
        IRReactiveConditional* cond = &manifest->conditionals[i];
        if (cond->component_id == component_id) {
            // Free existing arrays
            free(cond->then_children_ids);
            free(cond->else_children_ids);

            // Set then children
            if (then_children_count > 0 && then_children_ids) {
                cond->then_children_ids = malloc(then_children_count * sizeof(uint32_t));
                if (cond->then_children_ids) {
                    memcpy(cond->then_children_ids, then_children_ids, then_children_count * sizeof(uint32_t));
                    cond->then_children_count = then_children_count;
                } else {
                    cond->then_children_count = 0;
                }
            } else {
                cond->then_children_ids = NULL;
                cond->then_children_count = 0;
            }

            // Set else children
            if (else_children_count > 0 && else_children_ids) {
                cond->else_children_ids = malloc(else_children_count * sizeof(uint32_t));
                if (cond->else_children_ids) {
                    memcpy(cond->else_children_ids, else_children_ids, else_children_count * sizeof(uint32_t));
                    cond->else_children_count = else_children_count;
                } else {
                    cond->else_children_count = 0;
                }
            } else {
                cond->else_children_ids = NULL;
                cond->else_children_count = 0;
            }

            return;
        }
    }

    fprintf(stderr, "[ReactiveManifest] Warning: No conditional found with component_id %u\n", component_id);
}

void ir_reactive_manifest_add_for_loop(IRReactiveManifest* manifest,
                                      uint32_t parent_component_id,
                                      const char* collection_expr,
                                      uint32_t collection_var_id) {
    if (!manifest) return;

    // Resize if needed
    if (manifest->for_loop_count >= manifest->for_loop_capacity) {
        if (!ir_grow_capacity(&manifest->for_loop_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow for_loops array - would overflow\n");
            return;
        }
        IRReactiveForLoop* new_loops = realloc(manifest->for_loops,
                                               manifest->for_loop_capacity * sizeof(IRReactiveForLoop));
        if (!new_loops) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize for_loops array\n");
            return;
        }
        manifest->for_loops = new_loops;
    }

    IRReactiveForLoop* loop = &manifest->for_loops[manifest->for_loop_count++];
    loop->parent_component_id = parent_component_id;
    loop->collection_expr = collection_expr ? strdup(collection_expr) : NULL;
    loop->collection_var_id = collection_var_id;
    loop->item_template = NULL;
    loop->child_component_ids = NULL;
    loop->child_count = 0;
}

IRReactiveVarDescriptor* ir_reactive_manifest_find_var(IRReactiveManifest* manifest,
                                                       const char* name) {
    if (!manifest || !name) return NULL;

    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        if (manifest->variables[i].name && strcmp(manifest->variables[i].name, name) == 0) {
            return &manifest->variables[i];
        }
    }

    return NULL;
}

IRReactiveVarDescriptor* ir_reactive_manifest_get_var(IRReactiveManifest* manifest,
                                                      uint32_t var_id) {
    if (!manifest) return NULL;

    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        if (manifest->variables[i].id == var_id) {
            return &manifest->variables[i];
        }
    }

    return NULL;
}

bool ir_reactive_manifest_update_var(IRReactiveManifest* manifest,
                                     uint32_t var_id,
                                     IRReactiveValue new_value) {
    IRReactiveVarDescriptor* var = ir_reactive_manifest_get_var(manifest, var_id);
    if (!var) return false;

    // Free old string value if replacing
    if (var->type == IR_REACTIVE_TYPE_STRING && var->value.as_string) {
        free(var->value.as_string);
    }

    var->value = new_value;

    // Copy new string value if needed
    if (var->type == IR_REACTIVE_TYPE_STRING && new_value.as_string) {
        var->value.as_string = strdup(new_value.as_string);
    }

    var->version++;
    return true;
}

void ir_reactive_manifest_print(IRReactiveManifest* manifest) {
    if (!manifest) {
        printf("Reactive Manifest: NULL\n");
        return;
    }

    printf("Reactive Manifest:\n");
    printf("  Component Definitions: %u\n", manifest->component_def_count);
    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        IRComponentDefinition* def = &manifest->component_defs[i];
        printf("    [%s] props=%u, state_vars=%u\n", def->name, def->prop_count, def->state_var_count);
    }

    printf("  Variables: %u\n", manifest->variable_count);
    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        printf("    [%u] %s (type=%d, v=%u)\n", var->id, var->name, var->type, var->version);
        if (var->type_string) printf("        type_string: %s\n", var->type_string);
        if (var->initial_value_json) printf("        initial: %s\n", var->initial_value_json);
        if (var->scope) printf("        scope: %s\n", var->scope);
    }

    printf("  Bindings: %u\n", manifest->binding_count);
    printf("  Conditionals: %u\n", manifest->conditional_count);
    printf("  For-loops: %u\n", manifest->for_loop_count);
}

void ir_reactive_manifest_add_component_def(IRReactiveManifest* manifest,
                                            const char* name,
                                            IRComponentProp* props,
                                            uint32_t prop_count,
                                            IRComponentStateVar* state_vars,
                                            uint32_t state_var_count,
                                            IRComponent* template_root) {
    if (!manifest || !name) return;

    // Check if already exists
    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        if (manifest->component_defs[i].name && strcmp(manifest->component_defs[i].name, name) == 0) {
            // Update template if not set
            if (!manifest->component_defs[i].template_root && template_root) {
                manifest->component_defs[i].template_root = template_root;
            }
            return;
        }
    }

    // Resize if needed
    if (manifest->component_def_count >= manifest->component_def_capacity) {
        if (!ir_grow_capacity(&manifest->component_def_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow component_defs array - would overflow\n");
            return;
        }
        IRComponentDefinition* new_defs = realloc(manifest->component_defs,
                                                   manifest->component_def_capacity * sizeof(IRComponentDefinition));
        if (!new_defs) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize component_defs array\n");
            return;
        }
        manifest->component_defs = new_defs;
    }

    IRComponentDefinition* def = &manifest->component_defs[manifest->component_def_count++];
    def->name = strdup(name);
    def->template_root = template_root;

    // Copy props
    if (prop_count > 0 && props) {
        def->props = calloc(prop_count, sizeof(IRComponentProp));
        def->prop_count = prop_count;
        for (uint32_t i = 0; i < prop_count; i++) {
            def->props[i].name = props[i].name ? strdup(props[i].name) : NULL;
            def->props[i].type = props[i].type ? strdup(props[i].type) : NULL;
            def->props[i].default_value = props[i].default_value ? strdup(props[i].default_value) : NULL;
        }
    } else {
        def->props = NULL;
        def->prop_count = 0;
    }

    // Copy state vars
    if (state_var_count > 0 && state_vars) {
        def->state_vars = calloc(state_var_count, sizeof(IRComponentStateVar));
        def->state_var_count = state_var_count;
        for (uint32_t i = 0; i < state_var_count; i++) {
            def->state_vars[i].name = state_vars[i].name ? strdup(state_vars[i].name) : NULL;
            def->state_vars[i].type = state_vars[i].type ? strdup(state_vars[i].type) : NULL;
            def->state_vars[i].initial_expr = state_vars[i].initial_expr ? strdup(state_vars[i].initial_expr) : NULL;
        }
    } else {
        def->state_vars = NULL;
        def->state_var_count = 0;
    }
}

IRComponentDefinition* ir_reactive_manifest_find_component_def(IRReactiveManifest* manifest,
                                                               const char* name) {
    if (!manifest || !name) return NULL;

    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        if (manifest->component_defs[i].name && strcmp(manifest->component_defs[i].name, name) == 0) {
            return &manifest->component_defs[i];
        }
    }

    return NULL;
}

// ============================================================================
// Source Code Management (for round-trip preservation)
// ============================================================================

void ir_reactive_manifest_add_source(IRReactiveManifest* manifest,
                                     const char* lang,
                                     const char* code) {
    if (!manifest || !lang || !code) return;

    // Check if this language already exists - if so, update it
    for (uint32_t i = 0; i < manifest->source_count; i++) {
        if (manifest->sources[i].lang && strcmp(manifest->sources[i].lang, lang) == 0) {
            // Update existing entry
            free(manifest->sources[i].code);
            manifest->sources[i].code = strdup(code);
            return;
        }
    }

    // Resize if needed
    if (manifest->source_count >= manifest->source_capacity) {
        if (!ir_grow_capacity(&manifest->source_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow sources array - would overflow\n");
            return;
        }
        IRSourceEntry* new_sources = realloc(manifest->sources,
                                              manifest->source_capacity * sizeof(IRSourceEntry));
        if (!new_sources) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize sources array\n");
            return;
        }
        manifest->sources = new_sources;
    }

    // Add new entry
    IRSourceEntry* entry = &manifest->sources[manifest->source_count++];
    entry->lang = strdup(lang);
    entry->code = strdup(code);
}

const char* ir_reactive_manifest_get_source(IRReactiveManifest* manifest,
                                            const char* lang) {
    if (!manifest || !lang) return NULL;

    for (uint32_t i = 0; i < manifest->source_count; i++) {
        if (manifest->sources[i].lang && strcmp(manifest->sources[i].lang, lang) == 0) {
            return manifest->sources[i].code;
        }
    }

    return NULL;
}

// ============================================================================
// Computed Property Management (for @computed decorator)
// ============================================================================

uint32_t ir_reactive_manifest_add_computed(IRReactiveManifest* manifest,
                                           const char* name,
                                           const char* function_name,
                                           const char** dependencies,
                                           uint32_t dependency_count) {
    if (!manifest || !name || !function_name) return 0;

    // Resize if needed
    if (manifest->computed_property_count >= manifest->computed_property_capacity) {
        if (!ir_grow_capacity(&manifest->computed_property_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow computed properties array - would overflow\n");
            return 0;
        }
        IRComputedProperty* new_props = realloc(manifest->computed_properties,
                                                manifest->computed_property_capacity * sizeof(IRComputedProperty));
        if (!new_props) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize computed properties array\n");
            return 0;
        }
        manifest->computed_properties = new_props;
    }

    uint32_t computed_id = manifest->next_computed_id++;
    IRComputedProperty* computed = &manifest->computed_properties[manifest->computed_property_count++];

    memset(computed, 0, sizeof(IRComputedProperty));
    computed->id = computed_id;
    computed->name = strdup(name);
    computed->function_name = strdup(function_name);
    computed->state = IR_COMPUTED_STATE_DIRTY;

    // Copy dependencies
    if (dependencies && dependency_count > 0) {
        computed->dependency_capacity = dependency_count;
        computed->dependencies = calloc(dependency_count, sizeof(char*));
        computed->dependency_count = dependency_count;

        for (uint32_t i = 0; i < dependency_count; i++) {
            computed->dependencies[i] = strdup(dependencies[i]);
        }
    }

    return computed_id;
}

IRComputedProperty* ir_reactive_manifest_find_computed(IRReactiveManifest* manifest,
                                                       const char* name) {
    if (!manifest || !name) return NULL;

    for (uint32_t i = 0; i < manifest->computed_property_count; i++) {
        if (manifest->computed_properties[i].name &&
            strcmp(manifest->computed_properties[i].name, name) == 0) {
            return &manifest->computed_properties[i];
        }
    }

    return NULL;
}

IRComputedProperty* ir_reactive_manifest_get_computed(IRReactiveManifest* manifest,
                                                      uint32_t computed_id) {
    if (!manifest) return NULL;

    for (uint32_t i = 0; i < manifest->computed_property_count; i++) {
        if (manifest->computed_properties[i].id == computed_id) {
            return &manifest->computed_properties[i];
        }
    }

    return NULL;
}

void ir_reactive_manifest_invalidate_computed(IRReactiveManifest* manifest,
                                              uint32_t var_id) {
    if (!manifest) return;

    // Mark all computed properties that depend on this variable as dirty
    for (uint32_t i = 0; i < manifest->computed_property_count; i++) {
        IRComputedProperty* computed = &manifest->computed_properties[i];

        for (uint32_t j = 0; j < computed->dependency_count; j++) {
            // Check if dependency string contains the variable ID
            char dep_str[32];
            snprintf(dep_str, sizeof(dep_str), "%u", var_id);

            if (computed->dependencies[j] && strstr(computed->dependencies[j], dep_str)) {
                computed->state = IR_COMPUTED_STATE_DIRTY;
                break;
            }
        }
    }
}

// ============================================================================
// Action Management (for @action decorator)
// ============================================================================

uint32_t ir_reactive_manifest_add_action(IRReactiveManifest* manifest,
                                        const char* name,
                                        const char* function_name,
                                        bool is_batched,
                                        bool auto_save) {
    if (!manifest || !name || !function_name) return 0;

    // Resize if needed
    if (manifest->action_count >= manifest->action_capacity) {
        if (!ir_grow_capacity(&manifest->action_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow actions array - would overflow\n");
            return 0;
        }
        IRAction* new_actions = realloc(manifest->actions,
                                       manifest->action_capacity * sizeof(IRAction));
        if (!new_actions) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize actions array\n");
            return 0;
        }
        manifest->actions = new_actions;
    }

    uint32_t action_id = manifest->next_action_id++;
    IRAction* action = &manifest->actions[manifest->action_count++];

    memset(action, 0, sizeof(IRAction));
    action->id = action_id;
    action->name = strdup(name);
    action->function_name = strdup(function_name);
    action->is_batched = is_batched;
    action->auto_save = auto_save;

    return action_id;
}

IRAction* ir_reactive_manifest_find_action(IRReactiveManifest* manifest,
                                           const char* name) {
    if (!manifest || !name) return NULL;

    for (uint32_t i = 0; i < manifest->action_count; i++) {
        if (manifest->actions[i].name &&
            strcmp(manifest->actions[i].name, name) == 0) {
            return &manifest->actions[i];
        }
    }

    return NULL;
}

IRAction* ir_reactive_manifest_get_action(IRReactiveManifest* manifest,
                                          uint32_t action_id) {
    if (!manifest) return NULL;

    for (uint32_t i = 0; i < manifest->action_count; i++) {
        if (manifest->actions[i].id == action_id) {
            return &manifest->actions[i];
        }
    }

    return NULL;
}

void ir_reactive_manifest_add_action_watch_path(IRReactiveManifest* manifest,
                                                uint32_t action_id,
                                                const char* watch_path) {
    if (!manifest || !watch_path) return;

    IRAction* action = ir_reactive_manifest_get_action(manifest, action_id);
    if (!action) {
        fprintf(stderr, "[ReactiveManifest] Action ID %u not found\n", action_id);
        return;
    }

    // Reallocate watch paths array
    char** new_paths = realloc(action->watch_paths,
                               (action->watch_path_count + 1) * sizeof(char*));
    if (!new_paths) {
        fprintf(stderr, "[ReactiveManifest] Failed to add watch path to action\n");
        return;
    }

    action->watch_paths = new_paths;
    action->watch_paths[action->watch_path_count++] = strdup(watch_path);
}

// ============================================================================
// Watcher Management (for @watch decorator)
// ============================================================================

uint32_t ir_reactive_manifest_add_watcher(IRReactiveManifest* manifest,
                                          const char* watched_path,
                                          const char* handler_function,
                                          IRWatchDepth depth,
                                          bool immediate) {
    if (!manifest || !watched_path || !handler_function) return 0;

    // Resize if needed
    if (manifest->watcher_count >= manifest->watcher_capacity) {
        if (!ir_grow_capacity(&manifest->watcher_capacity)) {
            fprintf(stderr, "[ReactiveManifest] Cannot grow watchers array - would overflow\n");
            return 0;
        }
        IRWatcher* new_watchers = realloc(manifest->watchers,
                                         manifest->watcher_capacity * sizeof(IRWatcher));
        if (!new_watchers) {
            fprintf(stderr, "[ReactiveManifest] Failed to resize watchers array\n");
            return 0;
        }
        manifest->watchers = new_watchers;
    }

    uint32_t watcher_id = manifest->next_watcher_id++;
    IRWatcher* watcher = &manifest->watchers[manifest->watcher_count++];

    memset(watcher, 0, sizeof(IRWatcher));
    watcher->id = watcher_id;
    watcher->watched_path = strdup(watched_path);
    watcher->handler_function = strdup(handler_function);
    watcher->depth = depth;
    watcher->immediate = immediate;

    return watcher_id;
}

IRWatcher* ir_reactive_manifest_find_watcher(IRReactiveManifest* manifest,
                                             const char* watched_path) {
    if (!manifest || !watched_path) return NULL;

    for (uint32_t i = 0; i < manifest->watcher_count; i++) {
        if (manifest->watchers[i].watched_path &&
            strcmp(manifest->watchers[i].watched_path, watched_path) == 0) {
            return &manifest->watchers[i];
        }
    }

    return NULL;
}

IRWatcher* ir_reactive_manifest_get_watcher(IRReactiveManifest* manifest,
                                            uint32_t watcher_id) {
    if (!manifest) return NULL;

    for (uint32_t i = 0; i < manifest->watcher_count; i++) {
        if (manifest->watchers[i].id == watcher_id) {
            return &manifest->watchers[i];
        }
    }

    return NULL;
}

void ir_reactive_manifest_add_watcher_dependency(IRReactiveManifest* manifest,
                                                 uint32_t watcher_id,
                                                 uint32_t var_id) {
    if (!manifest) return;

    IRWatcher* watcher = ir_reactive_manifest_get_watcher(manifest, watcher_id);
    if (!watcher) {
        fprintf(stderr, "[ReactiveManifest] Watcher ID %u not found\n", watcher_id);
        return;
    }

    // Reallocate watched var IDs array
    uint32_t* new_ids = realloc(watcher->watched_var_ids,
                               (watcher->watched_var_count + 1) * sizeof(uint32_t));
    if (!new_ids) {
        fprintf(stderr, "[ReactiveManifest] Failed to add dependency to watcher\n");
        return;
    }

    watcher->watched_var_ids = new_ids;
    watcher->watched_var_ids[watcher->watched_var_count++] = var_id;
}
