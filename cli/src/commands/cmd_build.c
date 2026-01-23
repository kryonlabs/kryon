/**
 * Build Command
 * Orchestrates the build pipeline: Frontend → KIR → Codegen
 */

#include "kryon_cli.h"
#include "build.h"
#include "build/plugin_discovery.h"
#include "build/c_desktop.h"
#include "../template/docs_template.h"
#include "../utils/file_discovery.h"
#include "build/luajit_build.h"
#include "../../../codegens/c/ir_c_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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
 * Build C desktop binary from KRY frontend
 * Pipeline: KRY → KIR → C codegen → compile → native binary
 */
static int build_c_desktop(KryonConfig* config) {
    const char* entry = config->build_entry;
    const char* project_name = config->project_name;

    /* Validate entry file */
    if (!entry) {
        fprintf(stderr, "Error: No entry file specified in kryon.toml\n");
        return 1;
    }

    if (!str_ends_with(entry, ".kry")) {
        fprintf(stderr, "Error: C desktop build requires .kry entry file, got: %s\n", entry);
        return 1;
    }

    if (!file_exists(entry)) {
        fprintf(stderr, "Error: Entry file not found: %s\n", entry);
        return 1;
    }

    printf("[C Desktop] Building %s...\n", project_name);

    /* Step 1: Compile KRY to KIR */
    if (!file_is_directory(".kryon_cache")) {
        dir_create(".kryon_cache");
    }

    char kir_path[PATH_MAX];
    snprintf(kir_path, sizeof(kir_path), ".kryon_cache/main.kir");

    printf("[C Desktop] Compiling KRY → KIR...\n");
    if (compile_source_to_kir(entry, kir_path) != 0) {
        fprintf(stderr, "Error: Failed to compile KRY to KIR\n");
        return 1;
    }

    if (!file_exists(kir_path)) {
        fprintf(stderr, "Error: KIR file was not generated: %s\n", kir_path);
        return 1;
    }

    /* Step 2: Generate C code from KIR */
    const char* output_dir = config->build_output_dir ? config->build_output_dir : "build";
    char gen_dir[PATH_MAX];
    snprintf(gen_dir, sizeof(gen_dir), "%s/generated", output_dir);

    if (!file_is_directory(gen_dir)) {
        dir_create_recursive(gen_dir);
    }

    printf("[C Desktop] Generating C code → %s\n", gen_dir);
    if (!ir_generate_c_code_multi(kir_path, gen_dir)) {
        fprintf(stderr, "Error: Failed to generate C code from KIR\n");
        return 1;
    }

    /* Step 3: Compile and link C code to binary */
    printf("[C Desktop] Compiling C → native binary...\n");
    return compile_and_link_c_desktop(gen_dir, project_name, config);
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
                // Write extracted plugin code to build directory
                const char* build_dir = config->build_output_dir ? config->build_output_dir : "build";
                write_plugin_code_files(plugins, plugin_count, build_dir);

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
        if (!target_handler_find(cli_target)) {
            fprintf(stderr, "Error: Invalid target '%s'.\n", cli_target);
            fprintf(stderr, "       Valid targets: ");
            const char** targets = target_handler_list_names();
            for (int i = 0; targets[i]; i++) {
                fprintf(stderr, "%s%s", i > 0 ? ", " : "", targets[i]);
            }
            fprintf(stderr, "\n");
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

    // Desktop build - route based on frontend configuration
    if (is_desktop) {
        printf("Building desktop targets...\n");

        const char* frontend = config->build_frontend;
        int desktop_result;

        if (frontend && strcmp(frontend, "kry") == 0) {
            // KRY frontend → C codegen → native binary
            printf("Frontend: kry (C code generation)\n");
            desktop_result = build_c_desktop(config);
        } else if (frontend && strcmp(frontend, "lua") == 0) {
            // Lua frontend → LuaJIT binary
            printf("Frontend: lua (LuaJIT compilation)\n");
            desktop_result = build_lua_desktop(config);
        } else if (!frontend || strcmp(frontend, "auto") == 0) {
            // Auto-detect based on entry file extension
            const char* entry = config->build_entry;
            if (entry && str_ends_with(entry, ".kry")) {
                printf("Frontend: auto-detected kry (C code generation)\n");
                desktop_result = build_c_desktop(config);
            } else if (entry && str_ends_with(entry, ".lua")) {
                printf("Frontend: auto-detected lua (LuaJIT compilation)\n");
                desktop_result = build_lua_desktop(config);
            } else {
                fprintf(stderr, "Error: Cannot auto-detect frontend for entry '%s'\n",
                        entry ? entry : "(none)");
                fprintf(stderr, "Specify frontend = \"kry\" or \"lua\" in [build] section\n");
                config_free(config);
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown frontend '%s'\n", frontend);
            fprintf(stderr, "Supported frontends: kry, lua, auto\n");
            config_free(config);
            return 1;
        }

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

    // Web build: requires explicit entry point in kryon.toml
    if (is_web) {
        printf("Building web targets...\n");

        if (!config->build_entry) {
            fprintf(stderr, "Error: No entry point specified in kryon.toml\n");
            fprintf(stderr, "Add [build].entry to your configuration, e.g.:\n");
            fprintf(stderr, "  [build]\n");
            fprintf(stderr, "  entry = \"main.lua\"\n");
            config_free(config);
            return 1;
        }

        printf("Building entry file: %s\n", config->build_entry);
        result = build_single_file(config->build_entry, config);
    }

    config_free(config);
    return result;
}
