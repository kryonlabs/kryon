/**
 * Lua Parser Implementation
 * Executes Lua code using Kryon Lua bindings to generate KIR
 */

#define _POSIX_C_SOURCE 200809L

#include "lua_parser.h"
#include "module_collector.h"
#include "require_tracker.h"
#include "../include/ir_serialization.h"
#include "../../src/utils/ir_constants.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include "../../../third_party/tomlc99/toml.h"

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
    char cwd[IR_PATH_BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd))) {
        char check_path[IR_PATH_BUFFER_SIZE];
        strncpy(check_path, cwd, sizeof(check_path) - 1);
        check_path[sizeof(check_path) - 1] = '\0';

        // Walk up parent directories looking for Kryon root
        for (int depth = 0; depth < IR_MAX_DIRECTORY_DEPTH; depth++) {
            // Check for ir/ directory (characteristic of Kryon root)
            char test_path[IR_PATH_BUFFER_SIZE];
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
        "-- PHASE 1: Simple source preservation - just read the raw source\n"
        "local source_file_path = '%s'\n"
        "local source_content = nil\n"
        "do\n"
        "  local sf = io.open(source_file_path, 'r')\n"
        "  if sf then\n"
        "    source_content = sf:read('*a')\n"
        "    sf:close()\n"
        "  end\n"
        "end\n"
        "\n"
        "-- Store raw source for later (simplified approach)\n"
        "_G._kryon_source_content = source_content\n"
        "\n"
        "-- PHASE 1: Extract source declarations for codegen preservation\n"
        "local source_declarations = {\n"
        "  requires = {},\n"
        "  functions = {},\n"
        "  state_init = nil,\n"
        "  initialization = nil,\n"
        "  module_init = nil,\n"
        "  module_constants = nil,\n"
        "  conditional_blocks = nil,\n"
        "  non_reactive_state = nil\n"
        "}\n"
        "\n"
        "if source_content then\n"
        "  -- Split into lines for multi-pass processing\n"
        "  local lines = {}\n"
        "  for line in source_content:gmatch('[^\\n]+') do\n"
        "    table.insert(lines, line)\n"
        "  end\n"
        "\n"
        "  -- Extract require statements: local X = require(\"Y\") or require('Y')\n"
        "  for _, line in ipairs(lines) do\n"
        "    local var, mod = line:match('local%%s+([%%w_]+)%%s*=%%s*require%%s*%%(%%s*[\"\\']([^\"\\']+)[\"\\']%%s*%%)')\n"
        "    if var and mod then\n"
        "      table.insert(source_declarations.requires, { variable = var, module = mod })\n"
        "    end\n"
        "  end\n"
        "\n"
        "  -- Find key line numbers\n"
        "  local last_require_line = 0\n"
        "  local last_early_require_line = 0  -- Last require BEFORE first function\n"
        "  local first_function_line = 0\n"
        "  local state_line = 0\n"
        "  local state_end_line = 0\n"
        "  local buildui_line = 0\n"
        "  \n"
        "  -- First pass: find first function line\n"
        "  for i, line in ipairs(lines) do\n"
        "    if first_function_line == 0 and line:match('local%%s+function%%s+[%%w_]+%%s*%%(') then\n"
        "      first_function_line = i\n"
        "      break\n"
        "    end\n"
        "  end\n"
        "  \n"
        "  -- Second pass: find all requires and track early vs late\n"
        "  for i, line in ipairs(lines) do\n"
        "    if line:match('require%%s*%%([\"\\']') then\n"
        "      last_require_line = i\n"
        "      if first_function_line == 0 or i < first_function_line then\n"
        "        last_early_require_line = i\n"
        "      end\n"
        "    end\n"
        "    if state_line == 0 and line:match('local%%s+state%%s*=%%s*Reactive%%.reactive') then\n"
        "      state_line = i\n"
        "    end\n"
        "    if buildui_line == 0 and line:match('local%%s+function%%s+buildUI%%s*%%(') then\n"
        "      buildui_line = i\n"
        "    end\n"
        "  end\n"
        "\n"
        "  -- Find state_end_line (closing paren of Reactive.reactive)\n"
        "  if state_line > 0 then\n"
        "    local depth = 0\n"
        "    for i = state_line, #lines do\n"
        "      local line = lines[i]\n"
        "      for c in line:gmatch('.') do\n"
        "        if c == '(' or c == '{' then depth = depth + 1 end\n"
        "        if c == ')' or c == '}' then depth = depth - 1 end\n"
        "      end\n"
        "      if depth == 0 then\n"
        "        state_end_line = i\n"
        "        break\n"
        "      end\n"
        "    end\n"
        "  end\n"
        "\n"
        "  -- Extract local function definitions with full bodies\n"
        "  local i = 1\n"
        "  while i <= #lines do\n"
        "    local line = lines[i]\n"
        "    local name, params = line:match('local%%s+function%%s+([%%w_]+)%%s*%%(' .. '([^%%)]*)')\n"
        "    if name then\n"
        "      local func_lines = {line}\n"
        "      local depth = 1\n"
        "      local j = i + 1\n"
        "      while j <= #lines and depth > 0 do\n"
        "        local l = lines[j]\n"
        "        table.insert(func_lines, l)\n"
        "        if l:match('%%f[%%w]function%%f[%%W]') or l:match('%%f[%%w]if%%f[%%W].*then') or\n"
        "           l:match('%%f[%%w]for%%f[%%W].*do') or l:match('%%f[%%w]while%%f[%%W].*do') or\n"
        "           l:match('%%f[%%w]repeat%%f[%%W]') then\n"
        "          depth = depth + 1\n"
        "        end\n"
        "        if l:match('%%f[%%w]end%%f[%%W]') or l:match('%%f[%%w]until%%f[%%W]') then\n"
        "          depth = depth - 1\n"
        "        end\n"
        "        j = j + 1\n"
        "      end\n"
        "      local func_source = table.concat(func_lines, '\\n')\n"
        "      table.insert(source_declarations.functions, {\n"
        "        name = name,\n"
        "        params = params,\n"
        "        line = i,\n"
        "        source = func_source\n"
        "      })\n"
        "      i = j\n"
        "    else\n"
        "      i = i + 1\n"
        "    end\n"
        "  end\n"
        "\n"
        "  -- Extract state initialization expression\n"
        "  local state_match = source_content:match('local%%s+state%%s*=%%s*(Reactive%%.reactive%%s*%%b())')\n"
        "  if state_match then\n"
        "    source_declarations.state_init = { expression = 'local state = ' .. state_match }\n"
        "  end\n"
        "\n"
        "  -- =========================================================================\n"
        "  -- MODULE_INIT: All non-require code before first function\n"
        "  -- Includes: math.randomseed(), Storage.init(), simple assignments\n"
        "  -- Captures code that may appear between requires (not just after last one)\n"
        "  -- =========================================================================\n"
        "  local module_init_lines = {}\n"
        "  local init_end = first_function_line > 0 and first_function_line or state_line\n"
        "  if init_end == 0 then init_end = #lines + 1 end\n"
        "  \n"
        "  -- Find the first require line to start from\n"
        "  local first_require_line = 0\n"
        "  for i, line in ipairs(lines) do\n"
        "    if line:match('require%%s*%%([\"\\']') then\n"
        "      first_require_line = i\n"
        "      break\n"
        "    end\n"
        "  end\n"
        "  \n"
        "  -- Capture ALL initialization code between first require and first function\n"
        "  -- EXCLUDING require statements themselves\n"
        "  for i = first_require_line + 1, init_end - 1 do\n"
        "    local line = lines[i]\n"
        "    local trimmed = line:match('^%%s*(.-)%%s*$')\n"
        "    -- Skip empty, comments, and require statements\n"
        "    if trimmed and trimmed ~= '' and not trimmed:match('^%-%-') and not line:match('require%%s*%%([\"\\']') then\n"
        "      table.insert(module_init_lines, line)\n"
        "    end\n"
        "  end\n"
        "  if #module_init_lines > 0 then\n"
        "    source_declarations.module_init = table.concat(module_init_lines, '\\n')\n"
        "  end\n"
        "\n"
        "  -- =========================================================================\n"
        "  -- MODULE_CONSTANTS: Local table/value declarations at module level\n"
        "  -- Captures: local COLORS = {...}, local DEFAULT_COLOR = \"...\"\n"
        "  -- For library modules that have data constants before functions\n"
        "  -- =========================================================================\n"
        "  local module_constants = {}\n"
        "  local const_i = 1\n"
        "  -- Skip to after last require line if there are any\n"
        "  if first_require_line > 0 then\n"
        "    const_i = first_require_line + 1\n"
        "  end\n"
        "  local const_end = first_function_line > 0 and first_function_line - 1 or #lines\n"
        "  while const_i <= const_end do\n"
        "    local line = lines[const_i]\n"
        "    local trimmed = line:match('^%%s*(.-)%%s*$')\n"
        "    -- Skip empty lines, comments, requires, and already-captured function calls like math.randomseed\n"
        "    if trimmed and trimmed ~= '' and not trimmed:match('^%-%-') and not line:match('require%%s*%%([\"\\']') then\n"
        "      -- Match: local NAME = (table or value, not function)\n"
        "      if trimmed:match('^local%%s+[%%w_]+%%s*=') and not trimmed:match('function') then\n"
        "        -- Check if multi-line table (ends with { optionally followed by comment)\n"
        "        if trimmed:match('{%%s*$') or trimmed:match('{%%s*%-%-') then\n"
        "          -- Extract complete table with brace depth tracking\n"
        "          local table_lines = {line}\n"
        "          local depth = 0\n"
        "          for c in line:gmatch('.') do\n"
        "            if c == '{' then depth = depth + 1 end\n"
        "            if c == '}' then depth = depth - 1 end\n"
        "          end\n"
        "          local j = const_i + 1\n"
        "          while j <= #lines and depth > 0 do\n"
        "            local l = lines[j]\n"
        "            table.insert(table_lines, l)\n"
        "            for c in l:gmatch('.') do\n"
        "              if c == '{' then depth = depth + 1 end\n"
        "              if c == '}' then depth = depth - 1 end\n"
        "            end\n"
        "            j = j + 1\n"
        "          end\n"
        "          table.insert(module_constants, table.concat(table_lines, '\\n'))\n"
        "          const_i = j\n"
        "        else\n"
        "          -- Single line constant (like local DEFAULT_COLOR = \"#4a90e2\")\n"
        "          table.insert(module_constants, line)\n"
        "          const_i = const_i + 1\n"
        "        end\n"
        "      else\n"
        "        const_i = const_i + 1\n"
        "      end\n"
        "    else\n"
        "      const_i = const_i + 1\n"
        "    end\n"
        "  end\n"
        "  if #module_constants > 0 then\n"
        "    source_declarations.module_constants = table.concat(module_constants, '\\n\\n')\n"
        "  end\n"
        "\n"
        "  -- =========================================================================\n"
        "  -- CONDITIONAL_BLOCKS: if X then ... end blocks at module level\n"
        "  -- Specifically for: if Storage.get then ... end\n"
        "  -- =========================================================================\n"
        "  local conditional_blocks = {}\n"
        "  local cond_i = last_early_require_line + 1\n"
        "  while cond_i <= #lines do\n"
        "    local line = lines[cond_i]\n"
        "    -- Look for standalone if statements at module level (not inside functions)\n"
        "    -- Pattern matches 'if X then' even with trailing comments\n"
        "    local code_only = line:gsub('%-%-.*$', '')  -- Remove comments\n"
        "    if code_only:match('^if%%s+') and code_only:match('%%s+then%%s*$') then\n"
        "      -- Only capture if this is before state_line or state hasn't been found\n"
        "      if state_line == 0 or cond_i < state_line then\n"
        "        -- Capture the entire if block\n"
        "        local block_lines = {line}\n"
        "        local depth = 1\n"
        "        local j = cond_i + 1\n"
        "        while j <= #lines and depth > 0 do\n"
        "          local l = lines[j]\n"
        "          table.insert(block_lines, l)\n"
        "          -- Track depth (if adds, end subtracts)\n"
        "          if l:match('%%f[%%w]if%%f[%%W].*then') then\n"
        "            depth = depth + 1\n"
        "          end\n"
        "          if l:match('^%%s*end%%s*$') or l:match('^end$') then\n"
        "            depth = depth - 1\n"
        "          end\n"
        "          j = j + 1\n"
        "        end\n"
        "        local block_source = table.concat(block_lines, '\\n')\n"
        "        table.insert(conditional_blocks, block_source)\n"
        "        cond_i = j\n"
        "      else\n"
        "        cond_i = cond_i + 1\n"
        "      end\n"
        "    else\n"
        "      cond_i = cond_i + 1\n"
        "    end\n"
        "  end\n"
        "  if #conditional_blocks > 0 then\n"
        "    source_declarations.conditional_blocks = table.concat(conditional_blocks, '\\n\\n')\n"
        "  end\n"
        "\n"
        "  -- =========================================================================\n"
        "  -- INITIALIZATION: Variable assignments between last function and state\n"
        "  -- Captures: habitsData, currentDateTime, initialYear, initialMonth\n"
        "  -- =========================================================================\n"
        "  local init_code = {}\n"
        "  -- Find the end of the last function that appears BEFORE state_line\n"
        "  local last_func_end_before_state = 0  -- Initialize to 0, not last_require_line\n"
        "  for _, fn in ipairs(source_declarations.functions) do\n"
        "    if state_line > 0 and fn.line < state_line then\n"
        "      -- Find end of this function\n"
        "      local func_end = fn.line\n"
        "      local depth = 1\n"
        "      for j = fn.line + 1, #lines do\n"
        "        local l = lines[j]\n"
        "        if l:match('%%f[%%w]function%%f[%%W]') or l:match('%%f[%%w]if%%f[%%W].*then') or\n"
        "           l:match('%%f[%%w]for%%f[%%W].*do') or l:match('%%f[%%w]while%%f[%%W].*do') then\n"
        "          depth = depth + 1\n"
        "        end\n"
        "        if l:match('%%f[%%w]end%%f[%%W]') then\n"
        "          depth = depth - 1\n"
        "          if depth == 0 then\n"
        "            func_end = j\n"
        "            break\n"
        "          end\n"
        "        end\n"
        "      end\n"
        "      if func_end > last_func_end_before_state then\n"
        "        last_func_end_before_state = func_end\n"
        "      end\n"
        "    end\n"
        "  end\n"
        "  \n"
        "  -- Find where conditional blocks start (to stop init extraction there)\n"
        "  local first_cond_line = state_line > 0 and state_line or #lines + 1\n"
        "  for i = last_func_end_before_state + 1, (state_line > 0 and state_line - 1 or #lines) do\n"
        "    local line = lines[i]\n"
        "    local code_only = line:gsub('%-%-.*$', '')\n"
        "    if code_only:match('^if%%s+') and code_only:match('%%s+then%%s*$') then\n"
        "      first_cond_line = i\n"
        "      break\n"
        "    end\n"
        "  end\n"
        "  \n"
        "  for i = last_func_end_before_state + 1, first_cond_line - 1 do\n"
        "    local line = lines[i]\n"
        "    local trimmed = line:match('^%%s*(.-)%%s*$')\n"
        "    if trimmed and trimmed ~= '' and not trimmed:match('^%-%-') then\n"
        "      if trimmed:match('^local%%s+[%%w_]+%%s*=') and not trimmed:match('function') then\n"
        "        table.insert(init_code, line)\n"
        "      end\n"
        "    end\n"
        "  end\n"
        "  if #init_code > 0 then\n"
        "    source_declarations.initialization = table.concat(init_code, '\\n')\n"
        "  end\n"
        "\n"
        "  -- =========================================================================\n"
        "  -- NON_REACTIVE_STATE: Local declarations after state init, before buildUI\n"
        "  -- Specifically for: local editingState = { name = '' }\n"
        "  -- =========================================================================\n"
        "  local non_reactive_lines = {}\n"
        "  if state_end_line > 0 then\n"
        "    local end_line = buildui_line > 0 and buildui_line or #lines + 1\n"
        "    -- Also stop at the next function definition\n"
        "    for _, fn in ipairs(source_declarations.functions) do\n"
        "      if fn.line > state_end_line and fn.line < end_line then\n"
        "        end_line = fn.line\n"
        "      end\n"
        "    end\n"
        "    \n"
        "    for i = state_end_line + 1, end_line - 1 do\n"
        "      local line = lines[i]\n"
        "      local trimmed = line:match('^%%s*(.-)%%s*$')\n"
        "      if trimmed and trimmed ~= '' and not trimmed:match('^%-%-') then\n"
        "        -- Capture local declarations (tables, simple values, etc.)\n"
        "        if trimmed:match('^local%%s+[%%w_]+%%s*=') and not trimmed:match('function') then\n"
        "          -- Capture multi-line table literals\n"
        "          if trimmed:match('{%%s*$') then\n"
        "            local table_lines = {line}\n"
        "            local depth = 1\n"
        "            local j = i + 1\n"
        "            while j <= #lines and depth > 0 do\n"
        "              local l = lines[j]\n"
        "              table.insert(table_lines, l)\n"
        "              for c in l:gmatch('.') do\n"
        "                if c == '{' then depth = depth + 1 end\n"
        "                if c == '}' then depth = depth - 1 end\n"
        "              end\n"
        "              j = j + 1\n"
        "            end\n"
        "            table.insert(non_reactive_lines, table.concat(table_lines, '\\n'))\n"
        "          else\n"
        "            table.insert(non_reactive_lines, line)\n"
        "          end\n"
        "        end\n"
        "      end\n"
        "    end\n"
        "  end\n"
        "  if #non_reactive_lines > 0 then\n"
        "    source_declarations.non_reactive_state = table.concat(non_reactive_lines, '\\n')\n"
        "  end\n"
        "\n"
        "  -- Extract app export pattern (runtime.createReactiveApp ... return app)\n"
        "  local app_start = source_content:find('local%%s+app%%s*=%%s*runtime%.createReactiveApp')\n"
        "  if app_start then\n"
        "    local app_end = source_content:find('return%%s+app', app_start)\n"
        "    if app_end then\n"
        "      local app_export = source_content:sub(app_start, app_end + 10)\n"
        "      source_declarations.app_export = app_export\n"
        "    end\n"
        "  end\n"
        "end\n"
        "\n"
        "-- Store for JSON output\n"
        "_G._kryon_source_declarations = source_declarations\n"
        "\n"
        "-- Load the main Lua file (print is overridden to use stderr)\n"
        "local ok, app = pcall(dofile, source_file_path)\n"
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
        "  -- Phase 3: Get reactive manifest for self-contained KIR\n"
        "  local reactive_manifest = nil\n"
        "  local ok_reactive, Reactive = pcall(require, 'kryon.reactive')\n"
        "  if ok_reactive and Reactive and Reactive.getManifest then\n"
        "    reactive_manifest = Reactive.getManifest()\n"
        "    if reactive_manifest ~= nil then\n"
        "      io.stderr:write('[Lua Parser] Including reactive manifest in KIR\\n')\n"
        "    end\n"
        "  end\n"
        "\n"
        "  -- Serialize with metadata and manifest\n"
        "  local json = C.ir_serialize_json_complete(app.root, reactive_manifest, nil, metadata, nil)\n"
        "  if json ~= nil then\n"
        "    local json_str = ffi.string(json)\n"
        "    C.free(json)\n"
        "\n"
        "    -- Inject source_declarations into the JSON\n"
        "    local sd = _G._kryon_source_declarations\n"
        "    if sd and (#sd.requires > 0 or #sd.functions > 0 or sd.state_init) then\n"
        "      -- JSON escape helper\n"
        "      local function jsonEscape(s)\n"
        "        if not s then return 'null' end\n"
        "        return '\"' .. s:gsub('\\\\', '\\\\\\\\'):gsub('\"', '\\\\\"'):gsub('\\n', '\\\\n'):gsub('\\r', '\\\\r'):gsub('\\t', '\\\\t') .. '\"'\n"
        "      end\n"
        "\n"
        "      -- Build source_declarations JSON\n"
        "      local sd_parts = {'  \"source_declarations\": {\\n'}\n"
        "\n"
        "      -- Add requires\n"
        "      table.insert(sd_parts, '    \"requires\": [\\n')\n"
        "      for i, req in ipairs(sd.requires) do\n"
        "        table.insert(sd_parts, '      {\"variable\": ' .. jsonEscape(req.variable) .. ', \"module\": ' .. jsonEscape(req.module) .. '}')\n"
        "        if i < #sd.requires then table.insert(sd_parts, ',') end\n"
        "        table.insert(sd_parts, '\\n')\n"
        "      end\n"
        "      table.insert(sd_parts, '    ],\\n')\n"
        "\n"
        "      -- Add functions (with source bodies)\n"
        "      table.insert(sd_parts, '    \"functions\": [\\n')\n"
        "      for i, fn in ipairs(sd.functions) do\n"
        "        local line_str = fn.line and tostring(fn.line) or '0'\n"
        "        local source_str = fn.source and jsonEscape(fn.source) or 'null'\n"
        "        table.insert(sd_parts, '      {\"name\": ' .. jsonEscape(fn.name) .. ', \"params\": ' .. jsonEscape(fn.params) .. ', \"line\": ' .. line_str .. ', \"source\": ' .. source_str .. '}')\n"
        "        if i < #sd.functions then table.insert(sd_parts, ',') end\n"
        "        table.insert(sd_parts, '\\n')\n"
        "      end\n"
        "      table.insert(sd_parts, '    ],\\n')\n"
        "\n"
        "      -- Add state_init\n"
        "      if sd.state_init then\n"
        "        table.insert(sd_parts, '    \"state_init\": {\\n')\n"
        "        table.insert(sd_parts, '      \"expression\": ' .. jsonEscape(sd.state_init.expression) .. '\\n')\n"
        "        table.insert(sd_parts, '    },\\n')\n"
        "      else\n"
        "        table.insert(sd_parts, '    \"state_init\": null,\\n')\n"
        "      end\n"
        "\n"
        "      -- Add initialization (now a string, not array)\n"
        "      if sd.initialization and type(sd.initialization) == 'string' then\n"
        "        table.insert(sd_parts, '    \"initialization\": ' .. jsonEscape(sd.initialization) .. ',\\n')\n"
        "      else\n"
        "        table.insert(sd_parts, '    \"initialization\": null,\\n')\n"
        "      end\n"
        "\n"
        "      -- Add module_init (code between requires and first function)\n"
        "      if sd.module_init and type(sd.module_init) == 'string' then\n"
        "        table.insert(sd_parts, '    \"module_init\": ' .. jsonEscape(sd.module_init) .. ',\\n')\n"
        "      else\n"
        "        table.insert(sd_parts, '    \"module_init\": null,\\n')\n"
        "      end\n"
        "\n"
        "      -- Add module_constants (local table/value declarations at module level)\n"
        "      if sd.module_constants and type(sd.module_constants) == 'string' then\n"
        "        table.insert(sd_parts, '    \"module_constants\": ' .. jsonEscape(sd.module_constants) .. ',\\n')\n"
        "      else\n"
        "        table.insert(sd_parts, '    \"module_constants\": null,\\n')\n"
        "      end\n"
        "\n"
        "      -- Add conditional_blocks (if X then ... end blocks at module level)\n"
        "      if sd.conditional_blocks and type(sd.conditional_blocks) == 'string' then\n"
        "        table.insert(sd_parts, '    \"conditional_blocks\": ' .. jsonEscape(sd.conditional_blocks) .. ',\\n')\n"
        "      else\n"
        "        table.insert(sd_parts, '    \"conditional_blocks\": null,\\n')\n"
        "      end\n"
        "\n"
        "      -- Add non_reactive_state (local declarations after state init)\n"
        "      if sd.non_reactive_state and type(sd.non_reactive_state) == 'string' then\n"
        "        table.insert(sd_parts, '    \"non_reactive_state\": ' .. jsonEscape(sd.non_reactive_state) .. ',\\n')\n"
        "      else\n"
        "        table.insert(sd_parts, '    \"non_reactive_state\": null,\\n')\n"
        "      end\n"
        "\n"
        "      -- Add app_export\n"
        "      if sd.app_export and type(sd.app_export) == 'string' then\n"
        "        table.insert(sd_parts, '    \"app_export\": ' .. jsonEscape(sd.app_export) .. '\\n')\n"
        "      else\n"
        "        table.insert(sd_parts, '    \"app_export\": null\\n')\n"
        "      end\n"
        "\n"
        "      table.insert(sd_parts, '  },\\n')\n"
        "      local sd_json = table.concat(sd_parts)\n"
        "\n"
        "      -- Insert source_declarations after the opening brace\n"
        "      -- Find first newline after { and insert there\n"
        "      local insert_pos = json_str:find('\\n')\n"
        "      if insert_pos then\n"
        "        json_str = json_str:sub(1, insert_pos) .. sd_json .. json_str:sub(insert_pos + 1)\n"
        "      end\n"
        "\n"
        "      io.stderr:write('[Lua Parser] Added source_declarations: ' .. #sd.requires .. ' requires, ' .. #sd.functions .. ' functions\\n')\n"
        "    end\n"
        "\n"
        "    io.write(json_str)\n"
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
        "  -- Extract function source code using debug.getinfo\n"
        "  local function extractFunctionSource(fn)\n"
        "    if type(fn) ~= 'function' then return nil end\n"
        "    local info = debug.getinfo(fn, 'S')\n"
        "    if not info or not info.source or info.source:sub(1,1) ~= '@' then return nil end\n"
        "    local filepath = info.source:sub(2)\n"
        "    local file = io.open(filepath, 'r')\n"
        "    if not file then return nil end\n"
        "    local lines = {}\n"
        "    local lineNum = 0\n"
        "    for line in file:lines() do\n"
        "      lineNum = lineNum + 1\n"
        "      if lineNum >= info.linedefined and lineNum <= info.lastlinedefined then\n"
        "        table.insert(lines, line)\n"
        "      end\n"
        "    end\n"
        "    file:close()\n"
        "    return table.concat(lines, '\\n')\n"
        "  end\n"
        "\n"
        "  -- Build exports list with source code\n"
        "  local exports = {}\n"
        "  for k, v in pairs(app) do\n"
        "    local exp = {name = k, type = type(v)}\n"
        "    if type(v) == 'function' then\n"
        "      exp.source = extractFunctionSource(v)\n"
        "    end\n"
        "    table.insert(exports, exp)\n"
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
        "  -- JSON escape helper\n"
        "  local function jsonEscape(s)\n"
        "    if not s then return nil end\n"
        "    return s:gsub('\\\\', '\\\\\\\\'):gsub('\"', '\\\\\"'):gsub('\\n', '\\\\n'):gsub('\\r', '\\\\r'):gsub('\\t', '\\\\t')\n"
        "  end\n"
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
        "  -- Add logic_block with function sources\n"
        "  io.write('  \"logic_block\": {\\n')\n"
        "  io.write('    \"functions\": {\\n')\n"
        "  local funcCount = 0\n"
        "  local funcExports = {}\n"
        "  for _, exp in ipairs(exports) do\n"
        "    if exp.type == 'function' and exp.source then\n"
        "      table.insert(funcExports, exp)\n"
        "    end\n"
        "  end\n"
        "  for i, exp in ipairs(funcExports) do\n"
        "    io.write('      \"' .. exp.name .. '\": {\\n')\n"
        "    io.write('        \"sources\": {\\n')\n"
        "    io.write('          \"lua\": \"' .. jsonEscape(exp.source) .. '\"\\n')\n"
        "    io.write('        }\\n')\n"
        "    io.write('      }')\n"
        "    if i < #funcExports then io.write(',') end\n"
        "    io.write('\\n')\n"
        "  end\n"
        "  io.write('    }\\n')\n"
        "  io.write('  },\\n')\n"
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
    char* source;         // Full function source code (for functions)
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
    free(info->source);
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
 * Extract full function source code from a Lua source file
 * Finds "local function name(...)" or "local name = function(...)" and extracts to matching "end"
 */
static char* extract_function_source(const char* source, const char* func_name) {
    if (!source || !func_name) return NULL;

    char pattern[256];
    const char* func_start = NULL;

    // Try: local function name(params)
    snprintf(pattern, sizeof(pattern), "local function %s", func_name);
    func_start = strstr(source, pattern);

    if (!func_start) {
        // Try: local name = function(params)
        snprintf(pattern, sizeof(pattern), "local %s = function", func_name);
        func_start = strstr(source, pattern);
    }

    if (!func_start) return NULL;

    // Find the end of this function by counting end/function keywords
    const char* p = func_start;
    int depth = 0;
    bool in_string = false;
    char string_char = 0;
    bool found_function = false;

    while (*p) {
        // Skip strings
        if (!in_string && (*p == '"' || *p == '\'')) {
            in_string = true;
            string_char = *p;
            p++;
            continue;
        }
        if (in_string) {
            if (*p == '\\' && *(p+1)) {
                p += 2;
                continue;
            }
            if (*p == string_char) {
                in_string = false;
            }
            p++;
            continue;
        }

        // Skip long strings [[...]]
        if (*p == '[' && *(p+1) == '[') {
            p += 2;
            while (*p && !(*p == ']' && *(p+1) == ']')) p++;
            if (*p) p += 2;
            continue;
        }

        // Skip comments
        if (*p == '-' && *(p+1) == '-') {
            if (*(p+2) == '[' && *(p+3) == '[') {
                // Long comment
                p += 4;
                while (*p && !(*p == ']' && *(p+1) == ']')) p++;
                if (*p) p += 2;
            } else {
                // Single line comment
                while (*p && *p != '\n') p++;
            }
            continue;
        }

        // Track depth with function/if/for/while/repeat keywords
        // Note: Don't count "do" separately - it's part of for/while, not a separate block
        // In Lua: for-do-end and while-do-end are single blocks
        if (strncmp(p, "function", 8) == 0 &&
            (p == func_start || !isalnum((unsigned char)*(p-1))) &&
            !isalnum((unsigned char)*(p+8)) && *(p+8) != '_') {
            depth++;
            found_function = true;
            p += 8;
            continue;
        }
        // Check for block-starting keywords: if, for, while, repeat
        // Each of these pairs with exactly one "end"
        if (strncmp(p, "if", 2) == 0 && !isalnum((unsigned char)*(p+2)) && *(p+2) != '_' &&
            (p == func_start || !isalnum((unsigned char)*(p-1)))) {
            depth++;
            p += 2;
            continue;
        }
        if (strncmp(p, "for", 3) == 0 && !isalnum((unsigned char)*(p+3)) && *(p+3) != '_' &&
            (p == func_start || !isalnum((unsigned char)*(p-1)))) {
            depth++;
            p += 3;
            continue;
        }
        if (strncmp(p, "while", 5) == 0 && !isalnum((unsigned char)*(p+5)) && *(p+5) != '_' &&
            (p == func_start || !isalnum((unsigned char)*(p-1)))) {
            depth++;
            p += 5;
            continue;
        }
        if (strncmp(p, "repeat", 6) == 0 && !isalnum((unsigned char)*(p+6)) && *(p+6) != '_' &&
            (p == func_start || !isalnum((unsigned char)*(p-1)))) {
            depth++;
            p += 6;
            continue;
        }

        // Check for "end" keyword
        if (strncmp(p, "end", 3) == 0 &&
            (p == source || !isalnum((unsigned char)*(p-1))) &&
            !isalnum((unsigned char)*(p+3)) && *(p+3) != '_') {
            depth--;
            if (depth == 0 && found_function) {
                // Found the matching end
                const char* func_end = p + 3;
                size_t len = func_end - func_start;
                char* result = malloc(len + 1);
                if (result) {
                    memcpy(result, func_start, len);
                    result[len] = '\0';
                }
                return result;
            }
            p += 3;
            continue;
        }

        // Handle "until" for repeat blocks
        if (strncmp(p, "until", 5) == 0 &&
            (p == source || !isalnum((unsigned char)*(p-1))) &&
            !isalnum((unsigned char)*(p+5)) && *(p+5) != '_') {
            depth--;
            p += 5;
            continue;
        }

        p++;
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
        char* func_source = NULL;

        if (strncmp(p, "function", 8) == 0) {
            // Inline function: return { Name = function(params) ... }
            export_type = strdup("function");

            // Capture inline function source starting from "function"
            const char* inline_func_start = p;
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

            // Now find the end of this inline function
            // Track depth to find matching "end"
            int func_depth = 1;
            const char* scan = p;
            bool in_str = false;
            char str_char = 0;

            while (*scan && func_depth > 0) {
                // Skip strings
                if (!in_str && (*scan == '"' || *scan == '\'')) {
                    in_str = true;
                    str_char = *scan;
                    scan++;
                    continue;
                }
                if (in_str) {
                    if (*scan == '\\' && *(scan+1)) {
                        scan += 2;
                        continue;
                    }
                    if (*scan == str_char) {
                        in_str = false;
                    }
                    scan++;
                    continue;
                }

                // Skip comments
                if (*scan == '-' && *(scan+1) == '-') {
                    if (*(scan+2) == '[' && *(scan+3) == '[') {
                        scan += 4;
                        while (*scan && !(*scan == ']' && *(scan+1) == ']')) scan++;
                        if (*scan) scan += 2;
                    } else {
                        while (*scan && *scan != '\n') scan++;
                    }
                    continue;
                }

                // Track depth-increasing keywords
                if ((strncmp(scan, "function", 8) == 0 || strncmp(scan, "if", 2) == 0 ||
                     strncmp(scan, "for", 3) == 0 || strncmp(scan, "while", 5) == 0 ||
                     strncmp(scan, "do", 2) == 0 || strncmp(scan, "repeat", 6) == 0) &&
                    (scan == source || !isalnum((unsigned char)*(scan-1)))) {
                    if (strncmp(scan, "function", 8) == 0 && !isalnum((unsigned char)*(scan+8))) {
                        func_depth++;
                        scan += 8;
                        continue;
                    }
                    if (strncmp(scan, "if", 2) == 0 && !isalnum((unsigned char)*(scan+2))) {
                        func_depth++;
                        scan += 2;
                        continue;
                    }
                    if (strncmp(scan, "for", 3) == 0 && !isalnum((unsigned char)*(scan+3))) {
                        func_depth++;
                        scan += 3;
                        continue;
                    }
                    if (strncmp(scan, "while", 5) == 0 && !isalnum((unsigned char)*(scan+5))) {
                        func_depth++;
                        scan += 5;
                        continue;
                    }
                    if (strncmp(scan, "do", 2) == 0 && !isalnum((unsigned char)*(scan+2))) {
                        func_depth++;
                        scan += 2;
                        continue;
                    }
                    if (strncmp(scan, "repeat", 6) == 0 && !isalnum((unsigned char)*(scan+6))) {
                        func_depth++;
                        scan += 6;
                        continue;
                    }
                }

                // Track depth-decreasing keywords
                if (strncmp(scan, "end", 3) == 0 &&
                    (scan == source || !isalnum((unsigned char)*(scan-1))) &&
                    !isalnum((unsigned char)*(scan+3))) {
                    func_depth--;
                    if (func_depth == 0) {
                        // Found the end
                        size_t func_len = (scan + 3) - inline_func_start;
                        func_source = malloc(func_len + 1);
                        if (func_source) {
                            memcpy(func_source, inline_func_start, func_len);
                            func_source[func_len] = '\0';
                        }
                    }
                    scan += 3;
                    continue;
                }
                if (strncmp(scan, "until", 5) == 0 &&
                    (scan == source || !isalnum((unsigned char)*(scan-1))) &&
                    !isalnum((unsigned char)*(scan+5))) {
                    func_depth--;
                    scan += 5;
                    continue;
                }

                scan++;
            }
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
                // Extract full function source
                func_source = extract_function_source(source, var_name);
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
                    free(func_source);
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
            exp->source = func_source;  // Transfer ownership
        } else {
            free(export_type);
            free(func_source);
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
        "-- Extract source_declarations from source file\n"
        "local source_declarations = { requires = {}, functions = {} }\n"
        "local source_file_path = '%s'\n"
        "local source_content = ''\n"
        "local source_file = io.open(source_file_path, 'r')\n"
        "if source_file then\n"
        "  source_content = source_file:read('*a')\n"
        "  source_file:close()\n"
        "  \n"
        "  -- Extract requires\n"
        "  for line in source_content:gmatch('[^\\n]+') do\n"
        "    local var, mod = line:match('local%%s+([%%w_]+)%%s*=%%s*require%%s*%%([\"\\']([^\"\\']+)[\"\\']%%)')\n"
        "    if var and mod then\n"
        "      table.insert(source_declarations.requires, { variable = var, module = mod })\n"
        "    end\n"
        "  end\n"
        "  \n"
        "  -- Extract function definitions with full bodies\n"
        "  local lines = {}\n"
        "  for line in source_content:gmatch('[^\\n]+') do\n"
        "    table.insert(lines, line)\n"
        "  end\n"
        "  \n"
        "  local i = 1\n"
        "  while i <= #lines do\n"
        "    local line = lines[i]\n"
        "    local name, params = line:match('local%%s+function%%s+([%%w_]+)%%s*%%(' .. '([^%%)]*)' .. '%%)')\n"
        "    if name then\n"
        "      local func_lines = {line}\n"
        "      local depth = 1\n"
        "      local j = i + 1\n"
        "      while j <= #lines and depth > 0 do\n"
        "        local l = lines[j]\n"
        "        table.insert(func_lines, l)\n"
        "        if l:match('%%f[%%w]function%%f[%%W]') or l:match('%%f[%%w]if%%f[%%W].*then') or\n"
        "           l:match('%%f[%%w]for%%f[%%W].*do') or l:match('%%f[%%w]while%%f[%%W].*do') or\n"
        "           l:match('%%f[%%w]repeat%%f[%%W]') then\n"
        "          depth = depth + 1\n"
        "        end\n"
        "        if l:match('%%f[%%w]end%%f[%%W]') or l:match('^%%s*end%%s*$') or l:match('%%f[%%w]until%%f[%%W]') then\n"
        "          depth = depth - 1\n"
        "        end\n"
        "        j = j + 1\n"
        "      end\n"
        "      local func_source = table.concat(func_lines, '\\n')\n"
        "      table.insert(source_declarations.functions, {\n"
        "        name = name, params = params, line = i, source = func_source\n"
        "      })\n"
        "      i = j\n"
        "    else\n"
        "      i = i + 1\n"
        "    end\n"
        "  end\n"
        "end\n"
        "\n"
        "-- Track component definitions\n"
        "local component_defs = {}\n"
        "\n"
        "-- Helper function to check if a value is a component function\n"
        "local function is_component_function(fn)\n"
        "  if type(fn) ~= 'function' then return false end\n"
        "  local success, result = pcall(fn, {})\n"
        "  return success and type(result) == 'cdata' and result.type ~= nil\n"
        "end\n"
        "\n"
        "-- Iterate through exports and find component definitions\n"
        "for name, export_value in pairs(module_exports) do\n"
        "  if is_component_function(export_value) then\n"
        "    local success, component_root = pcall(export_value, {})\n"
        "    if success and component_root then\n"
        "      local info = debug.getinfo(export_value)\n"
        "      local params = {}\n"
        "      local func_source = nil\n"
        "      \n"
        "      -- Find the function source from source_declarations\n"
        "      for _, fn_def in ipairs(source_declarations.functions) do\n"
        "        if fn_def.name == name then\n"
        "          func_source = fn_def.source\n"
        "          -- Parse params from source_declarations\n"
        "          if fn_def.params then\n"
        "            for param in fn_def.params:gmatch('[^,]+') do\n"
        "              param = param:match('^%%s*(.-)%%s*$')\n"
        "              if param ~= '' then\n"
        "                table.insert(params, param)\n"
        "              end\n"
        "            end\n"
        "          end\n"
        "          break\n"
        "        end\n"
        "      end\n"
        "      \n"
        "      -- Store component definition info with source\n"
        "      table.insert(component_defs, {\n"
        "        name = name,\n"
        "        root = component_root,\n"
        "        params = params,\n"
        "        source = func_source\n"
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
        "      local template_json = json_str_lua:match('%%b{}')\n"
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
        "-- JSON escape helper\n"
        "local function json_escape(s)\n"
        "  if not s then return '' end\n"
        "  return s:gsub('\\\\', '\\\\\\\\'):gsub('\"', '\\\\\"'):gsub('\\n', '\\\\n'):gsub('\\r', '\\\\r'):gsub('\\t', '\\\\t')\n"
        "end\n"
        "\n"
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
        "\n"
        "-- Add source_declarations\n"
        "table.insert(json_parts, '  \"source_declarations\": {\\n')\n"
        "table.insert(json_parts, '    \"requires\": [\\n')\n"
        "for i, req in ipairs(source_declarations.requires) do\n"
        "  table.insert(json_parts, string.format('      {\"variable\": \"%%s\", \"module\": \"%%s\"}', req.variable, req.module))\n"
        "  if i < #source_declarations.requires then table.insert(json_parts, ',') end\n"
        "  table.insert(json_parts, '\\n')\n"
        "end\n"
        "table.insert(json_parts, '    ],\\n')\n"
        "table.insert(json_parts, '    \"functions\": [\\n')\n"
        "for i, fn in ipairs(source_declarations.functions) do\n"
        "  table.insert(json_parts, string.format('      {\"name\": \"%%s\", \"params\": \"%%s\", \"source\": \"%%s\"}', fn.name, fn.params or '', json_escape(fn.source or '')))\n"
        "  if i < #source_declarations.functions then table.insert(json_parts, ',') end\n"
        "  table.insert(json_parts, '\\n')\n"
        "end\n"
        "table.insert(json_parts, '    ]\\n')\n"
        "table.insert(json_parts, '  },\\n')\n"
        "\n"
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
        "  -- Add source field if available\n"
        "  if def.source then\n"
        "    table.insert(json_parts, ',\\n      \"source\": \"')\n"
        "    table.insert(json_parts, json_escape(def.source))\n"
        "    table.insert(json_parts, '\"')\n"
        "  end\n"
        "\n"
        "  -- Serialize and add the template\n"
        "  if def.root then\n"
        "    local json_str = C.ir_serialize_json_complete(def.root, nil, nil, nil, nil)\n"
        "    if json_str ~= nil then\n"
        "      local json_cdata = ffi.string(json_str)\n"
        "      C.free(json_str)\n"
        "      -- Parse to get the root component\n"
        "      local root_obj = json_cdata:match('%%b{}')\n"
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
 * Static source extraction for component modules (no execution)
 *
 * This is the key function for the KIR-to-Lua codegen overhaul.
 * It extracts source_declarations from Lua source WITHOUT executing the code.
 * This eliminates plugin dependency issues during compilation.
 *
 * Extracts:
 * - require() statements -> source_declarations.requires
 * - local function definitions -> source_declarations.functions
 * - export declarations from return statement -> component_definitions
 */
static char* ir_lua_static_source_extract(const char* source, size_t length,
                                          const char* source_file,
                                          const char* module_id) {
    if (!source) return NULL;

    if (length == 0) {
        length = strlen(source);
    }

    fprintf(stderr, "  [Static Extract] Processing %s (%zu bytes)\n", module_id, length);

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    // Add format and version
    cJSON_AddStringToObject(root, "format", "kir");
    cJSON_AddStringToObject(root, "version", "3.0");

    // Add metadata
    cJSON* meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "source_language", "lua");
    cJSON_AddStringToObject(meta, "module_id", module_id);
    cJSON_AddStringToObject(meta, "compiler_version", "0.3.0");
    if (source_file) {
        cJSON_AddStringToObject(meta, "source_file", source_file);
    }
    cJSON_AddItemToObject(root, "metadata", meta);

    // Add null root (component modules don't have a root component)
    cJSON_AddNullToObject(root, "root");

    // Create source_declarations object
    cJSON* source_decls = cJSON_CreateObject();

    // ========================================================================
    // Extract requires: local X = require("Y")
    // ========================================================================
    cJSON* requires_array = cJSON_CreateArray();
    const char* line_start = source;
    const char* line_end;

    while (*line_start) {
        // Find end of line
        line_end = line_start;
        while (*line_end && *line_end != '\n') line_end++;

        // Check for require pattern
        const char* local_pos = strstr(line_start, "local ");
        if (local_pos && local_pos < line_end) {
            const char* require_pos = strstr(local_pos, "require");
            if (require_pos && require_pos < line_end) {
                // Extract variable name: local VARNAME = require(...)
                const char* var_start = local_pos + 6;
                while (*var_start == ' ' || *var_start == '\t') var_start++;

                const char* var_end = var_start;
                while (*var_end && (isalnum((unsigned char)*var_end) || *var_end == '_')) var_end++;

                // Extract module name from quotes
                const char* quote_start = strchr(require_pos, '"');
                if (!quote_start || quote_start > line_end) {
                    quote_start = strchr(require_pos, '\'');
                }

                if (quote_start && quote_start < line_end) {
                    char quote_char = *quote_start;
                    quote_start++;
                    const char* quote_end = strchr(quote_start, quote_char);

                    if (quote_end && quote_end < line_end && var_end > var_start) {
                        // Create require entry
                        cJSON* req = cJSON_CreateObject();
                        char* var_name = strndup(var_start, var_end - var_start);
                        char* mod_name = strndup(quote_start, quote_end - quote_start);

                        cJSON_AddStringToObject(req, "variable", var_name);
                        cJSON_AddStringToObject(req, "module", mod_name);
                        cJSON_AddItemToArray(requires_array, req);

                        free(var_name);
                        free(mod_name);
                    }
                }
            }
        }

        // Move to next line
        if (*line_end == '\n') line_end++;
        line_start = line_end;
    }
    cJSON_AddItemToObject(source_decls, "requires", requires_array);

    // ========================================================================
    // Extract function definitions: local function name(params) ... end
    // ========================================================================
    cJSON* functions_array = cJSON_CreateArray();

    // Split source into lines for easier processing
    char** lines = NULL;
    int line_count = 0;
    int line_capacity = 256;
    lines = calloc(line_capacity, sizeof(char*));

    line_start = source;
    while (*line_start) {
        line_end = line_start;
        while (*line_end && *line_end != '\n') line_end++;

        size_t line_len = line_end - line_start;
        char* line = malloc(line_len + 1);
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        if (line_count >= line_capacity) {
            line_capacity *= 2;
            lines = realloc(lines, line_capacity * sizeof(char*));
        }
        lines[line_count++] = line;

        if (*line_end == '\n') line_end++;
        line_start = line_end;
    }

    // Process lines to find function definitions
    int i = 0;
    while (i < line_count) {
        const char* line = lines[i];

        // Look for "local function name(" pattern
        const char* func_pattern = strstr(line, "local function ");
        if (func_pattern) {
            // Extract function name
            const char* name_start = func_pattern + 15;  // strlen("local function ")
            while (*name_start == ' ' || *name_start == '\t') name_start++;

            const char* name_end = name_start;
            while (*name_end && (isalnum((unsigned char)*name_end) || *name_end == '_')) name_end++;

            if (name_end > name_start) {
                char* func_name = strndup(name_start, name_end - name_start);

                // Extract parameters
                const char* params_start = strchr(name_end, '(');
                const char* params_end = params_start ? strchr(params_start, ')') : NULL;
                char* params = NULL;
                if (params_start && params_end) {
                    params_start++;
                    params = strndup(params_start, params_end - params_start);
                }

                // Find the matching "end" by tracking depth
                int depth = 1;
                int func_start_line = i;
                int j = i + 1;

                while (j < line_count && depth > 0) {
                    const char* l = lines[j];

                    // Count block openers (skip comments)
                    const char* comment_check = strstr(l, "--");
                    size_t check_len = comment_check ? (size_t)(comment_check - l) : strlen(l);
                    char* check_line = strndup(l, check_len);

                    // Check for depth-increasing keywords
                    // Count ALL function definitions (including anonymous callbacks)
                    // Pattern: "function" followed by whitespace or "("
                    const char* func_check = check_line;
                    while ((func_check = strstr(func_check, "function")) != NULL) {
                        // Check if it's a standalone keyword
                        bool is_func_keyword = true;
                        if (func_check > check_line) {
                            char prev = *(func_check - 1);
                            if (isalnum((unsigned char)prev) || prev == '_') is_func_keyword = false;
                        }
                        if (is_func_keyword) {
                            char next = *(func_check + 8);
                            // Must be followed by whitespace, (, or identifier
                            if (next == ' ' || next == '\t' || next == '(' ||
                                isalpha((unsigned char)next) || next == '_') {
                                depth++;
                            }
                        }
                        func_check += 8;
                    }

                    // Check if this is a single-line if/for/while (has both opener and end on same line)
                    bool has_end = (strstr(check_line, " end") != NULL || strstr(check_line, "\tend") != NULL);

                    // if ... then (but not single-line if, and not elseif)
                    const char* if_pos = strstr(check_line, "if ");
                    if (if_pos && strstr(check_line, " then") && !has_end) {
                        // Make sure this is not "elseif" - check character before "if"
                        bool is_elseif = (if_pos > check_line && *(if_pos - 1) == 'e');
                        if (!is_elseif) {
                            depth++;
                        }
                    }
                    // for ... do
                    if (strstr(check_line, "for ") && strstr(check_line, " do") && !has_end) {
                        depth++;
                    }
                    // while ... do
                    if (strstr(check_line, "while ") && strstr(check_line, " do") && !has_end) {
                        depth++;
                    }
                    // repeat
                    if (strstr(check_line, "repeat") && !strstr(check_line, "until")) {
                        depth++;
                    }

                    // Count depth-decreasing keywords
                    // For single-line constructs (if...then...end), don't count the end
                    bool is_single_line_construct =
                        (strstr(check_line, "if ") && strstr(check_line, " then") && has_end) ||
                        (strstr(check_line, "for ") && strstr(check_line, " do") && has_end) ||
                        (strstr(check_line, "while ") && strstr(check_line, " do") && has_end);

                    if (!is_single_line_construct) {
                        // Standalone "end"
                        const char* end_check = check_line;
                        while ((end_check = strstr(end_check, "end")) != NULL) {
                            // Check if it's a standalone keyword (not "endif", "render", etc.)
                            bool is_standalone = true;
                            if (end_check > check_line) {
                                char prev = *(end_check - 1);
                                if (isalnum((unsigned char)prev) || prev == '_') is_standalone = false;
                            }
                            if (is_standalone) {
                                char next = *(end_check + 3);
                                if (isalnum((unsigned char)next) || next == '_') is_standalone = false;
                            }
                            if (is_standalone) {
                                depth--;
                            }
                            end_check += 3;
                        }
                    }
                    // until (for repeat blocks)
                    if (strstr(check_line, "until")) {
                        depth--;
                    }

                    free(check_line);
                    j++;
                }

                // Collect function source
                size_t source_size = 0;
                for (int k = func_start_line; k < j; k++) {
                    source_size += strlen(lines[k]) + 1;  // +1 for newline
                }

                char* func_source = malloc(source_size + 1);
                func_source[0] = '\0';
                for (int k = func_start_line; k < j; k++) {
                    strcat(func_source, lines[k]);
                    if (k < j - 1) strcat(func_source, "\n");
                }

                // Create function entry
                cJSON* fn = cJSON_CreateObject();
                cJSON_AddStringToObject(fn, "name", func_name);
                cJSON_AddStringToObject(fn, "params", params ? params : "");
                cJSON_AddNumberToObject(fn, "line", func_start_line + 1);
                cJSON_AddStringToObject(fn, "source", func_source);
                cJSON_AddItemToArray(functions_array, fn);

                free(func_name);
                free(params);
                free(func_source);

                i = j;  // Skip to after the function
                continue;
            }
        }
        i++;
    }
    cJSON_AddItemToObject(source_decls, "functions", functions_array);

    // ========================================================================
    // Extract module_constants: local NAME = value (tables, strings, numbers)
    // ========================================================================
    char* module_constants_buf = NULL;
    size_t constants_len = 0;
    size_t constants_cap = 0;

    // Find first function line and last require line
    int first_func_line = -1;
    int last_req_line = -1;
    for (int li = 0; li < line_count; li++) {
        if (strstr(lines[li], "local function ")) {
            first_func_line = li;
            break;
        }
    }
    for (int li = 0; li < line_count; li++) {
        if (strstr(lines[li], "require")) {
            last_req_line = li;
        }
    }

    // Extract constants between last require and first function
    int const_start = last_req_line >= 0 ? last_req_line + 1 : 0;
    int const_end = first_func_line >= 0 ? first_func_line : line_count;

    for (int li = const_start; li < const_end; li++) {
        const char* line = lines[li];

        // Skip empty lines and comments
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0' || strncmp(line, "--", 2) == 0) continue;
        if (strstr(lines[li], "require")) continue;

        // Check for local NAME = pattern (not function)
        if (strncmp(line, "local ", 6) == 0 && !strstr(line, "function")) {
            // Check if it ends with { (multi-line table)
            const char* stripped_end = lines[li] + strlen(lines[li]) - 1;
            while (stripped_end > lines[li] && (*stripped_end == ' ' || *stripped_end == '\t')) stripped_end--;

            if (*stripped_end == '{') {
                // Multi-line table - track brace depth
                int depth = 0;
                int table_start = li;
                for (int tj = li; tj < line_count; tj++) {
                    for (const char* c = lines[tj]; *c; c++) {
                        if (*c == '{') depth++;
                        if (*c == '}') depth--;
                    }
                    if (depth == 0 && tj > table_start) {
                        // Extract all lines from table_start to tj
                        for (int tk = table_start; tk <= tj; tk++) {
                            size_t llen = strlen(lines[tk]);
                            if (constants_len + llen + 2 > constants_cap) {
                                constants_cap = constants_cap == 0 ? 4096 : constants_cap * 2;
                                module_constants_buf = realloc(module_constants_buf, constants_cap);
                            }
                            memcpy(module_constants_buf + constants_len, lines[tk], llen);
                            constants_len += llen;
                            module_constants_buf[constants_len++] = '\n';
                        }
                        module_constants_buf[constants_len++] = '\n';
                        li = tj;  // Skip past the table
                        break;
                    }
                }
            } else {
                // Single line constant
                size_t llen = strlen(lines[li]);
                if (constants_len + llen + 3 > constants_cap) {
                    constants_cap = constants_cap == 0 ? 4096 : constants_cap * 2;
                    module_constants_buf = realloc(module_constants_buf, constants_cap);
                }
                memcpy(module_constants_buf + constants_len, lines[li], llen);
                constants_len += llen;
                module_constants_buf[constants_len++] = '\n';
                module_constants_buf[constants_len++] = '\n';
            }
        }
    }

    if (module_constants_buf && constants_len > 0) {
        module_constants_buf[constants_len] = '\0';
        cJSON_AddStringToObject(source_decls, "module_constants", module_constants_buf);
        free(module_constants_buf);
    } else {
        cJSON_AddNullToObject(source_decls, "module_constants");
    }

    // Add source_declarations to root
    cJSON_AddItemToObject(root, "source_declarations", source_decls);

    // ========================================================================
    // Extract exports from return statement -> component_definitions
    // ========================================================================
    cJSON* component_defs = cJSON_CreateArray();

    // Find the return { ... } statement at the end
    // Look backward from end of file for "return {"
    ExportList* exports = lua_extract_exports(source);
    if (exports && exports->count > 0) {
        for (int e = 0; e < exports->count; e++) {
            ExportInfo* exp = &exports->exports[e];

            cJSON* comp_def = cJSON_CreateObject();
            cJSON_AddStringToObject(comp_def, "name", exp->name);

            // Add props from parameters
            cJSON* props = cJSON_CreateArray();
            if (exp->parameters && exp->param_count > 0) {
                for (int p = 0; p < exp->param_count; p++) {
                    cJSON* prop = cJSON_CreateObject();
                    cJSON_AddStringToObject(prop, "name", exp->parameters[p]);
                    cJSON_AddStringToObject(prop, "type", "any");
                    cJSON_AddItemToArray(props, prop);
                }
            }
            cJSON_AddItemToObject(comp_def, "props", props);

            // Add source code if available
            if (exp->source) {
                cJSON_AddStringToObject(comp_def, "source", exp->source);
            }

            cJSON_AddItemToArray(component_defs, comp_def);
        }
        lua_export_list_free(exports);
    }
    cJSON_AddItemToObject(root, "component_definitions", component_defs);

    // Free lines
    for (int l = 0; l < line_count; l++) {
        free(lines[l]);
    }
    free(lines);

    // Get counts before deleting root (cJSON_Delete frees all child objects)
    int requires_count = cJSON_GetArraySize(requires_array);
    int functions_count = cJSON_GetArraySize(functions_array);
    int exports_count = cJSON_GetArraySize(component_defs);

    // Serialize to JSON
    char* result = cJSON_Print(root);
    cJSON_Delete(root);

    if (result) {
        fprintf(stderr, "  [Static Extract] ✓ Extracted %d requires, %d functions, %d exports\n",
                requires_count, functions_count, exports_count);
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

    // Generate plugin KIR files from kryon.toml
    // First, find kryon.toml in the source directory or parent
    char kryon_toml_path[4096];
    char source_dir[4096];
    strncpy(source_dir, abs_path, sizeof(source_dir) - 1);
    source_dir[sizeof(source_dir) - 1] = '\0';
    char* last_slash = strrchr(source_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    }
    snprintf(kryon_toml_path, sizeof(kryon_toml_path), "%s/kryon.toml", source_dir);
    if (access(kryon_toml_path, R_OK) == 0) {
        ir_lua_generate_plugin_kirs_from_toml(kryon_toml_path, output_dir);
    }

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
        } else {
            // For component modules: check if file was already written by main wrapper
            // with component_definitions
            char kir_file_path[2048];
            snprintf(kir_file_path, sizeof(kir_file_path), "%s/%s.kir", output_dir, module->module_id);

            // Check if file exists and has BOTH component_definitions AND source_declarations
            // If only component_definitions exists (from main wrapper), we need to re-extract to add source
            bool skip_extraction = false;
            FILE* kf = fopen(kir_file_path, "r");
            if (kf) {
                // Read file content to check for component_definitions and source_declarations
                fseek(kf, 0, SEEK_END);
                long ksize = ftell(kf);
                if (ksize > 100) {  // File has substantial content
                    fseek(kf, 0, SEEK_SET);
                    char* kcontent = malloc(ksize + 1);
                    if (kcontent) {
                        fread(kcontent, 1, ksize, kf);
                        kcontent[ksize] = '\0';
                        // Only skip if BOTH component_definitions AND source_declarations exist
                        // This ensures we re-extract to add source if it's missing
                        bool has_comp_defs = strstr(kcontent, "component_definitions") != NULL;
                        bool has_source_decls = strstr(kcontent, "source_declarations") != NULL;
                        if (has_comp_defs && has_source_decls) {
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
            } else {
                // Check if this is a component module (has UI components)
                bool has_ui = lua_has_ui_components(module->source);

                if (has_ui) {
                    fprintf(stderr, "  Extracting component definitions from: %s\n", module->module_id);

                    // Use STATIC source extraction - no execution required!
                    // This eliminates plugin dependency issues during compilation.
                    char* comp_result = ir_lua_static_source_extract(
                        module->source,
                        module->source_len,
                        module->file_path,
                        module->module_id
                    );

                    if (comp_result) {
                        // Parse the result
                        cJSON* comp_root = cJSON_Parse(comp_result);
                        free(comp_result);

                        if (comp_root) {
                            root = comp_root;

                            // Verify we got source_declarations and component_definitions
                            cJSON* source_decls = cJSON_GetObjectItem(root, "source_declarations");
                            cJSON* comp_defs = cJSON_GetObjectItem(root, "component_definitions");
                            if (source_decls && comp_defs) {
                                cJSON* funcs = cJSON_GetObjectItem(source_decls, "functions");
                                fprintf(stderr, "    ✓ Static extraction: %d function(s), %d export(s)\n",
                                        funcs ? cJSON_GetArraySize(funcs) : 0,
                                        cJSON_GetArraySize(comp_defs));
                            }
                        } else {
                            // Failed to parse, fall back to simple structure
                            fprintf(stderr, "    Warning: Failed to parse static extraction, using fallback\n");
                            root = cJSON_CreateObject();
                            cJSON_AddStringToObject(root, "format", "kir");
                            cJSON_AddNullToObject(root, "root");
                        }
                    } else {
                        // Static extraction failed, fall back to simple structure
                        fprintf(stderr, "    Warning: Static extraction failed, using fallback\n");
                        root = cJSON_CreateObject();
                        cJSON_AddStringToObject(root, "format", "kir");
                        cJSON_AddNullToObject(root, "root");
                    }
            } else {
                // For non-component library modules: use static extraction to get source_declarations
                // This ensures requires, functions, and module_constants are preserved
                fprintf(stderr, "  Extracting library module: %s\n", module->module_id);

                char* lib_result = ir_lua_static_source_extract(
                    module->source,
                    module->source_len,
                    module->file_path,
                    module->module_id
                );

                if (lib_result) {
                    root = cJSON_Parse(lib_result);
                    free(lib_result);

                    if (root) {
                        // Verify we got source_declarations
                        cJSON* source_decls = cJSON_GetObjectItem(root, "source_declarations");
                        if (source_decls) {
                            cJSON* funcs = cJSON_GetObjectItem(source_decls, "functions");
                            cJSON* reqs = cJSON_GetObjectItem(source_decls, "requires");
                            fprintf(stderr, "    ✓ Static extraction: %d require(s), %d function(s)\n",
                                    reqs ? cJSON_GetArraySize(reqs) : 0,
                                    funcs ? cJSON_GetArraySize(funcs) : 0);
                        }

                        // Also add exports for library module compatibility
                        ExportList* exports = lua_extract_exports(module->source);
                        if (exports && exports->count > 0) {
                            cJSON* exports_array = cJSON_CreateArray();
                            cJSON* logic_block = cJSON_CreateObject();
                            cJSON* functions = cJSON_CreateObject();

                            for (int e = 0; e < exports->count; e++) {
                                ExportInfo* exp = &exports->exports[e];
                                cJSON* exp_obj = cJSON_CreateObject();
                                cJSON_AddStringToObject(exp_obj, "name", exp->name);
                                cJSON_AddStringToObject(exp_obj, "type", exp->type);

                                if (exp->parameters && exp->param_count > 0) {
                                    cJSON* params = cJSON_CreateArray();
                                    for (int j = 0; j < exp->param_count; j++) {
                                        cJSON_AddItemToArray(params, cJSON_CreateString(exp->parameters[j]));
                                    }
                                    cJSON_AddItemToObject(exp_obj, "parameters", params);
                                }
                                cJSON_AddItemToArray(exports_array, exp_obj);

                                if (exp->source && strcmp(exp->type, "function") == 0) {
                                    cJSON* func_obj = cJSON_CreateObject();
                                    cJSON* sources = cJSON_CreateObject();
                                    cJSON_AddStringToObject(sources, "lua", exp->source);
                                    cJSON_AddItemToObject(func_obj, "sources", sources);
                                    cJSON_AddItemToObject(functions, exp->name, func_obj);
                                }
                            }
                            cJSON_AddItemToObject(root, "exports", exports_array);
                            cJSON_AddItemToObject(logic_block, "functions", functions);
                            cJSON_AddItemToObject(root, "logic_block", logic_block);

                            lua_export_list_free(exports);
                        }
                    }
                }

                if (!root) {
                    // Fallback: create minimal KIR structure
                    fprintf(stderr, "    Warning: Static extraction failed, using fallback\n");
                    root = cJSON_CreateObject();
                    cJSON_AddStringToObject(root, "format", "kir");
                    cJSON_AddNullToObject(root, "root");
                }
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

// ============================================================================
// Plugin KIR Generation from kryon.toml
// ============================================================================

/**
 * Extract function definitions from a plugin Lua binding file
 * Used to generate plugin KIR without executing the binding code
 */
static cJSON* extract_plugin_functions(const char* source) {
    if (!source) return NULL;

    cJSON* functions = cJSON_CreateArray();

    // Find "function ModuleName.functionName(" patterns
    const char* p = source;
    while (*p) {
        // Look for "function" keyword
        const char* func_pos = strstr(p, "function ");
        if (!func_pos) break;

        // Check if it's a module function: "function Module.name("
        const char* dot = strchr(func_pos + 9, '.');
        const char* paren = strchr(func_pos + 9, '(');

        if (dot && paren && dot < paren) {
            // Extract function name (after the dot)
            const char* name_start = dot + 1;
            const char* name_end = paren;

            // Trim whitespace
            while (*name_start == ' ' || *name_start == '\t') name_start++;
            while (name_end > name_start && (*(name_end-1) == ' ' || *(name_end-1) == '\t')) name_end--;

            if (name_end > name_start) {
                char* name = strndup(name_start, name_end - name_start);

                // Extract parameters
                const char* params_start = paren + 1;
                const char* params_end = strchr(params_start, ')');
                char* params = NULL;
                if (params_end && params_end > params_start) {
                    params = strndup(params_start, params_end - params_start);
                }

                // Find the end of the function to capture source
                // Track depth to find matching "end"
                int depth = 1;
                const char* scan = params_end ? params_end + 1 : paren + 1;
                bool in_string = false;
                char string_char = 0;

                while (*scan && depth > 0) {
                    // Skip strings
                    if (!in_string && (*scan == '"' || *scan == '\'')) {
                        in_string = true;
                        string_char = *scan;
                        scan++;
                        continue;
                    }
                    if (in_string) {
                        if (*scan == '\\' && *(scan+1)) {
                            scan += 2;
                            continue;
                        }
                        if (*scan == string_char) {
                            in_string = false;
                        }
                        scan++;
                        continue;
                    }

                    // Skip comments
                    if (*scan == '-' && *(scan+1) == '-') {
                        while (*scan && *scan != '\n') scan++;
                        continue;
                    }

                    // Track depth
                    if (strncmp(scan, "function", 8) == 0 && !isalnum((unsigned char)*(scan+8))) {
                        depth++;
                        scan += 8;
                        continue;
                    }
                    if (strncmp(scan, "if", 2) == 0 && !isalnum((unsigned char)*(scan+2))) {
                        depth++;
                        scan += 2;
                        continue;
                    }
                    if (strncmp(scan, "for", 3) == 0 && !isalnum((unsigned char)*(scan+3))) {
                        depth++;
                        scan += 3;
                        continue;
                    }
                    if (strncmp(scan, "while", 5) == 0 && !isalnum((unsigned char)*(scan+5))) {
                        depth++;
                        scan += 5;
                        continue;
                    }
                    if (strncmp(scan, "repeat", 6) == 0 && !isalnum((unsigned char)*(scan+6))) {
                        depth++;
                        scan += 6;
                        continue;
                    }
                    if (strncmp(scan, "end", 3) == 0 && !isalnum((unsigned char)*(scan+3)) &&
                        (scan == source || !isalnum((unsigned char)*(scan-1)))) {
                        depth--;
                        if (depth == 0) {
                            // Found the end - capture full source
                            size_t func_len = (scan + 3) - func_pos;
                            char* func_source = strndup(func_pos, func_len);

                            // Create function entry
                            cJSON* fn = cJSON_CreateObject();
                            cJSON_AddStringToObject(fn, "name", name);
                            cJSON_AddStringToObject(fn, "params", params ? params : "");
                            cJSON_AddStringToObject(fn, "source", func_source);
                            cJSON_AddItemToArray(functions, fn);

                            free(func_source);
                            break;
                        }
                        scan += 3;
                        continue;
                    }
                    if (strncmp(scan, "until", 5) == 0 && !isalnum((unsigned char)*(scan+5))) {
                        depth--;
                        scan += 5;
                        continue;
                    }

                    scan++;
                }

                free(name);
                free(params);
                p = scan;
                continue;
            }
        }

        p = func_pos + 1;
    }

    return functions;
}

/**
 * Generate KIR for a plugin from its Lua binding file
 * This creates a proper KIR file describing the plugin's exports without executing it
 */
char* ir_lua_generate_plugin_kir(const char* plugin_name, const char* binding_path) {
    if (!plugin_name || !binding_path) return NULL;

    fprintf(stderr, "  [Plugin KIR] Generating KIR for plugin: %s\n", plugin_name);

    // Read the binding file
    FILE* f = fopen(binding_path, "r");
    if (!f) {
        fprintf(stderr, "  [Plugin KIR] Warning: Could not read binding: %s\n", binding_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);

    // Create KIR structure
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "format", "kir");
    cJSON_AddStringToObject(root, "version", "3.0");

    // Metadata
    cJSON* meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "source_language", "plugin");
    cJSON_AddStringToObject(meta, "module_id", plugin_name);
    cJSON_AddStringToObject(meta, "compiler_version", "0.3.0");
    cJSON_AddStringToObject(meta, "binding_file", binding_path);
    cJSON_AddItemToObject(root, "metadata", meta);

    // No root component (plugin, not UI module)
    cJSON_AddNullToObject(root, "root");

    // Extract functions from the binding
    cJSON* source_decls = cJSON_CreateObject();
    cJSON* functions = extract_plugin_functions(source);
    cJSON_AddItemToObject(source_decls, "functions", functions);
    cJSON_AddItemToObject(root, "source_declarations", source_decls);

    // Build exports list from functions
    cJSON* exports = cJSON_CreateArray();
    cJSON* fn = NULL;
    cJSON_ArrayForEach(fn, functions) {
        cJSON* name_item = cJSON_GetObjectItem(fn, "name");
        cJSON* params_item = cJSON_GetObjectItem(fn, "params");
        if (name_item && cJSON_IsString(name_item)) {
            cJSON* exp = cJSON_CreateObject();
            cJSON_AddStringToObject(exp, "name", cJSON_GetStringValue(name_item));
            cJSON_AddStringToObject(exp, "type", "function");
            if (params_item && cJSON_IsString(params_item)) {
                cJSON_AddStringToObject(exp, "params", cJSON_GetStringValue(params_item));
            }
            cJSON_AddItemToArray(exports, exp);
        }
    }
    cJSON_AddItemToObject(root, "exports", exports);

    free(source);

    // Get export count before serializing (cJSON_Delete will free everything)
    int export_count = cJSON_GetArraySize(exports);

    // Serialize
    char* result = cJSON_Print(root);
    cJSON_Delete(root);

    if (result) {
        fprintf(stderr, "  [Plugin KIR] ✓ Generated %d exports for %s\n",
                export_count, plugin_name);
    }

    return result;
}

/**
 * Generate plugin KIR files from kryon.toml configuration
 * Reads the [plugins] section and generates KIR for each plugin
 */
bool ir_lua_generate_plugin_kirs_from_toml(const char* toml_path, const char* output_dir) {
    if (!toml_path || !output_dir) return false;

    fprintf(stderr, "📦 Generating plugin KIR files from: %s\n", toml_path);

    // Read the toml file
    FILE* f = fopen(toml_path, "r");
    if (!f) {
        fprintf(stderr, "  Warning: Could not read kryon.toml\n");
        return false;
    }

    char errbuf[256];
    toml_table_t* conf = toml_parse_file(f, errbuf, sizeof(errbuf));
    fclose(f);

    if (!conf) {
        fprintf(stderr, "  Warning: Failed to parse kryon.toml: %s\n", errbuf);
        return false;
    }

    // Get [plugins] section
    toml_table_t* plugins = toml_table_in(conf, "plugins");
    if (!plugins) {
        fprintf(stderr, "  No [plugins] section in kryon.toml\n");
        toml_free(conf);
        return true; // Not an error, just no plugins
    }

    // Get the directory containing kryon.toml for relative path resolution
    char toml_dir[4096];
    strncpy(toml_dir, toml_path, sizeof(toml_dir) - 1);
    toml_dir[sizeof(toml_dir) - 1] = '\0';
    char* last_slash = strrchr(toml_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        strcpy(toml_dir, ".");
    }

    int plugins_generated = 0;

    // Iterate through plugins
    for (int i = 0; ; i++) {
        const char* key = toml_key_in(plugins, i);
        if (!key) break;

        // Get plugin config (table with "path" key)
        toml_table_t* plugin_conf = toml_table_in(plugins, key);
        if (!plugin_conf) continue;

        // Get path
        toml_datum_t path_datum = toml_string_in(plugin_conf, "path");
        if (!path_datum.ok) continue;

        const char* plugin_path = path_datum.u.s;

        // Resolve relative path from toml directory
        char full_plugin_path[4096];
        if (plugin_path[0] == '/') {
            strncpy(full_plugin_path, plugin_path, sizeof(full_plugin_path) - 1);
        } else {
            snprintf(full_plugin_path, sizeof(full_plugin_path), "%s/%s", toml_dir, plugin_path);
        }

        // Find the Lua binding file
        char binding_path[4096];
        snprintf(binding_path, sizeof(binding_path), "%s/bindings/lua/%s.lua",
                 full_plugin_path, key);

        // Check if binding file exists
        if (access(binding_path, R_OK) != 0) {
            fprintf(stderr, "  Warning: No Lua binding for plugin %s at %s\n", key, binding_path);
            free(path_datum.u.s);
            continue;
        }

        // Generate KIR for this plugin
        char* plugin_kir = ir_lua_generate_plugin_kir(key, binding_path);
        if (plugin_kir) {
            // Write to output directory
            char kir_path[4096];
            snprintf(kir_path, sizeof(kir_path), "%s/%s.kir", output_dir, key);

            FILE* kir_file = fopen(kir_path, "w");
            if (kir_file) {
                fputs(plugin_kir, kir_file);
                fclose(kir_file);
                fprintf(stderr, "  ✓ Generated plugin KIR: %s.kir\n", key);
                plugins_generated++;
            }
            free(plugin_kir);
        }

        free(path_datum.u.s);
    }

    toml_free(conf);

    fprintf(stderr, "📦 Generated %d plugin KIR files\n", plugins_generated);
    return true;
}
