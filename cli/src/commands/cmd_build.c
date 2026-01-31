/**
 * Build Command
 * Orchestrates the build pipeline: Frontend → KIR → Target Codegen
 *
 * Supported targets:
 *   - web:   KRY → KIR → HTML/CSS/JS
 *   - limbo: KRY → KIR → .b → .dis (TaijiOS Limbo → DIS bytecode)
 */

#include "kryon_cli.h"
#include "build.h"
#include "build/plugin_discovery.h"
#include "../template/docs_template.h"
#include "../utils/file_discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/**
 * Build a single source file for web target
 */
static int build_single_file_web(const char* source_file, KryonConfig* config) {
    const char* frontend = detect_frontend_type(source_file);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
        fprintf(stderr, "Supported extensions: .kry, .kir, .md, .html\n");
        return 1;
    }

    // Create cache directory
    if (!file_is_directory(".kryon_cache")) {
        dir_create(".kryon_cache");
    }

    // Generate KIR filename
    char kir_file[1024];
    const char* basename = strrchr(source_file, '/');
    basename = basename ? basename + 1 : source_file;

    char* name_copy = str_copy(basename);
    char* dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';

    snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s.kir", name_copy);
    free(name_copy);

    // Compile to KIR using shared function
    int result = compile_source_to_kir(source_file, kir_file);
    if (result != 0) {
        fprintf(stderr, "Error: Compilation to KIR failed\n");
        return result;
    }

    // Verify KIR was generated
    if (!file_exists(kir_file)) {
        fprintf(stderr, "Error: KIR file was not generated: %s\n", kir_file);
        return 1;
    }

    // Create output directory
    const char* output_dir = config->build_output_dir ? config->build_output_dir : "dist";
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    // Generate HTML using shared function
    result = generate_html_from_kir(kir_file, output_dir, config->project_name, ".");
    if (result != 0) {
        fprintf(stderr, "Error: Code generation failed\n");
        return result;
    }

    printf("✓ Build complete: %s → %s\n", source_file, output_dir);
    return 0;
}

/* ============================================================================
 * Command Entry Point
 * ============================================================================ */

