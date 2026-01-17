#define _GNU_SOURCE
#include "lua_bundler.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_logic.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

// Web-compatible shims for Fengari (browser Lua VM)
// Fengari uses Lua 5.3 which has different APIs than Lua 5.1:
// - 'unpack' is now 'table.unpack'
// - 'os' library is not included
// NOTE: In Fengari, use js.new(Constructor, args...) NOT Constructor.new()
// NOTE: Fengari has a funcOff bug with colon method calls - use pcall wrappers
static const char* LUA_OS_SHIM =
    "-- Kryon Web: compatibility shims for Fengari browser environment\n"
    "\n"
    "-- Lua 5.1 compatibility: unpack is table.unpack in Lua 5.2+\n"
    "if not unpack then\n"
    "    unpack = table.unpack\n"
    "end\n"
    "\n"
    "-- Provides os.time() and os.date() using JavaScript Date\n"
    "local js = require(\"js\")\n"
    "local window = js.global\n"
    "local Date = window.Date\n"
    "\n"
    "-- Safe Date method wrapper (handles Fengari funcOff bug)\n"
    "local function safeCall(obj, method, ...)\n"
    "    local args = {...}\n"
    "    local ok, result = pcall(function()\n"
    "        return obj[method](obj, table.unpack(args))\n"
    "    end)\n"
    "    if ok then return result end\n"
    "    return 0\n"
    "end\n"
    "\n"
    "os = os or {}\n"
    "\n"
    "function os.time(t)\n"
    "    local d\n"
    "    if t then\n"
    "        d = js.new(Date, t.year, (t.month or 1) - 1, t.day or 1,\n"
    "                   t.hour or 0, t.min or 0, t.sec or 0)\n"
    "    else\n"
    "        d = js.new(Date)\n"
    "    end\n"
    "    return math.floor(safeCall(d, 'getTime') / 1000)\n"
    "end\n"
    "\n"
    "function os.date(format, time)\n"
    "    local d\n"
    "    if time then\n"
    "        d = js.new(Date, time * 1000)\n"
    "    else\n"
    "        d = js.new(Date)\n"
    "    end\n"
    "\n"
    "    local year = safeCall(d, 'getFullYear')\n"
    "    local month = safeCall(d, 'getMonth') + 1\n"
    "    local day = safeCall(d, 'getDate')\n"
    "    local hour = safeCall(d, 'getHours')\n"
    "    local min = safeCall(d, 'getMinutes')\n"
    "    local sec = safeCall(d, 'getSeconds')\n"
    "    local wday = safeCall(d, 'getDay') + 1\n"
    "\n"
    "    if format == \"*t\" then\n"
    "        return {\n"
    "            year = year,\n"
    "            month = month,\n"
    "            day = day,\n"
    "            hour = hour,\n"
    "            min = min,\n"
    "            sec = sec,\n"
    "            wday = wday,\n"
    "            yday = 0,\n"
    "            isdst = false\n"
    "        }\n"
    "    end\n"
    "\n"
    "    -- Handle common format strings\n"
    "    if format == \"%Y-%m-%d\" then\n"
    "        return string.format(\"%04d-%02d-%02d\", year, month, day)\n"
    "    elseif format == \"%Y\" then\n"
    "        return string.format(\"%d\", year)\n"
    "    elseif format == \"%m\" then\n"
    "        return string.format(\"%02d\", month)\n"
    "    elseif format == \"%d\" then\n"
    "        return string.format(\"%02d\", day)\n"
    "    elseif format == \"%w\" then\n"
    "        return tostring(wday - 1)\n"
    "    elseif format == \"%B %Y\" then\n"
    "        local months = {\"January\", \"February\", \"March\", \"April\", \"May\", \"June\",\n"
    "                        \"July\", \"August\", \"September\", \"October\", \"November\", \"December\"}\n"
    "        return months[month] .. \" \" .. year\n"
    "    end\n"
    "\n"
    "    -- Fallback: formatted date\n"
    "    return string.format(\"%04d-%02d-%02dT%02d:%02d:%02d\", year, month, day, hour, min, sec)\n"
    "end\n"
    "\n"
    "print(\"[Kryon Web] os shim loaded\")\n"
    "\n";

