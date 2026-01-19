/**
 * Plugin Discovery for Lua Binary Builds
 *
 * Discovers plugins from kryon.toml configuration and validates
 * that all required files exist. NO auto-discovery - plugins
 * MUST be explicitly listed in config with paths.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

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
 * Create a directory and all parents (like mkdir -p)
 */
static int mkdir_p(const char* path) {
    char tmp[DISCOVERY_PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!dir_exists(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }

    if (!dir_exists(tmp)) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    return 0;
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
            int written = snprintf(filepath, sizeof(filepath), "%s/%s", src_dir, entry->d_name);
            if (written < 0 || (size_t)written >= sizeof(filepath)) {
                closedir(dir);
                return 0;  /* Path too long, skip this file */
            }
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
 * Install plugin's .so to ~/.local/share/kryon/plugins/<plugin-name>/
 * for runtime discovery by ir_plugin_discover().
 *
 * The runtime discovery expects:
 *   ~/.local/share/kryon/plugins/<plugin-name>/
 *     - plugin.toml (metadata)
 *     - libkryon_<plugin-name>.so (shared library)
 *
 * Returns 0 on success, -1 on failure
 */
static int plugin_install_runtime(const char* plugin_dir, const char* plugin_name) {
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[plugin] Warning: HOME not set, skipping runtime install\n");
        return -1;
    }

    /* Source .so path */
    char src_so_path[DISCOVERY_PATH_MAX];
    snprintf(src_so_path, sizeof(src_so_path), "%s/build/libkryon_%s.so", plugin_dir, plugin_name);

    /* Check if .so exists */
    if (!file_exists(src_so_path)) {
        /* Not an error - some plugins might only have .a */
        return 0;
    }

    /* Destination directory: ~/.local/share/kryon/plugins/<plugin-name>/ */
    char plugin_dest_dir[DISCOVERY_PATH_MAX];
    snprintf(plugin_dest_dir, sizeof(plugin_dest_dir),
             "%s/.local/share/kryon/plugins/%s", home, plugin_name);

    /* Create destination directory if it doesn't exist */
    if (mkdir_p(plugin_dest_dir) != 0) {
        fprintf(stderr, "[plugin] Warning: Failed to create directory %s\n", plugin_dest_dir);
        return -1;
    }

    /* Copy the .so file */
    char dst_so_path[DISCOVERY_PATH_MAX];
    int written = snprintf(dst_so_path, sizeof(dst_so_path), "%s/libkryon_%s.so", plugin_dest_dir, plugin_name);
    if (written < 0 || (size_t)written >= sizeof(dst_so_path)) {
        fprintf(stderr, "[plugin] Warning: .so path too long for %s\n", plugin_name);
        return -1;
    }

    if (copy_file(src_so_path, dst_so_path) != 0) {
        fprintf(stderr, "[plugin] Warning: Failed to install %s\n", plugin_name);
        return -1;
    }

    /* Copy plugin.toml if it exists in the plugin directory */
    char src_toml_path[DISCOVERY_PATH_MAX];
    char dst_toml_path[DISCOVERY_PATH_MAX];
    written = snprintf(src_toml_path, sizeof(src_toml_path), "%s/plugin.toml", plugin_dir);
    (void)written; /* Length is bounded */
    written = snprintf(dst_toml_path, sizeof(dst_toml_path), "%s/plugin.toml", plugin_dest_dir);
    (void)written; /* Length is bounded */

    if (file_exists(src_toml_path)) {
        if (copy_file(src_toml_path, dst_toml_path) != 0) {
            fprintf(stderr, "[plugin] Warning: Failed to copy plugin.toml for %s\n", plugin_name);
            /* Continue anyway - .so is more critical */
        }
    }

    printf("[plugin] Installed %s -> %s/\n", plugin_name, plugin_dest_dir);
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
 * Extract code block from plugin.kry file
 * Finds @language { ... } blocks and extracts the source code
 * Returns allocated string that must be freed, or NULL if not found
 */
static char* extract_code_block(const char* filepath, const char* language) {
    FILE* f = fopen(filepath, "r");
    if (!f) return NULL;

    // Read entire file
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return NULL;
    }

    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(content, 1, file_size, f);
    content[read_size] = '\0';
    fclose(f);

    // Search for @language { ... }
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "@%s {", language);

    char* block_start = strstr(content, search_pattern);
    if (!block_start) {
        free(content);
        return NULL;
    }

    // Find opening brace
    char* code_start = strchr(block_start, '{');
    if (!code_start) {
        free(content);
        return NULL;
    }
    code_start++; // Skip '{'

    // Skip whitespace after opening brace
    while (*code_start == ' ' || *code_start == '\t' || *code_start == '\n' || *code_start == '\r') {
        code_start++;
    }

    // Find matching closing brace
    int brace_count = 1;
    bool in_string = false;
    char string_char = '\0';
    char* code_end = code_start;

    while (*code_end && brace_count > 0) {
        if (!in_string) {
            if (*code_end == '"' || *code_end == '\'' || *code_end == '`') {
                in_string = true;
                string_char = *code_end;
            } else if (*code_end == '{') {
                brace_count++;
            } else if (*code_end == '}') {
                brace_count--;
            }
        } else {
            if (*code_end == string_char) {
                in_string = false;
            }
        }
        code_end++;
    }

    if (brace_count > 0) {
        free(content);
        return NULL; // Unclosed block
    }

    // code_end now points to character after closing brace
    // Move back to skip whitespace and closing brace
    code_end--; // Now points to '}'
    code_end--; // Skip the closing brace
    while (code_end > code_start && (*code_end == ' ' || *code_end == '\t' ||
           *code_end == '\n' || *code_end == '\r')) {
        code_end--;
    }

    // Extract code (without braces)
    size_t code_length = code_end - code_start + 1;
    char* code = malloc(code_length + 1);
    if (!code) {
        free(content);
        return NULL;
    }

    memcpy(code, code_start, code_length);
    code[code_length] = '\0';

    free(content);
    return code;
}

