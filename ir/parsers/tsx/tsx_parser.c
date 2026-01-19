/**
 * TSX/JSX Parser Implementation
 * Wraps Bun + TypeScript parser for TSX/JSX → KIR conversion
 */

#define _POSIX_C_SOURCE 200809L

#include "tsx_parser.h"
#include "../include/ir_serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

static char cached_parser_path[1024] = "";
static char cached_bun_path[512] = "";

/**
 * Find Bun executable in PATH or common locations
 */
static const char* find_bun(void) {
    if (cached_bun_path[0] != '\0') {
        return cached_bun_path;
    }

    // Try PATH first
    FILE* fp = popen("which bun 2>/dev/null", "r");
    if (fp) {
        if (fgets(cached_bun_path, sizeof(cached_bun_path), fp)) {
            // Remove newline
            char* nl = strchr(cached_bun_path, '\n');
            if (nl) *nl = '\0';

            pclose(fp);
            if (cached_bun_path[0] != '\0' && access(cached_bun_path, X_OK) == 0) {
                return cached_bun_path;
            }
        }
        pclose(fp);
    }

    // Try common locations
    const char* paths[] = {
        "/usr/local/bin/bun",
        "/usr/bin/bun",
        NULL
    };

    // Try $HOME/.bun/bin/bun
    const char* home = getenv("HOME");
    char home_bun[512];
    if (home) {
        snprintf(home_bun, sizeof(home_bun), "%s/.bun/bin/bun", home);
        if (access(home_bun, X_OK) == 0) {
            strncpy(cached_bun_path, home_bun, sizeof(cached_bun_path) - 1);
            return cached_bun_path;
        }
    }

    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], X_OK) == 0) {
            strncpy(cached_bun_path, paths[i], sizeof(cached_bun_path) - 1);
            return cached_bun_path;
        }
    }

    return NULL;
}

bool ir_tsx_check_bun_available(void) {
    return find_bun() != NULL;
}

/**
 * Get path to TSX parser script, creating cache if needed
 */
const char* ir_tsx_get_parser_script(void) {
    if (cached_parser_path[0] != '\0') {
        struct stat st;
        if (stat(cached_parser_path, &st) == 0) {
            return cached_parser_path;
        }
    }

    // Search paths in order of preference
    const char* kryon_root = getenv("KRYON_ROOT");
    const char* home = getenv("HOME");

    char search_paths[10][1024];
    int num_paths = 0;

    // 1. Installed location
    if (home) {
        snprintf(search_paths[num_paths++], 1024,
                "%s/.local/share/kryon/tsx_parser/tsx_to_kir.ts", home);
    }

    // 2. Source tree location
    if (kryon_root) {
        snprintf(search_paths[num_paths++], 1024,
                "%s/ir/parsers/tsx/tsx_to_kir.ts", kryon_root);
    }

    // 3. Relative to current executable
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char* dir = strrchr(exe_path, '/');
        if (dir) {
            *dir = '\0';
            snprintf(search_paths[num_paths++], 1024,
                    "%s/../share/kryon/tsx_parser/tsx_to_kir.ts", exe_path);
            snprintf(search_paths[num_paths++], 1024,
                    "%s/ir/parsers/tsx/tsx_to_kir.ts", exe_path);
        }
    }

    // Check each search path
    for (int i = 0; i < num_paths; i++) {
        struct stat st;
        if (stat(search_paths[i], &st) == 0) {
            strncpy(cached_parser_path, search_paths[i], sizeof(cached_parser_path) - 1);
            return cached_parser_path;
        }
    }

    // Not found - provide helpful error message
    fprintf(stderr, "Error: TSX parser (tsx_to_kir.ts) not found.\n");
    fprintf(stderr, "Searched locations:\n");
    for (int i = 0; i < num_paths; i++) {
        fprintf(stderr, "  - %s\n", search_paths[i]);
    }
    fprintf(stderr, "\nTo fix:\n");
    fprintf(stderr, "  1. Run 'make install' in the kryon/cli directory, or\n");
    fprintf(stderr, "  2. Set KRYON_ROOT environment variable to the kryon source directory\n");

    return NULL;
}

/**
 * Parse TSX source to KIR JSON
 */
char* ir_tsx_to_kir(const char* source, size_t length) {
    if (!source) return NULL;

    // Determine length if not provided
    if (length == 0) {
        length = strlen(source);
    }

    // Check for Bun
    const char* bun = find_bun();
    if (!bun) {
        fprintf(stderr, "Error: Bun not found. Install from: https://bun.sh\n");
        return NULL;
    }

    // Get parser script
    const char* script = ir_tsx_get_parser_script();
    if (!script) {
        fprintf(stderr, "Error: TSX parser script not found\n");
        return NULL;
    }

    // Write source to temp file
    const char* tmp_dir = getenv("TMPDIR");
    if (!tmp_dir) tmp_dir = "/tmp";

    char input_file[1024];
    snprintf(input_file, sizeof(input_file), "%s/kryon_tsx_XXXXXX", tmp_dir);

    int fd = mkstemp(input_file);
    if (fd < 0) {
        perror("mkstemp");
        return NULL;
    }

    write(fd, source, length);
    close(fd);

    // Run parser
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\" 2>&1", bun, script, input_file);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        unlink(input_file);
        perror("popen");
        return NULL;
    }

    // Read output
    char* result = NULL;
    size_t result_size = 0;
    size_t result_capacity = 4096;
    result = malloc(result_capacity);

    if (result) {
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            size_t len = strlen(buffer);
            if (result_size + len >= result_capacity) {
                result_capacity *= 2;
                char* new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(result);
                    result = NULL;
                    break;
                }
                result = new_result;
            }
            memcpy(result + result_size, buffer, len);
            result_size += len;
        }

        if (result) {
            result[result_size] = '\0';
        }
    }

    int status = pclose(pipe);
    unlink(input_file);

    if (status != 0 || !result) {
        fprintf(stderr, "TSX parser failed\n");
        if (result) free(result);
        return NULL;
    }

    return result;
}

/**
 * Parse TSX file to KIR JSON
 */
char* ir_tsx_file_to_kir(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        perror(filepath);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read file
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);

    // Parse
    char* result = ir_tsx_to_kir(source, read_size);
    free(source);

    return result;
}
