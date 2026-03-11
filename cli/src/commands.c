/*
 * Kryon CLI - Command Implementations
 * C89 compliant
 *
 * Individual command handlers
 */

#include <lib9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include "cli.h"

/* Helper: Check if file exists */
int cli_file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

/* Helper: Check if directory exists */
int cli_dir_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* Helper: Create directory */
int cli_create_dir(const char *path)
{
    return mkdir(path, 0755);
}

/* Helper: Copy file */
int cli_copy_file(const char *src, const char *dest)
{
    FILE *in, *out;
    char buffer[4096];
    size_t n;

    in = fopen(src, "rb");
    if (in == NULL) {
        return -1;
    }

    out = fopen(dest, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }

    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

/*
 * Command: create <name>
 * Create a new Kryon project
 */
int cmd_create(int argc, char **argv)
{
    const char *project_name;
    char project_path[CLI_MAX_PATH];
    char yaml_path[CLI_MAX_PATH];
    char src_path[CLI_MAX_PATH];
    char main_path[CLI_MAX_PATH];
    char assets_path[CLI_MAX_PATH];
    char images_path[CLI_MAX_PATH];
    char fonts_path[CLI_MAX_PATH];
    char gitignore_path[CLI_MAX_PATH];
    FILE *fp;

    if (argc < 2) {
        cli_print_error("Missing project name");
        printf("Usage: kryon create <name>\n");
        return CLI_INVALID_ARGS;
    }

    project_name = argv[1];

    /* Validate project name (basic check) */
    if (strlen(project_name) == 0 || strlen(project_name) > 50) {
        cli_print_error("Invalid project name (must be 1-50 characters)");
        return CLI_INVALID_ARGS;
    }

    /* Create project directory */
    snprint(project_path, sizeof(project_path), "./%s", project_name);

    if (cli_dir_exists(project_path)) {
        cli_print_error("Directory already exists: %s", project_name);
        return CLI_ERROR;
    }

    cli_print_info("Creating project: %s", project_name);

    /* Create directory structure */
    if (cli_create_dir(project_path) < 0) {
        cli_print_error("Failed to create project directory");
        return CLI_ERROR;
    }

    snprint(src_path, sizeof(src_path), "%s/src", project_path);
    if (cli_create_dir(src_path) < 0) {
        cli_print_error("Failed to create src directory");
        return CLI_ERROR;
    }

    /* Create kryon.yaml */
    snprint(yaml_path, sizeof(yaml_path), "%s/kryon.yaml", project_path);
    fp = fopen(yaml_path, "w");
    if (fp == NULL) {
        cli_print_error("Failed to create kryon.yaml");
        return CLI_ERROR;
    }

    fprintf(fp, "# Kryon Project Configuration\n");
    fprintf(fp, "name: %s\n", project_name);
    fprintf(fp, "version: 1.0.0\n");
    fprintf(fp, "kryon_version: \">=0.1.0\"\n");
    fprintf(fp, "entry_point: src/main.kry\n");
    fprintf(fp, "targets:\n");
    fprintf(fp, "  - linux\n");
    fprintf(fp, "assets:\n");
    fprintf(fp, "  - assets/images/*\n");
    fprintf(fp, "  - assets/fonts/*\n");
    fclose(fp);

    /* Create main.kry */
    snprint(main_path, sizeof(main_path), "%s/src/main.kry", project_path);
    fp = fopen(main_path, "w");
    if (fp == NULL) {
        cli_print_error("Failed to create main.kry");
        return CLI_ERROR;
    }

    fprintf(fp, "# %s - Main Window\n", project_name);
    fprintf(fp, "window main \"%s\" {\n", project_name);
    fprintf(fp, "    width 800\n");
    fprintf(fp, "    height 600\n");
    fprintf(fp, "    color \"#121212\"\n");
    fprintf(fp, "\n");
    fprintf(fp, "    label title {\n");
    fprintf(fp, "        text \"Welcome to Kryon!\"\n");
    fprintf(fp, "        x 320\n");
    fprintf(fp, "        y 250\n");
    fprintf(fp, "        width 160\n");
    fprintf(fp, "        height 30\n");
    fprintf(fp, "        color \"#BB86FC\"\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "    \n");
    fprintf(fp, "    button btn_quit \"Quit\" {\n");
    fprintf(fp, "        x 350\n");
    fprintf(fp, "        y 300\n");
    fprintf(fp, "        width 100\n");
    fprintf(fp, "        height 40\n");
    fprintf(fp, "        color \"#03DAC6\"\n");
    fprintf(fp, "        \n");
    fprintf(fp, "        on click {\n");
    fprintf(fp, "            exit\n");
    fprintf(fp, "        }\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "}\n");
    fclose(fp);

    /* Create assets directory structure */
    snprint(assets_path, sizeof(assets_path), "%s/assets", project_path);
    snprint(images_path, sizeof(images_path), "%s/assets/images", project_path);
    snprint(fonts_path, sizeof(fonts_path), "%s/assets/fonts", project_path);

    cli_create_dir(assets_path);
    cli_create_dir(images_path);
    cli_create_dir(fonts_path);

    /* Create .gitignore */
    snprint(gitignore_path, sizeof(gitignore_path), "%s/.gitignore", project_path);
    fp = fopen(gitignore_path, "w");
    if (fp != NULL) {
        fprintf(fp, "# Build outputs\n");
        fprintf(fp, "build/\n");
        fprintf(fp, "*.o\n");
        fprintf(fp, "*.a\n");
        fprintf(fp, "*.so\n");
        fprintf(fp, "\n");
        fprintf(fp, "# Platform-specific outputs\n");
        fprintf(fp, "*.AppImage\n");
        fprintf(fp, "*.apk\n");
        fprintf(fp, "*.aab\n");
        fprintf(fp, "*.wasm\n");
        fprintf(fp, "*.krb\n");
        fprintf(fp, "\n");
        fprintf(fp, "# Editor files\n");
        fprintf(fp, "*.swp\n");
        fprintf(fp, "*~\n");
        fprintf(fp, ".vscode/\n");
        fprintf(fp, ".idea/\n");
        fprintf(fp, "\n");
        fprintf(fp, "# Logs\n");
        fprintf(fp, "*.log\n");
        fprintf(fp, "\n");
        fprintf(fp, "# Temporary files\n");
        fprintf(fp, "*.tmp\n");
        fprintf(fp, "*~\n");
        fclose(fp);
    }

    cli_print_success("Project created successfully!");
    printf("\n");
    printf("Next steps:\n");
    printf("  cd %s\n", project_name);
    printf("  kryon run\n");
    printf("\n");

    return CLI_OK;
}

/*
 * Command: build [target]
 * Build the project
 */
int cmd_build(int argc, char **argv)
{
    const char *target_name = "linux";  /* Default target */
    const KryonTarget *target;
    KryonBuildOutput output;
    char make_cmd[512];
    int result;

    /* Parse target from argv */
    if (argc >= 2) {
        target_name = argv[1];
    }

    /* Check if we're in a project */
    if (!cli_file_exists("kryon.yaml")) {
        cli_print_error("Not a Kryon project (no kryon.yaml found)");
        cli_print_info("Run 'kryon create <name>' to create a new project");
        return CLI_ERROR;
    }

    /* Find target configuration */
    target = target_find(target_name);
    if (target == NULL) {
        cli_print_error("Unknown target: %s", target_name);
        cli_print_info("Available targets: linux, appimage, wasm, android");
        return CLI_INVALID_ARGS;
    }

    cli_print_info("Building project for target: %s (%s)",
                    target_name, target_get_platform_name(target->platform));

    /* Check if toolchain is available */
    if (!target_is_available(target)) {
        cli_print_error("Required toolchain not available for %s", target_name);
        if (target->needs_emscripten) {
            cli_print_info("Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html");
        }
        if (target->needs_android_ndk) {
            cli_print_info("Set ANDROID_NDK environment variable");
        }
        return CLI_ERROR;
    }

    /* Create output directories */
    if (target_create_output_dirs(target, ".", &output) < 0) {
        cli_print_error("Failed to create output directories");
        return CLI_ERROR;
    }

    /* Build based on target type */
    if (target->platform == KRYON_PLATFORM_LINUX) {
        cli_print_info("Building native Linux binary...");
        /* Use nix-shell for reproducible build */
        snprint(make_cmd, sizeof(make_cmd),
              "cd ../.. && nix-shell --run 'cd kryon && make clean && make all'");
        result = system(make_cmd);
        if (result != 0) {
            cli_print_error("Build failed");
            return CLI_BUILD_FAILED;
        }
    } else if (target->platform == KRYON_PLATFORM_APPIMAGE) {
        cli_print_info("Building AppImage package...");
        /* First build Linux binary, then bundle */
        snprint(make_cmd, sizeof(make_cmd),
              "cd ../.. && nix-shell --run 'cd kryon && make clean && make all'");
        result = system(make_cmd);
        if (result == 0) {
            /* TODO: Run AppImage bundler */
            cli_print_info("AppImage bundling coming soon");
        }
    } else if (target->platform == KRYON_PLATFORM_WASM) {
        cli_print_info("Building WebAssembly version...");
        /* Use Emscripten */
        cli_print_info("WebAssembly support - check platform/wasm/ directory");
        cli_print_info("Manual build: cd platform/wasm && ./build.sh");
    } else if (target->platform == KRYON_PLATFORM_ANDROID) {
        cli_print_info("Building Android APK...");
        /* Use Android NDK */
        cli_print_info("Android support - check platform/android/ directory");
        cli_print_info("Manual build: cd platform/android && ./gradlew assembleRelease");
    }

    cli_print_success("Build complete!");
    printf("Output: %s\n", output.binary);
    return CLI_OK;
}

/*
 * Command: run [file.kry]
 * Run a .kry file
 */
int cmd_run(int argc, char **argv)
{
    const char *kry_file = NULL;
    int watch_mode = 0;
    int stay_open = 0;
    int i;
    char cmd[512];

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (cli_streq(argv[i], "--watch")) {
            watch_mode = 1;
        } else if (cli_streq(argv[i], "--stay-open")) {
            stay_open = 1;
        } else if (cli_streq(argv[i], "--help") || cli_streq(argv[i], "-h")) {
            printf("Usage: kryon run [file.kry] [--watch] [--stay-open]\n");
            return CLI_OK;
        } else {
            /* Assume it's a file path */
            kry_file = argv[i];
        }
    }

    /* If no file specified, look for kryon.yaml */
    if (kry_file == NULL) {
        if (cli_file_exists("kryon.yaml")) {
            /* TODO: Parse kryon.yaml and use entry_point */
            kry_file = "src/main.kry";
            if (!cli_file_exists(kry_file)) {
                cli_print_error("Entry point not found: %s", kry_file);
                return CLI_FILE_NOT_FOUND;
            }
        } else {
            cli_print_error("No .kry file specified and no kryon.yaml found");
            printf("Usage: kryon run <file.kry>\n");
            return CLI_INVALID_ARGS;
        }
    }

    /* Check if file exists */
    if (!cli_file_exists(kry_file)) {
        cli_print_error("File not found: %s", kry_file);
        return CLI_FILE_NOT_FOUND;
    }

    /* For now, delegate to existing kryon-wm */
    /* TODO: Replace with direct execution */
    snprint(cmd, sizeof(cmd), "./bin/kryon-wm --run %s", kry_file);
    if (watch_mode) {
        seprint(cmd + strlen(cmd), cmd + sizeof(cmd), " --watch");
    }

    cli_print_info("Running: %s", kry_file);
    if (watch_mode) {
        cli_print_info("Watch mode enabled - edit file to hot-reload");
    }

    printf("  (Delegating to kryon-wm: %s)\n", cmd);
    /* system(cmd); */  /* Uncomment when ready */

    return CLI_OK;
}

/*
 * Command: package [target]
 * Package the project
 */
int cmd_package(int argc, char **argv)
{
    const char *target = "bundle";  /* Default */

    if (argc >= 2) {
        target = argv[1];
    }

    cli_print_info("Packaging for target: %s", target);
    printf("  (Packaging support coming soon)\n");

    return CLI_OK;
}

/*
 * Command: device
 * List connected devices
 */
int cmd_device(int argc, char **argv)
{
    printf("Connected devices:\n");
    printf("  (No devices detected)\n");
    printf("\n");
    printf("Device support coming soon.\n");

    return CLI_OK;
}

/*
 * Command: doctor
 * Diagnose environment
 */
int cmd_doctor(int argc, char **argv)
{
    printf("Kryon Environment Diagnostics\n");
    printf("==============================\n");
    printf("\n");

    /* Check for compiler */
    if (system("which gcc > /dev/null 2>&1") == 0) {
        printf("✓ GCC compiler found\n");
    } else if (system("which tcc > /dev/null 2>&1") == 0) {
        printf("✓ TCC compiler found\n");
    } else {
        printf("✗ No C compiler found (need gcc or tcc)\n");
    }

    /* Check for make */
    if (system("which make > /dev/null 2>&1") == 0) {
        printf("✓ Make found\n");
    } else {
        printf("✗ Make not found\n");
    }

    /* Check for SDL2 */
    if (system("pkg-config --exists sdl2 2>/dev/null") == 0 ||
        system("which sdl2-config > /dev/null 2>&1") == 0) {
        printf("✓ SDL2 found\n");
    } else {
        printf("✗ SDL2 not found (display client will not build)\n");
    }

    /* Check for OpenSSL */
    if (system("pkg-config --exists openssl 2>/dev/null") == 0) {
        printf("✓ OpenSSL found\n");
    } else {
        printf("✗ OpenSSL not found\n");
    }

    /* Check for Nix */
    if (system("which nix-shell > /dev/null 2>&1") == 0) {
        printf("✓ Nix found (recommended)\n");
    } else {
        printf("○ Nix not found (optional but recommended)\n");
    }

    printf("\n");
    printf("Run 'nix-shell' to enter the development environment.\n");

    return CLI_OK;
}

/*
 * Command: stop [--all]
 * Stop running services
 */
int cmd_stop(int argc, char **argv)
{
    int kill_marrow = 0;

    if (argc >= 2 && cli_streq(argv[1], "--all")) {
        kill_marrow = 1;
    }

    /* Stop kryon-wm */
    if (system("pgrep -x kryon-wm > /dev/null 2>&1") == 0) {
        system("pkill -TERM kryon-wm 2>/dev/null");
        cli_print_info("kryon-wm stopped");
    } else {
        cli_print_info("kryon-wm not running");
    }

    /* Stop kryon-view */
    if (system("pgrep -x kryon-view > /dev/null 2>&1") == 0) {
        system("pkill -TERM kryon-view 2>/dev/null");
        cli_print_info("kryon-view stopped");
    } else {
        cli_print_info("kryon-view not running");
    }

    /* Stop Marrow if --all */
    if (kill_marrow) {
        if (system("pgrep -x marrow > /dev/null 2>&1") == 0) {
            system("pkill -TERM marrow 2>/dev/null");
            cli_print_info("Marrow stopped");
        } else {
            cli_print_info("Marrow not running");
        }
    } else {
        cli_print_info("Marrow left running (use: kryon stop --all)");
    }

    return CLI_OK;
}

/*
 * Command: status
 * Show status of services
 */
int cmd_status(int argc, char **argv)
{
    printf("=== Kryon Status ===\n");

    /* Check Marrow */
    if (system("pgrep -x marrow > /dev/null 2>&1") == 0) {
        printf("  Marrow:     running\n");
    } else {
        printf("  Marrow:     stopped\n");
    }

    /* Check kryon-wm */
    if (system("pgrep -x kryon-wm > /dev/null 2>&1") == 0) {
        printf("  kryon-wm:   running\n");
    } else {
        printf("  kryon-wm:   stopped\n");
    }

    /* Check kryon-view */
    if (system("pgrep -x kryon-view > /dev/null 2>&1") == 0) {
        printf("  kryon-view: running\n");
    } else {
        printf("  kryon-view: stopped\n");
    }

    return CLI_OK;
}

/*
 * Command: list
 * List available example files
 */
int cmd_list(int argc, char **argv)
{
    printf("Available .kry files in examples/:\n");
    printf("\n");

    /* Reuse the list_kry_files function from server/main.c */
    /* For now, just delegate to kryon-wm --list-examples */
    if (cli_file_exists("./bin/kryon-wm")) {
        system("./bin/kryon-wm --list-examples");
    } else {
        printf("  (kryon-wm not built - run 'make' first)\n");
    }

    return CLI_OK;
}

/*
 * Command: help [command]
 * Show help
 */
int cmd_help(int argc, char **argv)
{
    const char *command = NULL;

    if (argc >= 2) {
        command = argv[1];
    }

    cli_print_usage(command);
    return CLI_OK;
}
