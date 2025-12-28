/**
 * Command Stub Implementations
 * Placeholder implementations for all commands
 * Will be replaced with real implementations in later steps
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



int cmd_plugin(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("plugin command not yet implemented\n");
    return 1;
}


int cmd_test(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("test command not yet implemented\n");
    return 1;
}

/**
 * Dev Command
 * Start development server with automatic rebuilding
 */
int cmd_dev(int argc, char** argv) {
    // Load config
    KryonConfig* config = config_find_and_load();
    if (!config) {
        fprintf(stderr, "Error: No kryon.toml found\n");
        fprintf(stderr, "Run 'kryon new <name>' to create a new project\n");
        return 1;
    }

    // Validate config
    if (!config_validate(config)) {
        config_free(config);
        return 1;
    }

    // Ensure we have a web target
    bool has_web_target = false;
    for (int i = 0; i < config->build_targets_count; i++) {
        if (strcmp(config->build_targets[i], "web") == 0) {
            has_web_target = true;
            break;
        }
    }

    if (!has_web_target) {
        fprintf(stderr, "Error: dev server requires 'web' target in build.targets\n");
        fprintf(stderr, "Add to your kryon.toml:\n");
        fprintf(stderr, "  [build]\n");
        fprintf(stderr, "  targets = [\"web\"]\n");
        config_free(config);
        return 1;
    }

    // Determine source file
    const char* source_file = NULL;
    if (argc > 0) {
        source_file = argv[0];
    } else if (config->build_entry) {
        source_file = config->build_entry;
    } else {
        fprintf(stderr, "Error: No source file specified\n");
        fprintf(stderr, "Usage: kryon dev <file>  OR  specify build.entry in kryon.toml\n");
        config_free(config);
        return 1;
    }

    // Build the project first
    printf("Building project...\n");
    char build_cmd[1024];
    snprintf(build_cmd, sizeof(build_cmd), "kryon build \"%s\"", source_file);

    int build_result = system(build_cmd);
    if (build_result != 0) {
        fprintf(stderr, "Error: Initial build failed\n");
        config_free(config);
        return 1;
    }

    printf("✓ Build complete\n\n");

    // Start dev server
    const char* output_dir = config->build_output_dir ? config->build_output_dir : "build";
    int port = config->dev_port > 0 ? config->dev_port : 3000;
    bool auto_open = config->dev_auto_open;

    // Declare the function (implemented in dev_server.c)
    extern int start_dev_server(const char* document_root, int port, bool auto_open);

    int result = start_dev_server(output_dir, port, auto_open);

    config_free(config);
    return result;
}

int cmd_doctor(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("doctor command not yet implemented\n");
    return 1;
}
