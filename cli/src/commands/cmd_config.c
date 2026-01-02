/**
 * Config Command
 * Shows and validates kryon.toml configuration
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <string.h>

int cmd_config(int argc, char** argv) {
    const char* action = (argc > 0) ? argv[0] : "show";

    KryonConfig* config = config_find_and_load();
    if (!config) {
        fprintf(stderr, "Error: Could not find or load kryon.toml\n");
        return 1;
    }

    if (strcmp(action, "validate") == 0) {
        printf("Validating configuration...\n");
        if (config_validate(config)) {
            printf("✓ Configuration is valid\n");
            config_free(config);
            return 0;
        } else {
            printf("✗ Configuration is invalid\n");
            config_free(config);
            return 1;
        }
    }
    else if (strcmp(action, "show") == 0) {
        printf("Kryon Configuration:\n\n");

        printf("[project]\n");
        if (config->project_name)
            printf("  name = \"%s\"\n", config->project_name);
        if (config->project_version)
            printf("  version = \"%s\"\n", config->project_version);
        if (config->project_author)
            printf("  author = \"%s\"\n", config->project_author);
        if (config->project_description)
            printf("  description = \"%s\"\n", config->project_description);

        printf("\n[build]\n");
        if (config->build_target)
            printf("  target = \"%s\"\n", config->build_target);
        if (config->build_targets && config->build_targets_count > 0) {
            printf("  targets = [");
            for (int i = 0; i < config->build_targets_count; i++) {
                printf("\"%s\"%s", config->build_targets[i],
                       i < config->build_targets_count - 1 ? ", " : "");
            }
            printf("]\n");
        }
        if (config->build_output_dir)
            printf("  output_dir = \"%s\"\n", config->build_output_dir);

        printf("\n[optimization]\n");
        printf("  enabled = %s\n", config->optimization_enabled ? "true" : "false");
        printf("  minify_css = %s\n", config->optimization_minify_css ? "true" : "false");
        printf("  minify_js = %s\n", config->optimization_minify_js ? "true" : "false");
        printf("  tree_shake = %s\n", config->optimization_tree_shake ? "true" : "false");

        printf("\n[dev]\n");
        printf("  hot_reload = %s\n", config->dev_hot_reload ? "true" : "false");
        printf("  port = %d\n", config->dev_port);
        printf("  auto_open = %s\n", config->dev_auto_open ? "true" : "false");

        if (config->plugins_count > 0) {
            printf("\n[plugins]\n");
            for (int i = 0; i < config->plugins_count; i++) {
                printf("  %s = \"%s\"\n",
                       config->plugins[i].name ? config->plugins[i].name : "?",
                       config->plugins[i].version ? config->plugins[i].version : "?");
            }
        }

        config_free(config);
        return 0;
    }
    else {
        fprintf(stderr, "Unknown config action: %s\n", action);
        fprintf(stderr, "Usage: kryon config [show|validate]\n");
        config_free(config);
        return 1;
    }
}
