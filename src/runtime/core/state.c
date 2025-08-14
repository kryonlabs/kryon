/**
 * @file state.c
 * @brief Kryon State Management System Implementation
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#include "state.h"
#include "memory.h"
#include <string.h>

#define STATE_MANAGER_INITIAL_CAPACITY 16

// Basic hash function for strings
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619;
    }
    return hash;
}

KryonStateManager *kryon_state_manager_create(void) {
    KryonStateManager *manager = kryon_alloc_type(KryonStateManager);
    if (!manager) return NULL;

    manager->capacity = STATE_MANAGER_INITIAL_CAPACITY;
    manager->count = 0;
    manager->variables = kryon_alloc(manager->capacity * sizeof(KryonStateVariable*));
    if (!manager->variables) {
        kryon_free(manager);
        return NULL;
    }
    memset(manager->variables, 0, manager->capacity * sizeof(KryonStateVariable*));
    return manager;
}

void kryon_state_manager_destroy(KryonStateManager *manager) {
    if (!manager) return;

    for (size_t i = 0; i < manager->capacity; i++) {
        KryonStateVariable *var = manager->variables[i];
        while (var) {
            KryonStateVariable *next = var->next;
            kryon_free(var->name);
            if (var->type == KRYON_STATE_STRING) {
                kryon_free(var->value.string_value);
            }
            kryon_free(var);
            var = next;
        }
    }
    kryon_free(manager->variables);
    kryon_free(manager);
}

static KryonStateVariable* find_variable(KryonStateManager *manager, const char *name) {
    uint32_t index = hash_string(name) % manager->capacity;
    KryonStateVariable *var = manager->variables[index];
    while (var) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }
    return NULL;
}

void kryon_state_manager_set_string(KryonStateManager *manager, const char *name, const char *value) {
    KryonStateVariable *var = find_variable(manager, name);
    if (var) {
        if (var->type == KRYON_STATE_STRING) {
            kryon_free(var->value.string_value);
            var->value.string_value = kryon_strdup(value);
        }
        return;
    }

    uint32_t index = hash_string(name) % manager->capacity;
    var = kryon_alloc_type(KryonStateVariable);
    var->name = kryon_strdup(name);
    var->type = KRYON_STATE_STRING;
    var->value.string_value = kryon_strdup(value);
    var->next = manager->variables[index];
    manager->variables[index] = var;
    manager->count++;
}

const char *kryon_state_manager_get_string(KryonStateManager *manager, const char *name) {
    KryonStateVariable *var = find_variable(manager, name);
    if (var && var->type == KRYON_STATE_STRING) {
        return var->value.string_value;
    }
    return NULL;
}
