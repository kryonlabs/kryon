/**
 * Require Tracker for Lua Parser
 * Extracts require() dependencies via static analysis
 */

#define _POSIX_C_SOURCE 200809L

#include "require_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

// Standard Lua library modules
static const char* stdlib_modules[] = {
    "_G", "coroutine", "package", "string", "table",
    "math", "io", "os", "debug", "utf8", "bit32",
    "ffi", "jit", NULL
};

bool lua_is_stdlib_module(const char* module) {
    if (!module) return false;

    // Check if it's a base library module
    for (int i = 0; stdlib_modules[i]; i++) {
        if (strcmp(module, stdlib_modules[i]) == 0) {
            return true;
        }
    }

    // Check if it starts with common stdlib prefixes
    if (strncmp(module, "string.", 7) == 0 ||
        strncmp(module, "table.", 6) == 0 ||
        strncmp(module, "io.", 3) == 0 ||
        strncmp(module, "os.", 3) == 0 ||
        strncmp(module, "math.", 5) == 0 ||
        strncmp(module, "debug.", 6) == 0 ||
        strncmp(module, "utf8.", 5) == 0 ||
        strncmp(module, "bit32.", 6) == 0 ||
        strncmp(module, "jit.", 4) == 0 ||
        strncmp(module, "ffi.", 4) == 0) {
        return true;
    }

    return false;
}

/**
 * Simple regex-like pattern matcher for require() extraction
 * Matches: require "module" or require('module')
 */
static const char* find_require_pattern(const char* source, int* out_len, char* module_buf, size_t buf_size) {
    const char* p = source;

    while (*p) {
        // Look for "require"
        if (strncmp(p, "require", 7) == 0) {
            const char* after = p + 7;

            // Skip whitespace
            while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') {
                after++;
            }

            // Expect opening parenthesis
            if (*after == '(') {
                after++;

                // Skip whitespace after (
                while (*after == ' ' || *after == '\t') {
                    after++;
                }

                // Expect quote (single or double)
                char quote = '\0';
                if (*after == '"' || *after == '\'') {
                    quote = *after;
                    after++;
                } else {
                    p++; // Not a valid require, continue searching
                    continue;
                }

                // Extract module name
                size_t i = 0;
                const char* module_start = after;
                while (*after && *after != quote && i < buf_size - 1) {
                    module_buf[i++] = *after++;
                }
                module_buf[i] = '\0';

                if (*after == quote) {
                    after++; // Skip closing quote

                    // Skip whitespace
                    while (*after == ' ' || *after == '\t') {
                        after++;
                    }

                    // Expect closing parenthesis
                    if (*after == ')') {
                        *out_len = after - p + 1;
                        return p;
                    }
                }
            }
        }
        p++;
    }

    return NULL;
}

RequireGraph* lua_extract_requires(const char* source, const char* source_dir) {
    if (!source) return NULL;

    RequireGraph* graph = calloc(1, sizeof(RequireGraph));
    if (!graph) return NULL;

    graph->capacity = 16;
    graph->dependencies = calloc(graph->capacity, sizeof(RequireDependency));
    if (!graph->dependencies) {
        free(graph);
        return NULL;
    }

    char module_buf[512];
    const char* p = source;

    while (*p) {
        int match_len = 0;
        const char* match = find_require_pattern(p, &match_len, module_buf, sizeof(module_buf));

        if (!match) break;

        // Skip if it's a comment
        // Check for -- before require
        const char* line_start = match;
        while (line_start > source && *(line_start - 1) != '\n') {
            line_start--;
        }

        // Check if require is within a comment
        bool is_comment = false;
        const char* check = line_start;
        while (check < match) {
            if (strncmp(check, "--", 2) == 0) {
                is_comment = true;
                break;
            }
            if (*check == '\n') break;
            check++;
        }

        // Also check for block comment --[[ ... ]]
        const char* before_match = match - 1;
        while (before_match >= source && (*before_match == ' ' || *before_match == '\t')) {
            before_match--;
        }

        if (!is_comment && before_match >= source + 1 &&
            strncmp(before_match - 1, "--", 2) == 0) {
            is_comment = true;
        }

        // Also check for block comments --[[ and --}}
        if (!is_comment) {
            // Look backwards for --[[ before this point
            const char* block_comment_start = match - 3;
            bool in_block_comment = false;
            while (block_comment_start >= source) {
                if (strncmp(block_comment_start, "--[[", 4) == 0) {
                    // Found opening, look for closing before match
                    const char* closing = block_comment_start + 4;
                    while (closing < match) {
                        if (strncmp(closing, "--]]", 4) == 0) {
                            break; // Block closed before match
                        }
                        closing++;
                    }
                    if (closing >= match) {
                        in_block_comment = true;
                    }
                    break;
                }
                if (*block_comment_start == '\n') break;
                block_comment_start--;
            }

            if (in_block_comment) {
                is_comment = true;
            }
        }

        if (!is_comment && strlen(module_buf) > 0) {
            // Determine if this is a relative/standard module
            bool is_stdlib = lua_is_stdlib_module(module_buf);
            bool is_relative = !is_stdlib && (
                strstr(module_buf, "components.") == module_buf ||
                strstr(module_buf, "utils.") == module_buf ||
                strstr(module_buf, ".") != NULL  // Contains a dot, likely local
            );

            lua_require_graph_add(graph, module_buf, NULL, is_relative, is_stdlib);
        }

        p = match + match_len;
    }

    return graph;
}

