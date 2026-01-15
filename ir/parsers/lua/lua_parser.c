/**
 * Lua Parser Implementation
 * Executes Lua code using Kryon Lua bindings to generate KIR
 */

#define _POSIX_C_SOURCE 200809L

#include "lua_parser.h"
#include "module_collector.h"
#include "require_tracker.h"
#include "../../ir_serialization.h"
#include "../../ir_constants.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

static char cached_luajit_path[IR_PATH_BUFFER_SIZE] = "";

/**
 * Find Kryon root directory using a fallback chain
 * Checks environment variable, then standard install locations
 */
static char* find_kryon_root(void) {
    static char cached_path[IR_PATH_BUFFER_SIZE] = "";

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
        "local DSL = require('kryon.dsl')\n"
        "\n"
        "-- Create and set global IR context BEFORE loading user code\n"
        "local global_ctx = C.ir_create_context()\n"
        "C.ir_set_context(global_ctx)\n"
        "\n"
        "-- Set current module for module reference tracking\n"
        "local source_file = os.getenv('KRYON_SOURCE_FILE')\n"
        "if source_file then\n"
        "  -- Extract module_id from source file path\n"
        "  local module_id = source_file:gsub('^.*/', ''):gsub('%%.lua$', '')\n"
        "  -- Check if in a subdirectory\n"
        "  local subdir = source_file:match('([^/]+/[^/]+)%%.lua$')\n"
        "  if subdir then\n"
        "    module_id = subdir:gsub('%%.lua$', '')\n"
        "  end\n"
        "  DSL.setCurrentModule(module_id)\n"
        "end\n"
        "\n"
        "-- Wrap require() to tag imported functions with module origin\n"
        "local old_require = require\n"
        "-- Modules that should not be wrapped (Kryon internal and standard library)\n"
        "local skip_modules = {\n"
        "  ['ffi'] = true,\n"
        "  ['io'] = true,\n"
        "  ['os'] = true,\n"
        "  ['string'] = true,\n"
        "  ['table'] = true,\n"
        "  ['math'] = true,\n"
        "  ['debug'] = true\n"
        "}\n"
        "local function wrapped_require(modname)\n"
        "  local result = old_require(modname)\n"
        "  -- If the module returned a table, wrap its functions with module origin\n"
        "  -- But only wrap user modules, not Kryon internal or standard library modules\n"
        "  local is_kryon = modname:match('^kryon%%.') ~= nil\n"
        "  local is_stdlib = skip_modules[modname] ~= nil\n"
        "  if type(result) == 'table' and not is_kryon and not is_stdlib then\n"
        "    -- Extract module_id from modname (e.g., 'components.habit_panel' -> 'components/habit_panel')\n"
        "    local module_id = modname:gsub('%%.', '/')\n"
        "    -- Wrap each exported function with its name for tracking\n"
        "    for k, v in pairs(result) do\n"
        "      if type(v) == 'function' then\n"
        "        result[k] = DSL.wrapWithModuleOrigin(v, module_id, k)\n"
        "      end\n"
        "    end\n"
        "  end\n"
        "  return result\n"
        "end\n"
        "_G.require = wrapped_require\n"
        "\n"
        "-- Load the main Lua file (print is overridden to use stderr)\n"
        "local ok, app = pcall(dofile, '%s')\n"
        "if not ok then\n"
        "  io.stderr:write('[WRAPPER] dofile error: ' .. tostring(app) .. '\\n')\n"
        "  os.exit(1)\n"
        "end\n"
        "\n"
        "-- Serialize and output JSON to stdout\n"
        "-- (print() goes to stderr, io.write() goes to stdout)\n"
        "if app and app.root then\n"
        "  -- Normal app module with root component\n"
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
        "  -- Allocate strings on heap so they persist for serialization\n"
        "  local lang_str = ffi.new('char[?]', 4)\n"
        "  ffi.copy(lang_str, 'lua')\n"
        "  local ver_str = ffi.new('char[?]', 6)\n"
        "  ffi.copy(ver_str, '0.3.0')\n"
        "  local timestamp = os.date('%%Y-%%m-%%dT%%H:%%M:%%S')\n"
        "  local time_str = ffi.new('char[?]', #timestamp + 1)\n"
        "  ffi.copy(time_str, timestamp)\n"
        "  metadata.source_language = lang_str\n"
        "  metadata.compiler_version = ver_str\n"
        "  metadata.timestamp = time_str\n"
        "  -- Add source file path for runtime re-execution\n"
        "  local source_file = os.getenv('KRYON_SOURCE_FILE')\n"
        "  if source_file then\n"
        "    local source_file_str = ffi.new('char[?]', #source_file + 1)\n"
        "    ffi.copy(source_file_str, source_file)\n"
        "    metadata.source_file = source_file_str\n"
        "  end\n"
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
        "  -- Debug: check if this is a component module\n"
        "  if app and type(app) == 'table' then\n"
        "    -- Component module: exports functions/components but no root\n"
        "  -- Generate KIR with source code and exports metadata\n"
        "  local module_name = os.getenv('KRYON_SOURCE_FILE') or 'module'\n"
        "  local module_id = module_name:gsub('^.*/', ''):gsub('%%.lua$', '')\n"
        "\n"
        "  -- Build exports list\n"
        "  local exports = {}\n"
        "  for k, v in pairs(app) do\n"
        "    table.insert(exports, {name = k, type = type(v)})\n"
        "  end\n"
        "\n"
        "  -- Generate KIR JSON\n"
        "  io.write('{\\n')\n"
        "  io.write('  \"format\": \"kir\",\\n')\n"
        "  io.write('  \"version\": \"3.0\",\\n')\n"
        "  io.write('  \"metadata\": {\\n')\n"
        "  io.write('    \"source_language\": \"lua\",\\n')\n"
        "  io.write('    \"module_id\": \"' .. module_id .. '\",\\n')\n"
        "  io.write('    \"compiler_version\": \"0.3.0\"\\n')\n"
        "  io.write('  },\\n')\n"
        "  io.write('  \"root\": null,\\n')\n"
        "\n"
        "  -- Add exports if any\n"
        "  if #exports > 0 then\n"
        "    io.write('  \"exports\": [\\n')\n"
        "    for i, exp in ipairs(exports) do\n"
        "      io.write('    {\"name\": \"' .. exp.name .. '\", \"type\": \"' .. exp.type .. '\"}')\n"
        "      if i < #exports then io.write(',') end\n"
        "      io.write('\\n')\n"
        "    end\n"
        "    io.write('  ],\\n')\n"
        "  end\n"
        "\n"
        "  io.write('  \"sources\": {\\n')\n"
        "  io.write('    \"' .. module_id .. '\": {\\n')\n"
        "  io.write('      \"language\": \"lua\"\\n')\n"
        "  io.write('    }\\n')\n"
        "  io.write('  }\\n')\n"
        "  io.write('}\\n')\n"
        "  else\n"
        "    io.stderr:write('Error: Lua file must return an app table with root component\\n')\n"
        "    os.exit(1)\n"
        "  end\n"
        "end\n"
        "\n"
        "-- ============================================================================\n"
        "-- Write component_definitions KIR files for captured component trees\n"
        "-- ============================================================================\n"
        "\n"
        "-- After main execution, we have captured component trees in DSL._capturedComponentTrees\n"
        "-- These trees were captured when wrapped module functions returned components\n"
        "-- Write each module's component_definitions to a separate KIR file\n"
        "\n"
        "local output_dir = os.getenv('KRYON_CACHE_DIR') or '.kryon_cache'\n"
        "local DSL = require('kryon.dsl')\n"
        "\n"
        "if DSL._capturedComponentTrees then\n"
        "  for module_id, module_functions in pairs(DSL._capturedComponentTrees) do\n"
        "    -- Skip if no functions captured for this module\n"
        "    local has_any = false\n"
        "    for _, _ in pairs(module_functions) do\n"
        "      has_any = true\n"
        "      break\n"
        "    end\n"
        "    if not has_any then\n"
        "      goto continue\n"
        "    end\n"
        "\n"
        "    -- Create directory if needed\n"
        "    local module_dir = output_dir .. '/' .. module_id:gsub('/[^/]+$', '')\n"
        "    local mkdir_cmd = 'mkdir -p \"' .. module_dir .. '\"'\n"
        "    os.execute(mkdir_cmd)\n"
        "\n"
        "    -- Build component_definitions array\n"
        "    local component_defs = {}\n"
        "    for fn_name, components in pairs(module_functions) do\n"
        "      if #components > 0 then\n"
        "        table.insert(component_defs, {\n"
        "          name = fn_name,\n"
        "          components = components\n"
        "        })\n"
        "      end\n"
        "    end\n"
        "\n"
        "    if #component_defs == 0 then\n"
        "      goto continue\n"
        "    end\n"
        "\n"
        "    -- Write KIR file with component_definitions\n"
        "    local kir_file_path = output_dir .. '/' .. module_id .. '.kir'\n"
        "    local kf = io.open(kir_file_path, 'w')\n"
        "    if kf then\n"
        "      local write_ok, write_err = pcall(function()\n"
        "      kf:write('{\\n')\n"
        "      kf:write('  \"format\": \"kir\",\\n')\n"
        "      kf:write('  \"version\": \"3.0\",\\n')\n"
        "      kf:write('  \"metadata\": {\\n')\n"
        "      kf:write('    \"source_language\": \"lua\",\\n')\n"
        "      kf:write('    \"module_id\": \"' .. module_id .. '\",\\n')\n"
        "      kf:write('    \"compiler_version\": \"0.3.0\"\\n')\n"
        "      kf:write('  },\\n')\n"
        "      kf:write('  \"root\": null,\\n')\n"
        "      kf:write('  \"component_definitions\": [\\n')\n"
        "\n"
        "      for i, def in ipairs(component_defs) do\n"
        "        kf:write('    {\\n')\n"
        "        kf:write('      \"name\": \"' .. def.name .. '\",\\n')\n"
        "        kf:write('      \"props\": []')\n"
        "\n"
        "        -- Serialize component tree as template\n"
        "        if def.components and #def.components > 0 then\n"
        "          -- For now, just take the first component as the template root\n"
        "          local template_root = def.components[1]\n"
        "\n"
        "          local ok, old_ref = pcall(C.ir_clear_tree_module_refs_json, template_root)\n"
        "          if not ok then\n"
        "            old_ref = nil\n"
        "          end\n"
        "\n"
        "          local ok2, json_str = pcall(C.ir_serialize_json_complete, template_root, nil, nil, nil, nil)\n"
        "          if not ok2 then\n"
        "            json_str = nil\n"
        "          end\n"
        "\n"
        "          -- Restore module_ref after serialization\n"
        "          if old_ref then\n"
        "            pcall(C.ir_restore_tree_module_refs_json, template_root, old_ref)\n"
        "            pcall(C.free, old_ref)\n"
        "          end\n"
        "\n"
        "          if json_str ~= nil then\n"
        "            local json_cdata = ffi.string(json_str)\n"
        "            C.free(json_str)\n"
        "            -- Extract the component from the JSON (skip outer KIR structure)\n"
        "            local comp_start = json_cdata:find('\"root\":')\n"
        "            if comp_start then\n"
        "              -- Find the { after \"root\":\n"
        "              local brace_start = json_cdata:find('{', comp_start)\n"
        "              if brace_start then\n"
        "                -- Find the matching closing brace by counting braces\n"
        "                local depth = 0\n"
        "                local brace_end = nil\n"
        "                for i = brace_start, #json_cdata do\n"
        "                  local ch = json_cdata:sub(i, i)\n"
        "                  if ch == '{' then\n"
        "                    depth = depth + 1\n"
        "                  elseif ch == '}' then\n"
        "                    depth = depth - 1\n"
        "                    if depth == 0 then\n"
        "                      brace_end = i\n"
        "                      break\n"
        "                    end\n"
        "                  end\n"
        "                end\n"
        "                if brace_end then\n"
        "                  local component_json = json_cdata:sub(brace_start, brace_end)\n"
        "                  kf:write(',\\n      \"template\": ')\n"
        "                  kf:write(component_json)\n"
        "                end\n"
        "              end\n"
        "            end\n"
        "          end\n"
        "        end\n"
        "\n"
        "        kf:write('\\n    }')\n"
        "        if i < #component_defs then kf:write(',') end\n"
        "        kf:write('\\n')\n"
        "      end\n"
        "\n"
        "      kf:write('  ]\\n')\n"
        "      kf:write('}\\n')\n"
        "      kf:close()\n"
        "      end)\n"
        "\n"
        "      if not write_ok then\n"
        "        if kf then kf:close() end\n"
        "      end\n"
        "    else\n"
        "    end\n"
        "\n"
        "    ::continue::\n"
        "  end\n"
        "end\n",
        main_file_path);

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
        // unlink(wrapper_file);  // DEBUG: Keep wrapper
        // rmdir(temp_dir);  // DEBUG: Keep temp dir
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
                // unlink(wrapper_file);  // DEBUG: Keep wrapper
                // rmdir(temp_dir);  // DEBUG: Keep temp dir
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
                // unlink(wrapper_file);  // DEBUG: Keep wrapper
                // rmdir(temp_dir);  // DEBUG: Keep temp dir
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
        // unlink(wrapper_file);  // DEBUG: Keep wrapper
        // rmdir(temp_dir);  // DEBUG: Keep temp dir
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
        // unlink(wrapper_file);  // DEBUG: Keep wrapper
        // rmdir(temp_dir);  // DEBUG: Keep temp dir
        return NULL;
    }

    // Cleanup
    if (!source_file) {
        unlink(main_file_path);  // Only cleanup temp file if we created it
    }
    free(main_file_path);
    // unlink(wrapper_file);  // DEBUG: Keep wrapper for inspection
    // rmdir(temp_dir);  // DEBUG: Keep temp dir for inspection

    return result;
}

