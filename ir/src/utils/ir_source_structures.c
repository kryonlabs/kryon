#define _POSIX_C_SOURCE 200809L
#include "../include/ir_core.h"
#include "../include/ir_builder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Source Structures Management (for Kry → KIR → Kry round-trip codegen)
// ============================================================================

// Helper: Generate unique ID
static char* generate_id(const char* prefix, uint32_t* counter) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s_%u", prefix, (*counter)++);
    return strdup(buffer);
}

// ============================================================================
// IRSourceStructures Management
// ============================================================================

IRSourceStructures* ir_source_structures_create(void) {
    IRSourceStructures* ss = calloc(1, sizeof(IRSourceStructures));
    if (!ss) return NULL;

    ss->static_block_capacity = 16;
    ss->static_blocks = calloc(ss->static_block_capacity, sizeof(IRStaticBlockData*));

    ss->var_decl_capacity = 32;
    ss->var_decls = calloc(ss->var_decl_capacity, sizeof(IRVarDecl*));

    ss->for_loop_capacity = 16;
    ss->for_loops = calloc(ss->for_loop_capacity, sizeof(IRForLoopData*));

    ss->next_static_block_id = 1;
    ss->next_var_decl_id = 1;
    ss->next_for_loop_id = 1;

    return ss;
}

void ir_source_structures_destroy(IRSourceStructures* ss) {
    if (!ss) return;

    // Free static blocks
    for (uint32_t i = 0; i < ss->static_block_count; i++) {
        IRStaticBlockData* block = ss->static_blocks[i];
        if (block) {
            free(block->id);
            free(block->children_ids);
            if (block->var_declaration_ids) {
                for (uint32_t j = 0; j < block->var_decl_count; j++) {
                    free(block->var_declaration_ids[j]);
                }
                free(block->var_declaration_ids);
            }
            if (block->for_loop_ids) {
                for (uint32_t j = 0; j < block->for_loop_count; j++) {
                    free(block->for_loop_ids[j]);
                }
                free(block->for_loop_ids);
            }
            free(block);
        }
    }
    free(ss->static_blocks);

    // Free variable declarations
    for (uint32_t i = 0; i < ss->var_decl_count; i++) {
        IRVarDecl* var = ss->var_decls[i];
        if (var) {
            free(var->id);
            free(var->name);
            free(var->var_type);
            free(var->value_type);
            free(var->value_json);
            free(var->scope);
            free(var);
        }
    }
    free(ss->var_decls);

    // Free for loops
    for (uint32_t i = 0; i < ss->for_loop_count; i++) {
        IRForLoopData* loop = ss->for_loops[i];
        if (loop) {
            free(loop->id);
            free(loop->parent_id);
            free(loop->iterator_name);
            free(loop->collection_ref);
            free(loop->collection_expr);
            // Note: template_component is owned by the component tree
            free(loop->expanded_component_ids);
            free(loop);
        }
    }
    free(ss->for_loops);

    free(ss);
}

// ============================================================================
// Static Block Management
// ============================================================================

IRStaticBlockData* ir_source_structures_add_static_block(IRSourceStructures* ss,
                                                          uint32_t parent_component_id) {
    if (!ss) return NULL;

    // Grow array if needed
    if (ss->static_block_count >= ss->static_block_capacity) {
        ss->static_block_capacity *= 2;
        ss->static_blocks = realloc(ss->static_blocks,
                                    ss->static_block_capacity * sizeof(IRStaticBlockData*));
    }

    IRStaticBlockData* block = calloc(1, sizeof(IRStaticBlockData));
    block->id = generate_id("static", &ss->next_static_block_id);
    block->parent_component_id = parent_component_id;

    ss->static_blocks[ss->static_block_count++] = block;
    return block;
}

void ir_static_block_add_child(IRStaticBlockData* block, uint32_t child_id) {
    if (!block) return;

    // Grow array if needed
    uint32_t capacity = block->children_count + 1;
    if (block->children_count == 0) {
        block->children_ids = malloc(16 * sizeof(uint32_t));
    } else if (capacity > 16 && (capacity & (capacity - 1)) == 0) {
        // Power of 2 growth
        block->children_ids = realloc(block->children_ids, capacity * 2 * sizeof(uint32_t));
    }

    block->children_ids[block->children_count++] = child_id;
}

