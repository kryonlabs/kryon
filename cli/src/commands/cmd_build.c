/**
 * Build Command
 * Orchestrates the build pipeline: Frontend → KIR → Codegen
 */

#include "kryon_cli.h"
#include "build.h"
#include "build/plugin_discovery.h"
#include "../template/docs_template.h"
#include "../utils/file_discovery.h"
#include "build/luajit_build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Build a single source file
 */
static int build_single_file(const char* source_file, KryonConfig* config) {
    const char* frontend = detect_frontend_type(source_file);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
        fprintf(stderr, "Use a supported file extension (.tsx, .html, .md, .kry, etc.)\n");
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

/**
 * Build a discovered file with template application if needed
 */
static int build_discovered_file(DiscoveredFile* file, KryonConfig* config, const char* docs_template) {
    if (!file || !file->source_path) {
        fprintf(stderr, "Error: Invalid file entry\n");
        return 1;
    }

    printf("  %s → %s\n", file->source_path, file->output_path);

    // Create cache directory
    if (!file_is_directory(".kryon_cache")) {
        dir_create(".kryon_cache");
    }

    // Generate KIR filename
    const char* slash = strrchr(file->source_path, '/');
    const char* basename = slash ? slash + 1 : file->source_path;
    char kir_name[512];
    strncpy(kir_name, basename, sizeof(kir_name) - 1);
    kir_name[sizeof(kir_name) - 1] = '\0';
    char* dot = strrchr(kir_name, '.');
    if (dot) *dot = '\0';

    char content_kir_file[1024];
    snprintf(content_kir_file, sizeof(content_kir_file), ".kryon_cache/%s_content.kir", kir_name);

    // Compile to KIR
    int result = compile_source_to_kir(file->source_path, content_kir_file);
    if (result != 0 || !file_exists(content_kir_file)) {
        fprintf(stderr, "Error: Compilation failed\n");
        return result != 0 ? result : 1;
    }

    // Extract directory from output path
    char output_dir[2048];
    strncpy(output_dir, file->output_path, sizeof(output_dir) - 1);
    output_dir[sizeof(output_dir) - 1] = '\0';
    char* last_slash = strrchr(output_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    }

    // Create output directory
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    // Use shared docs template function
    result = build_with_docs_template(
        content_kir_file,
        file->source_path,
        file->route,
        output_dir,
        docs_template,
        config
    );

    return result;
}

/**
 * Check if "desktop" is in build targets
 */
static int has_desktop_target(KryonConfig* config) {
    if (!config->build_targets || config->build_targets_count == 0) {
        return 0;
    }
    for (int i = 0; i < config->build_targets_count; i++) {
        if (strcmp(config->build_targets[i], "desktop") == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * Build Lua desktop binary
 */
static int build_lua_desktop(KryonConfig* config) {
    // Check if entry file is Lua
    if (!config->build_entry) {
        return 1;  // No entry file, not a Lua build
    }

    size_t entry_len = strlen(config->build_entry);
    if (entry_len < 4 || strcmp(config->build_entry + entry_len - 4, ".lua") != 0) {
        return 1;  // Not a Lua file
    }

    // Check if desktop is a target
    if (!has_desktop_target(config)) {
        return 1;  // Desktop not in targets
    }

    // Check if LuaJIT is available
    if (!check_luajit_available()) {
        fprintf(stderr, "Warning: LuaJIT not found, skipping desktop binary build\n");
        fprintf(stderr, "Install LuaJIT to enable desktop binary builds\n");
        return 1;
    }

    // Create output directory
    const char* output_dir = config->build_output_dir ? config->build_output_dir : "dist";
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    // Generate binary path
    char binary_path[1024];
    snprintf(binary_path, sizeof(binary_path), "%s/%s", output_dir, config->project_name);

    // Build the binary
    int result = build_lua_binary(config->project_name,
                                   config->build_entry,
                                   binary_path,
                                   NULL,  /* runtime_dir - use standard paths */
                                   0);    /* verbose */

    return result;
}

/**
 * Build command entry point
 */
int cmd_build(int argc, char** argv) {
    // Parse command line arguments
    const char* cli_target = NULL;
    const char* cli_output_dir = NULL;
    int file_arg_start = 0;

    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--target=", 9) == 0) {
            cli_target = argv[i] + 9;
        } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            cli_target = argv[++i];
        } else if (strncmp(argv[i], "--output-dir=", 13) == 0) {
            cli_output_dir = argv[i] + 13;
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            cli_output_dir = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Supported options: --target=<web|desktop>, --output-dir=<dir>\n");
            return 1;
        } else if (file_arg_start == 0) {
            // First non-option argument is the file
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

    if (!file_exists(config_path)) {
        fprintf(stderr, "Error: Could not find kryon.toml\n");
        fprintf(stderr, "Run 'kryon new <name>' to create a new project\n");
        free(config_path);
        return 1;
    }

    // Load config WITHOUT loading plugins (plugins may not be compiled yet)
    KryonConfig* config = config_load(config_path);
    free(config_path);

    if (!config) {
        fprintf(stderr, "Error: Failed to load kryon.toml\n");
        return 1;
    }

    // Auto-build plugins BEFORE loading them
    if (config->plugins_count > 0) {
        char* project_dir = dir_get_current();
        if (project_dir) {
            int plugin_count = 0;
            BuildPluginInfo* plugins = discover_build_plugins(project_dir, config, &plugin_count);
            if (plugins) {
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
        // Validate CLI target
        if (strcmp(cli_target, "web") != 0 && strcmp(cli_target, "desktop") != 0) {
            fprintf(stderr, "Error: Invalid target '%s'. Valid targets: web, desktop\n", cli_target);
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
    int is_desktop = primary_target && strcmp(primary_target, "desktop") == 0;
    int is_web = primary_target && strcmp(primary_target, "web") == 0;

    // Desktop build (Lua binary) takes priority
    if (is_desktop) {
        printf("Building desktop targets...\n");
        int desktop_result = build_lua_desktop(config);
        if (desktop_result != 0) {
            // Desktop build failed
            config_free(config);
            return 1;
        }
        // Desktop only, we're done
        config_free(config);
        return 0;
    }

    // If specific file argument provided, build just that file (web only)
    if (file_arg_start > 0 && file_arg_start < argc) {
        if (!is_web) {
            fprintf(stderr, "Error: Single file build only supports web targets\n");
            config_free(config);
            return 1;
        }

        const char* source_file = argv[file_arg_start];

        if (!file_exists(source_file)) {
            fprintf(stderr, "Error: Source file not found: %s\n", source_file);
            config_free(config);
            return 1;
        }

        result = build_single_file(source_file, config);
        config_free(config);
        return result;
    }

    // Web build: auto-discover and build web files
    if (is_web) {
        printf("Building web targets...\n");

        // If entry file is explicitly specified, build only that file
        if (config->build_entry) {
            printf("Building entry file: %s\n", config->build_entry);
            result = build_single_file(config->build_entry, config);
        } else {
            // No entry specified - auto-discover all buildable files in project
            DiscoveryResult* discovered = discover_project_files(".");

            if (!discovered || discovered->count == 0) {
                fprintf(stderr, "No buildable files found.\n");
                fprintf(stderr, "Create index.html or docs/*.md to get started.\n");
                if (discovered) free_discovery_result(discovered);
                config_free(config);
                return 1;
            }

            // Build all discovered files
            printf("Building %d web file(s)...\n", discovered->count);
            for (uint32_t i = 0; i < discovered->count && result == 0; i++) {
                result = build_discovered_file(&discovered->files[i], config, discovered->docs_template);
            }

            if (result == 0) {
                printf("✓ Web build complete\n");
            }

            free_discovery_result(discovered);
        }
    }

    config_free(config);
    return result;
}
