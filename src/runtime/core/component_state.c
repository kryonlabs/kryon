/**
 * @file component_state.c
 * @brief Kryon Component State Management Implementation
 *
 * Implements type-safe, isolated state management for component instances
 * with unique ID generation and hash-based lookup.
 *
 * 0BSD License
 */

#include "component_state.h"
#include "memory.h"
#include "runtime.h"
#include "elements.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define COMPONENT_STATE_INITIAL_CAPACITY 16
#define MAX_ID_LENGTH 256
#define ID_COUNTER_START 1000

// Global counter for unique ID generation
static uint32_t g_component_id_counter = ID_COUNTER_START;

// =============================================================================
//  Helper Functions
// =============================================================================

static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619;
    }
    return hash;
}

static KryonComponentStateVariable* find_state_variable(KryonComponentStateTable *table, const char *name) {
    uint32_t index = hash_string(name) % table->capacity;
    KryonComponentStateVariable *var = table->variables[index];
    while (var) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }
    return NULL;
}

static bool resize_state_table(KryonComponentStateTable *table) {
    size_t new_capacity = table->capacity * 2;
    KryonComponentStateVariable **new_variables = kryon_alloc(new_capacity * sizeof(KryonComponentStateVariable*));
    if (!new_variables) {
        return false;
    }

    memset(new_variables, 0, new_capacity * sizeof(KryonComponentStateVariable*));

    // Rehash all existing variables
    for (size_t i = 0; i < table->capacity; i++) {
        KryonComponentStateVariable *var = table->variables[i];
        while (var) {
            KryonComponentStateVariable *next = var->next;
            uint32_t new_index = hash_string(var->name) % new_capacity;
            var->next = new_variables[new_index];
            new_variables[new_index] = var;
            var = next;
        }
    }

    kryon_free(table->variables);
    table->variables = new_variables;
    table->capacity = new_capacity;
    return true;
}

// =============================================================================
//  Component ID Generation
// =============================================================================

char* kryon_component_generate_unique_id(const char *component_name, const char *user_id) {
    char *unique_id = kryon_alloc(MAX_ID_LENGTH);
    if (!unique_id) {
        return NULL;
    }

    if (user_id && strlen(user_id) > 0) {
        // Use user-provided ID with suffix to ensure uniqueness
        snprintf(unique_id, MAX_ID_LENGTH, "%s_%u", user_id, g_component_id_counter++);
    } else if (component_name && strlen(component_name) > 0) {
        // Generate ID based on component name
        snprintf(unique_id, MAX_ID_LENGTH, "%s_%u", component_name, g_component_id_counter++);
    } else {
        // Generate generic ID
        snprintf(unique_id, MAX_ID_LENGTH, "component_%u", g_component_id_counter++);
    }

    return unique_id;
}

bool kryon_component_is_id_unique(struct KryonRuntime *runtime, const char *component_id) {
    if (!runtime || !component_id) {
        return false;
    }

    // Check all component instances for duplicate ID
    for (size_t i = 0; i < runtime->element_count; i++) {
        KryonElement *element = runtime->elements[i];
        if (element && element->component_instance) {
            KryonComponentInstance *instance = element->component_instance;
            if (instance->component_id && strcmp(instance->component_id, component_id) == 0) {
                return false; // Duplicate found
            }
        }
    }

    return true; // Unique
}

// =============================================================================
//  Component State Table Management
// =============================================================================

KryonComponentStateTable* kryon_component_state_table_create(const char *component_id) {
    if (!component_id) {
        return NULL;
    }

    KryonComponentStateTable *table = kryon_alloc_type(KryonComponentStateTable);
    if (!table) {
        return NULL;
    }

    table->capacity = COMPONENT_STATE_INITIAL_CAPACITY;
    table->count = 0;
    table->component_id = kryon_strdup(component_id);

    table->variables = kryon_alloc(table->capacity * sizeof(KryonComponentStateVariable*));
    if (!table->variables || !table->component_id) {
        kryon_free(table->component_id);
        kryon_free(table->variables);
        kryon_free(table);
        return NULL;
    }

    memset(table->variables, 0, table->capacity * sizeof(KryonComponentStateVariable*));
    return table;
}

