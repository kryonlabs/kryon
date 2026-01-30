/**
 * @file emu_command.c
 * @brief Kryon EMU Command - Run Kryon apps in TaijiOS/Inferno emu
 *
 * Pipeline: .kry → .kir → .b (Limbo) → .dis (bytecode) → run in emu
 */

#include "lib9.h"
#include "limbo_generator.h"
#include "kir_format.h"
#include "error.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

// Forward declaration for compile_command
extern int compile_command(int argc, char *argv[]);

/**
 * Get file modification time
 */
static time_t get_file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return st.st_mtime;
}

/**
 * Check if file exists
 */
static bool file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

/**
 * Create directory (including parents)
 */
static bool mkdir_p(const char *path) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}

/**
 * Get cache directory path
 */
static char *get_cache_dir(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char *cache_dir = malloc(1024);
    if (!cache_dir) return NULL;

    snprintf(cache_dir, 1024, "%s/.cache/kryon", home);

    // Create cache directory if it doesn't exist
    if (!mkdir_p(cache_dir)) {
        free(cache_dir);
        return NULL;
    }

    return cache_dir;
}

/**
 * Generate cache file path
 */
static char *get_cache_path(const char *source_file, const char *extension) {
    char *cache_dir = get_cache_dir();
    if (!cache_dir) return NULL;

    // Extract base filename
    char *source_copy = strdup(source_file);
    char *base_name = basename(source_copy);

    // Remove existing extension
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    // Get file stats for cache key
    struct stat st;
    if (stat(source_file, &st) != 0) {
        free(cache_dir);
        free(source_copy);
        return NULL;
    }

    // Create cache path with mtime and size
    char *cache_path = malloc(2048);
    if (!cache_path) {
        free(cache_dir);
        free(source_copy);
        return NULL;
    }

    snprintf(cache_path, 2048, "%s/%s_%ld_%ld%s",
             cache_dir, base_name, (long)st.st_size, (long)st.st_mtime, extension);

    free(cache_dir);
    free(source_copy);

    return cache_path;
}

/**
 * Compile .kry to .kir (using existing compile command)
 */
static char *compile_kry_to_kir(const char *kry_file, bool verbose) {
    if (verbose) {
        fprintf(stderr, "[1/4] Compiling .kry → .kir...\n");
    }

    // Check if cached .kir exists and is up-to-date
    char *kir_path = get_cache_path(kry_file, ".kir");
    if (!kir_path) {
        fprint(2, "Error: Failed to generate cache path\n");
        return NULL;
    }

    time_t kry_mtime = get_file_mtime(kry_file);
    time_t kir_mtime = get_file_mtime(kir_path);

    if (kir_mtime > 0 && kir_mtime >= kry_mtime) {
        if (verbose) {
            fprintf(stderr, "  Using cached: %s\n", kir_path);
        }
        return kir_path;
    }

    // Compile using existing compile command
    char *compile_args[] = {"compile", (char*)kry_file, "-o", kir_path, NULL};
    int result = compile_command(4, compile_args);

    if (result != 0) {
        fprint(2, "Error: Failed to compile .kry to .kir\n");
        free(kir_path);
        return NULL;
    }

    if (verbose) {
        fprintf(stderr, "  Generated: %s\n", kir_path);
    }

    return kir_path;
}

/**
 * Generate .b (Limbo source) from .kir
 */
static char *generate_limbo_from_kir(const char *kir_file, bool verbose, bool keep_source) {
    if (verbose) {
        fprintf(stderr, "[2/4] Generating Limbo source (.b)...\n");
    }

    // Get output path
    char *b_path = get_cache_path(kir_file, ".b");
    if (!b_path) {
        fprint(2, "Error: Failed to generate cache path for .b file\n");
        return NULL;
    }

    // Generate Limbo code
    if (!kryon_generate_limbo_from_kir(kir_file, b_path)) {
        fprint(2, "Error: Failed to generate Limbo source\n");
        free(b_path);
        return NULL;
    }

    if (verbose) {
        fprintf(stderr, "  Generated: %s\n", b_path);
    }

    if (keep_source) {
        fprintf(stderr, "  Kept source for inspection: %s\n", b_path);
    }

    return b_path;
}

/**
 * Compile .b to .dis using Limbo compiler
 */
static char *compile_limbo_to_dis(const char *b_file, bool verbose) {
    if (verbose) {
        fprintf(stderr, "[3/4] Compiling Limbo → bytecode (.dis)...\n");
    }

    // Check if cached .dis exists and is up-to-date
    char *dis_path = get_cache_path(b_file, ".dis");
    if (!dis_path) {
        fprint(2, "Error: Failed to generate cache path for .dis file\n");
        return NULL;
    }

    time_t b_mtime = get_file_mtime(b_file);
    time_t dis_mtime = get_file_mtime(dis_path);

    if (dis_mtime > 0 && dis_mtime >= b_mtime) {
        if (verbose) {
            fprintf(stderr, "  Using cached: %s\n", dis_path);
        }
        return dis_path;
    }

    // Find Limbo compiler
    const char *limbo_path = getenv("LIMBO");
    if (!limbo_path) {
        // Try default TaijiOS location
        const char *taijios = getenv("ROOT");
        if (taijios) {
            char limbo_full[1024];
            snprintf(limbo_full, sizeof(limbo_full), "%s/Linux/amd64/bin/limbo", taijios);
            if (file_exists(limbo_full)) {
                limbo_path = limbo_full;
            }
        }
    }

    if (!limbo_path || !file_exists(limbo_path)) {
        fprint(2, "Error: Limbo compiler not found\n");
        fprint(2, "  Set LIMBO environment variable or ensure TaijiOS is configured\n");
        free(dis_path);
        return NULL;
    }

    // Compile with limbo
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s -o %s %s", limbo_path, dis_path, b_file);

    if (verbose) {
        fprintf(stderr, "  Running: %s\n", cmd);
    }

    int result = system(cmd);
    if (result != 0) {
        fprint(2, "Error: Limbo compilation failed (exit code: %d)\n", result);
        free(dis_path);
        return NULL;
    }

    if (verbose) {
        fprintf(stderr, "  Generated: %s\n", dis_path);
    }

    return dis_path;
}