// ============================================================================
// Export Extraction for Multi-File KIR
// ============================================================================

typedef struct ExportInfo {
    char* name;           // Export name (e.g., "buildColorPicker", "Calendar")
    char* type;           // "function", "component", or "value"
    char** parameters;    // Parameter names
    int param_count;
} ExportInfo;

typedef struct ExportList {
    ExportInfo* exports;
    int count;
    int capacity;
} ExportList;

/**
 * Free a single export info
 */
static void export_info_free(ExportInfo* info) {
    if (!info) return;
    free(info->name);
    free(info->type);
    if (info->parameters) {
        for (int i = 0; i < info->param_count; i++) {
            free(info->parameters[i]);
        }
        free(info->parameters);
    }
}

/**
 * Free an export list
 */
void lua_export_list_free(ExportList* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        export_info_free(&list->exports[i]);
    }
    free(list->exports);
    free(list);
}

/**
 * Check if a string is a Lua keyword
 */
static bool is_lua_keyword(const char* str) {
    const char* keywords[] = {
        "local", "function", "return", "if", "then", "else", "elseif",
        "end", "for", "while", "do", "repeat", "until", "break",
        "goto", "not", "and", "or", "true", "false", "nil",
        NULL
    };
    for (int i = 0; keywords[i]; i++) {
        if (strcmp(str, keywords[i]) == 0) return true;
    }
    return false;
}

