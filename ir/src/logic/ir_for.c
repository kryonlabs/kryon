// Enable strdup in C99 mode
#define _POSIX_C_SOURCE 200809L

#include "ir_for.h"
#include "../include/ir_core.h"
#include "cJSON.h"
#include "../utils/ir_json_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declaration for variable source resolution
cJSON* ir_for_resolve_variable_source(IRForDef* def, IRSourceStructures* source_structures);

// ============================================================================
// Builder API Implementation
// ============================================================================

IRForDef* ir_for_def_create(const char* item_name, const char* index_name) {
    IRForDef* def = calloc(1, sizeof(IRForDef));
    if (!def) return NULL;

    def->loop_type = IR_LOOP_TYPE_FOR_IN;  // Default to for-in loop
    def->item_name = item_name ? strdup(item_name) : strdup("item");
    def->index_name = index_name ? strdup(index_name) : strdup("index");
    def->source.type = FOR_SOURCE_NONE;
    def->tmpl.component = NULL;
    def->tmpl.bindings = NULL;
    def->tmpl.binding_count = 0;
    def->tmpl.binding_capacity = 0;

    // NEW: Default to compile-time expansion
    def->is_compile_time = true;
    def->reactive_var_id = NULL;

    return def;
}

void ir_for_def_destroy(IRForDef* def) {
    if (!def) return;

    free(def->item_name);
    free(def->index_name);
    free(def->source.expression);
    free(def->source.literal_json);
    free(def->reactive_var_id);

    // Free bindings
    for (uint32_t i = 0; i < def->tmpl.binding_count; i++) {
        free(def->tmpl.bindings[i].target_property);
        free(def->tmpl.bindings[i].source_expression);
    }
    free(def->tmpl.bindings);

    // Note: template component is not freed here - it's part of the component tree
    // and will be freed when the tree is destroyed

    free(def);
}

void ir_for_set_source_literal(IRForDef* def, const char* json_array) {
    if (!def) return;

    free(def->source.expression);
    free(def->source.literal_json);
    def->source.expression = NULL;

    def->source.type = FOR_SOURCE_LITERAL;
    def->source.literal_json = json_array ? strdup(json_array) : NULL;
}

void ir_for_set_source_variable(IRForDef* def, const char* var_name) {
    if (!def) return;

    free(def->source.expression);
    free(def->source.literal_json);
    def->source.literal_json = NULL;

    def->source.type = FOR_SOURCE_VARIABLE;
    def->source.expression = var_name ? strdup(var_name) : NULL;
}

void ir_for_set_source_expression(IRForDef* def, const char* expr) {
    if (!def) return;

    free(def->source.expression);
    free(def->source.literal_json);
    def->source.literal_json = NULL;

    def->source.type = FOR_SOURCE_EXPRESSION;
    def->source.expression = expr ? strdup(expr) : NULL;
}

void ir_for_set_template(IRForDef* def, struct IRComponent* template_component) {
    if (!def) return;
    def->tmpl.component = template_component;
}

void ir_for_add_binding(IRForDef* def,
                        const char* target_property,
                        const char* source_expression,
                        bool is_computed) {
    if (!def || !target_property || !source_expression) return;

    // Grow capacity if needed
    if (def->tmpl.binding_count >= def->tmpl.binding_capacity) {
        uint32_t new_capacity = def->tmpl.binding_capacity == 0 ? 4 : def->tmpl.binding_capacity * 2;
        IRForBinding* new_bindings = realloc(def->tmpl.bindings,
                                             new_capacity * sizeof(IRForBinding));
        if (!new_bindings) return;
        def->tmpl.bindings = new_bindings;
        def->tmpl.binding_capacity = new_capacity;
    }

    // Add binding
    IRForBinding* binding = &def->tmpl.bindings[def->tmpl.binding_count];
    binding->target_property = strdup(target_property);
    binding->source_expression = strdup(source_expression);
    binding->is_computed = is_computed;
    def->tmpl.binding_count++;
}

void ir_for_clear_bindings(IRForDef* def) {
    if (!def) return;

    for (uint32_t i = 0; i < def->tmpl.binding_count; i++) {
        free(def->tmpl.bindings[i].target_property);
        free(def->tmpl.bindings[i].source_expression);
    }
    def->tmpl.binding_count = 0;
}

// ============================================================================
// Serialization API Implementation
// ============================================================================

cJSON* ir_for_binding_to_json(IRForBinding* binding) {
    if (!binding) return NULL;

    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringOrNull(json, "target", binding->target_property);
    cJSON_AddStringOrNull(json, "source", binding->source_expression);
    if (binding->is_computed) {
        cJSON_AddBoolToObject(json, "computed", true);
    }

    return json;
}

