// IR Module References Module
// Module reference management functions extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_module_refs.h"
#include "../utils/ir_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

// Helper function from ir_json_helpers (or define inline if needed)
extern cJSON* cJSON_AddStringOrNull(cJSON* object, const char* name, const char* string);

// ============================================================================
// Module Reference Management (for cross-file component references)
// ============================================================================

void ir_set_component_module_ref(IRComponent* component, const char* module_ref, const char* export_name) {
    if (!component) return;

    if (component->module_ref) {
        free(component->module_ref);
    }
    component->module_ref = module_ref ? strdup(module_ref) : NULL;

    if (component->export_name) {
        free(component->export_name);
    }
    component->export_name = export_name ? strdup(export_name) : NULL;
}

// Recursively clear module_ref for a component tree (for serialization)
// Returns a cJSON array with the old values for each component that had module_ref set
cJSON* ir_clear_tree_module_refs(IRComponent* component) {
    if (!component) return NULL;

    cJSON* refs_array = cJSON_CreateArray();

    // Process this component first
    if (component->module_ref) {
        cJSON* ref = cJSON_CreateObject();
        cJSON_AddNumberToObject(ref, "id", component->id);
        cJSON_AddStringOrNull(ref, "module_ref", component->module_ref);
        if (component->export_name) {
            cJSON_AddStringOrNull(ref, "export_name", component->export_name);
        }
        cJSON_AddItemToArray(refs_array, ref);

        free(component->module_ref);
        component->module_ref = NULL;
    }
    if (component->export_name) {
        free(component->export_name);
        component->export_name = NULL;
    }

    // Recursively process children
    for (uint32_t i = 0; i < component->child_count; i++) {
        cJSON* child_refs = ir_clear_tree_module_refs(component->children[i]);
        if (child_refs) {
            // Merge child refs into our array
            for (int j = 0; j < cJSON_GetArraySize(child_refs); j++) {
                cJSON* item = cJSON_DetachItemFromArray(child_refs, j);
                cJSON_AddItemToArray(refs_array, item);
            }
            cJSON_Delete(child_refs);
        }
    }

    return refs_array;
}

// Restore module_ref from the refs array returned by ir_clear_tree_module_refs
void ir_restore_tree_module_refs(IRComponent* component, cJSON* refs_array) {
    if (!component || !refs_array) return;

    // Create a map by id for quick lookup
    cJSON* id_map = cJSON_CreateObject();

    for (int i = 0; i < cJSON_GetArraySize(refs_array); i++) {
        cJSON* ref = cJSON_GetArrayItem(refs_array, i);
        cJSON* id_obj = cJSON_GetObjectItem(ref, "id");
        if (id_obj && cJSON_IsNumber(id_obj)) {
            cJSON_AddItemToObject(id_map, ref->string, ref);
        }
    }

    // Helper function to restore refs for a component tree
    void restore_for_component(IRComponent* comp) {
        if (!comp) return;

        // Find this component's refs by id
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%u", comp->id);

        cJSON* ref = cJSON_GetObjectItem(id_map, id_str);
        if (ref) {
            cJSON* mod_ref = cJSON_GetObjectItem(ref, "module_ref");
            cJSON* exp_name = cJSON_GetObjectItem(ref, "export_name");

            if (mod_ref && cJSON_IsString(mod_ref)) {
                comp->module_ref = strdup(mod_ref->valuestring);
            }
            if (exp_name && cJSON_IsString(exp_name)) {
                comp->export_name = strdup(exp_name->valuestring);
            }
        }

        // Recursively restore children
        for (uint32_t i = 0; i < comp->child_count; i++) {
            restore_for_component(comp->children[i]);
        }
    }

    restore_for_component(component);
    cJSON_Delete(id_map);
}

// JSON string wrapper for ir_clear_tree_module_refs (for FFI compatibility)
// Returns a JSON string that must be freed by the caller
char* ir_clear_tree_module_refs_json(IRComponent* component) {
    if (!component) return NULL;

    cJSON* refs_array = ir_clear_tree_module_refs(component);
    if (!refs_array) return NULL;

    char* result = cJSON_PrintUnformatted(refs_array);
    cJSON_Delete(refs_array);
    return result;
}

// JSON string wrapper for ir_restore_tree_module_refs (for FFI compatibility)
void ir_restore_tree_module_refs_json(IRComponent* component, const char* json_str) {
    if (!component || !json_str) return;

    cJSON* refs_array = cJSON_Parse(json_str);
    if (refs_array) {
        ir_restore_tree_module_refs(component, refs_array);
        cJSON_Delete(refs_array);
    }
}

// Temporarily clear module_ref for serialization (returns old values for restoration)
// DEPRECATED: Use ir_clear_tree_module_refs instead
char* ir_clear_component_module_ref(IRComponent* component) {
    if (!component) return NULL;
    if (!component->module_ref) return NULL;

    // Create a JSON string with the old values
    cJSON* old_vals = cJSON_CreateObject();
    if (component->module_ref) {
        cJSON_AddStringToObject(old_vals, "module_ref", component->module_ref);
        free(component->module_ref);
        component->module_ref = NULL;
    }
    if (component->export_name) {
        cJSON_AddStringToObject(old_vals, "export_name", component->export_name);
        free(component->export_name);
        component->export_name = NULL;
    }

    char* result = cJSON_PrintUnformatted(old_vals);
    cJSON_Delete(old_vals);
    return result;
}

// Restore module_ref from the JSON string returned by ir_clear_component_module_ref
// DEPRECATED: Use ir_restore_tree_module_refs instead
void ir_restore_component_module_ref(IRComponent* component, const char* json_str) {
    if (!component || !json_str) return;

    cJSON* old_vals = cJSON_Parse(json_str);
    if (old_vals) {
        cJSON* mod_ref = cJSON_GetObjectItem(old_vals, "module_ref");
        cJSON* exp_name = cJSON_GetObjectItem(old_vals, "export_name");

        if (mod_ref && cJSON_IsString(mod_ref)) {
            component->module_ref = strdup(mod_ref->valuestring);
        }
        if (exp_name && cJSON_IsString(exp_name)) {
            component->export_name = strdup(exp_name->valuestring);
        }

        cJSON_Delete(old_vals);
    }
}
