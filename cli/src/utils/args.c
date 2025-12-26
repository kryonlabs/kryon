/**
 * Argument Parsing
 * Parses CLI arguments and extracts commands and flags
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>

/**
 * Parse command line arguments
 * Returns CLIArgs structure with command and remaining arguments
 */
CLIArgs* cli_args_parse(int argc, char** argv) {
    if (argc < 1) return NULL;

    CLIArgs* args = (CLIArgs*)malloc(sizeof(CLIArgs));
    if (!args) return NULL;

    // Initialize
    args->command = NULL;
    args->argc = 0;
    args->argv = NULL;
    args->help = false;
    args->version = false;

    // Check for global flags
    if (argc >= 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            args->help = true;
            return args;
        }
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            args->version = true;
            return args;
        }

        // First non-flag argument is the command
        args->command = str_copy(argv[1]);

        // Remaining arguments
        if (argc > 2) {
            args->argc = argc - 2;
            args->argv = (char**)malloc(args->argc * sizeof(char*));
            if (args->argv) {
                for (int i = 0; i < args->argc; i++) {
                    args->argv[i] = str_copy(argv[i + 2]);
                }
            }
        }
    }

    return args;
}

/**
 * Free CLIArgs structure
 */
void cli_args_free(CLIArgs* args) {
    if (!args) return;

    free(args->command);

    if (args->argv) {
        for (int i = 0; i < args->argc; i++) {
            free(args->argv[i]);
        }
        free(args->argv);
    }

    free(args);
}