/**
 * Find Kryon root directory
 */
static char* find_kryon_root(void) {
    static char cached_path[1024] = "";

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
 * Convert dots to slashes in a module name
 * e.g., "components.calendar" -> "components/calendar"
 */
static void dots_to_slashes(char* path) {
    while (*path) {
        if (*path == '.') {
            *path = '/';
        }
        path++;
    }
}

char* lua_resolve_require_path(const char* module, const char* source_dir, const char* kryon_root) {
    if (!module) return NULL;

    char result[1024];
    char module_path[512];

    // Copy and convert dots to slashes
    strncpy(module_path, module, sizeof(module_path) - 1);
    module_path[sizeof(module_path) - 1] = '\0';
    dots_to_slashes(module_path);

    // Check if it's a standard library module (don't resolve)
    if (lua_is_stdlib_module(module)) {
        return strdup(module); // Return as-is for stdlib
    }

    // 1. Try relative to source directory first
    if (source_dir) {
        snprintf(result, sizeof(result), "%s/%s.lua", source_dir, module_path);
        if (access(result, F_OK) == 0) {
            return strdup(result);
        }
    }

    // 2. Try in the same directory with init.lua
    if (source_dir) {
        snprintf(result, sizeof(result), "%s/%s/init.lua", source_dir, module_path);
        if (access(result, F_OK) == 0) {
            return strdup(result);
        }
    }

    // 3. Try Kryon bindings for "kryon.*" modules
    if (strncmp(module, "kryon.", 6) == 0) {
        const char* root = kryon_root ? kryon_root : find_kryon_root();
        if (root) {
            // Remove "kryon." prefix
            const char* rest = module + 6;
            snprintf(result, sizeof(result), "%s/bindings/lua/kryon/%s.lua", root, rest);
            if (access(result, F_OK) == 0) {
                return strdup(result);
            }
        }
    }

    // 4. Try as absolute path from project root (for components.* modules)
    if (source_dir) {
        // Walk up from source_dir to find project root
        char check_path[1024];
        strncpy(check_path, source_dir, sizeof(check_path) - 1);
        check_path[sizeof(check_path) - 1] = '\0';

        for (int depth = 0; depth < 5; depth++) {
            snprintf(result, sizeof(result), "%s/%s.lua", check_path, module_path);
            if (access(result, F_OK) == 0) {
                return strdup(result);
            }

            snprintf(result, sizeof(result), "%s/%s/init.lua", check_path, module_path);
            if (access(result, F_OK) == 0) {
                return strdup(result);
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

    // 5. Try Lua package.path
    const char* package_path = getenv("LUA_PATH");
    if (package_path) {
        // Simple tokenization of package.path
        char* path_copy = strdup(package_path);
        char* saveptr = NULL;
        char* token = strtok_r(path_copy, ";", &saveptr);

        while (token) {
            // Replace ? with module name
            char expanded[1024];
            const char* src = token;
            char* dst = expanded;
            while (*src && dst < expanded + sizeof(expanded) - 1) {
                if (*src == '?') {
                    strncpy(dst, module_path, sizeof(expanded) - (dst - expanded) - 1);
                    dst += strlen(module_path);
                    src++;
                } else {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';

            if (access(expanded, F_OK) == 0) {
                free(path_copy);
                return strdup(expanded);
            }

            token = strtok_r(NULL, ";", &saveptr);
        }

        free(path_copy);
    }

    // Not found
    return NULL;
}

void lua_require_graph_add(RequireGraph* graph, const char* module, const char* source_file,
                           bool is_relative, bool is_stdlib) {
    if (!graph || !module) return;

    // Check for duplicates
    for (int i = 0; i < graph->count; i++) {
        if (strcmp(graph->dependencies[i].module, module) == 0) {
            return; // Already exists
        }
    }

    // Expand if needed
    if (graph->count >= graph->capacity) {
        graph->capacity *= 2;
        RequireDependency* new_deps = realloc(graph->dependencies,
                                               graph->capacity * sizeof(RequireDependency));
        if (!new_deps) return;
        graph->dependencies = new_deps;
    }

    RequireDependency* dep = &graph->dependencies[graph->count++];
    dep->module = strdup(module);
    dep->source_file = source_file ? strdup(source_file) : NULL;
    dep->is_relative = is_relative;
    dep->is_stdlib = is_stdlib;
    dep->visited = false;
}

void lua_require_graph_free(RequireGraph* graph) {
    if (!graph) return;

    for (int i = 0; i < graph->count; i++) {
        free(graph->dependencies[i].module);
        free(graph->dependencies[i].source_file);
    }

    free(graph->dependencies);
    free(graph);
}