// Read entire file into a string
static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);

    return buffer;
}

// Check if file exists
static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Find a module by name in the bundler
static LuaBundleModule* find_module(LuaBundler* bundler, const char* name) {
    for (int i = 0; i < bundler->module_count; i++) {
        if (strcmp(bundler->modules[i].module_name, name) == 0) {
            return &bundler->modules[i];
        }
    }
    return NULL;
}

// Add a module to the bundler
static LuaBundleModule* add_module(LuaBundler* bundler, const char* name, const char* path, const char* source) {
    if (bundler->module_count >= LUA_BUNDLER_MAX_MODULES) {
        fprintf(stderr, "[LuaBundler] Error: Too many modules\n");
        return NULL;
    }

    LuaBundleModule* mod = &bundler->modules[bundler->module_count++];
    mod->module_name = strdup(name);
    mod->file_path = strdup(path);
    mod->source = strdup(source);
    mod->is_bundled = false;

    return mod;
}

// Convert module name to file path component
// "kryon.reactive" -> "kryon/reactive.lua"
static void module_to_path(const char* module, char* out, size_t out_size) {
    size_t i = 0;
    size_t j = 0;

    while (module[i] && j < out_size - 5) {
        if (module[i] == '.') {
            out[j++] = '/';
        } else {
            out[j++] = module[i];
        }
        i++;
    }

    out[j] = '\0';
    strncat(out, ".lua", out_size - j - 1);
}

