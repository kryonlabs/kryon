/**
 * @file ir_json_context.c
 * @brief Component definition context management for JSON serialization
 */

#define _GNU_SOURCE
#include "../include/ir_json_context.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Global Cache
// ============================================================================

// Global cache for module definitions
static ComponentDefContext* g_module_def_cache = NULL;

// ============================================================================
// Context Management
// ============================================================================

ComponentDefContext* ir_json_context_create(void) {
    ComponentDefContext* ctx = calloc(1, sizeof(ComponentDefContext));
    if (!ctx) return NULL;

    ctx->capacity = 8;
    ctx->entries = calloc(ctx->capacity, sizeof(ComponentDefEntry));
    if (!ctx->entries) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void ir_json_context_free(ComponentDefContext* ctx) {
    if (!ctx) return;

    for (int i = 0; i < ctx->count; i++) {
        free(ctx->entries[i].name);
        // Note: definition JSON is part of root cJSON, freed separately
    }

    free(ctx->entries);
    free(ctx);
}

// ============================================================================
// Definition Registration and Lookup
// ============================================================================

bool ir_json_context_add(ComponentDefContext* ctx, const char* name, cJSON* definition) {
    if (!ctx || !name || !definition) return false;

    // Expand capacity if needed
    if (ctx->count >= ctx->capacity) {
        int new_capacity = ctx->capacity * 2;
        ComponentDefEntry* new_entries = realloc(ctx->entries,
            new_capacity * sizeof(ComponentDefEntry));
        if (!new_entries) return false;

        ctx->entries = new_entries;
        ctx->capacity = new_capacity;
    }

    ctx->entries[ctx->count].name = strdup(name);
    ctx->entries[ctx->count].definition = definition;
    ctx->count++;

    return true;
}

cJSON* ir_json_context_lookup(ComponentDefContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;

    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->entries[i].name, name) == 0) {
            return ctx->entries[i].definition;
        }
    }

    return NULL;
}

// Find a component definition by module ID prefix (for $module: refs without export name)
// Returns the first definition that starts with the given module ID
cJSON* ir_json_context_lookup_by_module(ComponentDefContext* ctx, const char* module_id) {
    if (!ctx || !module_id) return NULL;

    size_t prefix_len = strlen(module_id);
    for (int i = 0; i < ctx->count; i++) {
        const char* entry_name = ctx->entries[i].name;
        // Check if entry name starts with module_id followed by '/'
        if (strncmp(entry_name, module_id, prefix_len) == 0 &&
            entry_name[prefix_len] == '/') {
            return ctx->entries[i].definition;
        }
    }

    return NULL;
}

// ============================================================================
// Global Cache Management
// ============================================================================

void ir_json_context_cache_init(void) {
    if (!g_module_def_cache) {
        g_module_def_cache = ir_json_context_create();
    }
}

void ir_json_context_cache_shutdown(void) {
    ir_json_context_free(g_module_def_cache);
    g_module_def_cache = NULL;
}

ComponentDefContext* ir_json_context_cache_get(void) {
    return g_module_def_cache;
}
