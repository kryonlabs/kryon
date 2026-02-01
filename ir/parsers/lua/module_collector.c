/**
 * Module Collector for Lua Parser
 * Traverses dependency graph and collects all module sources
 */

#define _POSIX_C_SOURCE 200809L

#include "module_collector.h"
#include "require_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// Include IR core for IRModuleSource and IRSourceCollection
#include "ir_core.h"

/**
 * Read entire file into memory
 */
static char* read_file(const char* filepath, size_t* out_len) {
    FILE* f = fopen(filepath, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';

    fclose(f);

    if (out_len) *out_len = read_size;
    return content;
}

/**
 * Get directory path from file path
 */
static void get_directory(const char* filepath, char* dir_buf, size_t buf_size) {
    strncpy(dir_buf, filepath, buf_size - 1);
    dir_buf[buf_size - 1] = '\0';

    char* last_slash = strrchr(dir_buf, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        strcpy(dir_buf, ".");
    }
}

/**
 * Convert file path to module ID
 * e.g., "/path/to/components/calendar.lua" -> "components/calendar"
 * e.g., "/path/to/main.lua" -> "main"
 */
static char* path_to_module_id(const char* file_path, const char* base_dir) {
    if (!file_path || !base_dir) return NULL;

    // Check if file is under base_dir
    size_t base_len = strlen(base_dir);
    if (strncmp(file_path, base_dir, base_len) == 0) {
        const char* rest = file_path + base_len;
        if (*rest == '/') rest++;

        // Remove .lua extension
        char result[512];
        strncpy(result, rest, sizeof(result) - 1);
        result[sizeof(result) - 1] = '\0';

        char* dot_lua = strstr(result, ".lua");
        if (dot_lua) {
            *dot_lua = '\0';
        }

        // Also check for /init.lua
        char* init_lua = strstr(result, "/init");
        if (init_lua) {
            *init_lua = '\0';
        }

        return strdup(result);
    }

    // Fallback: use filename without extension
    const char* filename = strrchr(file_path, '/');
    if (!filename) filename = file_path;
    else filename++;

    char result[512];
    strncpy(result, filename, sizeof(result) - 1);
    result[sizeof(result) - 1] = '\0';

    char* dot_lua = strstr(result, ".lua");
    if (dot_lua) {
        *dot_lua = '\0';
    }

    return strdup(result);
}

/**
 * DFS traversal to collect modules
 */
static void collect_modules_dfs(ModuleCollection* collection,
                                const char* file_path,
                                const char* base_dir,
                                const char* kryon_root,
                                bool is_main) {
    if (!file_path) return;

    // Check if already collected
    if (lua_module_collection_find_by_path(collection, file_path) >= 0) {
        return;
    }

    // Read source file
    size_t source_len = 0;
    char* source = read_file(file_path, &source_len);
    if (!source) {
        fprintf(stderr, "Warning: Could not read file: %s\n", file_path);
        return;
    }

    // Generate module ID
    char* module_id = path_to_module_id(file_path, base_dir);
    if (!module_id) {
        // Fallback: use "module_X" naming
        char buf[64];
        snprintf(buf, sizeof(buf), "module_%d", collection->count);
        module_id = strdup(buf);
    }

    // Add to collection
    ModuleSource* module = lua_module_collection_add(collection, module_id, file_path,
                                                      source, source_len, is_main);

    free(module_id);

    if (!module) {
        free(source);
        return;
    }

    // Extract dependencies
    char source_dir[1024];
    get_directory(file_path, source_dir, sizeof(source_dir));

    RequireGraph* deps = lua_extract_requires(source, source_dir);
    if (!deps) {
        return;
    }

    // Recursively collect dependencies (DFS)
    for (int i = 0; i < deps->count; i++) {
        RequireDependency* dep = &deps->dependencies[i];

        // Skip stdlib modules
        if (dep->is_stdlib) {
            continue;
        }

        // Resolve path if not already resolved
        if (!dep->source_file) {
            dep->source_file = lua_resolve_require_path(dep->module, source_dir, kryon_root);
        }

        if (dep->source_file) {
            // Recursive DFS
            collect_modules_dfs(collection, dep->source_file, base_dir, kryon_root, false);
        }
    }

    lua_require_graph_free(deps);
}

ModuleCollection* lua_collect_modules(const char* main_file, const char* kryon_root) {
    if (!main_file) return NULL;

    ModuleCollection* collection = calloc(1, sizeof(ModuleCollection));
    if (!collection) return NULL;

    collection->capacity = 32;
    collection->modules = calloc(collection->capacity, sizeof(ModuleSource));
    if (!collection->modules) {
        free(collection);
        return NULL;
    }

    // Get base directory (project root)
    char base_dir[1024];
    get_directory(main_file, base_dir, sizeof(base_dir));

    // For components, we might need to go up one level
    // e.g., if main.lua is in project/src/, base_dir is project
    // Check for common patterns
    char* src_dir = strstr(base_dir, "/src");
    if (src_dir) {
        *src_dir = '\0'; // Truncate at /src
    }

    // Start DFS from main file
    collect_modules_dfs(collection, main_file, base_dir, kryon_root, true);

    return collection;
}

void lua_module_collection_free(ModuleCollection* collection) {
    if (!collection) return;

    for (int i = 0; i < collection->count; i++) {
        free(collection->modules[i].module_id);
        free(collection->modules[i].file_path);
        free(collection->modules[i].source);
    }

    free(collection->modules);
    free(collection);
}

int lua_module_collection_find_by_path(ModuleCollection* collection, const char* file_path) {
    if (!collection || !file_path) return -1;

    for (int i = 0; i < collection->count; i++) {
        if (strcmp(collection->modules[i].file_path, file_path) == 0) {
            return i;
        }
    }

    return -1;
}

ModuleSource* lua_module_collection_add(ModuleCollection* collection, const char* module_id,
                                        const char* file_path, const char* source, size_t source_len,
                                        bool is_main) {
    if (!collection || !module_id || !file_path) return NULL;

    // Check for duplicates by module_id
    for (int i = 0; i < collection->count; i++) {
        if (strcmp(collection->modules[i].module_id, module_id) == 0) {
            return &collection->modules[i]; // Already exists
        }
    }

    // Expand if needed
    if (collection->count >= collection->capacity) {
        collection->capacity *= 2;
        ModuleSource* new_modules = realloc(collection->modules,
                                            collection->capacity * sizeof(ModuleSource));
        if (!new_modules) return NULL;
        collection->modules = new_modules;
    }

    ModuleSource* module = &collection->modules[collection->count++];
    module->module_id = strdup(module_id);
    module->file_path = strdup(file_path);
    module->source = source ? strdup(source) : NULL;
    module->source_len = source_len;
    module->is_main = is_main;

    return module;
}

/**
 * Simple SHA-256 hash for source verification
 * (Simplified implementation - for production use a proper crypto library)
 */
static char* hash_source(const char* source, size_t len) {
    if (!source) return NULL;

    // Simple hash for now - can be replaced with proper SHA-256
    unsigned int hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)source[i];
    }

    char* result = malloc(16);
    snprintf(result, 16, "%08x", hash);
    return result;
}

IRSourceCollection* ir_source_collection_from_modules(ModuleCollection* modules) {
    if (!modules) return NULL;

    IRSourceCollection* sources = calloc(1, sizeof(IRSourceCollection));
    if (!sources) return NULL;

    sources->entry_count = modules->count;
    sources->entry_capacity = modules->count;
    sources->entries = calloc(sources->entry_count, sizeof(IRModuleSource));

    if (!sources->entries) {
        free(sources);
        return NULL;
    }

    for (uint32_t i = 0; i < sources->entry_count; i++) {
        ModuleSource* src = &modules->modules[i];
        IRModuleSource* dest = &sources->entries[i];

        dest->module_id = strdup(src->module_id);
        dest->language = strdup("lua");
        dest->source = strdup(src->source);
        dest->hash = hash_source(src->source, src->source_len);
    }

    return sources;
}

/**
 * Free an IRSourceCollection
 */
void ir_source_collection_free(IRSourceCollection* sources) {
    if (!sources) return;

    for (uint32_t i = 0; i < sources->entry_count; i++) {
        free(sources->entries[i].module_id);
        free(sources->entries[i].language);
        free(sources->entries[i].source);
        free(sources->entries[i].hash);
    }

    free(sources->entries);
    free(sources);
}