IRForBinding* ir_for_binding_from_json(cJSON* json) {
    if (!json) return NULL;

    IRForBinding* binding = calloc(1, sizeof(IRForBinding));
    if (!binding) return NULL;

    cJSON* target = cJSON_GetObjectItem(json, "target");
    cJSON* source = cJSON_GetObjectItem(json, "source");
    cJSON* computed = cJSON_GetObjectItem(json, "computed");

    if (target && cJSON_IsString(target)) {
        binding->target_property = strdup(target->valuestring);
    }
    if (source && cJSON_IsString(source)) {
        binding->source_expression = strdup(source->valuestring);
    }
    binding->is_computed = computed && cJSON_IsBool(computed) && cJSON_IsTrue(computed);

    return binding;
}

cJSON* ir_for_def_to_json(IRForDef* def) {
    if (!def) return NULL;

    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;

    // Loop type
    const char* loop_type_str = "for_in";
    switch (def->loop_type) {
        case IR_LOOP_TYPE_FOR_IN: loop_type_str = "for_in"; break;
        case IR_LOOP_TYPE_FOR_RANGE: loop_type_str = "for_range"; break;
        case IR_LOOP_TYPE_FOR_EACH: loop_type_str = "for_each"; break;
    }
    cJSON_AddStringOrNull(json, "loop_type", loop_type_str);

    // Item and index names
    if (def->item_name) {
        cJSON_AddStringOrNull(json, "item_name", def->item_name);
    }
    if (def->index_name) {
        cJSON_AddStringOrNull(json, "index_name", def->index_name);
    }

    // NEW: is_compile_time flag
    if (def->is_compile_time) {
        cJSON_AddBoolToObject(json, "is_compile_time", true);
    }

    // NEW: reactive_var_id if present
    if (def->reactive_var_id) {
        cJSON_AddStringOrNull(json, "reactive_var_id", def->reactive_var_id);
    }

    // Data source
    cJSON* source = cJSON_CreateObject();
    if (source) {
        const char* type_str = "none";
        switch (def->source.type) {
            case FOR_SOURCE_LITERAL: type_str = "literal"; break;
            case FOR_SOURCE_VARIABLE: type_str = "variable"; break;
            case FOR_SOURCE_EXPRESSION: type_str = "expression"; break;
            default: break;
        }
        cJSON_AddStringOrNull(source, "type", type_str);

        if (def->source.expression) {
            cJSON_AddStringOrNull(source, "expression", def->source.expression);
        }
        if (def->source.literal_json) {
            // Parse and add as raw JSON
            cJSON* data = cJSON_Parse(def->source.literal_json);
            if (data) {
                cJSON_AddItemToObject(source, "data", data);
            } else {
                // Store as string if parsing fails
                cJSON_AddStringOrNull(source, "data_raw", def->source.literal_json);
            }
        }
        cJSON_AddItemToObject(json, "source", source);
    }

    // Template bindings
    if (def->tmpl.binding_count > 0) {
        cJSON* bindings = cJSON_CreateArray();
        if (bindings) {
            for (uint32_t i = 0; i < def->tmpl.binding_count; i++) {
                cJSON* binding_json = ir_for_binding_to_json(&def->tmpl.bindings[i]);
                if (binding_json) {
                    cJSON_AddItemToArray(bindings, binding_json);
                }
            }
            cJSON_AddItemToObject(json, "bindings", bindings);
        }
    }

    // Note: template component is serialized separately as the For's child[0]

    return json;
}

