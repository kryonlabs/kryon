/**
 * Configuration Management
 * Loads and validates kryon.toml configuration files
 */

#include "kryon_cli.h"
#include "../utils/plugin_installer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ir_capability.h>

// ============================================================================
// Plugin Dependency Tracking
// ============================================================================

/**
 * Track loaded plugins to prevent duplicate loading and detect cycles.
 * Uses static state since plugin loading is a one-time operation.
 */
#define MAX_LOADED_PLUGINS 64

static char* g_loaded_plugin_names[MAX_LOADED_PLUGINS];
static int g_loaded_plugin_count = 0;

/**
 * Check if a plugin has already been loaded.
 */
static bool is_plugin_loaded(const char* name) {
    if (!name) return false;
    for (int i = 0; i < g_loaded_plugin_count; i++) {
        if (strcmp(g_loaded_plugin_names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Mark a plugin as loaded.
 */
static void mark_plugin_loaded(const char* name) {
    if (!name) return;
    if (g_loaded_plugin_count < MAX_LOADED_PLUGINS) {
        g_loaded_plugin_names[g_loaded_plugin_count++] = str_copy(name);
    }
}

/**
 * Free loaded plugin tracking state.
 * Called when plugin loading is complete.
 */
static void free_loaded_plugin_tracking(void) {
    for (int i = 0; i < g_loaded_plugin_count; i++) {
        free(g_loaded_plugin_names[i]);
    }
    g_loaded_plugin_count = 0;
}

/**
 * Create empty config with defaults
 */
static KryonConfig* config_create_default(void) {
    KryonConfig* config = (KryonConfig*)calloc(1, sizeof(KryonConfig));
    if (!config) return NULL;

    // Set defaults
    config->build_target = str_copy("web");
    config->build_output_dir = str_copy("build");
    config->optimization_enabled = true;
    config->optimization_minify_css = true;
    config->optimization_minify_js = true;
    config->optimization_tree_shake = true;
    config->dev_hot_reload = true;
    config->dev_port = 3000;
    config->dev_auto_open = true;
    config->desktop_renderer = str_copy("sdl3");  // Default to SDL3

    return config;
}


/**
 * Parse plugins from TOML using named plugin syntax
 * Format: [plugins]
 *         storage = { path = "../kryon-storage", version = "1.0.0" }
 */
static void config_parse_plugins(KryonConfig* config, TOMLTable* toml) {
    // Get all plugin names from TOML
    int plugin_count = 0;
    char** plugin_names = toml_get_plugin_names(toml, &plugin_count);

    if (!plugin_names || plugin_count == 0) {
        config->plugins_count = 0;
        config->plugins = NULL;
        return;
    }

    // Allocate plugin array
    config->plugins = (PluginDep*)calloc(plugin_count, sizeof(PluginDep));
    if (!config->plugins) {
        // Free plugin names
        for (int i = 0; i < plugin_count; i++) {
            free(plugin_names[i]);
        }
        free(plugin_names);
        config->plugins_count = 0;
        return;
    }

    config->plugins_count = plugin_count;

    // Parse each plugin's properties
    for (int i = 0; i < plugin_count; i++) {
        const char* plugin_name = plugin_names[i];
        PluginDep* plugin = &config->plugins[i];

        // Set plugin name
        plugin->name = str_copy(plugin_name);

        // Get plugin properties
        char key[256];

        // Get path (optional if git is specified)
        snprintf(key, sizeof(key), "plugins.%s.path", plugin_name);
        const char* path = toml_get_string(toml, key, NULL);
        if (path) {
            plugin->path = str_copy(path);
        } else {
            plugin->path = NULL;
        }

        // Get git URL (optional)
        snprintf(key, sizeof(key), "plugins.%s.git", plugin_name);
        const char* git = toml_get_string(toml, key, NULL);
        if (git) {
            plugin->git = str_copy(git);
        } else {
            plugin->git = NULL;
        }

        // Get git branch (optional, defaults to "master")
        snprintf(key, sizeof(key), "plugins.%s.branch", plugin_name);
        const char* branch = toml_get_string(toml, key, "master");
        plugin->branch = str_copy(branch);

        // Get git subdir (optional, for sparse checkout)
        snprintf(key, sizeof(key), "plugins.%s.subdir", plugin_name);
        const char* subdir = toml_get_string(toml, key, NULL);
        if (subdir) {
            plugin->subdir = str_copy(subdir);
        } else {
            plugin->subdir = NULL;
        }

        // Get version (optional)
        snprintf(key, sizeof(key), "plugins.%s.version", plugin_name);
        const char* version = toml_get_string(toml, key, NULL);
        if (version) {
            plugin->version = str_copy(version);
        } else {
            plugin->version = NULL;
        }

        // Get enabled flag (optional, default: true)
        snprintf(key, sizeof(key), "plugins.%s.enabled", plugin_name);
        plugin->enabled = toml_get_bool(toml, key, true);

        // Resolved path will be set later
        plugin->resolved_path = NULL;
    }

    // Free plugin names
    for (int i = 0; i < plugin_count; i++) {
        free(plugin_names[i]);
    }
    free(plugin_names);
}

/**
 * Parse install configuration from TOML
 */
static void config_parse_install(KryonConfig* config, TOMLTable* toml) {
    // Only parse install section if it exists
    if (!toml_get_string(toml, "install.mode", NULL) &&
        !toml_get_string(toml, "install.binary.path", NULL) &&
        !toml_get_string(toml, "install.binary.name", NULL)) {
        config->install = NULL;
        return;
    }

    InstallConfig* install = (InstallConfig*)calloc(1, sizeof(InstallConfig));
    if (!install) {
        config->install = NULL;
        return;
    }

    // Parse install.mode (default: symlink)
    const char* mode_str = toml_get_string(toml, "install.mode", "symlink");
    if (strcmp(mode_str, "copy") == 0) {
        install->mode = INSTALL_MODE_COPY;
    } else if (strcmp(mode_str, "system") == 0) {
        install->mode = INSTALL_MODE_SYSTEM;
    } else {
        install->mode = INSTALL_MODE_SYMLINK;  // default
    }

    // Parse install.target (optional, NULL for auto-detect)
    const char* target_str = toml_get_string(toml, "install.target", NULL);
    if (target_str) {
        install->target = str_copy(target_str);
    } else {
        install->target = NULL;  // Auto-detect later
    }

    // Parse install.binary.path (default: $HOME/bin)
    const char* binary_path = toml_get_string(toml, "install.binary.path", "$HOME/bin");
    install->binary_path = str_copy(binary_path);

    // Parse install.binary.name (default: project.name)
    const char* binary_name = toml_get_string(toml, "install.binary.name", config->project_name);
    install->binary_name = str_copy(binary_name);

    // Parse install.binary.executable (default: true)
    install->binary_executable = toml_get_bool(toml, "install.binary.executable", true);

    // Parse [[install.files]] array
    // Count files first
    int file_count = 0;
    for (int i = 0; i < 100; i++) {  // max 100 files
        char key[256];
        snprintf(key, sizeof(key), "install.files.%d.source", i);
        if (toml_get_string(toml, key, NULL)) {
            file_count++;
        } else {
            break;
        }
    }

    if (file_count > 0) {
        install->files = (InstallFile*)calloc(file_count, sizeof(InstallFile));
        if (install->files) {
            install->files_count = file_count;
            for (int i = 0; i < file_count; i++) {
                char key[256];
                snprintf(key, sizeof(key), "install.files.%d.source", i);
                const char* source = toml_get_string(toml, key, NULL);
                if (source) {
                    install->files[i].source = str_copy(source);
                }

                snprintf(key, sizeof(key), "install.files.%d.target", i);
                const char* target = toml_get_string(toml, key, NULL);
                if (target) {
                    install->files[i].target = str_copy(target);
                }
            }
        }
    }

    // Parse install.desktop section
    install->desktop.enabled = toml_get_bool(toml, "install.desktop.enabled", false);
    const char* desktop_name = toml_get_string(toml, "install.desktop.name", config->project_name);
    install->desktop.name = str_copy(desktop_name);

    const char* desktop_icon = toml_get_string(toml, "install.desktop.icon", NULL);
    if (desktop_icon) {
        install->desktop.icon = str_copy(desktop_icon);
    }

    // Parse install.desktop.categories array
    int category_count = 0;
    for (int i = 0; i < 20; i++) {  // max 20 categories
        char key[256];
        snprintf(key, sizeof(key), "install.desktop.categories.%d", i);
        if (toml_get_string(toml, key, NULL)) {
            category_count++;
        } else {
            break;
        }
    }

    if (category_count > 0) {
        install->desktop.categories = (char**)calloc(category_count, sizeof(char*));
        if (install->desktop.categories) {
            install->desktop.categories_count = category_count;
            for (int i = 0; i < category_count; i++) {
                char key[256];
                snprintf(key, sizeof(key), "install.desktop.categories.%d", i);
                const char* category = toml_get_string(toml, key, NULL);
                if (category) {
                    install->desktop.categories[i] = str_copy(category);
                }
            }
        }
    }

    config->install = install;
}

/**
 * Load configuration from TOML file
 */
KryonConfig* config_load(const char* config_path) {
    if (!config_path || !file_exists(config_path)) {
        return NULL;
    }

    TOMLTable* toml = toml_parse_file(config_path);
    if (!toml) {
        return NULL;
    }

    // Check for deprecated field names and show clear errors
    bool has_deprecated = false;

    if (toml_get_string(toml, "build.entry_point", NULL)) {
        fprintf(stderr, "Error: Invalid field 'entry_point' in [build]\n");
        fprintf(stderr, "       Use 'entry' instead\n");
        has_deprecated = true;
    }

    if (toml_get_string(toml, "build.target", NULL)) {
        fprintf(stderr, "Error: Invalid field 'target' in [build]\n");
        fprintf(stderr, "       Use 'targets' (array) instead: targets = [\"web\"]\n");
        has_deprecated = true;
    }

    if (toml_get_string(toml, "project.entry", NULL)) {
        fprintf(stderr, "Error: Invalid field 'entry' in [project]\n");
        fprintf(stderr, "       Move to [build] section: build.entry = \"...\"\n");
        has_deprecated = true;
    }

    if (toml_get_string(toml, "project.frontend", NULL)) {
        fprintf(stderr, "Error: Invalid field 'frontend' in [project]\n");
        fprintf(stderr, "       Move to [build] section: build.frontend = \"...\"\n");
        has_deprecated = true;
    }

    if (toml_get_string(toml, "build.language", NULL)) {
        fprintf(stderr, "Error: Invalid field 'language' in [build]\n");
        fprintf(stderr, "       Use 'frontend' instead\n");
        has_deprecated = true;
    }

    if (toml_get_string(toml, "desktop.window_title", NULL) ||
        toml_get_string(toml, "desktop.window_width", NULL) ||
        toml_get_string(toml, "desktop.window_height", NULL)) {
        fprintf(stderr, "Error: Invalid window fields in [desktop]\n");
        fprintf(stderr, "       Move to [window] section:\n");
        fprintf(stderr, "       [window]\n");
        fprintf(stderr, "       title = \"...\"\n");
        fprintf(stderr, "       width = ...\n");
        fprintf(stderr, "       height = ...\n");
        has_deprecated = true;
    }

    // Check for old pages format
    if (toml_get_string(toml, "build.pages.0.file", NULL) ||
        toml_get_string(toml, "build.pages.0.path", NULL)) {
        fprintf(stderr, "Error: Invalid fields in [[build.pages]]\n");
        fprintf(stderr, "       Use: name = \"...\", route = \"...\", source = \"...\"\n");
        fprintf(stderr, "       NOT: file = \"...\", path = \"...\"\n");
        has_deprecated = true;
    }

    if (has_deprecated) {
        fprintf(stderr, "\nSee docs/configuration.md for the complete configuration reference\n");
        toml_free(toml);
        return NULL;
    }

    KryonConfig* config = config_create_default();
    if (!config) {
        toml_free(toml);
        return NULL;
    }

    // Parse project metadata
    const char* name = toml_get_string(toml, "project.name", NULL);
    if (name) {
        free(config->project_name);
        config->project_name = str_copy(name);
    }

    const char* version = toml_get_string(toml, "project.version", NULL);
    if (version) {
        free(config->project_version);
        config->project_version = str_copy(version);
    }

    const char* author = toml_get_string(toml, "project.author", NULL);
    if (author) {
        free(config->project_author);
        config->project_author = str_copy(author);
    }

    const char* description = toml_get_string(toml, "project.description", NULL);
    if (description) {
        free(config->project_description);
        config->project_description = str_copy(description);
    }

    // Parse build settings - strict format enforcement
    const char* output_dir = toml_get_string(toml, "build.output_dir", NULL);
    if (output_dir) {
        free(config->build_output_dir);
        config->build_output_dir = str_copy(output_dir);
    } else {
        // Default to "build" directory
        free(config->build_output_dir);
        config->build_output_dir = str_copy("build");
    }

    // Parse build.entry (optional entry point file)
    const char* entry = toml_get_string(toml, "build.entry", NULL);
    if (entry) {
        config->build_entry = str_copy(entry);
    }

    // Parse build.targets array (required)
    // Count targets by looking for build.targets.N keys
    int max_target_idx = -1;
    for (int i = 0; i < 10; i++) {
        char key[128];
        snprintf(key, sizeof(key), "build.targets.%d", i);
        const char* target_val = toml_get_string(toml, key, NULL);
        if (target_val) {
            max_target_idx = i;
        }
    }

    if (max_target_idx >= 0) {
        config->build_targets_count = max_target_idx + 1;
        config->build_targets = (char**)calloc(config->build_targets_count, sizeof(char*));
        if (config->build_targets) {
            for (int i = 0; i <= max_target_idx; i++) {
                char key[128];
                snprintf(key, sizeof(key), "build.targets.%d", i);
                const char* target_val = toml_get_string(toml, key, NULL);
                if (target_val) {
                    config->build_targets[i] = str_copy(target_val);
                    // Also set build_target to first target for backward compat in commands
                    if (i == 0) {
                        free(config->build_target);
                        config->build_target = str_copy(target_val);
                    }
                }
            }
        }
    }

    // Parse optimization settings
    config->optimization_enabled = toml_get_bool(toml, "optimization.enabled", true);
    config->optimization_minify_css = toml_get_bool(toml, "optimization.minify_css", true);
    config->optimization_minify_js = toml_get_bool(toml, "optimization.minify_js", true);
    config->optimization_tree_shake = toml_get_bool(toml, "optimization.tree_shake", true);

    // Parse dev settings
    config->dev_hot_reload = toml_get_bool(toml, "dev.hot_reload", true);
    config->dev_port = toml_get_int(toml, "dev.port", 3000);
    config->dev_auto_open = toml_get_bool(toml, "dev.auto_open", true);

    // Parse desktop renderer
    const char* renderer = toml_get_string(toml, "desktop.renderer", NULL);
    if (renderer) {
        free(config->desktop_renderer);
        config->desktop_renderer = str_copy(renderer);
    }

    // Parse codegen.output_dir (optional)
    const char* codegen_output_dir = toml_get_string(toml, "codegen.output_dir", NULL);
    if (codegen_output_dir) {
        config->codegen_output_dir = str_copy(codegen_output_dir);
    }

    // Parse plugins
    config_parse_plugins(config, toml);

    // Parse install configuration
    config_parse_install(config, toml);

    toml_free(toml);
    return config;
}

/**
 * Check if a plugin's shared library exists
 */
static bool plugin_so_exists(const char* plugin_path, const char* plugin_name) {
    // Check for libkryon_<name>.so in plugin directory
    char* so_path = path_join(plugin_path, "libkryon_");
    // Append name and .so
    size_t base_len = strlen(so_path);
    char* full_so_path = (char*)malloc(base_len + strlen(plugin_name) + 4);
    if (full_so_path) {
        sprintf(full_so_path, "%s%s.so", so_path, plugin_name);
        free(so_path);
        bool exists = file_exists(full_so_path);
        free(full_so_path);
        return exists;
    }
    free(so_path);
    return false;
}

/**
 * Compile a plugin by running make in its directory
 * Returns 0 on success, non-zero on failure
 */
static int plugin_compile_runtime(const char* plugin_path, const char* plugin_name) {
    printf("[kryon][plugin] Compiling plugin '%s' from source...\n", plugin_name);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" 2>&1", plugin_path);

    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to compile plugin '%s' (exit code %d)\n",
                plugin_name, result);
        return -1;
    }

    printf("[kryon][plugin] Successfully compiled plugin '%s'\n", plugin_name);
    return 0;
}

/**
 * Ensure a plugin is compiled, compiling if necessary
 * Returns true if plugin is ready (either was already compiled or was compiled successfully)
 * Returns false if compilation failed
 */
static bool ensure_plugin_compiled(const char* plugin_path, const char* plugin_name) {
    // Check if .so already exists
    if (plugin_so_exists(plugin_path, plugin_name)) {
        return true;
    }

    // Not compiled - try to compile it
    return plugin_compile_runtime(plugin_path, plugin_name) == 0;
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
 * Create a directory and all parents (like mkdir -p)
 */
static int mkdir_p(const char* path) {
    char tmp[1024];
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
            if (!file_is_directory(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }

    if (!file_is_directory(tmp)) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    return 0;
}

/**
 * Ensure a plugin is installed to the discovery path
 * Copies .so and plugin.toml to ~/.local/share/kryon/plugins/<name>/
 * This is needed so kir_to_html can discover the plugin at runtime
 */
static void ensure_plugin_in_discovery_path(const char* plugin_name, const char* plugin_path) {
    const char* home = getenv("HOME");
    if (!home) {
        return;
    }

    /* Source paths */
    char src_so[1024], src_toml[1024];
    snprintf(src_so, sizeof(src_so), "%s/build/libkryon_%s.so", plugin_path, plugin_name);
    snprintf(src_toml, sizeof(src_toml), "%s/plugin.toml", plugin_path);

    /* Check if .so exists in build/ */
    if (!file_exists(src_so)) {
        /* Try root directory */
        snprintf(src_so, sizeof(src_so), "%s/libkryon_%s.so", plugin_path, plugin_name);
        if (!file_exists(src_so)) {
            /* No .so found, skip installation */
            return;
        }
    }

    /* Destination: ~/.local/share/kryon/plugins/<plugin_name>/ */
    char dest_dir[1024];
    snprintf(dest_dir, sizeof(dest_dir), "%s/.local/share/kryon/plugins/%s", home, plugin_name);

    /* Check if already installed */
    if (file_is_directory(dest_dir)) {
        char existing_so[1024];
        snprintf(existing_so, sizeof(existing_so), "%s/libkryon_%s.so", dest_dir, plugin_name);
        if (file_exists(existing_so)) {
            /* Already installed */
            return;
        }
    }

    /* Create destination directory */
    if (mkdir_p(dest_dir) != 0) {
        fprintf(stderr, "[kryon][plugin] Warning: Failed to create directory %s\n", dest_dir);
        return;
    }

    /* Copy .so file */
    char dest_so[1024];
    snprintf(dest_so, sizeof(dest_so), "%s/libkryon_%s.so", dest_dir, plugin_name);
    if (copy_file(src_so, dest_so) != 0) {
        fprintf(stderr, "[kryon][plugin] Warning: Failed to copy .so for %s\n", plugin_name);
        return;
    }

    /* Copy plugin.toml */
    char dest_toml[1024];
    snprintf(dest_toml, sizeof(dest_toml), "%s/plugin.toml", dest_dir);
    if (file_exists(src_toml)) {
        if (copy_file(src_toml, dest_toml) != 0) {
            fprintf(stderr, "[kryon][plugin] Warning: Failed to copy plugin.toml for %s\n", plugin_name);
        }
    }

    printf("[kryon][plugin] Installed %s to discovery path: %s/\n", plugin_name, dest_dir);
}

/**
 * Load a plugin's own kryon.toml to discover its dependencies.
 * Returns a config object containing the plugin's [plugins] section.
 * Caller must free the returned config with config_free().
 */
static KryonConfig* config_load_plugin_toml(const char* plugin_dir) {
    char toml_path[1024];
    snprintf(toml_path, sizeof(toml_path), "%s/kryon.toml", plugin_dir);

    if (!file_exists(toml_path)) {
        return NULL;
    }

    return config_load(toml_path);
}

/**
 * Pre-process git plugins and group them by repository URL.
 * This allows multiple plugins from the same repository to be cloned
 * in a single sparse checkout operation.
 *
 * @param plugins Array of plugin dependencies
 * @param count Number of plugins
 * @return true on success, false on failure
 */
static bool preinstall_git_plugins_grouped(PluginDep* plugins, int count) {
    if (count <= 0) return true;

    // Build array of git plugins for grouping
    PluginGitDep* git_deps = calloc(count, sizeof(PluginGitDep));
    if (!git_deps) {
        return false;
    }

    int git_count = 0;
    for (int i = 0; i < count; i++) {
        PluginDep* dep = &plugins[i];
        if (dep->git) {
            git_deps[git_count].name = dep->name;
            git_deps[git_count].subdir = dep->subdir;
            git_deps[git_count].git = dep->git;
            git_deps[git_count].branch = dep->branch;
            git_count++;
        }
    }

    if (git_count == 0) {
        free(git_deps);
        return true;  // No git plugins
    }

    // Group by git URL
    int group_count = 0;
    PluginGitGroup* groups = plugin_group_by_git(git_deps, git_count, &group_count);

    if (!groups || group_count == 0) {
        free(git_deps);
        return false;
    }

    // Install each group
    bool success = true;
    for (int i = 0; i < group_count; i++) {
        if (groups[i].count == 1) {
            // Single plugin - use existing single-plugin function
            PluginGitDep* dep = &groups[i].plugins[0];
            printf("[kryon][plugin] Installing git plugin '%s' from %s\n",
                   dep->name, groups[i].git_url);
            char* path = plugin_clone_from_git_sparse(groups[i].git_url, dep->subdir,
                                                       dep->name, groups[i].branch);
            if (!path) {
                fprintf(stderr, "[kryon][plugin] Failed to install '%s'\n", dep->name);
                success = false;
            } else {
                free(path);
            }
        } else {
            // Multiple plugins from same repo - use multi-sparse checkout
            printf("[kryon][plugin] Installing %d plugin(s) from shared repository: %s\n",
                   groups[i].count, groups[i].git_url);
            if (!plugin_clone_multi_sparse(groups[i].git_url, groups[i].plugins,
                                           groups[i].count, groups[i].branch)) {
                fprintf(stderr, "[kryon][plugin] Failed to install plugins from %s\n",
                        groups[i].git_url);
                success = false;
            }
        }
    }

    plugin_free_groups(groups, group_count);
    free(git_deps);
    return success;
}

/**
 * Get the resolved path for a plugin that was pre-installed via grouped checkout.
 * For plugins from git, this returns the shared cache path.
 *
 * @param plugin Plugin dependency
 * @return Allocated path string (caller must free), or NULL if not found
 */
static char* get_preinstalled_plugin_path(PluginDep* plugin) {
    if (!plugin->git || !plugin->subdir) {
        return NULL;
    }

    // For sparse checkout, plugins are in cache/<repo_name>/<subdir>
    char* cache_dir = NULL;
    char* xdg_cache = getenv("XDG_CACHE_HOME");
    if (xdg_cache && xdg_cache[0] != '\0') {
        cache_dir = path_join(xdg_cache, "kryon/plugins");
    } else {
        char* home = getenv("HOME");
        if (home) {
            char* cache_base = path_join(home, ".cache/kryon");
            cache_dir = path_join(cache_base, "plugins");
            free(cache_base);
        }
    }

    if (!cache_dir) {
        return NULL;
    }

    // Extract repo name from git URL
    const char* last_slash = strrchr(plugin->git, '/');
    const char* repo_name = last_slash ? last_slash + 1 : plugin->git;
    size_t repo_len = strlen(repo_name);
    if (repo_len > 4 && strcmp(repo_name + repo_len - 4, ".git") == 0) {
        repo_len -= 4;
    }

    // Build path: cache_dir/repo_name/subdir
    char* repo_path = malloc(strlen(cache_dir) + repo_len + 2);
    snprintf(repo_path, strlen(cache_dir) + repo_len + 2, "%s/%.*s", cache_dir, (int)repo_len, repo_name);
    free(cache_dir);

    char* full_path = path_join(repo_path, plugin->subdir);
    free(repo_path);

    return full_path;
}

/**
 * Recursively load a plugin and all its dependencies.
 * Dependencies are loaded BEFORE the parent plugin to ensure proper initialization order.
 *
 * @param plugin       Plugin dependency to load
 * @param base_dir     Base directory for resolving relative paths
 * @param depth        Recursion depth (for cycle detection and debugging)
 * @return true if plugin and all dependencies loaded successfully
 */
static bool load_plugin_with_dependencies(PluginDep* plugin, const char* base_dir, int depth) {
    // Prevent infinite recursion (max depth of 32 should be more than enough)
    if (depth > 32) {
        fprintf(stderr, "[kryon][plugin] Error: Maximum dependency depth exceeded for '%s'\n",
                plugin->name);
        return false;
    }

    // Skip if already loaded
    if (is_plugin_loaded(plugin->name)) {
        printf("[kryon][plugin] Dependency '%s' already loaded, skipping\n", plugin->name);
        return true;
    }

    // Skip disabled plugins
    if (!plugin->enabled) {
        printf("[kryon][plugin] Dependency '%s' is disabled, skipping\n", plugin->name);
        return true;
    }

    printf("[kryon][plugin] Loading dependency '%s' (depth %d)...\n", plugin->name, depth);

    // Resolve plugin path
    char* resolved_path = NULL;

    if (plugin->git) {
        // Check if plugin was pre-installed via grouped checkout
        resolved_path = get_preinstalled_plugin_path(plugin);
        if (resolved_path && file_is_directory(resolved_path)) {
            printf("[kryon][plugin]   Using pre-installed plugin at %s\n", resolved_path);
        } else {
            free(resolved_path);
            resolved_path = NULL;

            // Fallback: Install from git URL individually
            if (plugin->subdir) {
                printf("[kryon][plugin]   Installing '%s' from git (subdir: %s): %s\n",
                       plugin->name, plugin->subdir, plugin->git);
                resolved_path = plugin_clone_from_git_sparse(plugin->git, plugin->subdir,
                                                             plugin->name, plugin->branch);
            } else {
                printf("[kryon][plugin]   Installing '%s' from git: %s\n", plugin->name, plugin->git);
                resolved_path = plugin_install_from_git(plugin->git, plugin->branch, plugin->name);
            }
            if (!resolved_path) {
                fprintf(stderr, "[kryon][plugin]   Failed to install '%s' from git\n", plugin->name);
                return false;
            }
        }
    } else if (plugin->path) {
        // Resolve plugin path relative to base_dir
        resolved_path = path_resolve_canonical(plugin->path, base_dir);
    } else {
        fprintf(stderr, "[kryon][plugin]   Plugin '%s' missing path or git URL\n", plugin->name);
        return false;
    }

    if (!resolved_path) {
        fprintf(stderr, "[kryon][plugin]   Plugin '%s' path resolution failed\n", plugin->name);
        return false;
    }

    if (!file_is_directory(resolved_path)) {
        fprintf(stderr, "[kryon][plugin]   Plugin '%s' path is not a directory: %s\n",
                plugin->name, resolved_path);
        free(resolved_path);
        return false;
    }

    // Load the plugin's own kryon.toml to check for dependencies
    KryonConfig* plugin_config = config_load_plugin_toml(resolved_path);
    if (plugin_config && plugin_config->plugins_count > 0) {
        printf("[kryon][plugin]   Loading %d dependency(s) for '%s'...\n",
               plugin_config->plugins_count, plugin->name);

        // Recursively load all dependencies first
        for (int i = 0; i < plugin_config->plugins_count; i++) {
            PluginDep* dep = &plugin_config->plugins[i];
            if (!load_plugin_with_dependencies(dep, resolved_path, depth + 1)) {
                fprintf(stderr, "[kryon][plugin]   Failed to load dependency '%s' for '%s'\n",
                        dep->name, plugin->name);
                config_free(plugin_config);
                free(resolved_path);
                return false;
            }
        }
    }
    config_free(plugin_config);

    // Auto-compile if needed (both local and git plugins need compilation)
    if (!ensure_plugin_compiled(resolved_path, plugin->name)) {
        fprintf(stderr, "[kryon][plugin]   Plugin '%s' compilation failed\n", plugin->name);
        free(resolved_path);
        return false;
    }

    // Check if already loaded via capability system
    uint32_t loaded_count = ir_capability_get_plugin_count();
    bool already_loaded = false;
    for (uint32_t i = 0; i < loaded_count; i++) {
        const char* loaded_name = ir_capability_get_plugin_name(i);
        if (loaded_name && strcmp(loaded_name, plugin->name) == 0) {
            already_loaded = true;
            break;
        }
    }

    if (already_loaded) {
        printf("[kryon][plugin]   Plugin '%s' already loaded\n", plugin->name);
        free(resolved_path);
        mark_plugin_loaded(plugin->name);
        return true;
    }

    // Find the .so file in the plugin directory
    char so_path[1024];
    snprintf(so_path, sizeof(so_path), "%s/lib%s.so", resolved_path, plugin->name);

    // Check if .so file exists
    struct stat st;
    if (stat(so_path, &st) != 0) {
        // Try generic pattern
        snprintf(so_path, sizeof(so_path), "%s/lib*.so", resolved_path);
        fprintf(stderr, "[kryon][plugin]   No .so file found at: %s\n", resolved_path);
        free(resolved_path);
        return false;
    }

    // Load the plugin using capability system
    bool success = ir_capability_load_plugin(so_path, plugin->name);

    if (success) {
        printf("[kryon][plugin]   Loaded plugin '%s' from %s\n", plugin->name, resolved_path);
        mark_plugin_loaded(plugin->name);

        // Ensure plugin is installed to discovery path for kir_to_html
        ensure_plugin_in_discovery_path(plugin->name, resolved_path);
    } else {
        fprintf(stderr, "[kryon][plugin]   Failed to load plugin '%s' from %s\n",
                plugin->name, resolved_path);
        free(resolved_path);
        return false;
    }

    free(resolved_path);

    return true;
}

/**
 * Load plugins specified in config from their paths.
 * ONLY loads plugins explicitly listed in kryon.toml - no auto-discovery.
 * Now supports recursive plugin dependency resolution - when a plugin is loaded,
 * its own kryon.toml is checked for [plugins] and those are loaded first.
 *
 * Multi-plugin optimization: Plugins from the same git repository are cloned
 * together using sparse checkout, reducing network overhead.
 */
bool config_load_plugins(KryonConfig* config) {
    // If no config plugins specified, we're done
    if (!config || !config->plugins || config->plugins_count == 0) {
        return true;
    }

    char* cwd = dir_get_current();
    if (!cwd) return false;

    bool all_loaded = true;

    printf("[Plugins] Loading %d plugin(s) with recursive dependency resolution...\n",
           config->plugins_count);

    // Pre-install git plugins in groups for efficiency
    // This allows multiple plugins from the same repository to be cloned
    // in a single sparse checkout operation
    if (!preinstall_git_plugins_grouped(config->plugins, config->plugins_count)) {
        fprintf(stderr, "[kryon][config] Warning: Some git plugins failed to pre-install\n");
        // Continue anyway - individual plugins will be attempted below
    }

    // Load each plugin and its dependencies
    for (int i = 0; i < config->plugins_count; i++) {
        PluginDep* plugin = &config->plugins[i];

        // Use the recursive loading function
        if (!load_plugin_with_dependencies(plugin, cwd, 0)) {
            fprintf(stderr, "[kryon][config] Failed to load plugin '%s'\n", plugin->name);
            all_loaded = false;
        }
    }

    free(cwd);

    // Clean up tracking state (but keep plugin names for reference)
    // Note: We don't call free_loaded_plugin_tracking() here because the
    // loaded plugin list may be needed for other operations.

    if (!all_loaded) {
        fprintf(stderr, "[kryon][config] Some plugins failed to load\n");
    }

    return all_loaded;
}

/**
 * Find and load kryon.toml from current directory or parent directories
 */
KryonConfig* config_find_and_load(void) {
    char* cwd = dir_get_current();
    if (!cwd) return NULL;

    // Only check current directory for kryon.toml
    char* config_path = path_join(cwd, "kryon.toml");

    if (file_exists(config_path)) {
        KryonConfig* config = config_load(config_path);
        free(config_path);
        free(cwd);

        // Load plugins after loading config - MUST succeed or we fail
        if (config) {
            if (!config_load_plugins(config)) {
                fprintf(stderr, "[kryon] Failed to load plugins - aborting\n");
                config_free(config);
                return NULL;
            }
        }

        return config;
    }

    free(config_path);
    free(cwd);
    return NULL;
}

/**
 * Validate configuration
 */
bool config_validate(KryonConfig* config) {
    if (!config) return false;

    bool has_errors = false;

    // Validate required [project] fields
    if (!config->project_name) {
        fprintf(stderr, "Error: project.name is required in kryon.toml\n");
        has_errors = true;
    }

    if (!config->project_version) {
        fprintf(stderr, "Error: project.version is required in kryon.toml\n");
        has_errors = true;
    }

    // Validate required [build] fields
    if (!config->build_targets || config->build_targets_count == 0) {
        fprintf(stderr, "Error: build.targets is required in kryon.toml\n");
        fprintf(stderr, "       Specify as array: targets = [\"web\"] or targets = [\"web\", \"desktop\"]\n");
        has_errors = true;
    }

    // Validate each target is supported
    const char* valid_targets[] = {"web", "desktop", "terminal", "embedded"};
    if (config->build_targets && config->build_targets_count > 0) {
        for (int i = 0; i < config->build_targets_count; i++) {
            bool valid_target = false;
            for (size_t j = 0; j < sizeof(valid_targets) / sizeof(valid_targets[0]); j++) {
                if (strcmp(config->build_targets[i], valid_targets[j]) == 0) {
                    valid_target = true;
                    break;
                }
            }

            if (!valid_target) {
                fprintf(stderr, "Error: Invalid build target '%s'\n", config->build_targets[i]);
                fprintf(stderr, "       Valid targets: web, desktop, terminal, embedded\n");
                has_errors = true;
            }
        }
    }

    // Validate desktop.renderer value
    if (config->desktop_renderer) {
        if (strcmp(config->desktop_renderer, "sdl3") != 0 &&
            strcmp(config->desktop_renderer, "raylib") != 0) {
            fprintf(stderr, "Error: Invalid desktop.renderer '%s'\n", config->desktop_renderer);
            fprintf(stderr, "       Valid values: sdl3, raylib\n");
            has_errors = true;
        }
    }

    return !has_errors;
}

/**
 * Free configuration
 */
void config_free(KryonConfig* config) {
    if (!config) return;

    free(config->project_name);
    free(config->project_version);
    free(config->project_author);
    free(config->project_description);
    free(config->build_target);
    free(config->build_output_dir);
    free(config->build_entry);
    free(config->desktop_renderer);
    free(config->codegen_output_dir);

    if (config->build_targets) {
        for (int i = 0; i < config->build_targets_count; i++) {
            free(config->build_targets[i]);
        }
        free(config->build_targets);
    }

    if (config->plugins) {
        for (int i = 0; i < config->plugins_count; i++) {
            free(config->plugins[i].name);
            free(config->plugins[i].path);
            free(config->plugins[i].git);
            free(config->plugins[i].branch);
            free(config->plugins[i].subdir);
            free(config->plugins[i].version);
            free(config->plugins[i].resolved_path);
        }
        free(config->plugins);
    }

    if (config->install) {
        free(config->install->binary_path);
        free(config->install->binary_name);
        free(config->install->target);

        if (config->install->files) {
            for (int i = 0; i < config->install->files_count; i++) {
                free(config->install->files[i].source);
                free(config->install->files[i].target);
            }
            free(config->install->files);
        }

        free(config->install->desktop.name);
        free(config->install->desktop.icon);

        if (config->install->desktop.categories) {
            for (int i = 0; i < config->install->desktop.categories_count; i++) {
                free(config->install->desktop.categories[i]);
            }
            free(config->install->desktop.categories);
        }

        free(config->install);
    }

    free(config);
}
