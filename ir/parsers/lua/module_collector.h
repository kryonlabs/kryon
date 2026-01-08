/**
 * Module Collector for Lua Parser
 * Traverses dependency graph and collects all module sources
 */

#ifndef MODULE_COLLECTOR_H
#define MODULE_COLLECTOR_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct IRModuleSource IRModuleSource;
typedef struct IRSourceCollection IRSourceCollection;

/**
 * Individual module source
 */
typedef struct ModuleSource {
    char* module_id;      // Canonical ID (e.g., "components/calendar")
    char* file_path;      // Original file path
    char* source;         // File contents
    size_t source_len;
    bool is_main;         // True for the entry point module
} ModuleSource;

/**
 * Collection of module sources
 */
typedef struct ModuleCollection {
    ModuleSource* modules;
    int count;
    int capacity;
} ModuleCollection;

/**
 * Collect all module sources starting from entry point
 * Uses DFS traversal of dependency graph
 *
 * @param main_file Path to the main Lua file
 * @param kryon_root Kryon root directory (can be NULL, will be detected)
 * @return Newly allocated ModuleCollection, or NULL on failure
 */
ModuleCollection* lua_collect_modules(const char* main_file, const char* kryon_root);

/**
 * Free a module collection
 */
void lua_module_collection_free(ModuleCollection* collection);

/**
 * Convert module collection to IRSourceCollection
 *
 * @param modules Module collection to convert
 * @return Newly allocated IRSourceCollection, or NULL on failure
 */
IRSourceCollection* ir_source_collection_from_modules(ModuleCollection* modules);

/**
 * Find a module by file path in the collection
 *
 * @param collection Module collection to search
 * @param file_path File path to find
 * @return Index of module, or -1 if not found
 */
int lua_module_collection_find_by_path(ModuleCollection* collection, const char* file_path);

/**
 * Add a module to the collection
 */
ModuleSource* lua_module_collection_add(ModuleCollection* collection, const char* module_id,
                                        const char* file_path, const char* source, size_t source_len,
                                        bool is_main);

#ifdef __cplusplus
}
#endif

#endif // MODULE_COLLECTOR_H
