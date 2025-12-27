/**
 * Rust Source Parser Implementation
 * Compiles and executes Rust code using Kryon Rust API to generate KIR
 */

#define _POSIX_C_SOURCE 200809L

#include "ir_rust_parser.h"
#include "../../ir_serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

static char cached_compiler_path[512] = "";

/**
 * Find Rust compiler (rustc or cargo) in PATH or common locations
 */
static const char* find_rust_compiler(void) {
    if (cached_compiler_path[0] != '\0') {
        return cached_compiler_path;
    }

    // Try rustc first
    FILE* fp = popen("which rustc 2>/dev/null", "r");
    if (fp) {
        if (fgets(cached_compiler_path, sizeof(cached_compiler_path), fp)) {
            char* nl = strchr(cached_compiler_path, '\n');
            if (nl) *nl = '\0';

            pclose(fp);
            if (cached_compiler_path[0] != '\0' && access(cached_compiler_path, X_OK) == 0) {
                return cached_compiler_path;
            }
        }
        pclose(fp);
    }

    // Try common locations
    const char* paths[] = {
        "/usr/bin/rustc",
        "/usr/local/bin/rustc",
        NULL
    };

    // Try $HOME/.cargo/bin/rustc
    const char* home = getenv("HOME");
    char home_rustc[512];
    if (home) {
        snprintf(home_rustc, sizeof(home_rustc), "%s/.cargo/bin/rustc", home);
        if (access(home_rustc, X_OK) == 0) {
            strncpy(cached_compiler_path, home_rustc, sizeof(cached_compiler_path) - 1);
            return cached_compiler_path;
        }
    }

    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], X_OK) == 0) {
            strncpy(cached_compiler_path, paths[i], sizeof(cached_compiler_path) - 1);
            return cached_compiler_path;
        }
    }

    return NULL;
}

bool ir_rust_check_compiler_available(void) {
    return find_rust_compiler() != NULL;
}

/**
 * Parse Rust source to KIR JSON by compiling and executing it
 */
char* ir_rust_to_kir(const char* source, size_t length) {
    if (!source) return NULL;

    if (length == 0) {
        length = strlen(source);
    }

    // Check for compiler
    const char* compiler = find_rust_compiler();
    if (!compiler) {
        fprintf(stderr, "Error: Rust compiler not found\n");
        fprintf(stderr, "Install Rust from: https://rustup.rs\n");
        return NULL;
    }

    // Get Kryon root to find bindings
    const char* kryon_root = getenv("KRYON_ROOT");
    if (!kryon_root) {
        // Try to find it relative to the executable
        char exe_path[1024];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
            char* dir = strrchr(exe_path, '/');
            if (dir) {
                *dir = '\0';
                // Assume exe is in build/ and kryon_root is parent
                kryon_root = exe_path;
            }
        }
    }

    if (!kryon_root) {
        fprintf(stderr, "Error: Could not locate Kryon root directory\n");
        fprintf(stderr, "Set KRYON_ROOT environment variable: export KRYON_ROOT=/path/to/kryon\n");
        return NULL;
    }

    // Create temp directory for compilation
    const char* tmp_dir = getenv("TMPDIR");
    if (!tmp_dir) tmp_dir = "/tmp";

    char temp_base[1024];
    snprintf(temp_base, sizeof(temp_base), "%s/kryon_rust_XXXXXX", tmp_dir);

    char* temp_dir = mkdtemp(temp_base);
    if (!temp_dir) {
        perror("mkdtemp");
        return NULL;
    }

    // Write source to temp file
    char src_file[1200];
    snprintf(src_file, sizeof(src_file), "%s/main.rs", temp_dir);

    FILE* f = fopen(src_file, "w");
    if (!f) {
        perror("fopen");
        rmdir(temp_dir);
        return NULL;
    }

    fwrite(source, 1, length, f);
    fclose(f);

    // Compile the Rust source
    char exe_file[1200];
    char kir_file[1200];
    snprintf(exe_file, sizeof(exe_file), "%s/app", temp_dir);
    snprintf(kir_file, sizeof(kir_file), "%s/output.kir", temp_dir);

    // For Rust, we need to use rustc with extern crate linkage
    // Or use a Cargo.toml approach - for simplicity, we'll use rustc with library path
    char compile_cmd[4096];
    snprintf(compile_cmd, sizeof(compile_cmd),
             "%s \"%s\" -L \"%s/bindings/rust/target/release\" "
             "-L \"%s/build\" --extern kryon=\"%s/bindings/rust/target/release/libkryon.rlib\" "
             "-o \"%s\" 2>&1",
             compiler, src_file, kryon_root, kryon_root, kryon_root, exe_file);

    FILE* pipe = popen(compile_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to run Rust compiler\n");
        unlink(src_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Capture compilation output
    char compile_output[4096] = "";
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        strncat(compile_output, buffer, sizeof(compile_output) - strlen(compile_output) - 1);
    }

    int compile_status = pclose(pipe);
    if (compile_status != 0) {
        fprintf(stderr, "Error: Rust compilation failed\n");
        if (strlen(compile_output) > 0) {
            fprintf(stderr, "%s", compile_output);
        }
        fprintf(stderr, "\nNote: Kryon Rust bindings must be built first:\n");
        fprintf(stderr, "  cd %s/bindings/rust && cargo build --release\n", kryon_root);
        unlink(src_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Execute the compiled program
    char exec_cmd[2048];
    snprintf(exec_cmd, sizeof(exec_cmd),
             "cd \"%s\" && LD_LIBRARY_PATH=\"%s/build:%s/bindings/rust/target/release:$LD_LIBRARY_PATH\" "
             "\"%s\" 2>&1",
             temp_dir, kryon_root, kryon_root, exe_file);

    pipe = popen(exec_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to execute compiled program\n");
        unlink(src_file);
        unlink(exe_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Capture execution output
    char exec_output[4096] = "";
    while (fgets(buffer, sizeof(buffer), pipe)) {
        strncat(exec_output, buffer, sizeof(exec_output) - strlen(exec_output) - 1);
    }

    int exec_status = pclose(pipe);
    if (exec_status != 0) {
        fprintf(stderr, "Error: Program execution failed\n");
        if (strlen(exec_output) > 0) {
            fprintf(stderr, "%s", exec_output);
        }
        unlink(src_file);
        unlink(exe_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Read the generated KIR file
    FILE* kir_f = fopen(kir_file, "r");
    if (!kir_f) {
        fprintf(stderr, "Error: KIR output file not found\n");
        fprintf(stderr, "The Rust program must call finalize(\"%s\") to generate output\n", kir_file);
        unlink(src_file);
        unlink(exe_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Get file size
    fseek(kir_f, 0, SEEK_END);
    long size = ftell(kir_f);
    fseek(kir_f, 0, SEEK_SET);

    // Read content
    char* result = malloc(size + 1);
    if (!result) {
        fclose(kir_f);
        unlink(src_file);
        unlink(exe_file);
        unlink(kir_file);
        rmdir(temp_dir);
        return NULL;
    }

    size_t read_bytes = fread(result, 1, size, kir_f);
    result[read_bytes] = '\0';
    fclose(kir_f);

    // Cleanup
    unlink(src_file);
    unlink(exe_file);
    unlink(kir_file);
    rmdir(temp_dir);

    return result;
}

/**
 * Parse Rust file to KIR JSON
 */
char* ir_rust_file_to_kir(const char* filepath) {
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
    char* result = ir_rust_to_kir(source, read_size);
    free(source);

    return result;
}
