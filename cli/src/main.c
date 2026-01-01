/**
 * Kryon CLI - Pure C Implementation
 *
 * Entry point for the Kryon command-line interface.
 * Replaces the previous Nim implementation with pure C.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kryon_cli.h"
#include <ir_plugin.h>

// Track loaded plugin handles for cleanup
static IRPluginHandle** g_loaded_plugins = NULL;
static uint32_t g_loaded_plugin_count = 0;

/**
 * Initialize the plugin system - discover and load plugins from standard paths.
 */
static void init_plugins(void) {
    uint32_t discovered_count = 0;
    IRPluginDiscoveryInfo** discovered = ir_plugin_discover(NULL, &discovered_count);

    if (!discovered || discovered_count == 0) {
        // No plugins found - that's OK
        return;
    }

    // Allocate storage for handles
    g_loaded_plugins = calloc(discovered_count, sizeof(IRPluginHandle*));
    if (!g_loaded_plugins) {
        ir_plugin_free_discovery(discovered, discovered_count);
        return;
    }

    // Load each discovered plugin
    for (uint32_t i = 0; i < discovered_count; i++) {
        IRPluginDiscoveryInfo* info = discovered[i];
        IRPluginHandle* handle = ir_plugin_load(info->path, info->name);

        if (handle) {
            g_loaded_plugins[g_loaded_plugin_count++] = handle;

            // Call init function if present
            if (handle->init_func) {
                handle->init_func(NULL);
            }
        }
    }

    ir_plugin_free_discovery(discovered, discovered_count);
}

/**
 * Shutdown the plugin system - unload all plugins.
 */
static void shutdown_plugins(void) {
    if (!g_loaded_plugins) return;

    for (uint32_t i = 0; i < g_loaded_plugin_count; i++) {
        if (g_loaded_plugins[i]) {
            ir_plugin_unload(g_loaded_plugins[i]);
        }
    }

    free(g_loaded_plugins);
    g_loaded_plugins = NULL;
    g_loaded_plugin_count = 0;
}

static void print_version(void) {
    printf("Kryon CLI %s\n", KRYON_CLI_VERSION);
    printf("License: %s\n", KRYON_CLI_LICENSE);
    printf("Architecture: C99 ABI + Multi-language Frontends\n");
}

static void print_help(void) {
    printf("Kryon CLI - Universal UI Framework\n\n");
    printf("Usage:\n");
    printf("  kryon <command> [options]\n\n");
    printf("Commands:\n");
    printf("  new <name>              Create new project\n");
    printf("  build [targets]         Build project\n");
    printf("  compile <file>          Compile to KIR\n");
    printf("  run <target>            Run compiled app\n");
    printf("  dev <file>              Development server\n");
    printf("  test <file.kyt>         Run tests\n");
    printf("  plugin <cmd>            Plugin management\n");
    printf("  codegen <type> <file>   Code generation\n");
    printf("  inspect <file>          IR inspection\n");
    printf("  diff <f1> <f2>          IR comparison\n");
    printf("  config [show|validate]  Config management\n");
    printf("  doctor                  System diagnostics\n\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help\n");
    printf("  -v, --version           Show version\n");
}

int main(int argc, char** argv) {
    // Initialize plugin system
    init_plugins();

    // Handle no arguments
    if (argc < 2) {
        print_help();
        shutdown_plugins();
        return 1;
    }

    // Parse arguments
    CLIArgs* args = cli_args_parse(argc, argv);
    if (!args) {
        fprintf(stderr, "Error: Failed to parse arguments\n");
        shutdown_plugins();
        return 1;
    }

    // Handle global flags
    if (args->help) {
        print_help();
        cli_args_free(args);
        shutdown_plugins();
        return 0;
    }

    if (args->version) {
        print_version();
        cli_args_free(args);
        shutdown_plugins();
        return 0;
    }

    // Command dispatch
    int result = 1;

    if (strcmp(args->command, "build") == 0) {
        result = cmd_build(args->argc, args->argv);
    }
    else if (strcmp(args->command, "new") == 0) {
        result = cmd_new(args->argc, args->argv);
    }
    else if (strcmp(args->command, "compile") == 0) {
        result = cmd_compile(args->argc, args->argv);
    }
    else if (strcmp(args->command, "run") == 0) {
        result = cmd_run(args->argc, args->argv);
    }
    else if (strcmp(args->command, "dev") == 0) {
        result = cmd_dev(args->argc, args->argv);
    }
    else if (strcmp(args->command, "test") == 0) {
        result = cmd_test(args->argc, args->argv);
    }
    else if (strcmp(args->command, "plugin") == 0) {
        result = cmd_plugin(args->argc, args->argv);
    }
    else if (strcmp(args->command, "codegen") == 0) {
        result = cmd_codegen(args->argc, args->argv);
    }
    else if (strcmp(args->command, "inspect") == 0) {
        result = cmd_inspect(args->argc, args->argv);
    }
    else if (strcmp(args->command, "diff") == 0) {
        result = cmd_diff(args->argc, args->argv);
    }
    else if (strcmp(args->command, "config") == 0) {
        result = cmd_config(args->argc, args->argv);
    }
    else if (strcmp(args->command, "doctor") == 0) {
        result = cmd_doctor(args->argc, args->argv);
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", args->command);
        fprintf(stderr, "Run 'kryon --help' for usage information\n");
        result = 1;
    }

    cli_args_free(args);
    shutdown_plugins();
    return result;
}
