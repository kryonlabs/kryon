/**
 * Plugin Discovery for Lua Binary Builds
 *
 * Discovers plugins from kryon.toml configuration and validates
 * that all required files exist. NO auto-discovery - plugins
 * MUST be explicitly listed in config with paths.
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "plugin_discovery.h"
#include "../../include/kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>

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
 * Copy a file from src to dst
 */
static int copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return -1;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

/**
 * Get file modification time, returns 0 if file doesn't exist
 */
static time_t get_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

/**
 * Check if any source file in directory is newer than reference time
 * Recursively scans src/ directory for .c and .h files
 */
static int source_files_newer_than(const char* plugin_dir, time_t ref_time) {
    char src_dir[DISCOVERY_PATH_MAX];
    snprintf(src_dir, sizeof(src_dir), "%s/src", plugin_dir);

    DIR* dir = opendir(src_dir);
    if (!dir) return 0;  /* No src dir, assume no rebuild needed */

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        /* Check .c and .h files */
        size_t len = strlen(entry->d_name);
        if (len > 2 && (strcmp(entry->d_name + len - 2, ".c") == 0 ||
                        strcmp(entry->d_name + len - 2, ".h") == 0)) {
            char filepath[DISCOVERY_PATH_MAX];
            snprintf(filepath, sizeof(filepath), "%s/%s", src_dir, entry->d_name);
            if (get_mtime(filepath) > ref_time) {
                closedir(dir);
                return 1;  /* Source is newer */
            }
        }
    }
    closedir(dir);

    /* Also check Makefile */
    char makefile[DISCOVERY_PATH_MAX];
    snprintf(makefile, sizeof(makefile), "%s/Makefile", plugin_dir);
    if (get_mtime(makefile) > ref_time) {
        return 1;
    }

    return 0;
}

/**
 * Check if plugin needs to be rebuilt
 * Returns 1 if rebuild needed, 0 otherwise
 */
static int plugin_needs_rebuild(const char* plugin_dir, const char* plugin_name) {
    char lib_path[DISCOVERY_PATH_MAX];
    snprintf(lib_path, sizeof(lib_path), "%s/build/libkryon_%s.a", plugin_dir, plugin_name);

    /* Check if .a exists */
    time_t lib_mtime = get_mtime(lib_path);
    if (lib_mtime == 0) {
        return 1;  /* Missing = needs rebuild */
    }

    /* Check if any source file is newer than .a */
    return source_files_newer_than(plugin_dir, lib_mtime);
}

/**
 * Compile a plugin by running make in its directory
 * Returns 0 on success, -1 on failure
 */
static int plugin_compile(const char* plugin_dir, const char* plugin_name) {
    printf("[plugin] Compiling %s...\n", plugin_name);

    char cmd[DISCOVERY_PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" 2>&1", plugin_dir);

    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "[plugin] Failed to compile %s (exit code %d)\n", plugin_name, result);
        return -1;
    }

    printf("[plugin] Successfully compiled %s\n", plugin_name);
    return 0;
}

/**
 * Install plugin's .so to ~/.local/share/kryon/plugins/ for runtime loading
 * Returns 0 on success, -1 on failure
 */
static int plugin_install_runtime(const char* plugin_dir, const char* plugin_name) {
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[plugin] Warning: HOME not set, skipping runtime install\n");
        return -1;
    }

    /* Source .so path */
    char src_path[DISCOVERY_PATH_MAX];
    snprintf(src_path, sizeof(src_path), "%s/build/libkryon_%s.so", plugin_dir, plugin_name);

    /* Check if .so exists */
    if (!file_exists(src_path)) {
        /* Not an error - some plugins might only have .a */
        return 0;
    }

    /* Destination directory - matches runtime plugin loader search path */
    char lib_dir[DISCOVERY_PATH_MAX];
    snprintf(lib_dir, sizeof(lib_dir), "%s/.local/share/kryon/plugins", home);

    /* Create destination directory if it doesn't exist */
    if (!dir_exists(lib_dir)) {
        char mkdir_cmd[DISCOVERY_PATH_MAX + 32];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", lib_dir);
        system(mkdir_cmd);
    }

    /* Destination .so path */
    char dst_path[DISCOVERY_PATH_MAX];
    snprintf(dst_path, sizeof(dst_path), "%s/libkryon_%s.so", lib_dir, plugin_name);

    /* Copy the file */
    if (copy_file(src_path, dst_path) != 0) {
        fprintf(stderr, "[plugin] Warning: Failed to install %s to %s\n", plugin_name, dst_path);
        return -1;
    }

    printf("[plugin] Installed %s -> %s\n", plugin_name, dst_path);
    return 0;
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
 *
 * A plugin must have either:
 * - A Lua binding (<plugin_dir>/bindings/lua/<plugin_name>.lua), or
 * - A C shared library (<plugin_dir>/build/libkryon_<plugin_name>.so) for web/native renderers
 */
static int verify_plugin_files(const char* plugin_dir, const char* plugin_name,
                               char* out_lua_path, size_t lua_path_size,
                               char* out_lib_path, size_t lib_path_size) {
    /* Check for Lua binding: <plugin_dir>/bindings/lua/<plugin_name>.lua */
    snprintf(out_lua_path, lua_path_size, "%s/bindings/lua/%s.lua", plugin_dir, plugin_name);

    /* Check for C shared library: <plugin_dir>/build/libkryon_<plugin_name>.so */
    char so_path[DISCOVERY_PATH_MAX];
    snprintf(so_path, sizeof(so_path), "%s/build/libkryon_%s.so", plugin_dir, plugin_name);

    /* Plugin must have either Lua binding OR C shared library */
    bool has_lua = file_exists(out_lua_path);
    bool has_so = file_exists(so_path);

    if (!has_lua && !has_so) {
        fprintf(stderr, "Error: Plugin '%s' has no Lua binding or C library:\n", plugin_name);
        fprintf(stderr, "       Expected: %s\n", out_lua_path);
        fprintf(stderr, "       OR: %s\n", so_path);
        return -1;
    }

    /* If plugin has Lua binding, also check for static library */
    if (has_lua) {
        snprintf(out_lib_path, lib_path_size, "%s/build/libkryon_%s.a", plugin_dir, plugin_name);

        if (!file_exists(out_lib_path)) {
            fprintf(stderr, "Error: Plugin '%s' static library not found: %s\n",
                    plugin_name, out_lib_path);
            fprintf(stderr, "       Run 'make' in the plugin directory to build it.\n");
            return -1;
        }
    } else {
        /* Pure C plugin - copy .so path to lib_path output */
        strncpy(out_lib_path, so_path, lib_path_size - 1);
        out_lib_path[lib_path_size - 1] = '\0';
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

        /* Auto-compile plugin if needed */
        if (plugin_needs_rebuild(plugin_dir, dep->name)) {
            printf("  - %s: needs rebuild\n", dep->name);
            if (plugin_compile(plugin_dir, dep->name) != 0) {
                free(plugin_dir);
                free(plugins);
                exit(1);
            }
            /* Install .so to ~/.local/lib for runtime FFI loading */
            plugin_install_runtime(plugin_dir, dep->name);
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
        /* lua_binding is only set if Lua binding exists */
        info->lua_binding = file_exists(lua_path) ? strdup(lua_path) : NULL;
        info->static_lib = strdup(lib_path);

        printf("  - %s: OK\n", dep->name);
        printf("    dir:  %s\n", plugin_dir);
        if (info->lua_binding) {
            printf("    lua:  %s\n", lua_path);
        } else {
            printf("    type: C plugin (no Lua binding)\n");
        }
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
