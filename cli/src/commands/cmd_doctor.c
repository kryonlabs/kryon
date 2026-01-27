/**
 * Doctor Command
 * Comprehensive system diagnostics for Kryon development
 * Checks all dependencies, libraries, and environment
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>  // For PATH_MAX

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

static bool check_file_executable(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return (st.st_mode & S_IXUSR) != 0;
}

static void print_header(const char* text) {
    printf("\n%s%s═══════════════════════════════════════════════════════════%s\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("%s%s  %s%s\n", COLOR_BOLD, COLOR_CYAN, text, COLOR_RESET);
    printf("%s%s═══════════════════════════════════════════════════════════%s\n\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
}

static void print_check(const char* name, bool passed, const char* details) {
    const char* icon = passed ? "✓" : "✗";
    const char* color = passed ? COLOR_GREEN : COLOR_RED;

    printf("  %s%s%s %s", color, icon, COLOR_RESET, name);

    if (details && strlen(details) > 0) {
        printf("\n    %s%s%s", COLOR_YELLOW, details, COLOR_RESET);
    }
    printf("\n");
}

static void print_warning(const char* message) {
    printf("\n  %s⚠  %s%s\n", COLOR_YELLOW, message, COLOR_RESET);
}

static void print_info(const char* format, ...) {
    va_list args;
    va_start(args, format);

    printf("    %s→%s ", COLOR_CYAN, COLOR_RESET);
    vprintf(format, args);
    printf("\n");

    va_end(args);
}

int cmd_doctor(int argc, char** argv) {
    (void)argc;
    (void)argv;

    bool all_ok = true;

    printf("\n%s%sKryon System Diagnostics%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    // Get dynamic paths
    char* kryon_root = paths_get_kryon_root();
    char* build_path = kryon_root ? path_join(kryon_root, "build") : paths_get_build_path();
    char* home = paths_get_home_dir();

    // Check 1: Kryon Renderer Binary
    print_header("1. Kryon Renderer Binary");

    char renderer_binary[PATH_MAX];
    snprintf(renderer_binary, sizeof(renderer_binary), "%s/kryon", build_path);
    bool renderer_exists = file_exists(renderer_binary);
    bool renderer_executable = renderer_exists && check_file_executable(renderer_binary);

    if (renderer_exists && renderer_executable) {
        print_check("Renderer binary found and executable", true, renderer_binary);
    } else if (renderer_exists && !renderer_executable) {
        print_check("Renderer binary found but not executable", false, renderer_binary);
        print_info("Run: chmod +x %s", renderer_binary);
        all_ok = false;
    } else {
        print_check("Renderer binary not found", false, renderer_binary);
        print_info("Run 'make build' in the Kryon source directory");
        all_ok = false;
    }

    // Check 2: Shared Libraries
    print_header("2. Shared Libraries");

    // Build dynamic library search paths
    char* lib_paths[10];
    int lib_idx = 0;

    if (kryon_root) {
        lib_paths[lib_idx++] = path_join(kryon_root, "build");
    }
    if (home) {
        lib_paths[lib_idx++] = path_join(home, ".local/lib");
    }
    lib_paths[lib_idx++] = str_copy("/usr/local/lib");
    lib_paths[lib_idx++] = str_copy("/usr/lib");

    bool found_ir = false, found_desktop = false;

    for (int i = 0; i < lib_idx; i++) {
        char ir_lib[PATH_MAX];
        char desktop_lib[PATH_MAX];

        snprintf(ir_lib, sizeof(ir_lib), "%s/libkryon_ir.so", lib_paths[i]);
        snprintf(desktop_lib, sizeof(desktop_lib), "%s/libkryon_desktop.so", lib_paths[i]);

        if (file_exists(ir_lib) && !found_ir) {
            found_ir = true;
            print_check("libkryon_ir.so", true, ir_lib);
        }
        if (file_exists(desktop_lib) && !found_desktop) {
            found_desktop = true;
            print_check("libkryon_desktop.so", true, desktop_lib);
        }

        free(lib_paths[i]);
    }

    if (home) free(home);
    if (kryon_root) free(kryon_root);

    if (!found_ir) {
        print_check("libkryon_ir.so", false, "Not found in expected locations");
        print_info("Run 'make' in the Kryon ir/ directory");
        all_ok = false;
    }

    if (!found_desktop) {
        print_check("libkryon_desktop.so", false, "Not found in expected locations");
        print_info("Run 'make' in the Kryon runtime/desktop/ directory");
        all_ok = false;
    }

    // Check 3: SDL3 Library (via pkg-config)
    print_header("3. SDL3 Library");

    char* sdl3_output = NULL;
    int sdl3_result = process_run("pkg-config --modversion sdl3 2>&1", &sdl3_output);

    if (sdl3_result == 0 && sdl3_output && strlen(sdl3_output) > 0) {
        // Remove newline
        char* newline = strchr(sdl3_output, '\n');
        if (newline) *newline = '\0';

        char details[256];
        snprintf(details, sizeof(details), "Version: %s", sdl3_output);
        print_check("SDL3 library installed", true, details);
    } else {
        print_check("SDL3 library not found", false, NULL);
        print_info("Install SDL3:");
        print_info("  Ubuntu/Debian: sudo apt install libsdl3-dev");
        print_info("  Fedora:        sudo dnf install SDL3-devel");
        print_info("  Arch Linux:    sudo pacman -S sdl3");
        print_info("  macOS:         brew install sdl3");
        all_ok = false;
    }

    if (sdl3_output) free(sdl3_output);

    // Check 4: Display Environment
    print_header("4. Display Environment");

    const char* display = getenv("DISPLAY");
    const char* wayland_display = getenv("WAYLAND_DISPLAY");
    const char* xdg_session_type = getenv("XDG_SESSION_TYPE");

    bool has_display = (display && strlen(display) > 0) ||
                       (wayland_display && strlen(wayland_display) > 0);

    if (has_display) {
        if (display && strlen(display) > 0) {
            char details[256];
            snprintf(details, sizeof(details), "DISPLAY=%s", display);
            print_check("X11 display available", true, details);
        }
        if (wayland_display && strlen(wayland_display) > 0) {
            char details[256];
            snprintf(details, sizeof(details), "WAYLAND_DISPLAY=%s", wayland_display);
            print_check("Wayland display available", true, details);
        }
        if (xdg_session_type) {
            char details[256];
            snprintf(details, sizeof(details), "Session type: %s", xdg_session_type);
            printf("    %s\n", details);
        }
    } else {
        print_check("No display environment detected", false, NULL);
        print_info("Running in headless environment");
        print_info("For GUI apps, use: xvfb-run kryon run <file>");
        print_warning("This is normal for SSH sessions or CI environments");
    }

    return all_ok ? 0 : 1;
}
