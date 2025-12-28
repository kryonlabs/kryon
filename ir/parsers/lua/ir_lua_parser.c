/**
 * Lua Parser Implementation
 * Executes Lua code using Kryon Lua bindings to generate KIR
 */

#define _POSIX_C_SOURCE 200809L

#include "ir_lua_parser.h"
#include "../../ir_serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

static char cached_luajit_path[512] = "";

/**
 * Find LuaJIT executable in PATH or common locations
 */
static const char* find_luajit(void) {
    if (cached_luajit_path[0] != '\0') {
        return cached_luajit_path;
    }

    // Try luajit in PATH
    FILE* fp = popen("which luajit 2>/dev/null", "r");
    if (fp) {
        if (fgets(cached_luajit_path, sizeof(cached_luajit_path), fp)) {
            char* nl = strchr(cached_luajit_path, '\n');
            if (nl) *nl = '\0';

            pclose(fp);
            if (cached_luajit_path[0] != '\0' && access(cached_luajit_path, X_OK) == 0) {
                return cached_luajit_path;
            }
        }
        pclose(fp);
    }

    // Try common locations
    const char* paths[] = {
        "/usr/bin/luajit",
        "/usr/local/bin/luajit",
        "/opt/homebrew/bin/luajit",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], X_OK) == 0) {
            strncpy(cached_luajit_path, paths[i], sizeof(cached_luajit_path) - 1);
            return cached_luajit_path;
        }
    }

    return NULL;
}

bool ir_lua_check_luajit_available(void) {
    return find_luajit() != NULL;
}

/**
 * Parse Lua source to KIR JSON by executing it with LuaJIT
 */
