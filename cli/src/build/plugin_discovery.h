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
    char* static_lib;        /* Path to .a file (e.g., ".../build/libkryon_storage.a") */
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

#endif /* PLUGIN_DISCOVERY_H */