void ir_static_block_add_var_decl(IRStaticBlockData* block, const char* var_decl_id) {
    if (!block || !var_decl_id) return;

    // Grow array if needed
    if (block->var_decl_count == 0) {
        block->var_declaration_ids = malloc(8 * sizeof(char*));
    } else if (block->var_decl_count >= 8 &&
               (block->var_decl_count & (block->var_decl_count - 1)) == 0) {
        block->var_declaration_ids = realloc(block->var_declaration_ids,
                                             block->var_decl_count * 2 * sizeof(char*));
    }

    block->var_declaration_ids[block->var_decl_count++] = strdup(var_decl_id);
}

void ir_static_block_add_for_loop(IRStaticBlockData* block, const char* for_loop_id) {
    if (!block || !for_loop_id) return;

    // Grow array if needed
    if (block->for_loop_count == 0) {
        block->for_loop_ids = malloc(8 * sizeof(char*));
    } else if (block->for_loop_count >= 8 &&
               (block->for_loop_count & (block->for_loop_count - 1)) == 0) {
        block->for_loop_ids = realloc(block->for_loop_ids,
                                      block->for_loop_count * 2 * sizeof(char*));
    }

    block->for_loop_ids[block->for_loop_count++] = strdup(for_loop_id);
}

// ============================================================================
// Variable Declaration Management
// ============================================================================

IRVarDecl* ir_source_structures_add_var_decl(IRSourceStructures* ss,
                                              const char* name,
                                              const char* var_type,
                                              const char* value_json,
                                              const char* scope) {
    if (!ss || !name) return NULL;

    // Grow array if needed
    if (ss->var_decl_count >= ss->var_decl_capacity) {
        ss->var_decl_capacity *= 2;
        ss->var_decls = realloc(ss->var_decls,
                                ss->var_decl_capacity * sizeof(IRVarDecl*));
    }

    IRVarDecl* var = calloc(1, sizeof(IRVarDecl));
    var->id = generate_id("var", &ss->next_var_decl_id);
    var->name = strdup(name);
    var->var_type = var_type ? strdup(var_type) : strdup("const");
    var->value_json = value_json ? strdup(value_json) : NULL;
    var->scope = scope ? strdup(scope) : strdup("global");

    // Infer type from JSON value if not specified
    if (value_json && !var->value_type) {
        const char* json = value_json;
        while (*json == ' ') json++;  // Skip whitespace
        if (*json == '[') {
            var->value_type = strdup("array");
        } else if (*json == '{') {
            var->value_type = strdup("object");
        } else if (*json == '"') {
            var->value_type = strdup("string");
        } else if (*json == 't' || *json == 'f') {
            var->value_type = strdup("bool");
        } else if (*json >= '0' && *json <= '9') {
            var->value_type = strchr(json, '.') ? strdup("float") : strdup("number");
        } else {
            var->value_type = strdup("unknown");
        }
    }

    ss->var_decls[ss->var_decl_count++] = var;
    return var;
}

// ============================================================================
// For Loop Management (Compile-time only - static blocks)
// ============================================================================

IRForLoopData* ir_source_structures_add_for_loop(IRSourceStructures* ss,
                                                  const char* parent_static_id,
                                                  const char* iterator_name,
                                                  const char* collection_ref,
                                                  IRComponent* template_component) {
    if (!ss || !iterator_name || !collection_ref) return NULL;

    // Grow array if needed
    if (ss->for_loop_count >= ss->for_loop_capacity) {
        ss->for_loop_capacity *= 2;
        ss->for_loops = realloc(ss->for_loops,
                                ss->for_loop_capacity * sizeof(IRForLoopData*));
    }

    IRForLoopData* loop = calloc(1, sizeof(IRForLoopData));
    loop->id = generate_id("for", &ss->next_for_loop_id);
    loop->parent_id = parent_static_id ? strdup(parent_static_id) : NULL;
    loop->iterator_name = strdup(iterator_name);
    loop->collection_ref = strdup(collection_ref);
    loop->collection_expr = strdup(collection_ref);  // Same for now
    loop->template_component = template_component;

    ss->for_loops[ss->for_loop_count++] = loop;
    return loop;
}

void ir_for_loop_add_expanded_component(IRForLoopData* loop, uint32_t component_id) {
    if (!loop) return;

    // Grow array if needed
    if (loop->expanded_count == 0) {
        loop->expanded_component_ids = malloc(16 * sizeof(uint32_t));
    } else if (loop->expanded_count >= 16 &&
               (loop->expanded_count & (loop->expanded_count - 1)) == 0) {
        loop->expanded_component_ids = realloc(loop->expanded_component_ids,
                                               loop->expanded_count * 2 * sizeof(uint32_t));
    }

    loop->expanded_component_ids[loop->expanded_count++] = component_id;
}

// ============================================================================
// Property Binding Management
// ============================================================================