/**
 * Launch .dis file in emu
 */
static int launch_in_emu(const char *dis_file, bool verbose) {
    if (verbose) {
        fprintf(stderr, "[4/4] Launching in emu...\n");
    }

    // Find emu binary
    const char *emu_path = getenv("EMU");
    if (!emu_path) {
        // Try default TaijiOS location
        const char *taijios = getenv("ROOT");
        if (taijios) {
            char emu_full[1024];
            snprintf(emu_full, sizeof(emu_full), "%s/Linux/amd64/bin/emu", taijios);
            if (file_exists(emu_full)) {
                emu_path = emu_full;
            }
        }
    }

    if (!emu_path || !file_exists(emu_path)) {
        fprint(2, "Error: emu binary not found\n");
        fprint(2, "  Set EMU environment variable or ensure TaijiOS is configured\n");
        return 1;
    }

    // Get ROOT for emu
    const char *root = getenv("ROOT");
    if (!root) {
        fprint(2, "Error: ROOT environment variable not set\n");
        fprint(2, "  Please set ROOT to your TaijiOS installation directory\n");
        return 1;
    }

    // Launch emu
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s -r %s %s", emu_path, root, dis_file);

    if (verbose) {
        fprintf(stderr, "  Running: %s\n", cmd);
        fprintf(stderr, "\n");
    }

    // Execute emu (replace current process)
    execlp(emu_path, "emu", "-r", root, dis_file, NULL);

    // If exec fails
    fprint(2, "Error: Failed to launch emu: %r\n");
    return 1;
}

/**
 * Print usage information
 */
static void print_usage(void) {
    fprintf(stderr, "Usage: kryon emu <file.kry> [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --keep-source     Keep generated .b file for inspection\n");
    fprintf(stderr, "  --debug           Show detailed compilation steps\n");
    fprintf(stderr, "  -h, --help        Show this help message\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Environment Variables:\n");
    fprintf(stderr, "  ROOT              TaijiOS root directory (required)\n");
    fprintf(stderr, "  LIMBO             Path to Limbo compiler (optional)\n");
    fprintf(stderr, "  EMU               Path to emu binary (optional)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  kryon emu app.kry\n");
    fprintf(stderr, "  kryon emu app.kry --keep-source --debug\n");
}

/**
 * EMU command implementation
 */
int emu_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    bool keep_source = false;
    bool verbose = false;

    // Parse arguments
    static struct option long_options[] = {
        {"keep-source", no_argument, 0, 'k'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    optind = 1;  // Reset getopt
    while ((c = getopt_long(argc, argv, "kdh", long_options, NULL)) != -1) {
        switch (c) {
            case 'k':
                keep_source = true;
                break;
            case 'd':
                verbose = true;
                break;
            case 'h':
                print_usage();
                return 0;
            default:
                print_usage();
                return 1;
        }
    }

    // Get input file
    if (optind >= argc) {
        fprint(2, "Error: No input file specified\n");
        print_usage();
        return 1;
    }

    input_file = argv[optind];

    // Check input file exists
    if (!file_exists(input_file)) {
        fprint(2, "Error: File not found: %s\n", input_file);
        return 1;
    }

    // Verify environment
    if (!getenv("ROOT")) {
        fprint(2, "Error: ROOT environment variable not set\n");
        fprint(2, "  Please set ROOT to your TaijiOS installation directory\n");
        fprint(2, "  Example: export ROOT=/home/user/Projects/TaijiOS\n");
        return 1;
    }

    if (verbose) {
        fprintf(stderr, "Kryon EMU Pipeline: %s\n", input_file);
        fprintf(stderr, "==================================================\n");
    }

    // Step 1: Compile .kry → .kir
    char *kir_file = compile_kry_to_kir(input_file, verbose);
    if (!kir_file) {
        return 1;
    }

    // Step 2: Generate .kir → .b
    char *b_file = generate_limbo_from_kir(kir_file, verbose, keep_source);
    free(kir_file);
    if (!b_file) {
        return 1;
    }

    // Step 3: Compile .b → .dis
    char *dis_file = compile_limbo_to_dis(b_file, verbose);
    if (!keep_source) {
        // Remove temporary .b file
        unlink(b_file);
    }
    free(b_file);
    if (!dis_file) {
        return 1;
    }

    // Step 4: Launch in emu
    int result = launch_in_emu(dis_file, verbose);
    free(dis_file);

    return result;
}