/**
 * Verify plugin has all required files
 * Returns 0 on success, -1 on error (prints error message)
 *
 * A plugin can have:
 * - A plugin.kry file (<plugin_dir>/plugin.kry) - NEW format with @lua/@js blocks
 * - A Lua binding (<plugin_dir>/bindings/lua/<plugin_name>.lua) - legacy format
 * - A C shared library (<plugin_dir>/build/libkryon_<plugin_name>.so) for web/native renderers
 *
 * At minimum, a plugin must have EITHER:
 *   - plugin.kry, OR
 *   - Lua binding, OR
 *   - C shared library
 */
static int verify_plugin_files(const char* plugin_dir, const char* plugin_name,
                               char* out_lua_path, size_t lua_path_size,
                               char* out_kry_path, size_t kry_path_size,
                               char* out_lib_path, size_t lib_path_size,
                               bool* out_has_kry_file) {
    /* Check for plugin.kry file first (new format) */
    snprintf(out_kry_path, kry_path_size, "%s/plugin.kry", plugin_dir);
    *out_has_kry_file = file_exists(out_kry_path);

    /* Check for Lua binding: <plugin_dir>/bindings/lua/<plugin_name>.lua */
    snprintf(out_lua_path, lua_path_size, "%s/bindings/lua/%s.lua", plugin_dir, plugin_name);

    /* Check for C shared library: <plugin_dir>/build/libkryon_<plugin_name>.so */
    char so_path[DISCOVERY_PATH_MAX];
    snprintf(so_path, sizeof(so_path), "%s/build/libkryon_%s.so", plugin_dir, plugin_name);

    /* Check what files exist */
    bool has_kry = *out_has_kry_file;
    bool has_lua = file_exists(out_lua_path);
    bool has_so = file_exists(so_path);

    /* Plugin must have at least one of: plugin.kry, Lua binding, or C shared library */
    if (!has_kry && !has_lua && !has_so) {
        fprintf(stderr, "Error: Plugin '%s' has no plugin.kry, Lua binding, or C library:\n", plugin_name);
        fprintf(stderr, "       Expected: %s\n", out_kry_path);
        fprintf(stderr, "       OR: %s\n", out_lua_path);
        fprintf(stderr, "       OR: %s\n", so_path);
        return -1;
    }

    /* If plugin has plugin.kry or Lua binding, also check for static library */
    if (has_kry || has_lua) {
        snprintf(out_lib_path, lib_path_size, "%s/build/libkryon_%s.a", plugin_dir, plugin_name);

        if (!file_exists(out_lib_path)) {
            fprintf(stderr, "Error: Plugin '%s' static library not found: %s\n",
                    plugin_name, out_lib_path);
            fprintf(stderr, "       Run 'make' in the plugin directory to build it.\n");
            return -1;
        }
    } else {
        /* Pure C plugin - copy .so path to lib_path output */
        size_t copy_len = strlen(so_path);
        if (copy_len >= lib_path_size) {
            copy_len = lib_path_size - 1;
        }
        memcpy(out_lib_path, so_path, copy_len);
        out_lib_path[copy_len] = '\0';
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

        /* Skip git plugins - they are handled by config_load_plugins instead */
        if (dep->git) {
            printf("  - %s: git plugin, skipping (handled by plugin loader)\n", dep->name);
            continue;
        }

        /* Path is REQUIRED for local plugins */
        if (!dep->path || dep->path[0] == '\0') {
            fprintf(stderr, "Error: Plugin '%s' missing required 'path' field in kryon.toml\n",
                    dep->name ? dep->name : "<unknown>");
            fprintf(stderr, "       Required format:\n");
            fprintf(stderr, "         [plugins.%s]\n", dep->name ? dep->name : "name");
            fprintf(stderr, "         path = \"../path/to/plugin\"\n");
            fprintf(stderr, "       Or use git for remote plugins:\n");
            fprintf(stderr, "         git = \"https://github.com/...\"\n");
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
        char kry_path[DISCOVERY_PATH_MAX];
        char lib_path[DISCOVERY_PATH_MAX];
        bool has_kry_file = false;

        if (verify_plugin_files(plugin_dir, dep->name,
                               lua_path, sizeof(lua_path),
                               kry_path, sizeof(kry_path),
                               lib_path, sizeof(lib_path),
                               &has_kry_file) != 0) {
            free(plugin_dir);
            free(plugins);
            exit(1);
        }

        /* All checks passed - store plugin info */
        BuildPluginInfo* info = &plugins[valid_count];
        info->name = strdup(dep->name);
        info->plugin_dir = plugin_dir;
        info->has_kry_file = has_kry_file;
        /* lua_binding is only set if Lua binding exists */
        info->lua_binding = file_exists(lua_path) ? strdup(lua_path) : NULL;
        /* kry_file is only set if plugin.kry exists */
        info->kry_file = has_kry_file ? strdup(kry_path) : NULL;
        info->static_lib = strdup(lib_path);

        /* Extract code blocks from plugin.kry if present */
        info->lua_code = NULL;
        info->js_code = NULL;
        info->nim_code = NULL;

        if (has_kry_file) {
            printf("    Extracting code blocks from plugin.kry...\n");
            info->lua_code = extract_code_block(kry_path, "lua");
            info->js_code = extract_code_block(kry_path, "js");
            info->nim_code = extract_code_block(kry_path, "nim");

            if (info->lua_code) {
                printf("      ✓ @lua block extracted (%zu bytes)\n", strlen(info->lua_code));
            }
            if (info->js_code) {
                printf("      ✓ @js block extracted (%zu bytes)\n", strlen(info->js_code));
            }
            if (info->nim_code) {
                printf("      ✓ @nim block extracted (%zu bytes)\n", strlen(info->nim_code));
            }

            if (!info->lua_code && !info->js_code && !info->nim_code) {
                printf("      ⚠ Warning: No code blocks found in plugin.kry\n");
            }
        }

        printf("  - %s: OK\n", dep->name);
        printf("    dir:  %s\n", plugin_dir);
        if (info->kry_file) {
            printf("    kry:  %s\n", kry_path);
        }
        if (info->lua_binding) {
            printf("    lua:  %s\n", lua_path);
        } else if (!info->kry_file) {
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
 * Write extracted plugin code to build directory
 * Creates build/plugins/<plugin_name>.lua and .js files
 * Returns 0 on success, -1 on error
 */
int write_plugin_code_files(BuildPluginInfo* plugins, int count, const char* build_dir) {
    if (!plugins || count == 0) {
        return 0; // No plugins, nothing to write
    }

    // Create plugins directory
    char plugins_dir[DISCOVERY_PATH_MAX];
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/plugins", build_dir);

    if (!dir_exists(plugins_dir)) {
        if (mkdir_p(plugins_dir) != 0) {
            fprintf(stderr, "Error: Failed to create plugins directory: %s\n", plugins_dir);
            return -1;
        }
    }

    int written_count = 0;

    for (int i = 0; i < count; i++) {
        BuildPluginInfo* plugin = &plugins[i];

        if (!plugin->has_kry_file) {
            continue; // Skip plugins without .kry files
        }

        // Write Lua code if available
        if (plugin->lua_code) {
            char lua_path[DISCOVERY_PATH_MAX];
            snprintf(lua_path, sizeof(lua_path), "%s/%s.lua", plugins_dir, plugin->name);

            FILE* f = fopen(lua_path, "w");
            if (f) {
                fwrite(plugin->lua_code, 1, strlen(plugin->lua_code), f);
                fclose(f);
                printf("[Plugins] Wrote: %s\n", lua_path);
                written_count++;
            } else {
                fprintf(stderr, "Warning: Failed to write: %s\n", lua_path);
            }
        }

        // Write JavaScript code if available
        if (plugin->js_code) {
            char js_path[DISCOVERY_PATH_MAX];
            snprintf(js_path, sizeof(js_path), "%s/%s.js", plugins_dir, plugin->name);

            FILE* f = fopen(js_path, "w");
            if (f) {
                fwrite(plugin->js_code, 1, strlen(plugin->js_code), f);
                fclose(f);
                printf("[Plugins] Wrote: %s\n", js_path);
                written_count++;
            } else {
                fprintf(stderr, "Warning: Failed to write: %s\n", js_path);
            }
        }
    }

    if (written_count > 0) {
        printf("[Plugins] Wrote %d plugin code file(s) to %s\n", written_count, plugins_dir);
    }

    return 0;
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
        free(plugins[i].kry_file);
        free(plugins[i].static_lib);
        free(plugins[i].lua_code);
        free(plugins[i].js_code);
        free(plugins[i].nim_code);
    }

    free(plugins);
}

#pragma GCC diagnostic pop
