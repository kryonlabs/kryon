/**
 * Plugin Discovery for Lua Binary Builds
 *
 * Discovers plugins from kryon.toml configuration and validates
 * that all required files exist. NO auto-discovery - plugins
 * MUST be explicitly listed in config with paths.
 */

#ifndef PLUGIN_DISCOVERY_H
#define PLUGIN_DISCOVERY_H

#include "../../include/kryon_cli.h"

/**
 * Plugin information for building
 * ALL fields are required - if plugin is in config, all files must exist
 */
typedef struct {
    char* name;              /* "storage", "syntax", etc. from config */
    char* plugin_dir;        /* Resolved absolute path to plugin root */
    char* lua_binding;       /* Path to .lua file (e.g., ".../bindings/lua/storage.lua") */
    char* kry_file;          /* Path to .kry file (e.g., ".../plugin.kry") - NULL if not present */
    char* static_lib;        /* Path to .a file (e.g., ".../build/libkryon_storage.a") */
    bool has_kry_file;       /* true if plugin has a plugin.kry file */

    /* Extracted code from plugin.kry (NULL if not extracted) */
    char* lua_code;          /* Extracted @lua block code */
    char* js_code;           /* Extracted @js block code */
    char* c_code;            /* Extracted @c block code */
} BuildPluginInfo;

/**
 * Discover plugins from configuration
 *
 * Reads ONLY the [plugins] section from kryon.toml.
 * Returns NULL if no [plugins] section exists.
 * Exits with error if any plugin is missing required files.
 *
 * @param project_dir  Path to project directory (for resolving relative paths)
 * @param config       KryonConfig containing plugin definitions
 * @param out_count    Output: number of plugins found
 * @return Array of BuildPluginInfo, or NULL if no plugins section
 */
BuildPluginInfo* discover_build_plugins(const char* project_dir,
                                        KryonConfig* config,
                                        int* out_count);

/**
 * Free plugin info array
 *
 * @param plugins  Array to free (created by discover_build_plugins)
 * @param count    Number of plugins in array
 */
void free_build_plugins(BuildPluginInfo* plugins, int count);

/**
 * Write extracted plugin code to build directory
 * Creates build/plugins/<plugin_name>.lua and .js files from plugin.kry
 *
 * @param plugins    Array of plugin info (created by discover_build_plugins)
 * @param count      Number of plugins
 * @param build_dir  Build output directory (e.g., "build")
 * @return 0 on success, -1 on error
 */
int write_plugin_code_files(BuildPluginInfo* plugins, int count, const char* build_dir);

#endif /* PLUGIN_DISCOVERY_H */
