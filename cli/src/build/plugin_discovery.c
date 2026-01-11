/**
 * Plugin Discovery for Lua Binary Builds
 *
 * Discovers plugins from kryon.toml configuration and validates
 * that all required files exist. NO auto-discovery - plugins
 * MUST be explicitly listed in config with paths.
 */

#define _POSIX_C_SOURCE 200809L

#include "plugin_discovery.h"
#include "../../include/kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

/* Path buffer size */
#define DISCOVERY_PATH_MAX 4096

/**
 * Check if a directory exists
 */
static int dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * Resolve a path relative to project directory
 * Returns allocated string that must be freed
 */
static char* resolve_path(const char* project_dir, const char* config_path) {
    char resolved[DISCOVERY_PATH_MAX];

    /* If absolute path, use as-is */
    if (config_path[0] == '/') {
        return strdup(config_path);
    }

    /* Relative path - resolve against project directory */
    snprintf(resolved, sizeof(resolved), "%s/%s", project_dir, config_path);

    /* Canonicalize by resolving .. and . */
    char* canonical = realpath(resolved, NULL);
    if (canonical) {
        return canonical;
    }

    /* If realpath fails (path doesn't exist yet), return the resolved path */
    return strdup(resolved);
}

/**
 * Verify plugin has all required files
 * Returns 0 on success, -1 on error (prints error message)
 */
static int verify_plugin_files(const char* plugin_dir, const char* plugin_name,
                               char* out_lua_path, size_t lua_path_size,
                               char* out_lib_path, size_t lib_path_size) {
    /* Check for Lua binding: <plugin_dir>/bindings/lua/<plugin_name>.lua */
    snprintf(out_lua_path, lua_path_size, "%s/bindings/lua/%s.lua", plugin_dir, plugin_name);

    if (!file_exists(out_lua_path)) {
        fprintf(stderr, "Error: Plugin '%s' Lua binding not found: %s\n",
                plugin_name, out_lua_path);
        fprintf(stderr, "       Plugin must have: bindings/lua/%s.lua\n", plugin_name);
        return -1;
    }

    /* Check for static library: <plugin_dir>/build/libkryon_<plugin_name>.a */
    snprintf(out_lib_path, lib_path_size, "%s/build/libkryon_%s.a", plugin_dir, plugin_name);

    if (!file_exists(out_lib_path)) {
        fprintf(stderr, "Error: Plugin '%s' static library not found: %s\n",
                plugin_name, out_lib_path);
        fprintf(stderr, "       Plugin must have: build/libkryon_%s.a\n", plugin_name);
        fprintf(stderr, "       Run 'make' in the plugin directory to build it.\n");
        return -1;
    }

    return 0;
}

/**
 * Discover plugins from configuration
 *
 * Reads ONLY the [plugins] section from kryon.toml.
 * Returns NULL if no [plugins] section exists.
 * Exits with error if any plugin is missing required files.
 */
BuildPluginInfo* discover_build_plugins(const char* project_dir,
                                        KryonConfig* config,
                                        int* out_count) {
    *out_count = 0;

    /* Check if config has plugins */
    if (!config || config->plugins_count == 0) {
        return NULL;
    }

    int plugin_count = config->plugins_count;
    BuildPluginInfo* plugins = calloc(plugin_count, sizeof(BuildPluginInfo));
    if (!plugins) {
        fprintf(stderr, "Error: Out of memory allocating plugin info\n");
        return NULL;
    }

    printf("[Plugins] Discovering %d plugin(s) from configuration...\n", plugin_count);

    /* Process each plugin from config */
    int valid_count = 0;
    for (int i = 0; i < plugin_count; i++) {
        PluginDep* dep = &config->plugins[i];

        /* Check if plugin is disabled */
        if (!dep->enabled) {
            printf("  - %s: disabled, skipping\n", dep->name);
            continue;
        }

        /* Path is REQUIRED */
        if (!dep->path || dep->path[0] == '\0') {
            fprintf(stderr, "Error: Plugin '%s' missing required 'path' field in kryon.toml\n",
                    dep->name ? dep->name : "<unknown>");
            fprintf(stderr, "       Required format:\n");
            fprintf(stderr, "         [plugins.%s]\n", dep->name ? dep->name : "name");
            fprintf(stderr, "         path = \"../path/to/plugin\"\n");
            free(plugins);
            exit(1);
        }

        /* Resolve plugin directory path */
        char* plugin_dir = resolve_path(project_dir, dep->path);
        if (!plugin_dir) {
            fprintf(stderr, "Error: Failed to resolve path for plugin '%s': %s\n",
                    dep->name, dep->path);
            free(plugins);
            exit(1);
        }

        /* Verify plugin directory exists */
        if (!dir_exists(plugin_dir)) {
            fprintf(stderr, "Error: Plugin directory not found: %s\n", plugin_dir);
            fprintf(stderr, "       Specified in kryon.toml for plugin '%s'\n", dep->name);
            free(plugin_dir);
            free(plugins);
            exit(1);
        }

        /* Verify plugin has all required files */
        char lua_path[DISCOVERY_PATH_MAX];
        char lib_path[DISCOVERY_PATH_MAX];

        if (verify_plugin_files(plugin_dir, dep->name,
                               lua_path, sizeof(lua_path),
                               lib_path, sizeof(lib_path)) != 0) {
            free(plugin_dir);
            free(plugins);
            exit(1);
        }

        /* All checks passed - store plugin info */
        BuildPluginInfo* info = &plugins[valid_count];
        info->name = strdup(dep->name);
        info->plugin_dir = plugin_dir;
        info->lua_binding = strdup(lua_path);
        info->static_lib = strdup(lib_path);

        printf("  - %s: OK\n", dep->name);
        printf("    dir:  %s\n", plugin_dir);
        printf("    lua:  %s\n", lua_path);
        printf("    lib:  %s\n", lib_path);

        valid_count++;
    }

    *out_count = valid_count;

    if (valid_count == 0) {
        free(plugins);
        printf("[Plugins] No enabled plugins found\n");
        return NULL;
    }

    printf("[Plugins] Discovered %d plugin(s)\n", valid_count);
    return plugins;
}

/**
 * Free plugin info array
 */
void free_build_plugins(BuildPluginInfo* plugins, int count) {
    if (!plugins) return;

    for (int i = 0; i < count; i++) {
        free(plugins[i].name);
        free(plugins[i].plugin_dir);
        free(plugins[i].lua_binding);
        free(plugins[i].static_lib);
    }

    free(plugins);
}
