/**
 * Configuration Management
 * Loads and validates kryon.toml configuration files
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Create empty config with defaults
 */
static KryonConfig* config_create_default(void) {
    KryonConfig* config = (KryonConfig*)calloc(1, sizeof(KryonConfig));
    if (!config) return NULL;

    // Set defaults
    config->build_target = str_copy("web");
    config->build_output_dir = str_copy("dist");
    config->build_frontend = str_copy("tsx");
    config->optimization_enabled = true;
    config->optimization_minify_css = true;
    config->optimization_minify_js = true;
    config->optimization_tree_shake = true;
    config->dev_hot_reload = true;
    config->dev_port = 3000;
    config->dev_auto_open = true;
    config->docs_enabled = false;

    return config;
}

/**
 * Parse pages array from TOML
 */
static void config_parse_pages(KryonConfig* config, TOMLTable* toml) {
    // Count pages by looking for build.pages.N.* keys
    int max_page_idx = -1;

    // Simple implementation: try to find pages 0-99
    for (int i = 0; i < 100; i++) {
        char key[128];
        snprintf(key, sizeof(key), "build.pages.%d.name", i);
        if (toml_get_string(toml, key, NULL)) {
            max_page_idx = i;
        }
    }

    if (max_page_idx < 0) {
        config->pages_count = 0;
        config->build_pages = NULL;
        return;
    }

    config->pages_count = max_page_idx + 1;
    config->build_pages = (PageEntry*)calloc(config->pages_count, sizeof(PageEntry));
    if (!config->build_pages) {
        config->pages_count = 0;
        return;
    }

    for (int i = 0; i <= max_page_idx; i++) {
        char key[128];

        snprintf(key, sizeof(key), "build.pages.%d.name", i);
        const char* name = toml_get_string(toml, key, NULL);
        if (name) {
            config->build_pages[i].name = str_copy(name);
        }

        snprintf(key, sizeof(key), "build.pages.%d.route", i);
        const char* route = toml_get_string(toml, key, NULL);
        if (route) {
            config->build_pages[i].route = str_copy(route);
        }

        snprintf(key, sizeof(key), "build.pages.%d.source", i);
        const char* source = toml_get_string(toml, key, NULL);
        if (source) {
            config->build_pages[i].source = str_copy(source);
        }
    }
}

/**
 * Parse plugins array from TOML
 */
static void config_parse_plugins(KryonConfig* config, TOMLTable* toml) {
    // Count plugins by looking for plugins.N.* keys
    int max_plugin_idx = -1;

    for (int i = 0; i < 100; i++) {
        char key[128];
        snprintf(key, sizeof(key), "plugins.%d.name", i);
        if (toml_get_string(toml, key, NULL)) {
            max_plugin_idx = i;
        }
    }

    if (max_plugin_idx < 0) {
        config->plugins_count = 0;
        config->plugins = NULL;
        return;
    }

    config->plugins_count = max_plugin_idx + 1;
    config->plugins = (PluginDep*)calloc(config->plugins_count, sizeof(PluginDep));
    if (!config->plugins) {
        config->plugins_count = 0;
        return;
    }

    for (int i = 0; i <= max_plugin_idx; i++) {
        char key[128];

        snprintf(key, sizeof(key), "plugins.%d.name", i);
        const char* name = toml_get_string(toml, key, NULL);
        if (name) {
            config->plugins[i].name = str_copy(name);
        }

        snprintf(key, sizeof(key), "plugins.%d.version", i);
        const char* version = toml_get_string(toml, key, NULL);
        if (version) {
            config->plugins[i].version = str_copy(version);
        }
    }
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

    // Parse build settings
    const char* target = toml_get_string(toml, "build.target", NULL);
    if (target) {
        free(config->build_target);
        config->build_target = str_copy(target);
    }

    const char* output_dir = toml_get_string(toml, "build.output_dir", NULL);
    if (output_dir) {
        free(config->build_output_dir);
        config->build_output_dir = str_copy(output_dir);
    }

    // Support both build.entry and build.entry_point
    const char* entry = toml_get_string(toml, "build.entry", NULL);
    if (!entry) {
        entry = toml_get_string(toml, "build.entry_point", NULL);
    }
    if (entry) {
        config->build_entry = str_copy(entry);
    }

    // Support both build.frontend and project.frontend
    const char* frontend = toml_get_string(toml, "build.frontend", NULL);
    if (!frontend) {
        frontend = toml_get_string(toml, "project.frontend", NULL);
    }
    if (frontend) {
        free(config->build_frontend);
        config->build_frontend = str_copy(frontend);
    }

    // Parse pages
    config_parse_pages(config, toml);

    // Parse optimization settings
    config->optimization_enabled = toml_get_bool(toml, "optimization.enabled", true);
    config->optimization_minify_css = toml_get_bool(toml, "optimization.minify_css", true);
    config->optimization_minify_js = toml_get_bool(toml, "optimization.minify_js", true);
    config->optimization_tree_shake = toml_get_bool(toml, "optimization.tree_shake", true);

    // Parse dev settings
    config->dev_hot_reload = toml_get_bool(toml, "dev.hot_reload", true);
    config->dev_port = toml_get_int(toml, "dev.port", 3000);
    config->dev_auto_open = toml_get_bool(toml, "dev.auto_open", true);

    // Parse docs settings
    config->docs_enabled = toml_get_bool(toml, "docs.enabled", false);
    const char* docs_dir = toml_get_string(toml, "docs.directory", NULL);
    if (docs_dir) {
        config->docs_directory = str_copy(docs_dir);
    }

    // Parse plugins
    config_parse_plugins(config, toml);

    toml_free(toml);
    return config;
}

