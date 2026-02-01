/**
 * @file module_collector.h
 * @brief Module Collector for Lua Parser
 *
 * Traverses dependency graph and collects all module sources.
 */

#ifndef MODULE_COLLECTOR_H
#define MODULE_COLLECTOR_H

#include "ir/ir_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Collection of Lua modules
 */
typedef struct ModuleCollection {
    IRModuleSource** modules;
    size_t count;
    size_t capacity;
} ModuleCollection;

/**
 * Free a module collection
 */
void lua_module_collection_free(ModuleCollection* collection);

/**
 * Convert module collection to IR source collection
 */
IRSourceCollection* ir_source_collection_from_modules(ModuleCollection* modules);

/**
 * Free an IR source collection
 */
void ir_source_collection_free(IRSourceCollection* sources);

#ifdef __cplusplus
}
#endif

#endif // MODULE_COLLECTOR_H
