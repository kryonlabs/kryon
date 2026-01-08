/**
 * Require Tracker for Lua Parser
 * Extracts require() dependencies via static analysis
 */

#ifndef REQUIRE_TRACKER_H
#define REQUIRE_TRACKER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Individual require dependency
 */
typedef struct RequireDependency {
    char* module;         // Module name (e.g., "components.calendar")
    char* source_file;    // Resolved file path
    bool is_relative;     // True for local requires (components/*)
    bool is_stdlib;       // True for standard library requires
    bool visited;         // For cycle detection during DFS
} RequireDependency;

/**
 * Collection of require dependencies
 */
typedef struct RequireGraph {
    RequireDependency* dependencies;
    int count;
    int capacity;
} RequireGraph;

/**
 * Extract require() calls from Lua source using regex
 * Pattern: require\s*\(\s*["']([^"']+)["']\s*\)
 *
 * @param source Lua source code
 * @param source_dir Directory containing the source file (for resolving relative paths)
 * @return Newly allocated RequireGraph, or NULL on failure
 */
RequireGraph* lua_extract_requires(const char* source, const char* source_dir);

/**
 * Resolve a require() module name to a file path
 *
 * - "components.calendar" -> "components/calendar.lua"
 * - "kryon.dsl" -> "$KRYON_ROOT/bindings/lua/kryon/dsl.lua"
 * - "my.local.module" -> "my/local/module.lua"
 *
 * @param module Module name from require()
 * @param source_dir Directory containing the requiring file
 * @param kryon_root Kryon root directory (can be NULL, uses env var or detection)
 * @return Newly allocated file path, or NULL if not found
 */
char* lua_resolve_require_path(const char* module, const char* source_dir, const char* kryon_root);

/**
 * Free a require graph
 */
void lua_require_graph_free(RequireGraph* graph);

/**
 * Add a dependency to the graph
 */
void lua_require_graph_add(RequireGraph* graph, const char* module, const char* source_file,
                           bool is_relative, bool is_stdlib);

/**
 * Check if a module is a standard library module
 */
bool lua_is_stdlib_module(const char* module);

#ifdef __cplusplus
}
#endif

#endif // REQUIRE_TRACKER_H