/**
 * Extract function parameters from a Lua function definition
 * Pattern: function(param1, param2, ...)
 */
static char** extract_function_params(const char* params_str, int* out_count) {
    if (!params_str || !out_count) return NULL;

    char** params = NULL;
    int count = 0;
    int capacity = 8;

    params = calloc(capacity, sizeof(char*));
    if (!params) return NULL;

    char* copy = strdup(params_str);
    if (!copy) {
        free(params);
        return NULL;
    }

    char* token = strtok(copy, ",)");
    while (token && count < capacity) {
        // Trim whitespace
        while (*token == ' ' || *token == '\t') token++;

        // Remove default values (e.g., "param = defaultValue")
        char* eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            // Trim trailing whitespace
            eq--;
            while (eq > token && (*eq == ' ' || *eq == '\t')) {
                *eq = '\0';
                eq--;
            }
        }

        // Skip empty tokens and Lua keywords
        if (*token != '\0' && !is_lua_keyword(token)) {
            params[count++] = strdup(token);
            if (count >= capacity) {
                capacity *= 2;
                char** new_params = realloc(params, capacity * sizeof(char*));
                if (!new_params) {
                    // Cleanup on failure
                    for (int i = 0; i < count; i++) free(params[i]);
                    free(params);
                    free(copy);
                    return NULL;
                }
                params = new_params;
            }
        }
        token = strtok(NULL, ",)");
    }

    free(copy);
    *out_count = count;

    // Shrink to fit
    if (count == 0) {
        free(params);
        return NULL;
    }

    return params;
}

/**
 * Find a local function definition in Lua source
 * Pattern: local function name(params) or local name = function(params)
 */