IRForDef* ir_for_def_from_json(cJSON* json) {
    if (!json) return NULL;

    cJSON* item_name = cJSON_GetObjectItem(json, "item_name");
    cJSON* index_name = cJSON_GetObjectItem(json, "index_name");

    IRForDef* def = ir_for_def_create(
        item_name && cJSON_IsString(item_name) ? item_name->valuestring : "item",
        index_name && cJSON_IsString(index_name) ? index_name->valuestring : "index"
    );
    if (!def) return NULL;

    // Parse loop type
    cJSON* loop_type = cJSON_GetObjectItem(json, "loop_type");
    if (loop_type && cJSON_IsString(loop_type)) {
        const char* type_str = loop_type->valuestring;
        if (strcmp(type_str, "for_in") == 0) {
            def->loop_type = IR_LOOP_TYPE_FOR_IN;
        } else if (strcmp(type_str, "for_range") == 0) {
            def->loop_type = IR_LOOP_TYPE_FOR_RANGE;
        } else if (strcmp(type_str, "for_each") == 0) {
            def->loop_type = IR_LOOP_TYPE_FOR_EACH;
        }
    }

    // NEW: Parse is_compile_time flag
    cJSON* is_compile_time = cJSON_GetObjectItem(json, "is_compile_time");
    if (is_compile_time && cJSON_IsBool(is_compile_time)) {
        def->is_compile_time = cJSON_IsTrue(is_compile_time);
    }

    // NEW: Parse reactive_var_id
    cJSON* reactive_var_id = cJSON_GetObjectItem(json, "reactive_var_id");
    if (reactive_var_id && cJSON_IsString(reactive_var_id)) {
        def->reactive_var_id = strdup(reactive_var_id->valuestring);
    }

    // Parse data source
    cJSON* source = cJSON_GetObjectItem(json, "source");
    if (source) {
        cJSON* type = cJSON_GetObjectItem(source, "type");
        cJSON* expr = cJSON_GetObjectItem(source, "expression");
        cJSON* data = cJSON_GetObjectItem(source, "data");
        cJSON* data_raw = cJSON_GetObjectItem(source, "data_raw");

        if (type && cJSON_IsString(type)) {
            const char* type_str = type->valuestring;
            if (strcmp(type_str, "literal") == 0) {
                def->source.type = FOR_SOURCE_LITERAL;
                if (data) {
                    char* json_str = cJSON_PrintUnformatted(data);
                    def->source.literal_json = json_str;
                } else if (data_raw && cJSON_IsString(data_raw)) {
                    def->source.literal_json = strdup(data_raw->valuestring);
                }
            } else if (strcmp(type_str, "variable") == 0) {
                def->source.type = FOR_SOURCE_VARIABLE;
                if (expr && cJSON_IsString(expr)) {
                    def->source.expression = strdup(expr->valuestring);
                }
            } else if (strcmp(type_str, "expression") == 0) {
                def->source.type = FOR_SOURCE_EXPRESSION;
                if (expr && cJSON_IsString(expr)) {
                    def->source.expression = strdup(expr->valuestring);
                }
            }
        }
    }

    // Parse bindings
    cJSON* bindings = cJSON_GetObjectItem(json, "bindings");
    if (bindings && cJSON_IsArray(bindings)) {
        cJSON* binding_json;
        cJSON_ArrayForEach(binding_json, bindings) {
            cJSON* target = cJSON_GetObjectItem(binding_json, "target");
            cJSON* source_expr = cJSON_GetObjectItem(binding_json, "source");
            cJSON* computed = cJSON_GetObjectItem(binding_json, "computed");

            if (target && cJSON_IsString(target) &&
                source_expr && cJSON_IsString(source_expr)) {
                ir_for_add_binding(def,
                                   target->valuestring,
                                   source_expr->valuestring,
                                   computed && cJSON_IsTrue(computed));
            }
        }
    }

    // Note: template component is set separately when parsing the For's children

    return def;
}

// ============================================================================
// Utility Functions
// ============================================================================

IRForDef* ir_for_def_copy(IRForDef* src) {
    if (!src) return NULL;

    IRForDef* copy = ir_for_def_create(src->item_name, src->index_name);
    if (!copy) return NULL;

    // Copy loop type
    copy->loop_type = src->loop_type;

    // Copy source
    copy->source.type = src->source.type;
    if (src->source.expression) {
        copy->source.expression = strdup(src->source.expression);
    }
    if (src->source.literal_json) {
        copy->source.literal_json = strdup(src->source.literal_json);
    }

    // Copy bindings
    for (uint32_t i = 0; i < src->tmpl.binding_count; i++) {
        ir_for_add_binding(copy,
                           src->tmpl.bindings[i].target_property,
                           src->tmpl.bindings[i].source_expression,
                           src->tmpl.bindings[i].is_computed);
    }

    // NEW: Copy is_compile_time flag
    copy->is_compile_time = src->is_compile_time;

    // NEW: Copy reactive_var_id
    if (src->reactive_var_id) {
        copy->reactive_var_id = strdup(src->reactive_var_id);
    }

    // Note: template component is not copied - it's a reference to the component tree

    return copy;
}

bool ir_for_needs_runtime_data(IRForDef* def) {
    if (!def) return false;
    return def->source.type == FOR_SOURCE_VARIABLE ||
           def->source.type == FOR_SOURCE_EXPRESSION;
}

cJSON* ir_for_get_source_data(IRForDef* def) {
    if (!def) return NULL;

    if (def->source.type == FOR_SOURCE_LITERAL && def->source.literal_json) {
        return cJSON_Parse(def->source.literal_json);
    }

    // For variable/expression sources, caller must provide data
    return NULL;
}

// ============================================================================
// Variable Source Resolution
// ============================================================================

cJSON* ir_for_resolve_variable_source(IRForDef* def, IRSourceStructures* source_structures) {
    if (!def || !source_structures) return NULL;

    // Only handle variable sources
    if (def->source.type != FOR_SOURCE_VARIABLE || !def->source.expression) {
        return NULL;
    }

    // Look up const declaration by name
    IRVarDecl* var_decl = ir_source_structures_find_var_decl(source_structures, def->source.expression);
    if (!var_decl || !var_decl->value_json) {
        return NULL;
    }

    // Parse and return the value
    cJSON* parsed = cJSON_Parse(var_decl->value_json);
    if (!parsed) {
        return NULL;
    }

    // If parsed result is a string, parse it again (value_json is stored as escaped JSON string)
    if (cJSON_IsString(parsed)) {
        const char* str_value = cJSON_GetStringValue(parsed);
        cJSON* array = cJSON_Parse(str_value);
        cJSON_Delete(parsed);
        parsed = array;
    }

    return parsed;
}