int cmd_build(int argc, char** argv) {
    (void)argc;  // Unused

    const char* cli_target = NULL;
    const char* cli_output_dir = NULL;
    int file_arg_start = 0;

    // Parse command-line arguments
    for (int i = 0; i < argc && argv[i]; i++) {
        if (strncmp(argv[i], "--target=", 9) == 0) {
            // Handle --target=value
            cli_target = argv[i] + 9;
        } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            // Handle --target value
            cli_target = argv[i + 1];
            i++;  // Skip next argument
        } else if (strncmp(argv[i], "--output-dir=", 13) == 0) {
            cli_output_dir = argv[i] + 13;
        } else if (argv[i][0] != '-' && argv[i][0] != '\0') {
            // First positional argument - could be a source file
            file_arg_start = i;
        }
    }

    // Find config path
    char* cwd = dir_get_current();
    if (!cwd) {
        fprintf(stderr, "Error: Could not determine current directory\n");
        return 1;
    }

    char* config_path = path_join(cwd, "kryon.toml");
    free(cwd);

    KryonConfig* config = NULL;

    if (!file_exists(config_path)) {
        // If no kryon.toml but target is specified, create minimal config
        fprintf(stderr, "[DEBUG] cli_target=%s, file_arg_start=%d\n", cli_target ? cli_target : "NULL", file_arg_start);
        if (file_arg_start > 0) {
            fprintf(stderr, "[DEBUG] argv[file_arg_start]=%s\n", argv[file_arg_start]);
        }

        if (cli_target && file_arg_start >= 0) {
            config = (KryonConfig*)calloc(1, sizeof(KryonConfig));
            if (config) {
                config->project_name = str_copy("app");
                config->project_version = str_copy("0.1.0");
                config->build_entry = str_copy(argv[file_arg_start]);
                config->build_output_dir = cli_output_dir ? str_copy(cli_output_dir) : str_copy("build");
                config->build_targets = malloc(sizeof(char*));
                config->build_targets[0] = str_copy(cli_target);
                config->build_targets_count = 1;
                fprintf(stderr, "[DEBUG] Created minimal config: target=%s, entry=%s\n", cli_target, argv[file_arg_start]);
            }
        }

        if (!config) {
            fprintf(stderr, "Error: Could not find kryon.toml\n");
            fprintf(stderr, "Run 'kryon new <name>' to create a new project\n");
            fprintf(stderr, "\nTo build without kryon.toml:\n");
            fprintf(stderr, "  kryon build --target=<target> <file>\n");
            free(config_path);
            return 1;
        }
        free(config_path);
    } else {
        // Load config WITHOUT loading plugins (plugins may not be compiled yet)
        config = config_load(config_path);
        free(config_path);

        if (!config) {
            fprintf(stderr, "Error: Failed to load kryon.toml\n");
            return 1;
        }
    }

    // Auto-build plugins BEFORE loading them
    if (config->plugins_count > 0) {
        char* project_dir = dir_get_current();
        if (project_dir) {
            int plugin_count = 0;
            BuildPluginInfo* plugins = discover_build_plugins(project_dir, config, &plugin_count);
            if (plugins) {
                // Write extracted plugin code to output directory
                const char* output_dir = config->build_output_dir ? config->build_output_dir : "build";
                write_plugin_code_files(plugins, plugin_count, output_dir);

                // Write plugin manifest to build/ (where KIR files live) for codegen to use
                write_plugin_manifest(plugins, plugin_count, "build");

                free_build_plugins(plugins, plugin_count);
            }
            free(project_dir);
        }
    }

    // Now load the compiled plugins
    if (!config_load_plugins(config)) {
        fprintf(stderr, "[kryon] Failed to load plugins - aborting\n");
        config_free(config);
        return 1;
    }

    // Validate config
    if (!config_validate(config)) {
        config_free(config);
        return 1;
    }

    // Override output_dir from CLI if specified
    if (cli_output_dir) {
        if (config->build_output_dir) {
            free((char*)config->build_output_dir);
        }
        config->build_output_dir = str_copy(cli_output_dir);
        printf("Using output directory from command line: %s\n", cli_output_dir);
    }

    int result = 0;

    // Determine what to build based on targets
    // CLI --target= overrides config file
    const char* primary_target;
    if (cli_target) {
        // Validate CLI target using handler registry
        // First check if it's a valid alias or registered target
        const char* resolved_target = NULL;
        bool is_alias = target_is_alias(cli_target, &resolved_target);

        if (!is_alias && !target_handler_find(cli_target)) {
            fprintf(stderr, "Error: Unknown target '%s'\n", cli_target);
            fprintf(stderr, "\nValid targets (with aliases):\n");
            fprintf(stderr, "  limbo, dis, emu     - TaijiOS Limbo/DIS bytecode\n");
            fprintf(stderr, "  sdl3, desktop       - Desktop SDL3 renderer\n");
            fprintf(stderr, "  raylib              - Desktop Raylib renderer\n");
            fprintf(stderr, "  web                 - Web browser\n");
            fprintf(stderr, "  android, kotlin     - Android APK\n");

            config_free(config);
            return 1;
        }
        primary_target = cli_target;
        printf("Using target from command line: %s\n", primary_target);
    } else {
        // Use first target from config
        primary_target = config->build_targets_count > 0 ?
                          config->build_targets[0] : NULL;
    }

    if (!primary_target) {
        fprintf(stderr, "Error: No target specified\n");
        fprintf(stderr, "Use --target=<name> or specify targets in kryon.toml\n");
        fprintf(stderr, "Valid targets: web, limbo, sdl3, raylib, android\n");
        config_free(config);
        return 1;
    }

    // Resolve desktop alias to sdl3
    if (strcmp(primary_target, "desktop") == 0) {
        primary_target = "sdl3";
    }

    int is_web = strcmp(primary_target, "web") == 0;
    int is_limbo = strcmp(primary_target, "limbo") == 0;
    int is_sdl3 = strcmp(primary_target, "sdl3") == 0;
    int is_raylib = strcmp(primary_target, "raylib") == 0;
    int is_android = strcmp(primary_target, "android") == 0;
    int is_tcltk = strcmp(primary_target, "tcltk") == 0 || strcmp(primary_target, "tcl") == 0;

    // If specific file argument provided, build just that file
    if (file_arg_start > 0 && file_arg_start < argc) {
        const char* source_file = argv[file_arg_start];

        if (!file_exists(source_file)) {
            fprintf(stderr, "Error: Source file not found: %s\n", source_file);
            config_free(config);
            return 1;
        }

        if (is_web) {
            result = build_single_file_web(source_file, config);
        } else if (is_limbo || is_sdl3 || is_raylib || is_android || is_tcltk) {
            // For all non-web targets, use target handler
            // First compile to KIR
            char kir_file[1024];
            const char* basename = strrchr(source_file, '/');
            basename = basename ? basename + 1 : source_file;
            snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s.kir", basename);

            if (compile_source_to_kir(source_file, kir_file) != 0) {
                fprintf(stderr, "Error: Failed to compile to KIR\n");
                config_free(config);
                return 1;
            }

            // Then use target handler
            result = target_handler_build(primary_target, kir_file, ".", "app", config);
        } else {
            fprintf(stderr, "Error: Unsupported target for single file build\n");
            config_free(config);
            return 1;
        }

        config_free(config);
        return result;
    }

    // WEB TARGET: Build web application
    if (is_web) {
        printf("Building web target...\n");

        if (!config->build_entry) {
            fprintf(stderr, "Error: No entry point specified in kryon.toml\n");
            fprintf(stderr, "Add [build].entry to your configuration, e.g.:\n");
            fprintf(stderr, "  [build]\n");
            fprintf(stderr, "  entry = \"main.kry\"\n");
            config_free(config);
            return 1;
        }

        printf("Building entry file: %s\n", config->build_entry);
        result = build_single_file_web(config->build_entry, config);
        config_free(config);
        return result;
    }

    // LIMBO TARGET: Build Limbo source and DIS bytecode using target handler
    if (is_limbo) {
        printf("Building Limbo target (Limbo source → DIS bytecode)...\n");

        if (!config->build_entry) {
            fprintf(stderr, "Error: No entry point specified in kryon.toml\n");
            fprintf(stderr, "Add [build].entry to your configuration, e.g.:\n");
            fprintf(stderr, "  [build]\n");
            fprintf(stderr, "  entry = \"main.kry\"\n");
            config_free(config);
            return 1;
        }

        // Compile source to KIR
        const char* kir_file = ".kryon_cache/output.kir";
        printf("Compiling entry file: %s\n", config->build_entry);

        if (compile_source_to_kir(config->build_entry, kir_file) != 0) {
            fprintf(stderr, "Error: Failed to compile entry file\n");
            config_free(config);
            return 1;
        }

        // Get output directory and project name
        const char* output_dir = config->build_output_dir ? config->build_output_dir : "dist";
        const char* project_name = config->project_name ? config->project_name : "app";

        printf("Building Limbo from %s...\n", kir_file);
        result = target_handler_build("limbo", kir_file, output_dir, project_name, config);
        config_free(config);
        return result;
    }

    // SDL3/RAYLIB/ANDROID TARGETS: Build using target handler
    if (is_sdl3 || is_raylib || is_android) {
        const char* target_name = is_sdl3 ? "SDL3" : (is_raylib ? "Raylib" : "Android");
        printf("Building %s target...\n", target_name);

        if (!config->build_entry) {
            fprintf(stderr, "Error: No entry point specified in kryon.toml\n");
            fprintf(stderr, "Add [build].entry to your configuration, e.g.:\n");
            fprintf(stderr, "  [build]\n");
            fprintf(stderr, "  entry = \"main.kry\"\n");
            config_free(config);
            return 1;
        }

        // Compile source to KIR
        const char* kir_file = ".kryon_cache/output.kir";
        printf("Compiling entry file: %s\n", config->build_entry);

        if (compile_source_to_kir(config->build_entry, kir_file) != 0) {
            fprintf(stderr, "Error: Failed to compile entry file\n");
            config_free(config);
            return 1;
        }

        // Get output directory and project name
        const char* output_dir = config->build_output_dir ? config->build_output_dir : "dist";
        const char* project_name = config->project_name ? config->project_name : "app";

        printf("Building %s from %s...\n", target_name, kir_file);
        result = target_handler_build(primary_target, kir_file, output_dir, project_name, config);
        config_free(config);
        return result;
    }

    // UNKNOWN TARGET
    fprintf(stderr, "Error: Unknown target '%s'\n", primary_target);
    fprintf(stderr, "Supported targets: web, limbo, sdl3, raylib, android\n");
    config_free(config);
    return 1;
}
