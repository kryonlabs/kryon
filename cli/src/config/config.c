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
    config->desktop_renderer = str_copy("sdl3");  // Default to SDL3

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
    }

    // Parse build.entry (required, NO fallback to entry_point)
    const char* entry = toml_get_string(toml, "build.entry", NULL);
    if (entry) {
        config->build_entry = str_copy(entry);
    }

    // Parse build.frontend (required, NO fallback to project.frontend)
    const char* frontend = toml_get_string(toml, "build.frontend", NULL);
    if (frontend) {
        free(config->build_frontend);
        config->build_frontend = str_copy(frontend);
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
    if (!config->build_entry) {
        fprintf(stderr, "Error: build.entry is required in kryon.toml\n");
        fprintf(stderr, "       Specify the entry point file (e.g., entry = \"main.tsx\")\n");
        has_errors = true;
    }

    if (!config->build_frontend) {
        fprintf(stderr, "Error: build.frontend is required in kryon.toml\n");
        fprintf(stderr, "       Valid values: tsx, jsx, lua, nim, md, kry, c\n");
        has_errors = true;
    }

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

    // Validate frontend is supported
    if (config->build_frontend) {
        const char* valid_frontends[] = {"tsx", "jsx", "lua", "nim", "md", "kry", "c"};
        bool valid_frontend = false;
        for (size_t i = 0; i < sizeof(valid_frontends) / sizeof(valid_frontends[0]); i++) {
            if (strcmp(config->build_frontend, valid_frontends[i]) == 0) {
                valid_frontend = true;
                break;
            }
        }

        if (!valid_frontend) {
            fprintf(stderr, "Error: Invalid build.frontend '%s'\n", config->build_frontend);
            fprintf(stderr, "       Valid frontends: tsx, jsx, lua, nim, md, kry, c\n");
            has_errors = true;
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
    free(config->build_frontend);
    free(config->docs_directory);
    free(config->desktop_renderer);

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