static const char* find_local_function_def(const char* source, const char* func_name, int* out_param_count) {
    if (!source || !func_name) return NULL;

    char pattern[256];
    snprintf(pattern, sizeof(pattern), "local function %s", func_name);

    const char* def_pos = strstr(source, pattern);
    if (def_pos) {
        // local function name(params) - find params
        const char* paren_start = strchr(def_pos + strlen(pattern), '(');
        if (paren_start) {
            const char* paren_end = strchr(paren_start, ')');
            if (paren_end) {
                // Count parameters
                int count = 0;
                const char* p = paren_start + 1;
                while (p < paren_end) {
                    while (p < paren_end && (*p == ' ' || *p == '\t')) p++;
                    if (p >= paren_end) break;
                    while (p < paren_end && *p != ',' && *p != ')') p++;
                    count++;
                    while (p < paren_end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
                }
                if (out_param_count) *out_param_count = count;
                return paren_start;
            }
        }
    }

    // Try: local name = function(params)
    snprintf(pattern, sizeof(pattern), "local %s = function", func_name);
    def_pos = strstr(source, pattern);
    if (def_pos) {
        const char* paren_start = strchr(def_pos + strlen(pattern), '(');
        if (paren_start) {
            const char* paren_end = strchr(paren_start, ')');
            if (paren_end) {
                int count = 0;
                const char* p = paren_start + 1;
                while (p < paren_end) {
                    while (p < paren_end && (*p == ' ' || *p == '\t')) p++;
                    if (p >= paren_end) break;
                    while (p < paren_end && *p != ',' && *p != ')') p++;
                    count++;
                    while (p < paren_end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
                }
                if (out_param_count) *out_param_count = count;
                return paren_start;
            }
        }
    }

    return NULL;
}

/**
 * Extract exports from Lua source code
 * Looks for patterns like:
 * - return { ExportName = function(params) ... }
 * - return { ExportName = variableName }
 */
ExportList* lua_extract_exports(const char* source) {
    if (!source) return NULL;

    ExportList* list = calloc(1, sizeof(ExportList));
    if (!list) return NULL;

    list->capacity = 8;
    list->exports = calloc(list->capacity, sizeof(ExportInfo));
    if (!list->exports) {
        free(list);
        return NULL;
    }

    // Find the LAST return { statement (module exports are at end of file)
    // Strategy: Find all "return" keywords, then check which one is followed by { after whitespace
    const char* return_pos = NULL;
    const char* search_pos = source;
    const char* last_return_with_brace = NULL;

    // First, find the position of the last "return" keyword in the file
    const char* last_return_keyword = NULL;
    while ((search_pos = strstr(search_pos, "return")) != NULL) {
        last_return_keyword = search_pos;
        search_pos += 6;  // Move past "return" to find next occurrence
    }

    if (!last_return_keyword) {
        // No return statement
        lua_export_list_free(list);
        return NULL;
    }

    // Now search backward from the last return to find one followed by { after whitespace
    search_pos = last_return_keyword;
    while (search_pos >= source) {
        // Check if this is "return" (not part of another word)
        if (strncmp(search_pos, "return", 6) == 0) {
            // Check if it's followed by whitespace and then {
            const char* after_return = search_pos + 6;
            const char* p = after_return;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (*p == '{') {
                last_return_with_brace = search_pos;
                break;  // Found the last return followed by {
            }
        }
        search_pos--;
    }

    if (!last_return_with_brace) {
        // No return { found
        lua_export_list_free(list);
        return NULL;
    }

    return_pos = last_return_with_brace;

    // Find the opening brace after return
    const char* brace_start = strchr(return_pos, '{');
    if (!brace_start) {
        lua_export_list_free(list);
        return NULL;
    }

    // Look for exports in the table: Name = value
    // Track brace depth to only get top-level exports, not nested properties
    const char* p = brace_start + 1;
    int brace_depth = 0;
    int paren_depth = 0;  // Track parentheses for function calls

    while (*p && (brace_depth > 0 || paren_depth > 0 || *p != '}')) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == '}' && brace_depth == 0 && paren_depth == 0) break;

        // Stop at end keyword (marks end of return block)
        if (strncmp(p, "end", 3) == 0) {
            const char* next = p + 3;
            if (*next == ' ' || *next == '\t' || *next == '\n' || *next == '\r' || *next == '}') {
                break;
            }
        }

        // Track brace/paren depth for nested structures
        if (*p == '{') {
            brace_depth++;
            p++;
            continue;
        }
        if (*p == '}') {
            if (brace_depth > 0) brace_depth--;
            p++;
            continue;
        }
        if (*p == '(') {
            paren_depth++;
            p++;
            continue;
        }
        if (*p == ')') {
            if (paren_depth > 0) paren_depth--;
            p++;
            continue;
        }

        // Skip nested content (we only want top-level exports)
        if (brace_depth > 0 || paren_depth > 0) {
            p++;
            continue;
        }

        // Extract export name
        const char* name_start = p;
        while (*p && *p != '=' && *p != ',' && *p != '}' && *p != '\n' && *p != '\r') p++;
        if (*p != '=') {
            // Skip to next entry or end
            while (*p && *p != ',' && *p != '}') p++;
            if (*p == ',') p++;
            continue;
        }

        // Copy and trim the name separately
        char* name_copy = strndup(name_start, p - name_start);
        char* name_ptr = name_copy;
        while (*name_ptr == ' ' || *name_ptr == '\t') name_ptr++;

        char* name_end = name_ptr + strlen(name_ptr) - 1;
        while (name_end > name_ptr && (*name_end == ' ' || *name_end == '\t' || *name_end == '\r' || *name_end == '\n')) {
            *name_end = '\0';
            name_end--;
        }

        // Skip '=' and whitespace
        p++;
        while (*p == ' ' || *p == '\t') p++;

        // Determine the type of export
        char* export_type = NULL;
        int param_count = 0;
        char** params = NULL;

        if (strncmp(p, "function", 8) == 0) {
            // Inline function: return { Name = function(params) ... }
            export_type = strdup("function");

            p += 8;  // Skip "function"
            while (*p == ' ' || *p == '\t') p++;

            // Extract parameters
            const char* params_start = p;
            if (*p == '(') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '(') depth++;
                    else if (*p == ')') depth--;
                    p++;
                }
            } else {
                while (*p && *p != ',' && *p != '}' && *p != '\n') p++;
            }

            char* params_str = strndup(params_start, p - params_start);
            params = extract_function_params(params_str, &param_count);
            free(params_str);
        } else {
            // Variable reference: return { Name = variableName }
            // Extract the variable name
            const char* var_start = p;
            while (*p && *p != ',' && *p != '}' && *p != '\n' && *p != '\r') p++;
            char* var_name = strndup(var_start, p - var_start);

            // Trim whitespace
            char* var_end = var_name + strlen(var_name) - 1;
            while (var_end > var_name && (*var_end == ' ' || *var_end == '\t' || *var_end == '\r' || *var_end == '\n')) {
                *var_end = '\0';
                var_end--;
            }

            // Try to find the original definition to determine type and parameters
            int local_param_count = 0;
            const char* func_def = find_local_function_def(source, var_name, &local_param_count);

            if (func_def) {
                // It's a function reference
                export_type = strdup("function");
                param_count = local_param_count;
                // We don't have parameter names from the reference, but we know the count
            } else {
                // It's a value/constant reference
                export_type = strdup("value");
                param_count = 0;
            }

            free(var_name);
        }

        // Add export
        if (export_type && list->count < list->capacity) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                ExportInfo* new_exports = realloc(list->exports, list->capacity * sizeof(ExportInfo));
                if (!new_exports) {
                    free(export_type);
                    free(name_copy);
                    lua_export_list_free(list);
                    return NULL;
                }
                list->exports = new_exports;
            }

            ExportInfo* exp = &list->exports[list->count++];
            exp->name = strdup(name_ptr);
            exp->type = export_type;
            exp->parameters = params;
            exp->param_count = param_count;
        } else {
            free(export_type);
        }

        free(name_copy);

        // Skip to next entry (handling nested braces and parens)
        int skip_depth = 0;
        int skip_paren = 0;
        while (*p) {
            if (*p == '{') {
                skip_depth++;
                p++;
                continue;
            }
            if (*p == '}') {
                if (skip_depth > 0) {
                    skip_depth--;
                    p++;
                    continue;
                }
                break;
            }
            if (*p == '(') {
                skip_paren++;
                p++;
                continue;
            }
            if (*p == ')') {
                if (skip_paren > 0) {
                    skip_paren--;
                    p++;
                    continue;
                }
                break;
            }
            if (*p == ',' && skip_depth == 0 && skip_paren == 0) {
                p++;
                break;
            }
            p++;
        }
    }

    if (list->count == 0) {
        lua_export_list_free(list);
        return NULL;
    }

    return list;
}

/**
 * Execute a component module and extract component definitions as KIR JSON
 * This is similar to ir_lua_to_kir but designed for library modules that export components
 * instead of returning a root component
 */
