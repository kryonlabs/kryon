/**
 * @file ir_lua_modules.c
 * @brief Implementation of Lua source module collection for KIR
 *
 * This module provides the API for capturing complete Lua source context
 * in KIR, enabling codegens to generate complete, working code without
 * reading Lua files directly. Part of the Lua → KIR → Codegen architecture.
 */

#define _GNU_SOURCE  // For strdup
#include "ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Collection Management
// ============================================================================

IRLuaModuleCollection* ir_lua_modules_create(void) {
    IRLuaModuleCollection* collection = calloc(1, sizeof(IRLuaModuleCollection));
    if (!collection) return NULL;

    collection->module_capacity = 8;
    collection->modules = calloc(collection->module_capacity, sizeof(IRLuaModule));
    if (!collection->modules) {
        free(collection);
        return NULL;
    }

    return collection;
}

static void ir_lua_module_destroy(IRLuaModule* module) {
    if (!module) return;

    free(module->name);
    free(module->source_language);
    free(module->full_source);

    // Free variables
    for (uint32_t i = 0; i < module->variable_count; i++) {
        free(module->variables[i].name);
        free(module->variables[i].initial_value_source);
        free(module->variables[i].var_type);
    }
    free(module->variables);

    // Free functions
    for (uint32_t i = 0; i < module->function_count; i++) {
        IRLuaFunc* func = &module->functions[i];
        free(func->name);
        free(func->source);
        for (uint32_t j = 0; j < func->parameter_count; j++) {
            free(func->parameters[j]);
        }
        free(func->parameters);
        for (uint32_t j = 0; j < func->closure_var_count; j++) {
            free(func->closure_vars[j]);
        }
        free(func->closure_vars);
    }
    free(module->functions);

    // Free foreach providers
    for (uint32_t i = 0; i < module->foreach_provider_count; i++) {
        free(module->foreach_providers[i].container_id);
        free(module->foreach_providers[i].expression);
        free(module->foreach_providers[i].source_function);
    }
    free(module->foreach_providers);

    // Free imports
    for (uint32_t i = 0; i < module->import_count; i++) {
        free(module->imports[i]);
    }
    free(module->imports);
}

void ir_lua_modules_destroy(IRLuaModuleCollection* collection) {
    if (!collection) return;

    for (uint32_t i = 0; i < collection->module_count; i++) {
        ir_lua_module_destroy(&collection->modules[i]);
    }
    free(collection->modules);
    free(collection);
}

// ============================================================================
// Module Management
// ============================================================================

IRLuaModule* ir_lua_modules_add(IRLuaModuleCollection* collection, const char* name, const char* source_language) {
    if (!collection || !name) return NULL;

    // Check if already exists
    IRLuaModule* existing = ir_lua_modules_find(collection, name);
    if (existing) return existing;

    // Grow array if needed
    if (collection->module_count >= collection->module_capacity) {
        uint32_t new_capacity = collection->module_capacity * 2;
        IRLuaModule* new_modules = realloc(collection->modules, new_capacity * sizeof(IRLuaModule));
        if (!new_modules) return NULL;
        collection->modules = new_modules;
        collection->module_capacity = new_capacity;
    }

    // Initialize new module
    IRLuaModule* module = &collection->modules[collection->module_count++];
    memset(module, 0, sizeof(IRLuaModule));

    module->name = strdup(name);
    module->source_language = source_language ? strdup(source_language) : strdup("lua");

    // Initialize capacities
    module->variable_capacity = 16;
    module->variables = calloc(module->variable_capacity, sizeof(IRLuaVariable));

    module->function_capacity = 32;
    module->functions = calloc(module->function_capacity, sizeof(IRLuaFunc));

    module->foreach_provider_capacity = 8;
    module->foreach_providers = calloc(module->foreach_provider_capacity, sizeof(IRLuaForEachProvider));

    module->import_capacity = 16;
    module->imports = calloc(module->import_capacity, sizeof(char*));

    return module;
}

IRLuaModule* ir_lua_modules_find(IRLuaModuleCollection* collection, const char* name) {
    if (!collection || !name) return NULL;

    for (uint32_t i = 0; i < collection->module_count; i++) {
        if (collection->modules[i].name && strcmp(collection->modules[i].name, name) == 0) {
            return &collection->modules[i];
        }
    }
    return NULL;
}

// ============================================================================
// Variable Management
// ============================================================================

