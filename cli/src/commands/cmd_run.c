/**
 * Run Command
 * Executes source files or KIR files using C backend
 */

#define _POSIX_C_SOURCE 200809L

#include "kryon_cli.h"
#include "build.h"
// file_discovery.h removed - we only build explicit entry points from kryon.toml
#include "../../ir/include/ir_core.h"
#include "../../ir/include/ir_serialization.h"
#include "../../ir/include/ir_executor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

/* ============================================================================
 * Terminal Renderer
 * ============================================================================ */

static int run_terminal(const char* kir_file) {
    (void)kir_file;  // Currently unused
    fprintf(stderr, "Error: Terminal renderer is not available in this build\n");
    fprintf(stderr, "Please use desktop or other rendering backends instead.\n");
    return 1;
}

/* ============================================================================
 * Web Target
 * ============================================================================ */

static int run_web_target(const char* kir_file) {
    if (!kir_file || !file_exists(kir_file)) {
        fprintf(stderr, "Error: KIR file not found: %s\n", kir_file ? kir_file : "(null)");
        fprintf(stderr, "Ensure [build].entry is specified in kryon.toml\n");
        return 1;
    }

    KryonConfig* config = config_find_and_load();
    if (!config) {
        fprintf(stderr, "Error: kryon.toml not found or invalid\n");
        return 1;
    }

    const char* output_dir = config->build_output_dir ? config->build_output_dir : "build";
    int port = (config->dev && config->dev->port > 0) ? config->dev->port : 3000;
    bool auto_open = config->dev ? config->dev->auto_open : true;

    // Create output directory
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    // Generate HTML from the explicit entry point KIR file
    printf("Building %s → %s/\n", kir_file, output_dir);
    int result = generate_html_from_kir(kir_file, output_dir, config->project_name, ".");

    if (result != 0) {
        fprintf(stderr, "Error: Code generation failed\n");
        config_free(config);
        return 1;
    }

    printf("✓ HTML files generated in %s/\n\n", output_dir);

    // Start dev server
    extern int start_dev_server(const char*, int, bool);
    result = start_dev_server(output_dir, port, auto_open);

    config_free(config);
    return result;
}

/* ============================================================================
 * KIR File Execution
 * ============================================================================ */

static int run_kir_file(const char* kir_file, const char* target_platform,
                       const ScreenshotRunOptions* screenshot_opts) {
    // Use target handler registry for all targets
    TargetHandler* handler = target_handler_find(target_platform);
    if (handler && handler->run_handler) {
        return target_handler_run(target_platform, kir_file, NULL, screenshot_opts);
    }

    fprintf(stderr, "Error: Target '%s' has no run handler\n", target_platform);
    return 1;
}

/* ============================================================================
 * C File Compilation and Execution
 * ============================================================================ */