// Try to resolve a module name to a file path
static char* resolve_module(LuaBundler* bundler, const char* module_name) {
    char rel_path[512];
    char full_path[1024];

    module_to_path(module_name, rel_path, sizeof(rel_path));

    // Check if this is a kryon.* module
    if (strncmp(module_name, "kryon.", 6) == 0) {
        if (bundler->kryon_bindings_path) {
            // First try _web variant if enabled
            if (bundler->use_web_modules) {
                char web_rel_path[512];
                // Remove .lua, add _web.lua
                size_t len = strlen(rel_path);
                if (len > 4) {
                    memcpy(web_rel_path, rel_path, len - 4);
                    web_rel_path[len - 4] = '\0';
                    strcat(web_rel_path, "_web.lua");
                }

                snprintf(full_path, sizeof(full_path), "%s/%s", bundler->kryon_bindings_path, web_rel_path);
                if (file_exists(full_path)) {
                    return strdup(full_path);
                }
            }

            // Try regular path
            snprintf(full_path, sizeof(full_path), "%s/%s", bundler->kryon_bindings_path, rel_path);
            if (file_exists(full_path)) {
                return strdup(full_path);
            }
        }
    }

    // Check if this is a storage module
    if (strcmp(module_name, "storage") == 0) {
        if (bundler->use_web_modules && bundler->kryon_bindings_path) {
            snprintf(full_path, sizeof(full_path), "%s/storage_web.lua", bundler->kryon_bindings_path);
            if (file_exists(full_path)) {
                return strdup(full_path);
            }
        }
    }

    // Try each search path
    for (int i = 0; i < bundler->search_path_count; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s", bundler->search_paths[i], rel_path);
        if (file_exists(full_path)) {
            return strdup(full_path);
        }

        // Also try init.lua for directories
        char init_path[512];
        module_to_path(module_name, init_path, sizeof(init_path) - 10);
        // Remove .lua, add /init.lua
        size_t len = strlen(init_path);
        if (len > 4) {
            init_path[len - 4] = '\0';
            strcat(init_path, "/init.lua");
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", bundler->search_paths[i], init_path);
        if (file_exists(full_path)) {
            return strdup(full_path);
        }
    }

    return NULL;
}

// Extract require() calls from Lua source
// This is a simple pattern-based parser, not a full Lua parser
static void extract_requires(const char* source, char requires[][256], int* count, int max_requires) {
    *count = 0;
    const char* p = source;

    while (*p && *count < max_requires) {
        // Look for require( or require "
        if (strncmp(p, "require", 7) == 0) {
            p += 7;

            // Skip whitespace
            while (*p && isspace(*p)) p++;

            // Check for ( or string delimiter
            char delim = 0;
            if (*p == '(') {
                p++;
                while (*p && isspace(*p)) p++;
                if (*p == '"' || *p == '\'') {
                    delim = *p;
                    p++;
                }
            } else if (*p == '"' || *p == '\'') {
                delim = *p;
                p++;
            }

            if (delim) {
                // Extract module name
                char* start = (char*)p;
                while (*p && *p != delim) p++;

                if (*p == delim) {
                    size_t len = p - start;
                    if (len < 256) {
                        strncpy(requires[*count], start, len);
                        requires[*count][len] = '\0';
                        (*count)++;
                    }
                }
            }
        }
        p++;
    }
}

// Strip trailing 'return' statement from Lua source
// In web context, we can't have 'return' in the middle of the file
// since we add code after the main source
static void strip_return_statement(char* source) {
    if (!source) return;

    size_t len = strlen(source);
    if (len == 0) return;

    // Find the last occurrence of "return" that's followed by whitespace and an identifier
    // Search backwards from the end
    char* p = source + len - 1;

    // Skip trailing whitespace
    while (p > source && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p--;
    }

    // Now look for a return statement before the trailing content
    // Pattern: "\nreturn <identifier>" or similar at end
    while (p > source + 7) {  // Need at least 7 chars for "\nreturn" + 1
        // Check if we found "\nreturn"
        if (strncmp(p - 7, "\nreturn", 7) == 0) {
            // Found a return statement - strip from "\nreturn" to end
            *(p - 7) = '\0';
            return;
        }
        // Move back and continue searching
        p--;
    }
}

// Extract local function definitions from Lua source
// Pattern: "local function name(...)" or "local name = function(...)"
// Returns array of function names
static void extract_local_functions(const char* source, char functions[][256], int* count, int max_funcs) {
    *count = 0;
    const char* p = source;

    while (*p && *count < max_funcs) {
        // Look for "local function" pattern
        if (strncmp(p, "local function", 14) == 0) {
            p += 14;  // Skip "local function"

            // Skip whitespace
            while (*p && isspace(*p)) p++;

            // Extract function name
            char* start = (char*)p;
            while (*p && isalnum(*p)) p++;

            if (p > start) {
                size_t len = p - start;
                if (len < 256) {
                    strncpy(functions[*count], start, len);
                    functions[*count][len] = '\0';
                    (*count)++;
                }
            }
        }
        // Look for "local name = function" pattern
        else if (strncmp(p, "local ", 6) == 0) {
            const char* after_local = p + 6;

            // Skip whitespace after "local"
            while (*after_local && isspace(*after_local)) after_local++;

            // Extract potential name
            char* start = (char*)after_local;
            while (*after_local && isalnum(*after_local)) after_local++;

            if (after_local > start) {
                // Check if followed by " = function" or "=function"
                const char* after_name = after_local;
                while (*after_name && isspace(*after_name)) after_name++;

                if (strncmp(after_name, "=", 1) == 0) {
                    after_name++;
                    while (*after_name && isspace(*after_name)) after_name++;

                    if (strncmp(after_name, "function", 8) == 0) {
                        size_t len = after_name - start;
                        if (len < 256) {
                            strncpy(functions[*count], start, len);
                            functions[*count][len] = '\0';
                            (*count)++;
                        }
                    }
                }
            }
            p = after_local;
        }
        // Look for "_G.name = function" pattern (global function assignments)
        else if (strncmp(p, "_G.", 3) == 0) {
            const char* after_g = p + 3;

            // Extract function name after "_G."
            char* start = (char*)after_g;
            while (*after_g && (isalnum(*after_g) || *after_g == '_')) after_g++;

            if (after_g > start) {
                size_t name_len = after_g - start;

                // Check if followed by " = function" or "=function"
                const char* after_name = after_g;
                while (*after_name && isspace(*after_name)) after_name++;

                if (strncmp(after_name, "=", 1) == 0) {
                    after_name++;
                    while (*after_name && isspace(*after_name)) after_name++;

                    if (strncmp(after_name, "function", 8) == 0) {
                        if (name_len < 256) {
                            strncpy(functions[*count], start, name_len);
                            functions[*count][name_len] = '\0';
                            (*count)++;
                        }
                    }
                }
            }
            p = after_g;
        } else {
            p++;
        }
    }
}

// Recursively bundle a module and its dependencies
static bool bundle_module(LuaBundler* bundler, const char* module_name) {
    // Skip if already bundled
    LuaBundleModule* existing = find_module(bundler, module_name);
    if (existing && existing->is_bundled) {
        return true;
    }

    // Skip built-in modules that Fengari provides
    if (strcmp(module_name, "js") == 0 ||
        strcmp(module_name, "ffi") == 0 ||
        strcmp(module_name, "string") == 0 ||
        strcmp(module_name, "table") == 0 ||
        strcmp(module_name, "math") == 0 ||
        strcmp(module_name, "os") == 0 ||
        strcmp(module_name, "io") == 0 ||
        strcmp(module_name, "bit") == 0) {
        return true;
    }

    // Resolve module path
    char* path = resolve_module(bundler, module_name);
    if (!path) {
        fprintf(stderr, "[LuaBundler] Warning: Could not resolve module '%s'\n", module_name);
        return false;
    }

    // Read source
    char* source = read_file(path);
    if (!source) {
        fprintf(stderr, "[LuaBundler] Error: Could not read '%s'\n", path);
        free(path);
        return false;
    }

    printf("[LuaBundler] Bundling: %s -> %s\n", module_name, path);

    // Add to modules list
    LuaBundleModule* mod;
    if (existing) {
        mod = existing;
        free(mod->source);
        mod->source = source;
    } else {
        mod = add_module(bundler, module_name, path, source);
    }
    free(path);

    if (!mod) {
        free(source);
        return false;
    }

    mod->is_bundled = true;

    // Extract and bundle dependencies
    char requires[64][256];
    int require_count;
    extract_requires(source, requires, &require_count, 64);

    for (int i = 0; i < require_count; i++) {
        bundle_module(bundler, requires[i]);
    }

    return true;
}

// ============================================================================
// Public API
// ============================================================================

LuaBundler* lua_bundler_create(void) {
    LuaBundler* bundler = calloc(1, sizeof(LuaBundler));
    if (!bundler) return NULL;

    bundler->use_web_modules = true;  // Default to web modules
    bundler->local_function_count = 0;  // Initialize local functions
    return bundler;
}

void lua_bundler_destroy(LuaBundler* bundler) {
    if (!bundler) return;

    for (int i = 0; i < bundler->module_count; i++) {
        free(bundler->modules[i].module_name);
        free(bundler->modules[i].file_path);
        free(bundler->modules[i].source);
    }

    for (int i = 0; i < bundler->search_path_count; i++) {
        free(bundler->search_paths[i]);
    }

    if (bundler->kryon_bindings_path) {
        free(bundler->kryon_bindings_path);
    }

    if (bundler->project_name) {
        free(bundler->project_name);
    }

    free(bundler);
}

void lua_bundler_add_search_path(LuaBundler* bundler, const char* path) {
    if (!bundler || !path) return;
    if (bundler->search_path_count >= 16) return;

    bundler->search_paths[bundler->search_path_count++] = strdup(path);
}

void lua_bundler_set_kryon_path(LuaBundler* bundler, const char* path) {
    if (!bundler) return;

    if (bundler->kryon_bindings_path) {
        free(bundler->kryon_bindings_path);
    }

    bundler->kryon_bindings_path = path ? strdup(path) : NULL;
}

void lua_bundler_use_web_modules(LuaBundler* bundler, bool enable) {
    if (!bundler) return;
    bundler->use_web_modules = enable;
}

void lua_bundler_set_project_name(LuaBundler* bundler, const char* name) {
    if (!bundler) return;

    if (bundler->project_name) {
        free(bundler->project_name);
    }

    bundler->project_name = name ? strdup(name) : NULL;
}

// lua_bundler_set_event_ids removed - see KRYON_EVENT_IDS_REMOVED.md
char* lua_bundler_bundle(LuaBundler* bundler, const char* main_file) {
    if (!bundler || !main_file) return NULL;

    // Read main file
    char* main_source = read_file(main_file);
    if (!main_source) {
        fprintf(stderr, "[LuaBundler] Error: Could not read main file '%s'\n", main_file);
        return NULL;
    }

    // Strip trailing 'return' statement - we can't have return in the middle
    // of the file since we add code after the main source (namespace export)
    strip_return_statement(main_source);

    // Extract and bundle all dependencies
    char requires[64][256];
    int require_count;
    extract_requires(main_source, requires, &require_count, 64);

    for (int i = 0; i < require_count; i++) {
        bundle_module(bundler, requires[i]);
    }

    // Calculate total size (include os shim for Fengari compatibility)
    // Extra space for: os shim, module headers, pcall wrapper (~2KB), safety margin
    size_t os_shim_len = strlen(LUA_OS_SHIM);
    size_t total_size = strlen(main_source) + os_shim_len + 8192;  // Main + shim + header + wrapper
    for (int i = 0; i < bundler->module_count; i++) {
        total_size += strlen(bundler->modules[i].source) + 256;  // Source + wrapper
    }

    // Allocate output buffer
    char* output = malloc(total_size);
    if (!output) {
        free(main_source);
        return NULL;
    }

    output[0] = '\0';

    // Inject os shim FIRST - Fengari doesn't have os library
    strcat(output, LUA_OS_SHIM);

    // __KRYON_EVENT_IDS__ injection removed - direct index mapping is used instead
    // Handler index N maps directly to event ID N (both use sequential counters
    // in the same code execution order)

    // Generate package.preload entries for dependencies
    strcat(output, "-- Bundled Lua modules for Kryon Web\n");
    strcat(output, "-- Auto-generated by LuaBundler\n\n");

    for (int i = 0; i < bundler->module_count; i++) {
        LuaBundleModule* mod = &bundler->modules[i];

        // Check if this module shares a file path with an earlier module
        // If so, create an alias instead of duplicating the code
        const char* alias_to = NULL;
        for (int j = 0; j < i; j++) {
            if (bundler->modules[j].file_path && mod->file_path &&
                strcmp(bundler->modules[j].file_path, mod->file_path) == 0) {
                alias_to = bundler->modules[j].module_name;
                break;
            }
        }

        char header[512];
        if (alias_to) {
            // Generate an alias: package.preload["x"] = function() return require("y") end
            snprintf(header, sizeof(header),
                "package.preload[\"%s\"] = function() return require(\"%s\") end\n\n",
                mod->module_name, alias_to);
            strcat(output, header);
        } else {
            // Generate full module code
            snprintf(header, sizeof(header),
                "package.preload[\"%s\"] = function()\n",
                mod->module_name);
            strcat(output, header);
            strcat(output, mod->source);
            strcat(output, "\nend\n\n");
        }
    }

    // Auto-export local functions to app namespace (if project_name is set)
    // This allows event handlers to call these functions via namespace (e.g., habits.navigateMonth)
    if (bundler->project_name) {
        // Extract and store local functions from main source for later use in handler code generation
        extract_local_functions(main_source, bundler->local_functions, &bundler->local_function_count, 64);
    }

    // Run main code directly at module scope
    // NOTE: We cannot wrap main code in a function or do-block because that would
    // create a new scope and make local functions inaccessible to handlers.
    // Local functions in Lua are only accessible within the scope they're defined.
    // By running main code at module scope, local functions remain accessible.
    strcat(output, "-- Main application code\n");
    strcat(output, main_source);
    strcat(output, "\n\n");

    // Signal Lua initialization is complete
    strcat(output, "-- Signal initialization complete\n");
    strcat(output, "local js = require(\"js\")\n");
    strcat(output, "local window = js.global\n");
    strcat(output, "if window and window.kryon_lua_init_complete then\n");
    strcat(output, "    window.kryon_lua_init_complete()\n");
    strcat(output, "end\n\n");

    // Export local functions to app namespace AFTER main code runs (so functions are defined)
    // Uses local table instead of _G to avoid global namespace pollution
    if (bundler && bundler->project_name && bundler->local_function_count > 0) {
        strcat(output, "-- Auto-export local functions to app namespace (no _G pollution)\n");
        strcat(output, "local __kryon_app__ = {}\n");
        char namespace_line[512];

        // Export each local function to namespace (functions are now defined after main ran)
        for (int i = 0; i < bundler->local_function_count; i++) {
            snprintf(namespace_line, sizeof(namespace_line),
                    "__kryon_app__.%s = %s or __kryon_app__.%s\n",
                    bundler->local_functions[i], bundler->local_functions[i],
                    bundler->local_functions[i]);
            strcat(output, namespace_line);
        }

        strcat(output, "\n");
    }

    free(main_source);
    return output;
}

// Handler collection for generating registry
typedef struct {
    uint32_t component_id;
    char* code;
} CollectedHandler;

static CollectedHandler* g_handlers = NULL;
static size_t g_handler_count = 0;
static size_t g_handler_capacity = 0;

// Add web-specific wrapper for handlers that use closure variables
// This is target-specific codegen - the wrapper is only added for web output
static char* add_web_wrapper(const char* code, IRHandlerSource* handler_source) {
    if (!code || !handler_source || !handler_source->uses_closures) {
        return strdup(code);
    }

    // Calculate the wrapper size
    size_t code_len = strlen(code);
    size_t wrapper_len = code_len + 512;  // Extra space for wrapper code

    char* wrapped = malloc(wrapper_len);
    if (!wrapped) return strdup(code);

    // Build the wrapper with closure variable extraction
    int offset = 0;

    // Add opening function and js require
    offset += snprintf(wrapped + offset, wrapper_len - offset,
        "function()\n"
        "  local js = require(\"js\")\n"
        "  local element = js.global.__kryon_get_event_element()\n"
        "  local data = element and (element.data or (element.getData and element:getData()))\n"
    );

    // Add closure variable extraction based on metadata
    for (int i = 0; i < handler_source->closure_var_count; i++) {
        const char* var = handler_source->closure_vars[i];
        if (strcmp(var, "habitIndex") == 0) {
            offset += snprintf(wrapped + offset, wrapper_len - offset,
                "  local habitIndex = tonumber(data and data.habit)\n");
        } else if (strcmp(var, "day.date") == 0) {
            offset += snprintf(wrapped + offset, wrapper_len - offset,
                "  local dateStr = data and data.date\n");
        }
        // Add more closure variable types as needed
    }

    // Add the original handler code
    offset += snprintf(wrapped + offset, wrapper_len - offset, "%s\n", code);

    // Close the wrapper function
    offset += snprintf(wrapped + offset, wrapper_len - offset, "end");

    return wrapped;
}

static void collect_handler_from_kir(uint32_t component_id, IRHandlerSource* handler_source) {
    if (!handler_source || !handler_source->code) return;

    // Grow array if needed
    if (g_handler_count >= g_handler_capacity) {
        size_t new_capacity = g_handler_capacity == 0 ? 64 : g_handler_capacity * 2;
        CollectedHandler* new_handlers = realloc(g_handlers, new_capacity * sizeof(CollectedHandler));
        if (!new_handlers) return;
        g_handlers = new_handlers;
        g_handler_capacity = new_capacity;
    }

    g_handlers[g_handler_count].component_id = component_id;

    // Add web wrapper if handler uses closures (target-specific codegen)
    g_handlers[g_handler_count].code = add_web_wrapper(handler_source->code, handler_source);
    g_handler_count++;
}

// Walk KIR tree and collect handlers from events
static void extract_handlers_from_kir(IRComponent* comp) {
    if (!comp) return;

    // Check events for handler_source
    IREvent* event = comp->events;
    while (event) {
        if (event->handler_source && event->handler_source->code) {
            collect_handler_from_kir(comp->id, event->handler_source);
        }
        event = event->next;
    }

    // Recurse into children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        extract_handlers_from_kir(comp->children[i]);
    }
}