void kryon_component_state_table_destroy(KryonComponentStateTable *table) {
    if (!table) {
        return;
    }

    // Free all variables
    for (size_t i = 0; i < table->capacity; i++) {
        KryonComponentStateVariable *var = table->variables[i];
        while (var) {
            KryonComponentStateVariable *next = var->next;
            kryon_free(var->name);

            // Free value based on type
            if (var->type == KRYON_COMPONENT_STATE_STRING) {
                kryon_free(var->value.string_value);
            }

            kryon_free(var);
            var = next;
        }
    }

    kryon_free(table->variables);
    kryon_free(table->component_id);
    kryon_free(table);
}

bool kryon_component_state_set_string(KryonComponentStateTable *table, const char *name, const char *value) {
    if (!table || !name) {
        return false;
    }

    // Check if we need to resize
    if (table->count >= table->capacity * 0.75) {
        if (!resize_state_table(table)) {
            return false;
        }
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var) {
        // Update existing variable
        if (var->type == KRYON_COMPONENT_STATE_STRING) {
            kryon_free(var->value.string_value);
        }
        var->type = KRYON_COMPONENT_STATE_STRING;
        var->value.string_value = kryon_strdup(value);
        return var->value.string_value != NULL;
    }

    // Create new variable
    var = kryon_alloc_type(KryonComponentStateVariable);
    if (!var) {
        return false;
    }

    var->name = kryon_strdup(name);
    var->type = KRYON_COMPONENT_STATE_STRING;
    var->value.string_value = kryon_strdup(value);

    if (!var->name || !var->value.string_value) {
        kryon_free(var->name);
        kryon_free(var->value.string_value);
        kryon_free(var);
        return false;
    }

    // Add to hash table
    uint32_t index = hash_string(name) % table->capacity;
    var->next = table->variables[index];
    table->variables[index] = var;
    table->count++;

    return true;
}

const char* kryon_component_state_get_string(KryonComponentStateTable *table, const char *name) {
    if (!table || !name) {
        return NULL;
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var && var->type == KRYON_COMPONENT_STATE_STRING) {
        return var->value.string_value;
    }
    return NULL;
}

bool kryon_component_state_set_int(KryonComponentStateTable *table, const char *name, int64_t value) {
    if (!table || !name) {
        return false;
    }

    // Check if we need to resize
    if (table->count >= table->capacity * 0.75) {
        if (!resize_state_table(table)) {
            return false;
        }
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var) {
        // Update existing variable
        if (var->type == KRYON_COMPONENT_STATE_STRING) {
            kryon_free(var->value.string_value);
        }
        var->type = KRYON_COMPONENT_STATE_INTEGER;
        var->value.int_value = value;
        return true;
    }

    // Create new variable
    var = kryon_alloc_type(KryonComponentStateVariable);
    if (!var) {
        return false;
    }

    var->name = kryon_strdup(name);
    var->type = KRYON_COMPONENT_STATE_INTEGER;
    var->value.int_value = value;

    if (!var->name) {
        kryon_free(var->name);
        kryon_free(var);
        return false;
    }

    // Add to hash table
    uint32_t index = hash_string(name) % table->capacity;
    var->next = table->variables[index];
    table->variables[index] = var;
    table->count++;

    return true;
}

bool kryon_component_state_get_int(KryonComponentStateTable *table, const char *name, int64_t *out_value) {
    if (!table || !name || !out_value) {
        return false;
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var && var->type == KRYON_COMPONENT_STATE_INTEGER) {
        *out_value = var->value.int_value;
        return true;
    }
    return false;
}

bool kryon_component_state_set_float(KryonComponentStateTable *table, const char *name, double value) {
    if (!table || !name) {
        return false;
    }

    // Check if we need to resize
    if (table->count >= table->capacity * 0.75) {
        if (!resize_state_table(table)) {
            return false;
        }
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var) {
        // Update existing variable
        if (var->type == KRYON_COMPONENT_STATE_STRING) {
            kryon_free(var->value.string_value);
        }
        var->type = KRYON_COMPONENT_STATE_FLOAT;
        var->value.float_value = value;
        return true;
    }

    // Create new variable
    var = kryon_alloc_type(KryonComponentStateVariable);
    if (!var) {
        return false;
    }

    var->name = kryon_strdup(name);
    var->type = KRYON_COMPONENT_STATE_FLOAT;
    var->value.float_value = value;

    if (!var->name) {
        kryon_free(var->name);
        kryon_free(var);
        return false;
    }

    // Add to hash table
    uint32_t index = hash_string(name) % table->capacity;
    var->next = table->variables[index];
    table->variables[index] = var;
    table->count++;

    return true;
}