static int run_c_file(const char* target_file) {
    char exe_file[1024];
    snprintf(exe_file, sizeof(exe_file), "/tmp/kryon_app_%d", getpid());

    printf("Compiling %s to native executable...\n", target_file);

    // Find additional .c files in the same directory
    char additional_sources[2048] = "";
    {
        char dir_path[1024];
        const char* last_slash = strrchr(target_file, '/');
        if (last_slash) {
            size_t dir_len = last_slash - target_file;
            strncpy(dir_path, target_file, dir_len);
            dir_path[dir_len] = '\0';
        } else {
            strcpy(dir_path, ".");
        }

        char find_cmd[2048];
        snprintf(find_cmd, sizeof(find_cmd),
                 "find \"%s\" -name '*.c' ! -name '%s' 2>/dev/null",
                 dir_path,
                 last_slash ? last_slash + 1 : target_file);

        FILE* fp = popen(find_cmd, "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\n")] = 0;
                if (strlen(line) > 0) {
                    strncat(additional_sources, "\"", sizeof(additional_sources) - strlen(additional_sources) - 1);
                    strncat(additional_sources, line, sizeof(additional_sources) - strlen(additional_sources) - 1);
                    strncat(additional_sources, "\" ", sizeof(additional_sources) - strlen(additional_sources) - 1);
                }
            }
            pclose(fp);
        }
    }

    char compile_cmd[8192];
    char dir_include[1024] = "";
    const char* last_slash = strrchr(target_file, '/');
    if (last_slash) {
        size_t dir_len = last_slash - target_file;
        snprintf(dir_include, sizeof(dir_include), "-I%.*s ", (int)dir_len, target_file);
    }

    char* kryon_root = paths_get_kryon_root();
    char* bindings_path = kryon_root ? path_join(kryon_root, "bindings/c") : paths_get_bindings_path();
    char* ir_path = kryon_root ? path_join(kryon_root, "ir") : str_copy("ir");
    char* desktop_path = kryon_root ? path_join(kryon_root, "runtime/desktop") : str_copy("runtime/desktop");
    char* cjson_path = kryon_root ? path_join(kryon_root, "ir/third_party/cJSON") : str_copy("ir/third_party/cJSON");
    char* build_path = kryon_root ? path_join(kryon_root, "build") : paths_get_build_path();

    char kryon_c[PATH_MAX];
    char kryon_dsl_c[PATH_MAX];
    int written;

    written = snprintf(kryon_c, sizeof(kryon_c), "%s/kryon.c", bindings_path);
    (void)written; /* Suppress unused warning, truncation checked by size */
    written = snprintf(kryon_dsl_c, sizeof(kryon_dsl_c), "%s/kryon_dsl.c", bindings_path);
    (void)written; /* Suppress unused warning, truncation checked by size */

    /* Build gcc command - buffer is 8192 which is sufficient for typical paths */
    written = snprintf(compile_cmd, sizeof(compile_cmd),
             "gcc -std=gnu99 -O2 \"%s\" %s"
             "\"%s\" \"%s\" "
             "-o \"%s\" "
             "%s"
             "-Iinclude "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-L\"%s\" "
             "-lkryon_desktop -lkryon_ir -lm -lraylib",
             target_file, additional_sources,
             kryon_c, kryon_dsl_c, exe_file, dir_include,
             bindings_path, ir_path, desktop_path, cjson_path, build_path);

    /* Check for truncation */
    if (written < 0 || (size_t)written >= sizeof(compile_cmd)) {
        fprintf(stderr, "Error: Compile command too long, buffer truncated\n");
        free(bindings_path);
        free(ir_path);
        free(desktop_path);
        free(cjson_path);
        free(build_path);
        if (kryon_root) free(kryon_root);
        return 1;
    }

    free(bindings_path);
    free(ir_path);
    free(desktop_path);
    free(cjson_path);
    free(build_path);
    if (kryon_root) free(kryon_root);

    int result = system(compile_cmd);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to compile C file\n");
        return 1;
    }

    printf("✓ Compiled successfully\n");
    printf("Running application...\n");

    // Run the executable
    char* lib_build_path = paths_get_build_path();
    char run_cmd[2048];
    snprintf(run_cmd, sizeof(run_cmd),
             "LD_LIBRARY_PATH=\"%s:$LD_LIBRARY_PATH\" \"%s\"",
             lib_build_path, exe_file);
    free(lib_build_path);
    result = system(run_cmd);

    // Cleanup
    remove(exe_file);

    return result == 0 ? 0 : 1;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int cmd_run(int argc, char** argv) {
    const char* target_file = NULL;
    bool free_target = false;
    const char* target_platform = NULL;
    bool free_target_platform = false;
    bool explicit_target = false;

    // Screenshot options
    const char* screenshot_path = NULL;
    int screenshot_after_frames = 0;
    bool screenshot_requested = false;  // Track if --screenshot flag was used
    bool free_screenshot_path = false;  // Track if we allocated screenshot_path

    // Parse --target and --screenshot flags, collecting non-flag args in new_argv
    // IMPORTANT: Do NOT modify the original argv array to avoid double-free issues
    int pos_argc = 0;  // Count of positional (non-flag) arguments
    char* pos_argv[128];  // Positional arguments only
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--target=", 9) == 0) {
            // Handle --target=value
            target_platform = argv[i] + 9;
            explicit_target = true;
        } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            // Handle --target value
            target_platform = argv[i + 1];
            explicit_target = true;
            i++;  // Skip next argument
        } else if (strncmp(argv[i], "--screenshot=", 13) == 0) {
            // Handle --screenshot=value
            screenshot_path = argv[i] + 13;
            screenshot_requested = true;
        } else if (strcmp(argv[i], "--screenshot") == 0) {
            // Handle --screenshot (with or without value)
            screenshot_requested = true;
            // Check if next arg is a value (not a flag)
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                screenshot_path = argv[i + 1];
                i++;  // Skip next argument
            }
            // If no value provided, screenshot_path remains NULL
            // and we'll generate a default filename later
        } else if (strncmp(argv[i], "--screenshot-after-frames=", 25) == 0) {
            // Handle --screenshot-after-frames=value
            screenshot_after_frames = atoi(argv[i] + 25);
        } else if (strcmp(argv[i], "--screenshot-after-frames") == 0 && i + 1 < argc) {
            // Handle --screenshot-after-frames value
            screenshot_after_frames = atoi(argv[i + 1]);
            i++;  // Skip next argument
        } else if (strncmp(argv[i], "--", 2) == 0) {
            // Unknown flag (with or without =)
            fprintf(stderr, "Error: Unknown flag '%s'\n", argv[i]);
            fprintf(stderr, "\nValid flags:\n");
            fprintf(stderr, "  --target=<platform>       Specify target platform\n");
            fprintf(stderr, "  --screenshot[=<path>]     Capture screenshot to file\n");
            fprintf(stderr, "  --screenshot-after-frames=N Wait N frames before capturing\n");
            return 1;
        } else if (pos_argc < 128) {
            pos_argv[pos_argc++] = argv[i];
        }
    }

    // If screenshot not specified via CLI, check environment variables
    if (!screenshot_path) {
        screenshot_path = getenv("KRYON_SCREENSHOT");
        if (screenshot_path && strlen(screenshot_path) == 0) {
            screenshot_path = NULL;
        }
    }
    if (screenshot_after_frames == 0) {
        const char* env_frames = getenv("KRYON_SCREENSHOT_AFTER_FRAMES");
        if (env_frames) {
            screenshot_after_frames = atoi(env_frames);
        }
    }

    // Prepare screenshot options structure
    ScreenshotRunOptions screenshot_opts = {0};
    screenshot_opts.screenshot_path = screenshot_path;
    screenshot_opts.screenshot_after_frames = screenshot_after_frames;
    // Note: screenshot_ptr is set AFTER default filename generation (below)

    // Use positional args from now on (don't modify original argv)
    argc = pos_argc;
    // Note: We use pos_argv directly below instead of modifying argv

    // Load config early
    KryonConfig* early_config = config_find_and_load();

    // Check if first positional argument is a valid target (alias or registered)
    if (!explicit_target && argc >= 1) {
        const char* potential_target = pos_argv[0];
        const char* resolved = NULL;

        // Check if it's a valid alias or registered target
        if (target_is_alias(potential_target, &resolved) || target_handler_find(potential_target)) {
            target_platform = potential_target;
            explicit_target = true;
            // Shift args in pos_argv (safe - it's our local array)
            for (int j = 0; j < argc - 1; j++) {
                pos_argv[j] = pos_argv[j + 1];
            }
            argc--;
        }
    }

    // If no explicit target specified, error out
    if (!target_platform) {
        fprintf(stderr, "Error: No target specified\n");
        fprintf(stderr, "\nUsage:\n");
        fprintf(stderr, "  kryon run <target> <file>           # Target before file\n");
        fprintf(stderr, "  kryon run <file> --target=<target>  # Target after file\n");
        fprintf(stderr, "  kryon run --target=<target> <file>  # Target as flag\n");
        fprintf(stderr, "\nValid targets: limbo, dis, emu, sdl3, desktop, raylib, web, android, kotlin\n");
        if (early_config) config_free(early_config);
        return 1;
    }

    // Validate target platform using handler registry
    // First check if it's a valid alias or registered target
    const char* resolved_target = NULL;
    bool is_alias = target_is_alias(target_platform, &resolved_target);

    if (!is_alias && !target_handler_find(target_platform)) {
        fprintf(stderr, "Error: Unknown target '%s'\n", target_platform);
        fprintf(stderr, "\nValid targets (with aliases):\n");
        fprintf(stderr, "  limbo, dis, emu     - TaijiOS Limbo/DIS bytecode VM\n");
        fprintf(stderr, "  sdl3                - SDL3 renderer\n");
        fprintf(stderr, "  raylib              - Raylib renderer\n");
        fprintf(stderr, "  web                 - Web browser\n");
        fprintf(stderr, "  android, kotlin     - Android APK\n");

        if (free_target_platform) free((char*)target_platform);
        return 1;
    }

    // If no file specified, use entry from config
    if (argc < 1) {
        if (!early_config) {
            fprintf(stderr, "Error: No file specified and no kryon.toml found\n");
            if (free_target_platform) free((char*)target_platform);
            return 1;
        }

        if (!config_validate(early_config)) {
            config_free(early_config);
            if (free_target_platform) free((char*)target_platform);
            return 1;
        }

        if (!early_config->build_entry) {
            fprintf(stderr, "Error: No file specified and no build.entry in kryon.toml\n");
            config_free(early_config);
            if (free_target_platform) free((char*)target_platform);
            return 1;
        }

        target_file = str_copy(early_config->build_entry);
        free_target = true;
        config_free(early_config);
        early_config = NULL;  // Mark as freed
    } else {
        target_file = pos_argv[0];

        // Validate config if it exists (reuse early_config)
        if (early_config) {
            if (!config_validate(early_config)) {
                config_free(early_config);
                if (free_target) free((char*)target_file);
                if (free_target_platform) free((char*)target_platform);
                if (free_screenshot_path) free((char*)screenshot_path);
                return 1;
            }
            config_free(early_config);
            early_config = NULL;  // Mark as freed
        }
    }

    // Check if file exists
    if (!file_exists(target_file)) {
        fprintf(stderr, "Error: File not found: %s\n", target_file);
        if (free_target) free((char*)target_file);
        if (free_target_platform) free((char*)target_platform);
        if (free_screenshot_path) free((char*)screenshot_path);
        return 1;
    }

    // Generate default screenshot filename if --screenshot was specified without a path
    if (screenshot_requested && !screenshot_path) {
        // Extract filename from target_file and replace extension with .png
        const char* basename = strrchr(target_file, '/');
        basename = basename ? basename + 1 : target_file;

        // Copy basename and remove extension
        char default_path[512];
        const char* dot = strrchr(basename, '.');
        if (dot) {
            size_t len = dot - basename;
            snprintf(default_path, sizeof(default_path), "%.*s.png", (int)len, basename);
        } else {
            snprintf(default_path, sizeof(default_path), "%s.png", basename);
        }

        // Allocate and store the path (needs to be freed later)
        screenshot_path = str_copy(default_path);
        free_screenshot_path = true;  // Mark for cleanup
        printf("Screenshot will be saved to: %s\n", screenshot_path);
    }

    // Now set screenshot_ptr (after default filename may have been generated)
    // Update screenshot_opts with the final screenshot_path
    screenshot_opts.screenshot_path = screenshot_path;
    screenshot_opts.screenshot_after_frames = screenshot_after_frames;
    const ScreenshotRunOptions* screenshot_ptr = screenshot_path ? &screenshot_opts : NULL;

    // Detect frontend
    const char* frontend = detect_frontend_type(target_file);
    if (!frontend) {
        const char* ext = path_extension(target_file);
        fprintf(stderr, "Error: Unsupported file type: %s\n", ext);
        fprintf(stderr, "Supported file types: .kry, .kir, .md, .html, .c\n");
        if (free_target) free((char*)target_file);
        if (free_target_platform) free((char*)target_platform);
        if (free_screenshot_path) free((char*)screenshot_path);
        return 1;
    }

    printf("Running: %s\n", target_file);

    // SDL3 and Raylib targets require the desktop renderer library
    bool needs_renderer_lib = false;
    if (strcmp(frontend, "c") == 0) {
        needs_renderer_lib = true;
    } else if (strcmp(frontend, "kir") == 0) {
        // Only SDL3 and Raylib targets need the renderer library
        if (strcmp(target_platform, "sdl3") == 0 ||
            strcmp(target_platform, "raylib") == 0) {
            needs_renderer_lib = true;
        }
    }

    if (needs_renderer_lib) {
        const char* renderer_lib = getenv("KRYON_DESKTOP_LIB");
        if (!renderer_lib) {
            char* found_lib = paths_find_library("libkryon_desktop.so");
            if (!found_lib) {
                fprintf(stderr, "Error: Renderer library not found (required for SDL3/Raylib targets)\n");
                if (free_target) free((char*)target_file);
                if (free_target_platform) free((char*)target_platform);
                if (free_screenshot_path) free((char*)screenshot_path);
                return 1;
            }
            renderer_lib = found_lib;  // Note: leaked but used immediately
        }
    }

    int result = 0;

    // C files: compile and run directly
    if (strcmp(frontend, "c") == 0) {
        result = run_c_file(target_file);
        if (free_target) free((char*)target_file);
        if (free_target_platform) free((char*)target_platform);
        if (free_screenshot_path) free((char*)screenshot_path);
        return result;
    }

    // KIR files: execute directly
    if (strcmp(frontend, "kir") == 0) {
        result = run_kir_file(target_file, target_platform, screenshot_ptr);
        if (free_target) free((char*)target_file);
        if (free_target_platform) free((char*)target_platform);
        if (free_screenshot_path) free((char*)screenshot_path);
        return result;
    }

    // All other source files: compile to KIR first, then run
    char kir_file[1024];
    const char* basename = strrchr(target_file, '/');
    basename = basename ? basename + 1 : target_file;
    snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s", basename);
    char* dot = strrchr(kir_file, '.');
    if (dot) strcpy(dot, ".kir");

    printf("Compiling %s...\n", target_file);
    int compile_result = compile_source_to_kir(target_file, kir_file);
    if (compile_result != 0) {
        fprintf(stderr, "Error: Failed to compile %s\n", target_file);
        if (free_target) free((char*)target_file);
        if (free_target_platform) free((char*)target_platform);
        if (free_screenshot_path) free((char*)screenshot_path);
        return 1;
    }

    printf("✓ Compiled to KIR: %s\n", kir_file);
    result = run_kir_file(kir_file, target_platform, screenshot_ptr);

    if (free_target) free((char*)target_file);
    if (free_target_platform) free((char*)target_platform);
    if (free_screenshot_path) free((char*)screenshot_path);
    return result;
}