static char* ir_lua_component_to_kir_definitions(const char* source, size_t length,
                                                  const char* source_file,
                                                  const char* module_id) {
    if (!source) return NULL;

    if (length == 0) {
        length = strlen(source);
    }

    // Check for LuaJIT
    const char* luajit = find_luajit();
    if (!luajit) {
        fprintf(stderr, "Error: LuaJIT not found\n");
        return NULL;
    }

    // Get Kryon root
    char* kryon_root = find_kryon_root();
    if (!kryon_root) {
        fprintf(stderr, "Error: Could not locate Kryon root\n");
        return NULL;
    }

    // Create temp directory
    const char* tmp_dir = getenv("TMPDIR");
    if (!tmp_dir) tmp_dir = "/tmp";
    char temp_base[1024];
    snprintf(temp_base, sizeof(temp_base), "%s/kryon_component_XXXXXX", tmp_dir);
    char* temp_dir = mkdtemp(temp_base);
    if (!temp_dir) {
        perror("mkdtemp");
        return NULL;
    }

    // Determine source directory
    char source_dir[4096];
    if (source_file) {
        snprintf(source_dir, sizeof(source_dir), "%s", source_file);
        char* last_slash = strrchr(source_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
    } else {
        getcwd(source_dir, sizeof(source_dir));
    }

    // Create wrapper script for component extraction
    char wrapper_file[1200];
    snprintf(wrapper_file, sizeof(wrapper_file), "%s/wrapper.lua", temp_dir);

    FILE* f = fopen(wrapper_file, "w");
    if (!f) {
        perror("fopen wrapper");
        rmdir(temp_dir);
        return NULL;
    }

    // Write wrapper that loads the component module and extracts component definitions
    fprintf(f,
        "-- Kryon Component Definition Extraction Wrapper\n"
        "-- This wrapper loads a component module and extracts component definitions\n"
        "\n"
        "_G.print = function(...)\n"
        "  local args = {...}\n"
        "  for i, v in ipairs(args) do\n"
        "    io.stderr:write(tostring(v))\n"
        "    if i < #args then io.stderr:write('\\t') end\n"
        "  end\n"
        "  io.stderr:write('\\n')\n"
        "end\n"
        "\n"
        "os.execute('export KRYON_RUN_DIRECT=false')\n"
        "_G.KRYON_RUN_DIRECT = false\n"
        "\n"
        "-- Add source directory for local module requires\n"
        "package.path = '%s/?.lua;%s/?/init.lua;' .. package.path\n"
        "\n"
        "-- Add Kryon bindings\n"
        "package.path = package.path .. ';%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua'\n"
        "\n"
        "local ffi = require('ffi')\n"
        "local C = require('kryon.ffi').C\n"
        "\n"
        "-- Create and set global IR context\n"
        "local global_ctx = C.ir_create_context()\n"
        "C.ir_set_context(global_ctx)\n"
        "\n"
        "-- Load the component module\n"
        "local module_exports = dofile('%s')\n"
        "\n"
        "-- Track component definitions\n"
        "local component_defs = {}\n"
        "\n"
        "-- Helper function to check if a value is a component function\n"
        "-- (i.e., it returns a component tree when called)\n"
        "local function is_component_function(fn)\n"
        "  if type(fn) ~= 'function' then return false end\n"
        "  -- Try calling with empty props table to see if it returns a component\n"
        "  local success, result = pcall(fn, {})\n"
        "  return success and type(result) == 'cdata' and result.type ~= nil\n"
        "end\n"
        "\n"
        "-- Iterate through exports and find component definitions\n"
        "for name, export_value in pairs(module_exports) do\n"
        "  if is_component_function(export_value) then\n"
        "    -- Call the component function with empty props to get the template\n"
        "    local success, component_root = pcall(export_value, {})\n"
        "    if success and component_root then\n"
        "      -- Get function info for parameter extraction\n"
        "      local info = debug.getinfo(export_value)\n"
        "      local params = {}\n"
        "      if info.what == 'Lua' and info.linedefined > 0 then\n"
        "        -- Read source to extract parameters\n"
        "        local filepath = info.source:sub(2)\n"
        "        local file = io.open(filepath, 'r')\n"
        "        if file then\n"
        "          local content = file:read('*a')\n"
        "          file:close()\n"
        "          -- Find function definition and extract params\n"
        "          local func_pattern = 'function[ %s]*' .. name .. '[ %s]*%%((.-)%%)'\n"
        "          local param_str = content:match(func_pattern)\n"
        "          if param_str then\n"
        "            for param in param_str:gmatch('[^,]+') do\n"
        "              param = param:match('^%s*(.-)%s*$')\n"
        "              if param ~= '' and param ~= 'props' then\n"
        "                table.insert(params, param)\n"
        "              end\n"
        "            end\n"
        "          end\n"
        "        end\n"
        "      end\n"
        "\n"
        "      -- Store component definition info\n"
        "      table.insert(component_defs, {\n"
        "        name = name,\n"
        "        root = component_root,\n"
        "        params = params\n"
        "      })\n"
        "    end\n"
        "  end\n"
        "end\n"
        "\n"
        "-- Build KIR JSON structure with component_definitions\n"
        "local kir = {\n"
        "  format = 'kir',\n"
        "  version = '3.0',\n"
        "  metadata = {\n"
        "    source_language = 'lua',\n"
        "    module_id = '%s',\n"
        "    compiler_version = '0.3.0'\n"
        "  },\n"
        "  root = nil,  -- Component modules don't have a root\n"
        "  component_definitions = {}\n"
        "}\n"
        "\n"
        "-- Add component definitions to KIR\n"
        "for i, def in ipairs(component_defs) do\n"
        "  local comp_def = {\n"
        "    name = def.name,\n"
        "    props = {}\n"
        "  }\n"
        "\n"
        "  -- Add props from parameters\n"
        "  for j, param in ipairs(def.params) do\n"
        "    table.insert(comp_def.props, {\n"
        "      name = param,\n"
        "      type = 'any'\n"
        "    })\n"
        "  end\n"
        "\n"
        "  -- Serialize component template using C function\n"
        "  if def.root then\n"
        "    local json_str = C.ir_serialize_json_complete(def.root, nil, nil, nil, nil)\n"
        "    if json_str ~= nil then\n"
        "      local json_str_lua = ffi.string(json_str)\n"
        "      C.free(json_str)\n"
        "      -- Parse the JSON to extract the component tree\n"
        "      -- We'll use a simple approach: wrap it in a template field\n"
        "      local template_json = json_str_lua:match('%b{}')\n"
        "      if template_json then\n"
        "        -- This is the component template\n"
        "        comp_def.template = template_json\n"
        "      end\n"
        "    end\n"
        "  end\n"
        "\n"
        "  table.insert(kir.component_definitions, comp_def)\n"
        "end\n"
        "\n"
        "-- Serialize to JSON using cJSON (if available) or simple JSON encoder\n"
        "local function simple_json_encode(val, indent)\n"
        "  indent = indent or 0\n"
        "  local indent_str = string.rep('  ', indent)\n"
        "  local indent_str_2 = string.rep('  ', indent + 1)\n"
        "\n"
        "  local t = type(val)\n"
        "  if t == 'table' then\n"
        "    -- Check if array\n"
        "    local is_array = #val > 0\n"
        "    if is_array then\n"
        "      local result = {'[\\n'}\n"
        "      for i, v in ipairs(val) do\n"
        "        table.insert(result, indent_str_2)\n"
        "        table.insert(result, simple_json_encode(v, indent + 1))\n"
        "        if i < #val then table.insert(result, ',') end\n"
        "        table.insert(result, '\\n')\n"
        "      end\n"
        "      table.insert(result, indent_str)\n"
        "      table.insert(result, ']')\n"
        "      return table.concat(result)\n"
        "    else\n"
        "      local result = {'{\\n'}\n"
        "      local keys = {}\n"
        "      for k, _ in pairs(val) do table.insert(keys, k) end\n"
        "      table.sort(keys)\n"
        "      for i, k in ipairs(keys) do\n"
        "        table.insert(result, indent_str_2)\n"
        "        table.insert(result, string.format('\"%%s\": ', k))\n"
        "        table.insert(result, simple_json_encode(val[k], indent + 1))\n"
        "        if i < #keys then table.insert(result, ',') end\n"
        "        table.insert(result, '\\n')\n"
        "      end\n"
        "      table.insert(result, indent_str)\n"
        "      table.insert(result, '}')\n"
        "      return table.concat(result)\n"
        "    end\n"
        "  elseif t == 'string' then\n"
        "    return string.format('\"%%s\"', val:gsub('\"', '\\\\\"'))\n"
        "  elseif t == 'number' then\n"
        "    return tostring(val)\n"
        "  elseif t == 'boolean' then\n"
        "    return val and 'true' or 'false'\n"
        "  elseif t == 'nil' then\n"
        "    return 'null'\n"
        "  else\n"
        "    return '\"' .. tostring(val) .. '\"'\n"
        "  end\n"
        "end\n"
        "\n"
        "-- For the template field, we need to insert raw JSON\n"
        "-- So we'll do a custom serialization\n"
        "local json_parts = {}\n"
        "table.insert(json_parts, '{\\n')\n"
        "table.insert(json_parts, '  \"format\": \"kir\",\\n')\n"
        "table.insert(json_parts, '  \"version\": \"3.0\",\\n')\n"
        "table.insert(json_parts, '  \"metadata\": {\\n')\n"
        "table.insert(json_parts, '    \"source_language\": \"lua\",\\n')\n"
        "table.insert(json_parts, string.format('    \"module_id\": \"%%s\",\\n', module_id))\n"
        "table.insert(json_parts, '    \"compiler_version\": \"0.3.0\"\\n')\n"
        "table.insert(json_parts, '  },\\n')\n"
        "table.insert(json_parts, '  \"root\": null,\\n')\n"
        "table.insert(json_parts, '  \"component_definitions\": [\\n')\n"
        "\n"
        "for i, def in ipairs(component_defs) do\n"
        "  table.insert(json_parts, '    {\\n')\n"
        "  table.insert(json_parts, string.format('      \"name\": \"%%s\",\\n', def.name))\n"
        "  table.insert(json_parts, '      \"props\": [\\n')\n"
        "  for j, param in ipairs(def.params) do\n"
        "    table.insert(json_parts, '        {\"name\": \"')\n"
        "    table.insert(json_parts, param)\n"
        "    table.insert(json_parts, '\", \"type\": \"any\"}')\n"
        "    if j < #def.params then table.insert(json_parts, ',') end\n"
        "    table.insert(json_parts, '\\n')\n"
        "  end\n"
        "  table.insert(json_parts, '      ]')\n"
        "\n"
        "  -- Serialize and add the template\n"
        "  if def.root then\n"
        "    local json_str = C.ir_serialize_json_complete(def.root, nil, nil, nil, nil)\n"
        "    if json_str ~= nil then\n"
        "      local json_cdata = ffi.string(json_str)\n"
        "      C.free(json_str)\n"
        "      -- Parse to get the root component\n"
        "      local root_obj = json_cdata:match('%b{}')\n"
        "      if root_obj then\n"
        "        table.insert(json_parts, ',\\n      \"template\": ')\n"
        "        table.insert(json_parts, root_obj)\n"
        "      end\n"
        "    end\n"
        "  end\n"
        "\n"
        "  table.insert(json_parts, '\\n    }')\n"
        "  if i < #component_defs then table.insert(json_parts, ',') end\n"
        "  table.insert(json_parts, '\\n')\n"
        "end\n"
        "\n"
        "table.insert(json_parts, '  ]\\n')\n"
        "table.insert(json_parts, '}\\n')\n"
        "\n"
        "io.write(table.concat(json_parts))\n",
        source_dir, source_dir, kryon_root, kryon_root,
        source_file ? source_file : "module.lua",
        module_id);

    fclose(f);

    // Execute the wrapper
    char ld_library_path[4096];
    snprintf(ld_library_path, sizeof(ld_library_path), "%s/build:%s/bindings/c",
             kryon_root, kryon_root);

    char exec_cmd[4096];
    snprintf(exec_cmd, sizeof(exec_cmd),
             "export KRYON_RUN_DIRECT=false && "
             "export LD_LIBRARY_PATH=\"%s:$LD_LIBRARY_PATH\" && "
             "\"%s\" \"%s\"",
             ld_library_path, luajit, wrapper_file);

    FILE* pipe = popen(exec_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to execute LuaJIT for component extraction\n");
        unlink(wrapper_file);
        rmdir(temp_dir);
        return NULL;
    }

    // Capture output
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

    // Null-terminate
    if (result) {
        if (result_size >= result_capacity) {
            char* new_result = realloc(result, result_size + 1);
            if (!new_result) {
                free(result);
                unlink(wrapper_file);
                rmdir(temp_dir);
                return NULL;
            }
            result = new_result;
        }
        result[result_size] = '\0';
    }

    unlink(wrapper_file);
    rmdir(temp_dir);

    if (exec_status != 0) {
        fprintf(stderr, "Warning: Component extraction failed for %s\n", module_id);
        free(result);
        return NULL;
    }

    return result;
}

/**
 * Check if Lua source contains Kryon UI components
 */
bool lua_has_ui_components(const char* source) {
    if (!source) return false;

    const char* ui_patterns[] = {
        "UI.Container",
        "UI.Column",
        "UI.Row",
        "UI.Button",
        "UI.Modal",
        "UI.Text",
        "UI.Input",
        "UI.Checkbox",
        "UI.Slider",
        "UI.Switch",
        "UI.TabGroup",
        "UI.TabBar",
        "UI.TabPanel",
        "UI.Dropdown",
        "UI.Image",
        "UI.Canvas",
        "UI.Scroll",
        "ui.Container",
        "ui.Column",
        "ui.Row",
        "ui.Button",
        "ui.Modal",
        "ui.Text",
        "ui.Input",
        "ui.Checkbox",
        "ui.Slider",
        "ui.Switch",
        "ui.TabGroup",
        "ui.TabBar",
        "ui.TabPanel",
        "ui.Dropdown",
        "ui.Image",
        "ui.Canvas",
        "ui.Scroll",
        NULL
    };

    for (int i = 0; ui_patterns[i]; i++) {
        if (strstr(source, ui_patterns[i])) {
            return true;
        }
    }
    return false;
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

    // Collect all module sources (for multi-file KIR support)
    ModuleCollection* modules = lua_collect_modules(abs_path, NULL);

    // Determine output directory (default: .kryon_cache)
    const char* output_dir = getenv("KRYON_CACHE_DIR");
    if (!output_dir) {
        output_dir = ".kryon_cache";
    }

    // Create output directory (don't clean - main wrapper may have written component files)
    char mkdir_cmd[2048];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
    system(mkdir_cmd);

    // For each module, generate a separate KIR file
    int files_written = 0;
    char* main_result = NULL;

    fprintf(stderr, "📦 Collected %d modules\n", modules->count);

    for (int i = 0; i < modules->count; i++) {
        ModuleSource* module = &modules->modules[i];

        // Skip Kryon internal modules (dsl, ffi, runtime, etc.)
        if (strcmp(module->module_id, "dsl") == 0 ||
            strcmp(module->module_id, "ffi") == 0 ||
            strcmp(module->module_id, "runtime") == 0 ||
            strcmp(module->module_id, "reactive") == 0 ||
            strcmp(module->module_id, "reactive_web") == 0 ||
            strcmp(module->module_id, "runtime_web") == 0 ||
            strcmp(module->module_id, "kryon") == 0) {
            continue;
        }

        // Skip test files and garbage modules
        if (strstr(module->module_id, "test") != NULL ||
            strstr(module->module_id, "Test") != NULL ||
            strstr(module->module_id, "04_") != NULL ||
            strstr(module->module_id, "05_") != NULL ||
            strstr(module->module_id, "generated") != NULL ||
            strstr(module->module_id, "README") != NULL) {
            fprintf(stderr, "⚠ Skipping test/garbage module: %s\n", module->module_id);
            continue;
        }

        // Only write main module and components/* modules
        // Skip any other modules (storage, etc.)
        if (!module->is_main && strncmp(module->module_id, "components/", 11) != 0) {
            fprintf(stderr, "⚠ Skipping non-component module: %s\n", module->module_id);
            continue;
        }

        cJSON* root = NULL;

        if (module->is_main) {
            // For main module: execute to get component tree
            setenv("KRYON_SOURCE_FILE", module->file_path, 1);
            // NOTE: Don't set KRYON_TEMPLATE_MODE for desktop rendering
            // Template mode causes ForEach to store __dynamic__ which can't be expanded without Lua runtime
            // setenv("KRYON_TEMPLATE_MODE", "1", 1);
            char* result = ir_lua_to_kir(module->source, module->source_len);
            unsetenv("KRYON_SOURCE_FILE");
            // unsetenv("KRYON_TEMPLATE_MODE");

            if (!result) {
                fprintf(stderr, "Error: Failed to compile main module - aborting\n");
                if (modules) lua_module_collection_free(modules);
                free(source);
                return NULL;
            }

            root = cJSON_Parse(result);
            free(result);

            // Add sources section with original source for round-trip
            if (root) {
                cJSON* existing_sources = cJSON_GetObjectItem(root, "sources");
                if (existing_sources) {
                    cJSON_DeleteItemFromObject(root, "sources");
                }
                cJSON* sources = cJSON_CreateObject();
                cJSON* module_source = cJSON_CreateObject();
                cJSON_AddStringToObject(module_source, "language", "lua");
                cJSON_AddStringToObject(module_source, "source", module->source);
                cJSON_AddItemToObject(sources, "main", module_source);
                cJSON_AddItemToObject(root, "sources", sources);
            }
        } else {
            // For component modules: check if file was already written by main wrapper
            // with component_definitions
            char kir_file_path[2048];
            snprintf(kir_file_path, sizeof(kir_file_path), "%s/%s.kir", output_dir, module->module_id);

            // Check if file exists and has component_definitions
            bool skip_extraction = false;
            FILE* kf = fopen(kir_file_path, "r");
            if (kf) {
                // Read file content to check for component_definitions
                fseek(kf, 0, SEEK_END);
                long ksize = ftell(kf);
                if (ksize > 100) {  // File has substantial content
                    fseek(kf, 0, SEEK_SET);
                    char* kcontent = malloc(ksize + 1);
                    if (kcontent) {
                        fread(kcontent, 1, ksize, kf);
                        kcontent[ksize] = '\0';
                        // Check if it contains component_definitions
                        if (strstr(kcontent, "component_definitions")) {
                            skip_extraction = true;
                            fprintf(stderr, "  Using existing component_definitions for: %s\n", module->module_id);
                        }
                        free(kcontent);
                    }
                }
                fclose(kf);
            }

            if (skip_extraction) {
                // Read the existing file and use it as-is
                FILE* existing = fopen(kir_file_path, "r");
                if (existing) {
                    fseek(existing, 0, SEEK_END);
                    long esize = ftell(existing);
                    fseek(existing, 0, SEEK_SET);
                    char* econtent = malloc(esize + 1);
                    if (econtent) {
                        fread(econtent, 1, esize, existing);
                        econtent[esize] = '\0';
                        root = cJSON_Parse(econtent);
                        free(econtent);
                    }
                    fclose(existing);
                }

                // Still add sources if missing
                if (root) {
                    cJSON* existing_sources = cJSON_GetObjectItem(root, "sources");
                    if (!existing_sources) {
                        cJSON* sources = cJSON_CreateObject();
                        cJSON* module_source = cJSON_CreateObject();
                        cJSON_AddStringToObject(module_source, "language", "lua");
                        cJSON_AddStringToObject(module_source, "source", module->source);
                        cJSON_AddItemToObject(sources, module->module_id, module_source);
                        cJSON_AddItemToObject(root, "sources", sources);
                    }
                }
            } else {
                // Check if this is a component module (has UI components)
                bool has_ui = lua_has_ui_components(module->source);

                if (has_ui) {
                    fprintf(stderr, "  Extracting component definitions from: %s\n", module->module_id);

                // Execute the component module to extract component definitions
                char* comp_result = ir_lua_component_to_kir_definitions(
                    module->source,
                    module->source_len,
                    module->file_path,
                    module->module_id
                );

                if (comp_result) {
                    // Parse the result and merge with source info
                    cJSON* comp_root = cJSON_Parse(comp_result);
                    free(comp_result);

                    if (comp_root) {
                        root = comp_root;

                        // Add sources section with original source
                        cJSON* existing_sources = cJSON_GetObjectItem(root, "sources");
                        if (!existing_sources) {
                            cJSON* sources = cJSON_CreateObject();
                            cJSON* module_source = cJSON_CreateObject();
                            cJSON_AddStringToObject(module_source, "language", "lua");
                            cJSON_AddStringToObject(module_source, "source", module->source);
                            cJSON_AddItemToObject(sources, module->module_id, module_source);
                            cJSON_AddItemToObject(root, "sources", sources);
                        }

                        // Verify we got component_definitions
                        cJSON* comp_defs = cJSON_GetObjectItem(root, "component_definitions");
                        if (comp_defs) {
                            fprintf(stderr, "    ✓ Extracted %d component definition(s)\n",
                                    cJSON_GetArraySize(comp_defs));
                        }
                    } else {
                        // Failed to parse, fall back to simple structure
                        fprintf(stderr, "    Warning: Failed to parse component definitions, using fallback\n");
                        root = cJSON_CreateObject();
                        cJSON_AddStringToObject(root, "format", "kir");
                        cJSON_AddNullToObject(root, "root");
                    }
                } else {
                    // Execution failed, fall back to simple structure
                    fprintf(stderr, "    Warning: Component extraction failed, using fallback\n");
                    root = cJSON_CreateObject();
                    cJSON_AddStringToObject(root, "format", "kir");
                    cJSON_AddNullToObject(root, "root");
                }
            } else {
                // For non-component library modules: create a simple structure with source
                root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "format", "kir");

                // Create metadata with source info
                cJSON* meta = cJSON_CreateObject();
                cJSON_AddStringToObject(meta, "source_language", "lua");
                cJSON_AddStringToObject(meta, "module_id", module->module_id);
                cJSON_AddStringToObject(meta, "source_file", abs_path);
                cJSON_AddItemToObject(root, "metadata", meta);

                // Create sources section with original source
                cJSON* sources = cJSON_CreateObject();
                cJSON* module_source = cJSON_CreateObject();
                cJSON_AddStringToObject(module_source, "language", "lua");
                cJSON_AddStringToObject(module_source, "source", module->source);
                cJSON_AddItemToObject(sources, module->module_id, module_source);
                cJSON_AddItemToObject(root, "sources", sources);

                // Extract exports from the module source
                ExportList* exports = lua_extract_exports(module->source);

                // Add exports array if we found any
                if (exports && exports->count > 0) {
                    cJSON* exports_array = cJSON_CreateArray();
                    for (int i = 0; i < exports->count; i++) {
                        ExportInfo* exp = &exports->exports[i];
                        cJSON* exp_obj = cJSON_CreateObject();
                        cJSON_AddStringToObject(exp_obj, "name", exp->name);
                        cJSON_AddStringToObject(exp_obj, "type", exp->type);

                        // Add parameters array if present
                        if (exp->parameters && exp->param_count > 0) {
                            cJSON* params = cJSON_CreateArray();
                            for (int j = 0; j < exp->param_count; j++) {
                                cJSON_AddItemToArray(params, cJSON_CreateString(exp->parameters[j]));
                            }
                            cJSON_AddItemToObject(exp_obj, "parameters", params);
                        }
                        cJSON_AddItemToArray(exports_array, exp_obj);
                    }
                    cJSON_AddItemToObject(root, "exports", exports_array);
                    lua_export_list_free(exports);
                }

                // Empty root component (library modules store source + exports)
                cJSON_AddNullToObject(root, "root");
            }
            }  // End of else block for skip_extraction
        }

        if (root) {
            // Add imports array (dependencies of this module)
            RequireGraph* deps = lua_extract_requires(module->source, module->file_path);
            if (deps && deps->count > 0) {
                cJSON* imports_json = cJSON_CreateArray();
                for (int j = 0; j < deps->count; j++) {
                    RequireDependency* dep = &deps->dependencies[j];
                    // Skip stdlib and kryon internal modules
                    if (!dep->is_stdlib) {
                        // Convert module name to module_id format
                        // e.g., "components.calendar" -> "components/calendar"
                        char dep_id[512];
                        strncpy(dep_id, dep->module, sizeof(dep_id) - 1);
                        // Replace dots with slashes
                        for (char* p = dep_id; *p; p++) {
                            if (*p == '.') *p = '/';
                        }
                        cJSON_AddItemToArray(imports_json, cJSON_CreateString(dep_id));
                    }
                }
                cJSON_AddItemToObject(root, "imports", imports_json);
                lua_require_graph_free(deps);
            }

            // For main modules, add metadata with source info (library modules already have it)
            if (module->is_main) {
                cJSON* meta = cJSON_GetObjectItem(root, "metadata");
                if (!meta) {
                    meta = cJSON_CreateObject();
                    cJSON_AddItemToObject(root, "metadata", meta);
                }
                cJSON_AddStringToObject(meta, "module_id", module->module_id);
                cJSON_AddStringToObject(meta, "language", "lua");
                cJSON_AddStringToObject(meta, "source_file", abs_path);
            }

            // Format readable JSON
            char* formatted = cJSON_Print(root);
            cJSON_Delete(root);

            if (formatted) {
                // Keep main module's formatted JSON for return (before freeing)
                if (module->is_main) {
                    main_result = strdup(formatted);
                }

                // Write to separate .kir file
                char kir_path[2048];
                // Force main module to always be named "main.kir"
                const char* output_module_id = module->is_main ? "main" : module->module_id;
                snprintf(kir_path, sizeof(kir_path), "%s/%s.kir", output_dir, output_module_id);

                // Create subdirectories if needed
                char* last_slash = strrchr(kir_path, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    struct stat st2 = {0};
                    if (stat(kir_path, &st2) == -1) {
                        char mkdir_cmd[2048];
                        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", kir_path);
                        system(mkdir_cmd);
                    }
                    *last_slash = '/';
                }

                FILE* f = fopen(kir_path, "w");
                if (f) {
                    fputs(formatted, f);
                    fclose(f);
                    fprintf(stderr, "✓ Generated: %s.kir\n", output_module_id);
                    files_written++;
                }
                free(formatted);
            }
        }
    }

    // Cleanup
    if (modules) {
        lua_module_collection_free(modules);
    }

    // Return the main KIR content - NULL if compilation failed
    if (!main_result) {
        fprintf(stderr, "Error: No main module compiled\n");
        return NULL;
    }
    return main_result;
}