// Generate handler registry code
static char* generate_handler_registry(LuaBundler* bundler) {
    if (g_handler_count == 0) return NULL;

    // Estimate size: header + handlers + dispatcher + namespace expansion
    size_t estimated_size = 2048;  // Base overhead
    for (size_t i = 0; i < g_handler_count; i++) {
        // Add extra space for namespace prefixing (e.g., "__kryon_app__." before each function call)
        size_t code_len = strlen(g_handlers[i].code);
        size_t namespace_extra = 0;
        if (bundler && bundler->project_name && bundler->local_function_count > 0) {
            // Worst case: every function name gets prefixed with "__kryon_app__."
            namespace_extra = bundler->local_function_count * 16 * 2;  // "__kryon_app__." is ~14 chars
        }
        estimated_size += code_len + namespace_extra + 64;
    }

    char* registry = malloc(estimated_size);
    if (!registry) return NULL;

    registry[0] = '\0';

    // Import js module for window access
    strcat(registry, "\n-- Handler registry (generated from KIR handler_source)\n");
    strcat(registry, "local __kryon_handlers__ = {}\n");

    // Add each handler
    for (size_t i = 0; i < g_handler_count; i++) {
        char entry[32];
        snprintf(entry, sizeof(entry), "__kryon_handlers__[%u] = ", g_handlers[i].component_id);
        strcat(registry, entry);

        // If project_name is set, prefix local function calls with __kryon_app__
        if (bundler && bundler->project_name && bundler->local_function_count > 0) {
            const char* code = g_handlers[i].code;
            char* processed_code = strdup(code);
            char* temp = processed_code;

            // Replace each local function call with namespaced version
            // This is a simple replacement - it handles cases like "navigateMonth("
            for (int j = 0; j < bundler->local_function_count; j++) {
                const char* func_name = bundler->local_functions[j];
                size_t func_len = strlen(func_name);
                const char* namespace_prefix = "__kryon_app__";
                size_t namespace_len = strlen(namespace_prefix);

                // We need to find and replace func_name with __kryon_app__.func_name
                // Build the replacement string: "__kryon_app__.func_name"
                char* replacement = malloc(namespace_len + func_len + 2);
                snprintf(replacement, namespace_len + func_len + 2, "%s.%s",
                         namespace_prefix, func_name);

                // Simple search and replace (only when followed by '(' or end of string)
                char* found = strstr(temp, func_name);
                while (found) {
                    // Check if this looks like a function call (followed by '(' or whitespace + '(')
                    char next_char = found[func_len];
                    bool is_call = (next_char == '(') ||
                                   (next_char == ' ' && found[func_len + 1] == '(');

                    if (is_call) {
                        // Check it's not already namespaced (preceded by '.')
                        if (found == processed_code || found[-1] != '.') {
                            // Allocate new buffer with room for namespace
                            size_t prefix_len = found - temp;
                            size_t suffix_len = strlen(found + func_len);
                            char* new_buf = malloc(prefix_len + strlen(replacement) + suffix_len + 1);

                            strncpy(new_buf, temp, prefix_len);
                            strcpy(new_buf + prefix_len, replacement);
                            strcpy(new_buf + prefix_len + strlen(replacement), found + func_len);

                            free(temp);
                            temp = new_buf;
                            found = temp + prefix_len + strlen(replacement);
                        } else {
                            found += func_len;
                        }
                    } else {
                        found += func_len;
                    }
                    found = strstr(found, func_name);
                }

                free(replacement);
            }

            strcat(registry, temp);
            free(temp);
        } else {
            // No namespace - use original code
            strcat(registry, g_handlers[i].code);
        }
        strcat(registry, "\n");
    }

    // Add dispatcher function
    strcat(registry, "\n-- Dispatch function called by JavaScript\n");
    strcat(registry, "function kryonCallHandler(componentId)\n");
    strcat(registry, "  local handler = __kryon_handlers__[componentId]\n");
    strcat(registry, "  if handler then\n");
    strcat(registry, "    local success, err = pcall(handler)\n");
    strcat(registry, "    if not success then\n");
    strcat(registry, "      print('[Kryon] Handler error:', err)\n");
    strcat(registry, "    end\n");
    strcat(registry, "  end\n");
    strcat(registry, "end\n");

    // Export to JavaScript
    strcat(registry, "\n-- Export to JavaScript (Fengari passes 'this' as first arg)\n");
    strcat(registry, "local js = require(\"js\")\n");
    strcat(registry, "local window = js.global\n");
    strcat(registry, "window.kryonLuaCallHandler = function(this, componentId)\n");
    strcat(registry, "  kryonCallHandler(componentId)\n");
    strcat(registry, "end\n");

    return registry;
}

