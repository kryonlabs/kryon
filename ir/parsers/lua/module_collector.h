/**
 * @file module_collector.h
 * @brief Module Collector for Lua Parser
 *
 * Traverses dependency graph and collects all module sources.
 */

#ifndef MODULE_COLLECTOR_H
#define MODULE_COLLECTOR_H

#include "ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lua-specific module source with additional metadata
 * Extends IRModuleSource with fields needed by the Lua parser
 */
typedef struct ModuleSource {
    char* module_id;      // Module identifier (e.g., "main", "components/calendar")
    bool is_main;         // true if this is the main entry point
    char* file_path;      // Full path to the source file
    char* source;         // Original source code
    size_t source_len;    // Length of source code
    char* hash;           // SHA-256 hash for change detection
} ModuleSource;

/**
 * Collection of Lua modules
 */
typedef struct ModuleCollection {
    ModuleSource* modules;  // Array of modules (changed from IRModuleSource**)
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

/**
 * Collect all Lua modules from a directory
 *
 * @param main_file Path to main.lua file
 * @param kryon_root Kryon root directory (or NULL)
 * @return ModuleCollection or NULL on error
 */
ModuleCollection* lua_collect_modules(const char* main_file, const char* kryon_root);

/**
 * Free a single ModuleSource
 */
void module_source_free(ModuleSource* module);

#ifdef __cplusplus
}
#endif

#endif // MODULE_COLLECTOR_H
