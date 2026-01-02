/**
 * Configuration Management
 * Loads and validates kryon.toml configuration files
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <ir_plugin.h>

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

        // Get path (required)
        snprintf(key, sizeof(key), "plugins.%s.path", plugin_name);
        const char* path = toml_get_string(toml, key, NULL);
        if (path) {
            plugin->path = str_copy(path);
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

    // Parse plugins
    config_parse_plugins(config, toml);

    toml_free(toml);
    return config;
}

/**
 * Load plugins specified in config from their paths
 */
bool config_load_plugins(KryonConfig* config) {
    if (!config || !config->plugins || config->plugins_count == 0) {
        return true;  // No plugins to load is OK
    }

    char* cwd = dir_get_current();
    if (!cwd) return false;

    bool all_loaded = true;

    for (int i = 0; i < config->plugins_count; i++) {
        PluginDep* plugin = &config->plugins[i];

        // Skip disabled plugins
        if (!plugin->enabled) {
            continue;
        }

        if (!plugin->path) {
            fprintf(stderr, "[kryon][config] Plugin '%s' missing path\n", plugin->name);
            all_loaded = false;
            continue;
        }

        // Resolve plugin path (handle relative paths like "../kryon-syntax")
        // This canonicalizes the path, resolving .., ., and symlinks
        char* resolved_path = path_resolve_canonical(plugin->path, cwd);

        if (!resolved_path) {
            // Path doesn't exist, has permission issues, or resolution failed
            fprintf(stderr, "[kryon][config] Plugin '%s' path not found or inaccessible: %s\n",
                    plugin->name, plugin->path);
            if (errno == ENOENT) {
                fprintf(stderr, "                 (path does not exist)\n");
            } else if (errno == EACCES) {
                fprintf(stderr, "                 (permission denied)\n");
            }
            all_loaded = false;
            continue;
        }

        if (!file_is_directory(resolved_path)) {
            fprintf(stderr, "[kryon][config] Plugin '%s' path is not a directory: %s\n",
                    plugin->name, resolved_path);
            free(resolved_path);
            all_loaded = false;
            continue;
        }

        // Discover plugin in the specified path
        uint32_t count = 0;
        IRPluginDiscoveryInfo** discovered = ir_plugin_discover(resolved_path, &count);

        if (!discovered || count == 0) {
            fprintf(stderr, "[kryon][config] No plugin found at: %s\n", resolved_path);
            if (resolved_path != plugin->path) {
                free(resolved_path);
            }
            all_loaded = false;
            continue;
        }

        // Load the first discovered plugin
        IRPluginDiscoveryInfo* info = discovered[0];
        IRPluginHandle* handle = ir_plugin_load_with_metadata(info->path, info->name, info);

        if (handle) {
            printf("[kryon][plugin] Loaded plugin '%s' from %s\n", plugin->name, resolved_path);

            // Call init function if present
            if (handle->init_func) {
                handle->init_func(NULL);
            }

            // Store resolved path in config
            plugin->resolved_path = str_copy(resolved_path);
        } else {
            fprintf(stderr, "[kryon][config] Failed to load plugin '%s' from %s\n",
                    plugin->name, resolved_path);
            all_loaded = false;
        }

        ir_plugin_free_discovery(discovered, count);
        if (resolved_path != plugin->path) {
            free(resolved_path);
        }
    }

    free(cwd);
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

        // Load plugins after loading config
        if (config) {
            config_load_plugins(config);
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
            free(config->plugins[i].version);
            free(config->plugins[i].resolved_path);
        }
        free(config->plugins);
    }

    free(config);
}