IRPropertyBinding* ir_component_add_property_binding(IRComponent* component,
                                                      const char* property_name,
                                                      const char* source_expr,
                                                      const char* resolved_value,
                                                      const char* binding_type) {
    if (!component || !property_name) return NULL;

    // Grow array if needed
    if (component->property_binding_count == 0) {
        component->property_bindings = malloc(8 * sizeof(IRPropertyBinding*));
    } else if (component->property_binding_count >= 8 &&
               (component->property_binding_count & (component->property_binding_count - 1)) == 0) {
        component->property_bindings = realloc(component->property_bindings,
                                               component->property_binding_count * 2 * sizeof(IRPropertyBinding*));
    }

    IRPropertyBinding* binding = calloc(1, sizeof(IRPropertyBinding));
    binding->property_name = strdup(property_name);
    binding->source_expr = source_expr ? strdup(source_expr) : NULL;
    binding->resolved_value = resolved_value ? strdup(resolved_value) : NULL;
    binding->binding_type = binding_type ? strdup(binding_type) : strdup("static_template");

    component->property_bindings[component->property_binding_count++] = binding;
    return binding;
}

// ============================================================================
// Lookup Functions
// ============================================================================

IRStaticBlockData* ir_source_structures_find_static_block(IRSourceStructures* ss, const char* id) {
    if (!ss || !id) return NULL;

    for (uint32_t i = 0; i < ss->static_block_count; i++) {
        if (strcmp(ss->static_blocks[i]->id, id) == 0) {
            return ss->static_blocks[i];
        }
    }
    return NULL;
}

IRVarDecl* ir_source_structures_find_var_decl(IRSourceStructures* ss, const char* id) {
    if (!ss || !id) return NULL;

    for (uint32_t i = 0; i < ss->var_decl_count; i++) {
        if (strcmp(ss->var_decls[i]->id, id) == 0) {
            return ss->var_decls[i];
        }
    }
    return NULL;
}

IRForLoopData* ir_source_structures_find_for_loop(IRSourceStructures* ss, const char* id) {
    if (!ss || !id) return NULL;

    for (uint32_t i = 0; i < ss->for_loop_count; i++) {
        if (strcmp(ss->for_loops[i]->id, id) == 0) {
            return ss->for_loops[i];
        }
    }
    return NULL;
}

// ============================================================================
// Dynamic Binding Management (for runtime Lua expression evaluation in web)
// ============================================================================

IRDynamicBinding* ir_dynamic_binding_create(const char* binding_id,
                                             const char* element_selector,
                                             const char* update_type,
                                             const char* lua_expr) {
    IRDynamicBinding* binding = calloc(1, sizeof(IRDynamicBinding));
    if (!binding) return NULL;

    binding->binding_id = binding_id ? strdup(binding_id) : NULL;
    binding->element_selector = element_selector ? strdup(element_selector) : NULL;
    binding->update_type = update_type ? strdup(update_type) : strdup("text");
    binding->lua_expr = lua_expr ? strdup(lua_expr) : NULL;

    return binding;
}

void ir_dynamic_binding_destroy(IRDynamicBinding* binding) {
    if (!binding) return;

    free(binding->binding_id);
    free(binding->element_selector);
    free(binding->update_type);
    free(binding->lua_expr);
    free(binding);
}

void ir_component_add_dynamic_binding(IRComponent* component,
                                       const char* binding_id,
                                       const char* element_selector,
                                       const char* update_type,
                                       const char* lua_expr) {
    if (!component || !lua_expr) return;

    // Initialize array if needed
    if (component->dynamic_bindings == NULL) {
        component->dynamic_binding_capacity = 4;
        component->dynamic_bindings = calloc(component->dynamic_binding_capacity, sizeof(IRDynamicBinding*));
        component->dynamic_binding_count = 0;
    }

    // Grow array if needed
    if (component->dynamic_binding_count >= component->dynamic_binding_capacity) {
        component->dynamic_binding_capacity *= 2;
        component->dynamic_bindings = realloc(component->dynamic_bindings,
                                              component->dynamic_binding_capacity * sizeof(IRDynamicBinding*));
    }

    // Create and add the binding
    IRDynamicBinding* binding = ir_dynamic_binding_create(binding_id, element_selector, update_type, lua_expr);
    component->dynamic_bindings[component->dynamic_binding_count++] = binding;
}

uint32_t ir_component_get_dynamic_binding_count(IRComponent* component) {
    return component ? component->dynamic_binding_count : 0;
}

IRDynamicBinding* ir_component_get_dynamic_binding(IRComponent* component, uint32_t index) {
    if (!component || index >= component->dynamic_binding_count) return NULL;
    return component->dynamic_bindings[index];
}