// Clean up collected handlers
static void cleanup_handlers(void) {
    for (size_t i = 0; i < g_handler_count; i++) {
        free(g_handlers[i].code);
    }
    free(g_handlers);
    g_handlers = NULL;
    g_handler_count = 0;
    g_handler_capacity = 0;
}

char* lua_bundler_generate_script(LuaBundler* bundler, const char* main_file, IRComponent* kir_root) {
    char* lua_code = lua_bundler_bundle(bundler, main_file);
    if (!lua_code) return NULL;

    // Extract handlers from KIR tree
    if (kir_root) {
        extract_handlers_from_kir(kir_root);
    }

    // Generate handler registry (pass bundler for namespace prefixing)
    char* handler_registry = generate_handler_registry(bundler);

    // Calculate sizes
    size_t lua_len = strlen(lua_code);
    size_t registry_len = handler_registry ? strlen(handler_registry) : 0;
    size_t script_prefix_len = strlen("<script type=\"application/lua\">\n");
    size_t script_suffix_len = strlen("\n</script>\n");

    char* output = malloc(script_prefix_len + lua_len + registry_len + script_suffix_len + 1);
    if (!output) {
        free(lua_code);
        free(handler_registry);
        cleanup_handlers();
        return NULL;
    }

    strcpy(output, "<script type=\"application/lua\">\n");
    strcat(output, lua_code);
    if (handler_registry) {
        strcat(output, handler_registry);
        free(handler_registry);
    }
    strcat(output, "\n</script>\n");

    free(lua_code);
    cleanup_handlers();
    return output;
}