char* ir_lua_to_kir(const char* source, size_t length) {
    if (!source) return NULL;

    if (length == 0) {
        length = strlen(source);
    }

    // Check for LuaJIT
    const char* luajit = find_luajit();
    if (!luajit) {
        fprintf(stderr, "Error: LuaJIT not found in PATH or common locations\n");
        fprintf(stderr, "Install LuaJIT to run Lua files: apt-get install luajit (or brew install luajit)\n");
        return NULL;
    }

    // Get Kryon root to find bindings
    char* kryon_root = getenv("KRYON_ROOT");
    static char detected_path[1024];

    if (!kryon_root) {
        // Try to find it by checking common locations
        const char* try_paths[] = {
            "/mnt/storage/Projects/kryon",
            "/home/wao/Projects/kryon",
            "/home/wao/lyra/proj/kryon",
            "/usr/local/share/kryon",
            "/opt/kryon",
            NULL
        };

        char test_path[1024];
        for (int i = 0; try_paths[i] && !kryon_root; i++) {
            snprintf(test_path, sizeof(test_path), "%s/bindings/lua/kryon/ffi.lua", try_paths[i]);
            if (access(test_path, F_OK) == 0) {
                strncpy(detected_path, try_paths[i], sizeof(detected_path) - 1);
                detected_path[sizeof(detected_path) - 1] = '\0';
                kryon_root = detected_path;
                break;
            }
        }
    }

    if (!kryon_root) {
        fprintf(stderr, "Error: Could not locate Kryon root directory\n");
        fprintf(stderr, "Set KRYON_ROOT environment variable: export KRYON_ROOT=/path/to/kryon\n");
        fprintf(stderr, "Or install Kryon to a standard location\n");
        return NULL;
    }

    // Create temp directory for execution
    const char* tmp_dir = getenv("TMPDIR");
    if (!tmp_dir) tmp_dir = "/tmp";

    char temp_base[1024];
    snprintf(temp_base, sizeof(temp_base), "%s/kryon_lua_XXXXXX", tmp_dir);

    char* temp_dir = mkdtemp(temp_base);
    if (!temp_dir) {
        perror("mkdtemp");
        return NULL;
    }

    // Write source to temp file
    char src_file[1200];
    snprintf(src_file, sizeof(src_file), "%s/main.lua", temp_dir);

    FILE* f = fopen(src_file, "w");
    if (!f) {
        perror("fopen");
        rmdir(temp_dir);
        return NULL;
    }

    fwrite(source, 1, length, f);
    fclose(f);

    // Create wrapper script that serializes to JSON
    char wrapper_file[1200];
    snprintf(wrapper_file, sizeof(wrapper_file), "%s/wrapper.lua", temp_dir);

    f = fopen(wrapper_file, "w");
    if (!f) {
        perror("fopen wrapper");
        unlink(src_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Write wrapper that loads the main file and serializes the app
    fprintf(f,
        "-- Kryon Lua KIR Serialization Wrapper\n"
        "-- This wrapper loads a Lua app and serializes it to KIR JSON\n"
        "\n"
        "-- Disable running the desktop renderer\n"
        "os.execute('export KRYON_RUN_DIRECT=false')\n"
        "_G.KRYON_RUN_DIRECT = false\n"
        "\n"
        "-- Add Kryon bindings to package path\n"
        "package.path = package.path .. ';%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua'\n",
        kryon_root, kryon_root);

    // Add plugin paths if available
    char* plugin_paths = getenv("KRYON_PLUGIN_PATHS");
    if (plugin_paths && strlen(plugin_paths) > 0) {
        fprintf(f, "\n-- Add plugin paths\n");

        // Parse colon-separated plugin paths
        char* paths_copy = strdup(plugin_paths);
        char* path = strtok(paths_copy, ":");

        while (path) {
            fprintf(f, "package.path = package.path .. ';%s/bindings/lua/?.lua'\n", path);
            fprintf(f, "package.cpath = package.cpath .. ';%s/build/?.so'\n", path);
            path = strtok(NULL, ":");
        }

        free(paths_copy);
    }

    fprintf(f,
        "\n"
        "local ffi = require('ffi')\n"
        "local C = require('kryon.ffi').C\n"
        "\n"
        "-- Load the main Lua file\n"
        "local app = dofile('%s')\n"
        "\n"
        "-- If app is returned, serialize it\n"
        "if app and app.root then\n"
        "  local json = C.ir_serialize_json(app.root)\n"
        "  if json ~= nil then\n"
        "    io.write(ffi.string(json))\n"
        "    C.free(json)\n"
        "  else\n"
        "    io.stderr:write('Error: Failed to serialize IR to JSON\\n')\n"
        "    os.exit(1)\n"
        "  end\n"
        "else\n"
        "  io.stderr:write('Error: Lua file must return an app table with root component\\n')\n"
        "  os.exit(1)\n"
        "end\n",
        src_file);

    fclose(f);

    // Build LD_LIBRARY_PATH with plugin paths
    char ld_library_path[4096];
    snprintf(ld_library_path, sizeof(ld_library_path), "%s/build:%s/bindings/c", kryon_root, kryon_root);

    if (plugin_paths && strlen(plugin_paths) > 0) {
        // Add plugin build directories to LD_LIBRARY_PATH
        char* paths_copy = strdup(plugin_paths);
        char* path = strtok(paths_copy, ":");

        while (path) {
            size_t current_len = strlen(ld_library_path);
            snprintf(ld_library_path + current_len, sizeof(ld_library_path) - current_len,
                     ":%s/build", path);
            path = strtok(NULL, ":");
        }

        free(paths_copy);
    }

    // Execute the wrapper with LuaJIT
    char exec_cmd[4096];
    snprintf(exec_cmd, sizeof(exec_cmd),
             "cd \"%s\" && "
             "export KRYON_RUN_DIRECT=false && "
             "export LD_LIBRARY_PATH=\"%s:$LD_LIBRARY_PATH\" && "
             "\"%s\" \"%s\" 2>&1",
             temp_dir, ld_library_path, luajit, wrapper_file);

    FILE* pipe = popen(exec_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to execute LuaJIT\n");
        unlink(src_file);
        unlink(wrapper_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Capture output (should be JSON)
    char* result = NULL;
    size_t result_size = 0;
    size_t result_capacity = 0;
    char buffer[4096];

    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t len = strlen(buffer);
        if (result_size + len >= result_capacity) {
            result_capacity = result_capacity == 0 ? 8192 : result_capacity * 2;
            char* new_result = realloc(result, result_capacity);
            if (!new_result) {
                free(result);
                pclose(pipe);
                unlink(src_file);
                unlink(wrapper_file);
                rmdir(temp_dir);
                return NULL;
            }
            result = new_result;
        }
        memcpy(result + result_size, buffer, len);
        result_size += len;
    }

    int exec_status = pclose(pipe);

    // Null-terminate result
    if (result) {
        if (result_size >= result_capacity) {
            char* new_result = realloc(result, result_size + 1);
            if (!new_result) {
                free(result);
                unlink(src_file);
                unlink(wrapper_file);
                rmdir(temp_dir);
                return NULL;
            }
            result = new_result;
        }
        result[result_size] = '\0';
    }

    // Check for execution errors
    if (exec_status != 0) {
        fprintf(stderr, "Error: Lua execution failed\n");
        if (result && strlen(result) > 0) {
            fprintf(stderr, "%s", result);
        }
        free(result);
        unlink(src_file);
        unlink(wrapper_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Check if result looks like JSON (starts with { or [)
    if (!result || strlen(result) == 0) {
        fprintf(stderr, "Error: No output from Lua script\n");
        free(result);
        unlink(src_file);
        unlink(wrapper_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Cleanup
    unlink(src_file);
    unlink(wrapper_file);
    rmdir(temp_dir);

    return result;
}

/**
 * Parse Lua file to KIR JSON
 */
char* ir_lua_file_to_kir(const char* filepath) {
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
    char* result = ir_lua_to_kir(source, read_size);
    free(source);

    return result;
}
