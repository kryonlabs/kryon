/**
 * Run Command
 * Executes source files or KIR files
 * Matches original Nim CLI behavior
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_run(int argc, char** argv) {
    const char* target_file = NULL;

    // If no file specified, use entry from config
    if (argc < 1) {
        KryonConfig* config = config_find_and_load();
        if (!config) {
            fprintf(stderr, "Error: No file specified and no kryon.toml found\n\n");
            fprintf(stderr, "Usage: kryon run [file]\n\n");
            fprintf(stderr, "Supported file types:\n");
            fprintf(stderr, "  .tsx, .jsx  - TypeScript/JavaScript with JSX\n");
            fprintf(stderr, "  .md         - Markdown documents\n");
            fprintf(stderr, "  .kry        - Kryon DSL\n");
            fprintf(stderr, "  .nim        - Nim source files\n");
            fprintf(stderr, "  .lua        - Lua scripts\n");
            fprintf(stderr, "  .kir        - Kryon IR (compiled)\n");
            return 1;
        }

        if (!config->build_entry) {
            fprintf(stderr, "Error: No file specified and no build.entry or build.entry_point in kryon.toml\n\n");
            fprintf(stderr, "Add to your kryon.toml:\n");
            fprintf(stderr, "  [build]\n");
            fprintf(stderr, "  entry = \"your-file.tsx\"  # or entry_point = \"...\"\n\n");
            fprintf(stderr, "Or run with a file:\n");
            fprintf(stderr, "  kryon run <file>\n\n");
            fprintf(stderr, "Supported file types:\n");
            fprintf(stderr, "  .tsx, .jsx  - TypeScript/JavaScript with JSX\n");
            fprintf(stderr, "  .md         - Markdown documents\n");
            fprintf(stderr, "  .kry        - Kryon DSL\n");
            fprintf(stderr, "  .nim        - Nim source files\n");
            fprintf(stderr, "  .lua        - Lua scripts\n");
            fprintf(stderr, "  .kir        - Kryon IR (compiled)\n");
            config_free(config);
            return 1;
        }

        target_file = str_copy(config->build_entry);
        printf("Running entry point: %s\n", target_file);
        config_free(config);
    } else {
        target_file = argv[0];
    }

    if (!file_exists(target_file)) {
        fprintf(stderr, "Error: File not found: %s\n", target_file);
        if (argc < 1) free((char*)target_file);
        return 1;
    }

    // Delegate to Nim CLI for full feature parity
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "exec /mnt/storage/Projects/kryon/build/kryon run \"%s\"", target_file);

    int result = system(cmd);

    if (argc < 1) free((char*)target_file);
    return result;
}