// ============================================================================
// Self-Contained KIR Generation (Phase 1: No External File References)
// ============================================================================

// Generate script from KIR only - NO external file reading
// All handler code comes from IRHandlerSource embedded in the KIR
char* lua_bundler_generate_from_kir(LuaBundler* bundler, IRComponent* kir_root) {
    if (!bundler || !kir_root) {
        fprintf(stderr, "[LuaBundler] Error: bundler and kir_root are required\n");
        return NULL;
    }

    printf("[LuaBundler] Generating script from KIR (self-contained, no file reading)\n");

    // Extract handlers from KIR tree
    extract_handlers_from_kir(kir_root);
    printf("[LuaBundler] Extracted %zu handlers from KIR\n", g_handler_count);

    if (g_handler_count == 0) {
        printf("[LuaBundler] Warning: No handlers found in KIR\n");
    }

    // Generate handler registry (pass bundler for namespace prefixing)
    char* handler_registry = generate_handler_registry(bundler);

    // Calculate total size needed
    // OS shim + init signal + handler registry + script wrapper
    size_t os_shim_len = strlen(LUA_OS_SHIM);
    size_t registry_len = handler_registry ? strlen(handler_registry) : 0;
    size_t script_prefix_len = strlen("<script type=\"application/lua\">\n");
    size_t script_suffix_len = strlen("\n</script>\n");

    // Additional runtime code for web-only execution
    const char* init_complete_code =
        "\n-- Signal Lua initialization complete (no bundled main code)\n"
        "local js = require(\"js\")\n"
        "local window = js.global\n"
        "if window and window.kryon_lua_init_complete then\n"
        "    window.kryon_lua_init_complete()\n"
        "end\n\n";

    // Header comment explaining self-contained mode
    const char* header_comment =
        "-- Kryon Web: Self-Contained KIR Mode\n"
        "-- Handler code extracted from IRHandlerSource in KIR\n"
        "-- No external .lua files were read to generate this script\n\n";

    size_t header_len = strlen(header_comment);
    size_t init_complete_len = strlen(init_complete_code);

    size_t total_size = script_prefix_len + header_len + os_shim_len +
                        init_complete_len + registry_len + script_suffix_len + 1;

    char* output = malloc(total_size);
    if (!output) {
        free(handler_registry);
        cleanup_handlers();
        return NULL;
    }

    // Build the output
    output[0] = '\0';
    strcat(output, "<script type=\"application/lua\">\n");
    strcat(output, header_comment);
    strcat(output, LUA_OS_SHIM);  // Fengari compatibility shims

    // In self-contained mode, we don't bundle the main app code
    // The component tree is already rendered as HTML, and handlers are in the registry
    strcat(output, init_complete_code);

    if (handler_registry) {
        strcat(output, handler_registry);
        free(handler_registry);
    }

    strcat(output, "\n</script>\n");

    printf("[LuaBundler] Generated self-contained script (%zu bytes)\n", strlen(output));

    cleanup_handlers();
    return output;
}