bool kryon_component_state_get_float(KryonComponentStateTable *table, const char *name, double *out_value) {
    if (!table || !name || !out_value) {
        return false;
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var && var->type == KRYON_COMPONENT_STATE_FLOAT) {
        *out_value = var->value.float_value;
        return true;
    }
    return false;
}

bool kryon_component_state_set_bool(KryonComponentStateTable *table, const char *name, bool value) {
    if (!table || !name) {
        return false;
    }

    // Check if we need to resize
    if (table->count >= table->capacity * 0.75) {
        if (!resize_state_table(table)) {
            return false;
        }
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var) {
        // Update existing variable
        if (var->type == KRYON_COMPONENT_STATE_STRING) {
            kryon_free(var->value.string_value);
        }
        var->type = KRYON_COMPONENT_STATE_BOOLEAN;
        var->value.bool_value = value;
        return true;
    }

    // Create new variable
    var = kryon_alloc_type(KryonComponentStateVariable);
    if (!var) {
        return false;
    }

    var->name = kryon_strdup(name);
    var->type = KRYON_COMPONENT_STATE_BOOLEAN;
    var->value.bool_value = value;

    if (!var->name) {
        kryon_free(var->name);
        kryon_free(var);
        return false;
    }

    // Add to hash table
    uint32_t index = hash_string(name) % table->capacity;
    var->next = table->variables[index];
    table->variables[index] = var;
    table->count++;

    return true;
}

