/**
 * Lua Parser Implementation
 * Executes Lua code using Kryon Lua bindings to generate KIR
 */

#define _POSIX_C_SOURCE 200809L

#include "lua_parser.h"
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
 * Find Kryon root directory using a fallback chain
 * Checks environment variable, then standard install locations
 */
static char* find_kryon_root(void) {
    static char cached_path[1024] = "";

    // Return cached result if available
    if (cached_path[0] != '\0') {
        return cached_path;
    }

    // 1. Check KRYON_ROOT environment variable
    const char* env_root = getenv("KRYON_ROOT");
    if (env_root && strlen(env_root) > 0) {
        strncpy(cached_path, env_root, sizeof(cached_path) - 1);
        cached_path[sizeof(cached_path) - 1] = '\0';
        return cached_path;
    }

    // 2. Check XDG user data directory
    const char* home = getenv("HOME");
    if (home) {
        char xdg_path[1024];
        snprintf(xdg_path, sizeof(xdg_path), "%s/.local/share/kryon", home);
        if (access(xdg_path, F_OK) == 0) {
            strncpy(cached_path, xdg_path, sizeof(cached_path) - 1);
            cached_path[sizeof(cached_path) - 1] = '\0';
            return cached_path;
        }
    }

    // 3. Check system-wide location
    if (access("/usr/local/share/kryon", F_OK) == 0) {
        strncpy(cached_path, "/usr/local/share/kryon", sizeof(cached_path) - 1);
        cached_path[sizeof(cached_path) - 1] = '\0';
        return cached_path;
    }

    // 4. Walk up parent directories from current working directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        char check_path[1024];
        strncpy(check_path, cwd, sizeof(check_path) - 1);
        check_path[sizeof(check_path) - 1] = '\0';

        // Walk up to 5 levels
        for (int depth = 0; depth < 5; depth++) {
            // Check for ir/ directory (characteristic of Kryon root)
            char test_path[1024];
            snprintf(test_path, sizeof(test_path), "%s/ir/ir_core.h", check_path);
            if (access(test_path, F_OK) == 0) {
                strncpy(cached_path, check_path, sizeof(cached_path) - 1);
                cached_path[sizeof(cached_path) - 1] = '\0';
                return cached_path;
            }

            // Move to parent
            char* last_slash = strrchr(check_path, '/');
            if (last_slash && last_slash != check_path) {
                *last_slash = '\0';
            } else {
                break;
            }
        }
    }

    return NULL;
}

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
    fprintf(stderr, "[DEBUG] ir_lua_to_kir called\n");
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
    char* kryon_root = find_kryon_root();

    if (!kryon_root) {
        fprintf(stderr, "Error: Could not locate Kryon root directory\n");
        fprintf(stderr, "Set KRYON_ROOT environment variable: export KRYON_ROOT=/path/to/kryon\n");
        fprintf(stderr, "Or run 'make install' in the kryon/cli directory\n");
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

    // Get source file path and directory from environment
    const char* source_file = getenv("KRYON_SOURCE_FILE");
    fprintf(stderr, "[DEBUG] ir_lua_to_kir: KRYON_SOURCE_FILE = %s\n", source_file ? source_file : "(null)");

    // Determine main file path and source directory
    char* main_file_path = NULL;
    char source_dir[4096];

    if (!source_file) {
        // No original file - write source to temp file (backward compatibility)
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

        main_file_path = strdup(src_file);
        snprintf(source_dir, sizeof(source_dir), "%s", temp_dir);
    } else {
        // Use original source file directly (no copying needed!)
        main_file_path = strdup(source_file);

        // Extract source directory from source file path
        snprintf(source_dir, sizeof(source_dir), "%s", source_file);
        char* last_slash = strrchr(source_dir, '/');
        if (last_slash) {
            *last_slash = '\0';  // Null-terminate to get directory path
        }
    }

    fprintf(stderr, "[DEBUG] main_file_path = %s\n", main_file_path);
    fprintf(stderr, "[DEBUG] source_dir = %s\n", source_dir);

    // Create wrapper script that serializes to JSON
    char wrapper_file[1200];
    snprintf(wrapper_file, sizeof(wrapper_file), "%s/wrapper.lua", temp_dir);

    FILE* f = fopen(wrapper_file, "w");
    if (!f) {
        perror("fopen wrapper");
        if (!source_file) {
            unlink(main_file_path);
        }
        free(main_file_path);
        rmdir(temp_dir);
        return NULL;
    }

    // Write wrapper that loads the main file and serializes the app
    fprintf(f,
        "-- Kryon Lua KIR Serialization Wrapper\n"
        "-- This wrapper loads a Lua app and serializes it to KIR JSON\n"
        "\n"
        "-- PROPER FIX: Override global print() to write to stderr\n"
        "-- This ensures ALL print() statements go to stderr, not stdout\n"
        "_G.print = function(...)\n"
        "  local args = {...}\n"
        "  for i, v in ipairs(args) do\n"
        "    io.stderr:write(tostring(v))\n"
        "    if i < #args then io.stderr:write('\\t') end\n"
        "  end\n"
        "  io.stderr:write('\\n')\n"
        "end\n"
        "\n"
        "-- Disable running the desktop renderer\n"
        "os.execute('export KRYON_RUN_DIRECT=false')\n"
        "_G.KRYON_RUN_DIRECT = false\n"
        "\n"
        "-- Add source directory for local module requires\n"
        "package.path = '%s/?.lua;%s/?/init.lua;' .. package.path\n"
        "\n"
        "-- Add Kryon bindings to package path\n"
        "package.path = package.path .. ';%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua'\n",
        source_dir, source_dir, kryon_root, kryon_root);

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
        "-- Create and set global IR context BEFORE loading user code\n"
        "local global_ctx = C.ir_create_context()\n"
        "C.ir_set_context(global_ctx)\n"
        "\n"
        "-- Load the main Lua file (print is overridden to use stderr)\n"
        "local app = dofile('%s')\n"
        "\n"
        "-- Serialize and output JSON to stdout\n"
        "-- (print() goes to stderr, io.write() goes to stdout)\n"
        "if app and app.root then\n"
        "  -- Set window metadata before serializing\n"
        "  if app.window then\n"
        "    C.ir_set_window_metadata(\n"
        "      app.window.width or 0,\n"
        "      app.window.height or 0,\n"
        "      app.window.title\n"
        "    )\n"
        "  end\n"
        "  -- Create source metadata (using heap-allocated strings)\n"
        "  local metadata = ffi.new('IRSourceMetadata')\n"
        "  local source_file = os.getenv('KRYON_SOURCE_FILE') or '%s'\n"
        "  -- Allocate strings on heap so they persist for serialization\n"
        "  local lang_str = ffi.new('char[?]', 4)\n"
        "  ffi.copy(lang_str, 'lua')\n"
        "  local file_str = ffi.new('char[?]', #source_file + 1)\n"
        "  ffi.copy(file_str, source_file)\n"
        "  local ver_str = ffi.new('char[?]', 6)\n"
        "  ffi.copy(ver_str, '0.3.0')\n"
        "  local timestamp = os.date('%%Y-%%m-%%dT%%H:%%M:%%S')\n"
        "  local time_str = ffi.new('char[?]', #timestamp + 1)\n"
        "  ffi.copy(time_str, timestamp)\n"
        "  metadata.source_language = lang_str\n"
        "  metadata.source_file = file_str\n"
        "  metadata.compiler_version = ver_str\n"
        "  metadata.timestamp = time_str\n"
        "\n"
        "  -- Serialize with metadata\n"
        "  local json = C.ir_serialize_json_complete(app.root, nil, nil, metadata, nil)\n"
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
        main_file_path, main_file_path);

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
    // NOTE: We don't use 2>&1 here because we want stderr to go to the terminal
    // (for debug messages) and only stdout (JSON) to be captured
    // We execute from current directory - wrapper loads user code from original location
    char exec_cmd[4096];
    snprintf(exec_cmd, sizeof(exec_cmd),
             "export KRYON_RUN_DIRECT=false && "
             "export LD_LIBRARY_PATH=\"%s:$LD_LIBRARY_PATH\" && "
             "\"%s\" \"%s\"",
             ld_library_path, luajit, wrapper_file);

    FILE* pipe = popen(exec_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to execute LuaJIT\n");
        if (!source_file) {
            unlink(main_file_path);  // Only cleanup temp file if we created it
        }
        free(main_file_path);
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
                if (!source_file) {
                    unlink(main_file_path);
                }
                free(main_file_path);
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
                if (!source_file) {
                    unlink(main_file_path);
                }
                free(main_file_path);
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
        if (!source_file) {
            unlink(main_file_path);
        }
        free(main_file_path);
        unlink(wrapper_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Check if result looks like JSON (starts with { or [)
    if (!result || strlen(result) == 0) {
        fprintf(stderr, "Error: No output from Lua script\n");
        free(result);
        if (!source_file) {
            unlink(main_file_path);
        }
        free(main_file_path);
        unlink(wrapper_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Cleanup
    if (!source_file) {
        unlink(main_file_path);  // Only cleanup temp file if we created it
    }
    free(main_file_path);
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

    // Set KRYON_SOURCE_FILE environment variable for metadata tracking
    // Get absolute path to source file
    char abs_path[4096];
    if (filepath[0] == '/') {
        // Already absolute
        snprintf(abs_path, sizeof(abs_path), "%s", filepath);
    } else {
        // Make it absolute
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            snprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, filepath);
        } else {
            snprintf(abs_path, sizeof(abs_path), "%s", filepath);
        }
    }
    setenv("KRYON_SOURCE_FILE", abs_path, 1);

    // Parse
    char* result = ir_lua_to_kir(source, read_size);
    free(source);

    // Unset environment variable
    unsetenv("KRYON_SOURCE_FILE");

    return result;
}
