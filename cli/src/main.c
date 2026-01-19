/**
 * Kryon CLI - Pure C Implementation
 *
 * Entry point for the Kryon command-line interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kryon_cli.h"
#include <ir_capability.h>

// Plugin loading is now per-project, handled by config system
// Capability registry is initialized globally for all plugins

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
    printf("  install                 Install application\n");
    printf("  uninstall               Uninstall application\n");
    printf("  doctor                  System diagnostics\n\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help\n");
    printf("  -v, --version           Show version\n");
}

int main(int argc, char** argv) {
    // Handle no arguments
    if (argc < 2) {
        print_help();
        return 1;
    }

    // Parse arguments
    CLIArgs* args = cli_args_parse(argc, argv);
    if (!args) {
        fprintf(stderr, "Error: Failed to parse arguments\n");
        return 1;
    }

    // Handle global flags
    if (args->help) {
        print_help();
        cli_args_free(args);
        return 0;
    }

    if (args->version) {
        print_version();
        cli_args_free(args);
        return 0;
    }

    // Initialize the capability registry
    // This must be done before any plugin operations
    ir_capability_registry_init();

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
    else if (strcmp(args->command, "install") == 0) {
        result = cmd_install(args->argc, args->argv);
    }
    else if (strcmp(args->command, "uninstall") == 0) {
        result = cmd_uninstall(args->argc, args->argv);
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", args->command);
        fprintf(stderr, "Run 'kryon --help' for usage information\n");
        result = 1;
    }

    // Shutdown the capability registry
    // This unloads all plugins and frees resources
    ir_capability_registry_shutdown();

    cli_args_free(args);
    return result;
}