bool kryon_component_state_get_bool(KryonComponentStateTable *table, const char *name, bool *out_value) {
    if (!table || !name || !out_value) {
        return false;
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (var && var->type == KRYON_COMPONENT_STATE_BOOLEAN) {
        *out_value = var->value.bool_value;
        return true;
    }
    return false;
}

char* kryon_component_state_get_as_string(KryonComponentStateTable *table, const char *name) {
    if (!table || !name) {
        return NULL;
    }

    KryonComponentStateVariable *var = find_state_variable(table, name);
    if (!var) {
        return NULL;
    }

    char *result = kryon_alloc(64); // Sufficient for most conversions
    if (!result) {
        return NULL;
    }

    switch (var->type) {
        case KRYON_COMPONENT_STATE_STRING:
            kryon_free(result);
            return kryon_strdup(var->value.string_value);
        case KRYON_COMPONENT_STATE_INTEGER:
            snprintf(result, 64, "%lld", (long long)var->value.int_value);
            return result;
        case KRYON_COMPONENT_STATE_FLOAT:
            snprintf(result, 64, "%.6f", var->value.float_value);
            return result;
        case KRYON_COMPONENT_STATE_BOOLEAN:
            kryon_free(result);
            return kryon_strdup(var->value.bool_value ? "true" : "false");
    }

    kryon_free(result);
    return NULL;
}

bool kryon_component_state_has_variable(KryonComponentStateTable *table, const char *name) {
    return find_state_variable(table, name) != NULL;
}

bool kryon_component_state_remove_variable(KryonComponentStateTable *table, const char *name) {
    if (!table || !name) {
        return false;
    }

    uint32_t index = hash_string(name) % table->capacity;
    KryonComponentStateVariable *prev = NULL;
    KryonComponentStateVariable *var = table->variables[index];

    while (var) {
        if (strcmp(var->name, name) == 0) {
            if (prev) {
                prev->next = var->next;
            } else {
                table->variables[index] = var->next;
            }

            kryon_free(var->name);
            if (var->type == KRYON_COMPONENT_STATE_STRING) {
                kryon_free(var->value.string_value);
            }
            kryon_free(var);
            table->count--;
            return true;
        }
        prev = var;
        var = var->next;
    }

    return false;
}

// =============================================================================
//  Component Instance Management
// =============================================================================

KryonComponentInstance* kryon_component_instance_create(struct KryonRuntime *runtime,
                                                      KryonComponentDefinition *definition,
                                                      const char *user_id,
                                                      struct KryonElement *parent_element) {
    if (!runtime || !definition) {
        return NULL;
    }

    KryonComponentInstance *instance = kryon_alloc_type(KryonComponentInstance);
    if (!instance) {
        return NULL;
    }

    memset(instance, 0, sizeof(KryonComponentInstance));
    instance->definition = definition;
    instance->instance_id = runtime->next_element_id++;

    // Generate unique component ID
    instance->component_id = kryon_component_generate_unique_id(definition->name, user_id);
    if (!instance->component_id) {
        kryon_free(instance);
        return NULL;
    }

    // Create state table
    instance->state_table = kryon_component_state_table_create(instance->component_id);
    if (!instance->state_table) {
        kryon_free(instance->component_id);
        kryon_free(instance);
        return NULL;
    }

  
    return instance;
}

void kryon_component_instance_destroy(KryonComponentInstance *instance) {
    if (!instance) {
        return;
    }

    kryon_free(instance->component_id);
    kryon_component_state_table_destroy(instance->state_table);

    
    kryon_free(instance);
}

KryonComponentInstance* kryon_component_instance_find_by_id(struct KryonRuntime *runtime, const char *component_id) {
    if (!runtime || !component_id) {
        return NULL;
    }

    for (size_t i = 0; i < runtime->element_count; i++) {
        KryonElement *element = runtime->elements[i];
        if (element && element->component_instance) {
            KryonComponentInstance *instance = element->component_instance;
            if (instance->component_id && strcmp(instance->component_id, component_id) == 0) {
                return instance;
            }
        }
    }

    return NULL;
}

bool kryon_component_instance_initialize_state(KryonComponentInstance *instance) {
    if (!instance || !instance->definition || !instance->state_table) {
        return false;
    }

    // Initialize state variables from definition defaults
    for (size_t i = 0; i < instance->definition->state_count; i++) {
        KryonComponentStateVar *def_var = &instance->definition->state_vars[i];
        if (def_var->name && def_var->default_value) {
            // Store in new typed state system
            kryon_component_state_set_string(instance->state_table, def_var->name, def_var->default_value);
        }
    }

    return true;
}

// =============================================================================
//  State Resolution
// =============================================================================

char* kryon_component_get_state_path(const char *component_id, const char *variable_name) {
    if (!component_id || !variable_name) {
        return NULL;
    }

    char *path = kryon_alloc(strlen(component_id) + strlen(variable_name) + 2);
    if (!path) {
        return NULL;
    }

    sprintf(path, "%s.%s", component_id, variable_name);
    return path;
}

const char* kryon_resolve_scoped_state_variable(struct KryonRuntime *runtime,
                                              struct KryonElement *element,
                                              const char *state_path) {
    if (!runtime || !element || !state_path) {
        return NULL;
    }

    // Check if the path contains a component ID (scoped path)
    char *dot = strchr(state_path, '.');
    if (dot) {
        // Scoped path: "component_id.variable_name"
        size_t component_id_len = dot - state_path;
        char *component_id = kryon_alloc(component_id_len + 1);
        if (!component_id) {
            return NULL;
        }

        strncpy(component_id, state_path, component_id_len);
        component_id[component_id_len] = '\0';

        const char *variable_name = dot + 1;

        // Find component instance by ID
        KryonComponentInstance *instance = kryon_component_instance_find_by_id(runtime, component_id);
        kryon_free(component_id);

        if (instance && instance->state_table) {
            char *value = kryon_component_state_get_as_string(instance->state_table, variable_name);
            if (value) {
                // Note: This returns a temporary string that needs to be freed by the caller
                // For now, we'll return it directly, but in a real implementation we'd need
                // a better memory management strategy
                return value;
            }
        }
    } else {
        // Simple variable name - search up the element tree
        struct KryonElement *current = element;
        while (current) {
            if (current->component_instance && current->component_instance->state_table) {
                char *value = kryon_component_state_get_as_string(current->component_instance->state_table, state_path);
                if (value) {
                    return value;
                }
            }
            current = current->parent;
        }
    }

    return NULL;
}