/**
 * Find and load kryon.toml from current directory or parent directories
 */
KryonConfig* config_find_and_load(void) {
    char* cwd = dir_get_current();
    if (!cwd) return NULL;

    char* current_dir = cwd;
    char* config_path = NULL;

    // Walk up directory tree looking for kryon.toml
    while (true) {
        config_path = path_join(current_dir, "kryon.toml");

        if (file_exists(config_path)) {
            KryonConfig* config = config_load(config_path);
            free(config_path);
            if (current_dir != cwd) free(current_dir);
            free(cwd);
            return config;
        }

        free(config_path);

        // Check if we're at root
        if (strcmp(current_dir, "/") == 0) {
            break;
        }

        // Move to parent directory
        char* parent = path_join(current_dir, "..");
        if (current_dir != cwd) free(current_dir);
        current_dir = parent;
    }

    if (current_dir != cwd) free(current_dir);
    free(cwd);
    return NULL;
}

/**
 * Validate configuration
 */
bool config_validate(KryonConfig* config) {
    if (!config) return false;

    // Validate required fields
    if (!config->project_name) {
        fprintf(stderr, "Error: project.name is required in kryon.toml\n");
        return false;
    }

    if (!config->build_target) {
        fprintf(stderr, "Error: build.target is required in kryon.toml\n");
        return false;
    }

    // Validate target is supported
    const char* valid_targets[] = {"web", "desktop", "terminal", "embedded",
                                    "linux", "windows", "macos", "stm32", "esp32"};
    bool valid_target = false;
    for (size_t i = 0; i < sizeof(valid_targets) / sizeof(valid_targets[0]); i++) {
        if (strcmp(config->build_target, valid_targets[i]) == 0) {
            valid_target = true;
            break;
        }
    }

    if (!valid_target) {
        fprintf(stderr, "Error: Invalid build target '%s'\n", config->build_target);
        fprintf(stderr, "Valid targets: web, desktop, terminal, embedded, linux, windows, macos, stm32, esp32\n");
        return false;
    }

    return true;
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
    free(config->build_frontend);
    free(config->docs_directory);

    if (config->build_targets) {
        for (int i = 0; i < config->build_targets_count; i++) {
            free(config->build_targets[i]);
        }
        free(config->build_targets);
    }

    if (config->build_pages) {
        for (int i = 0; i < config->pages_count; i++) {
            free(config->build_pages[i].name);
            free(config->build_pages[i].route);
            free(config->build_pages[i].source);
        }
        free(config->build_pages);
    }

    if (config->plugins) {
        for (int i = 0; i < config->plugins_count; i++) {
            free(config->plugins[i].name);
            free(config->plugins[i].version);
        }
        free(config->plugins);
    }

    free(config);
}
