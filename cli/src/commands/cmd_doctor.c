/**
 * Doctor Command
 * Comprehensive system diagnostics for Kryon development
 * Checks all dependencies, libraries, and environment
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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

static void print_info(const char* message) {
    printf("    %s→%s %s\n", COLOR_CYAN, COLOR_RESET, message);
}

int cmd_doctor(int argc, char** argv) {
    (void)argc;
    (void)argv;

    bool all_ok = true;

    printf("\n%s%sKryon System Diagnostics%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    // Check 1: Kryon Renderer Binary
    print_header("1. Kryon Renderer Binary");

    const char* renderer_binary = "/mnt/storage/Projects/kryon/build/kryon";
    bool renderer_exists = file_exists(renderer_binary);
    bool renderer_executable = renderer_exists && check_file_executable(renderer_binary);

    if (renderer_exists && renderer_executable) {
        print_check("Renderer binary found and executable", true, renderer_binary);
    } else if (renderer_exists && !renderer_executable) {
        print_check("Renderer binary found but not executable", false, renderer_binary);
        print_info("Run: chmod +x /mnt/storage/Projects/kryon/build/kryon");
        all_ok = false;
    } else {
        print_check("Renderer binary not found", false, renderer_binary);
        print_info("Run 'make build' in /mnt/storage/Projects/kryon");
        all_ok = false;
    }

    // Check 2: Shared Libraries
    print_header("2. Shared Libraries");

    const char* libraries[] = {
        "/home/wao/.local/lib/libkryon_ir.so",
        "/mnt/storage/Projects/kryon/build/libkryon_ir.so",
        "/home/wao/.local/lib/libkryon_desktop.so",
        "/mnt/storage/Projects/kryon/build/libkryon_desktop.so",
        NULL
    };

    bool found_ir = false, found_desktop = false;

    for (int i = 0; libraries[i]; i++) {
        if (file_exists(libraries[i])) {
            if (strstr(libraries[i], "ir.so")) found_ir = true;
            if (strstr(libraries[i], "desktop.so")) found_desktop = true;
            print_check(libraries[i], true, "Found");
        }
    }

    if (!found_ir) {
        print_check("libkryon_ir.so", false, "Not found in expected locations");
        print_info("Run 'make' in /mnt/storage/Projects/kryon/ir");
        all_ok = false;
    }

    if (!found_desktop) {
        print_check("libkryon_desktop.so", false, "Not found in expected locations");
        print_info("Run 'make' in /mnt/storage/Projects/kryon/backends/desktop");
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

    // Check 5: Bun Runtime (for TSX compilation)
    print_header("5. Bun Runtime (for TSX/JSX)");

    char* bun_output = NULL;
    int bun_result = process_run("which bun 2>&1", &bun_output);

    if (bun_result == 0 && bun_output && strlen(bun_output) > 0) {
        char* newline = strchr(bun_output, '\n');
        if (newline) *newline = '\0';
        print_check("Bun runtime found", true, bun_output);

        // Get version
        char* bun_version = NULL;
        process_run("bun --version 2>&1", &bun_version);
        if (bun_version) {
            char* ver_newline = strchr(bun_version, '\n');
            if (ver_newline) *ver_newline = '\0';
            char details[256];
            snprintf(details, sizeof(details), "Version: %s", bun_version);
            printf("    %s\n", details);
            free(bun_version);
        }
    } else {
        print_check("Bun runtime not found", false, NULL);
        print_info("Install Bun for TSX/JSX support:");
        print_info("  curl -fsSL https://bun.sh/install | bash");
        print_warning("Required for .tsx and .jsx file compilation");
        all_ok = false;
    }

    if (bun_output) free(bun_output);

    // Check 6: C CLI Installation
    print_header("6. Kryon CLI Installation");

    const char* installed_cli = "/home/wao/.local/bin/kryon";
    if (file_exists(installed_cli) && check_file_executable(installed_cli)) {
        print_check("C CLI installed", true, installed_cli);

        // Check if it's in PATH
        char* which_output = NULL;
        int which_result = process_run("which kryon 2>&1", &which_output);
        if (which_result == 0 && which_output && strlen(which_output) > 0) {
            char* newline = strchr(which_output, '\n');
            if (newline) *newline = '\0';

            if (strcmp(which_output, installed_cli) == 0) {
                print_check("In PATH", true, "kryon command available");
            } else {
                print_check("Different kryon in PATH", false, which_output);
                print_warning("Multiple kryon binaries detected");
            }
        } else {
            print_check("Not in PATH", false, NULL);
            print_info("Add to PATH: export PATH=\"$HOME/.local/bin:$PATH\"");
        }

        if (which_output) free(which_output);
    } else {
        print_check("C CLI not installed", false, installed_cli);
        print_info("Run 'make install' in /mnt/storage/Projects/kryon/cli");
        all_ok = false;
    }

    // Check 7: Library Dependencies (if renderer binary exists)
    if (renderer_exists) {
        print_header("7. Renderer Binary Dependencies");

        char cmd[512];
        snprintf(cmd, sizeof(cmd), "ldd %s 2>&1 | grep -i 'not found' || echo 'All dependencies satisfied'", renderer_binary);

        char* ldd_output = NULL;
        process_run(cmd, &ldd_output);

        if (ldd_output) {
            if (strstr(ldd_output, "not found")) {
                print_check("Missing dependencies", false, NULL);
                printf("\n%s", ldd_output);
                all_ok = false;
            } else {
                print_check("All dependencies satisfied", true, NULL);
            }
            free(ldd_output);
        }
    }

    // Final Summary
    printf("\n%s%s═══════════════════════════════════════════════════════════%s\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    if (all_ok) {
        printf("\n  %s%s✓ System ready for Kryon development%s\n\n",
               COLOR_BOLD, COLOR_GREEN, COLOR_RESET);
    } else {
        printf("\n  %s%s⚠ Issues detected - see above for fixes%s\n\n",
               COLOR_BOLD, COLOR_YELLOW, COLOR_RESET);
        return 1;
    }

    return 0;
}