void ir_lua_module_add_variable(IRLuaModule* module, const char* name, const char* initial_value, bool is_reactive, const char* var_type) {
    if (!module || !name) return;

    // Grow array if needed
    if (module->variable_count >= module->variable_capacity) {
        uint32_t new_capacity = module->variable_capacity * 2;
        IRLuaVariable* new_vars = realloc(module->variables, new_capacity * sizeof(IRLuaVariable));
        if (!new_vars) return;
        module->variables = new_vars;
        module->variable_capacity = new_capacity;
    }

    IRLuaVariable* var = &module->variables[module->variable_count++];
    var->name = strdup(name);
    var->initial_value_source = initial_value ? strdup(initial_value) : NULL;
    var->is_reactive = is_reactive;
    var->var_type = var_type ? strdup(var_type) : strdup("local");
}

// ============================================================================
// Function Management
// ============================================================================

void ir_lua_module_add_function(IRLuaModule* module, const char* name, const char* source, bool is_local) {
    if (!module || !name) return;

    // Grow array if needed
    if (module->function_count >= module->function_capacity) {
        uint32_t new_capacity = module->function_capacity * 2;
        IRLuaFunc* new_funcs = realloc(module->functions, new_capacity * sizeof(IRLuaFunc));
        if (!new_funcs) return;
        module->functions = new_funcs;
        module->function_capacity = new_capacity;
    }

    IRLuaFunc* func = &module->functions[module->function_count++];
    memset(func, 0, sizeof(IRLuaFunc));
    func->name = strdup(name);
    func->source = source ? strdup(source) : NULL;
    func->is_local = is_local;
}

// ============================================================================
// Import Management
// ============================================================================

void ir_lua_module_add_import(IRLuaModule* module, const char* import_name) {
    if (!module || !import_name) return;

    // Check if already imported
    for (uint32_t i = 0; i < module->import_count; i++) {
        if (strcmp(module->imports[i], import_name) == 0) return;
    }

    // Grow array if needed
    if (module->import_count >= module->import_capacity) {
        uint32_t new_capacity = module->import_capacity * 2;
        char** new_imports = realloc(module->imports, new_capacity * sizeof(char*));
        if (!new_imports) return;
        module->imports = new_imports;
        module->import_capacity = new_capacity;
    }

    module->imports[module->import_count++] = strdup(import_name);
}

// ============================================================================
// ForEach Provider Management
// ============================================================================

void ir_lua_module_add_foreach_provider(IRLuaModule* module, const char* container_id, const char* expression, const char* source_function) {
    if (!module || !container_id || !expression) return;

    // Grow array if needed
    if (module->foreach_provider_count >= module->foreach_provider_capacity) {
        uint32_t new_capacity = module->foreach_provider_capacity * 2;
        IRLuaForEachProvider* new_providers = realloc(module->foreach_providers, new_capacity * sizeof(IRLuaForEachProvider));
        if (!new_providers) return;
        module->foreach_providers = new_providers;
        module->foreach_provider_capacity = new_capacity;
    }

    IRLuaForEachProvider* provider = &module->foreach_providers[module->foreach_provider_count++];
    provider->container_id = strdup(container_id);
    provider->expression = strdup(expression);
    provider->source_function = source_function ? strdup(source_function) : NULL;
}

// ============================================================================
// Debug / Print
// ============================================================================

void ir_lua_modules_print(IRLuaModuleCollection* collection) {
    if (!collection) {
        printf("IRLuaModuleCollection: (null)\n");
        return;
    }

    printf("IRLuaModuleCollection: %u modules\n", collection->module_count);
    for (uint32_t i = 0; i < collection->module_count; i++) {
        IRLuaModule* mod = &collection->modules[i];
        printf("  Module[%u]: %s (%s)\n", i, mod->name, mod->source_language);
        printf("    Variables: %u\n", mod->variable_count);
        for (uint32_t j = 0; j < mod->variable_count; j++) {
            printf("      - %s (%s)%s\n", mod->variables[j].name, mod->variables[j].var_type,
                   mod->variables[j].is_reactive ? " [reactive]" : "");
        }
        printf("    Functions: %u\n", mod->function_count);
        for (uint32_t j = 0; j < mod->function_count; j++) {
            printf("      - %s%s\n", mod->functions[j].name, mod->functions[j].is_local ? " (local)" : "");
        }
        printf("    Imports: %u\n", mod->import_count);
        for (uint32_t j = 0; j < mod->import_count; j++) {
            printf("      - %s\n", mod->imports[j]);
        }
        printf("    ForEach Providers: %u\n", mod->foreach_provider_count);
        for (uint32_t j = 0; j < mod->foreach_provider_count; j++) {
            printf("      - %s: %s\n", mod->foreach_providers[j].container_id, mod->foreach_providers[j].expression);
        }
    }
}
