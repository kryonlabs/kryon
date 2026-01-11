/**
 * Path Discovery and Resolution Utilities
 * Provides dynamic path discovery for Kryon installation
 */

#define _XOPEN_SOURCE 700
#include "kryon_cli.h"
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

/**
 * Get user home directory
 */
char* paths_get_home_dir(void) {
    const char* home = getenv("HOME");
    if (home && strlen(home) > 0) {
        return str_copy(home);
    }

    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return str_copy(pw->pw_dir);
    }

    return NULL;
}

/**
 * Get binary executable path
 */
char* paths_get_binary_path(void) {
    char* path = malloc(PATH_MAX);
    if (!path) return NULL;

    ssize_t len = readlink("/proc/self/exe", path, PATH_MAX - 1);
    if (len == -1) {
        free(path);
        return NULL;
    }

    path[len] = '\0';
    return path;
}

/**
 * Expand ~ in path to home directory
 */
char* paths_expand_home(const char* path) {
    if (!path || path[0] != '~') {
        return str_copy(path);
    }

    char* home = paths_get_home_dir();
    if (!home) {
        return str_copy(path);
    }

    // Handle ~/... or just ~
    const char* suffix = "";
    if (path[1] == '/') {
        suffix = path + 2;  // Skip "~/"
    } else if (path[1] == '\0') {
        suffix = "";
    } else {
        // ~user/... format not supported
        free(home);
        return str_copy(path);
    }

    char* result = path_join(home, suffix);
    free(home);

    return result;
}

/**
 * Get XDG base directory with fallback
 */
static char* get_xdg_dir(const char* env_var, const char* fallback_suffix) {
    const char* env_val = getenv(env_var);
    if (env_val && strlen(env_val) > 0) {
        return str_copy(env_val);
    }

    char* home = paths_get_home_dir();
    if (!home) {
        return NULL;
    }

    char* result = path_join(home, fallback_suffix);
    free(home);
    return result;
}

/**
 * Expand environment variables in path
 * Supports: $HOME, ~, $XDG_DATA_HOME, $XDG_BIN_HOME, $XDG_STATE_HOME
 */
char* path_expand_env_vars(const char* path) {
    if (!path) return NULL;

    char* result = str_copy(path);
    char* home = paths_get_home_dir();

    // Expand $HOME or ~
    if (home) {
        char* pos = result;
        while ((pos = strstr(pos, "$HOME")) != NULL) {
            // Allocate new string with expansion
            size_t prefix_len = pos - result;
            size_t home_len = strlen(home);
            size_t suffix_len = strlen(pos + 5);  // 5 = strlen("$HOME")
            char* new_result = malloc(prefix_len + home_len + suffix_len + 1);
            if (new_result) {
                strncpy(new_result, result, prefix_len);
                strcpy(new_result + prefix_len, home);
                strcpy(new_result + prefix_len + home_len, pos + 5);
                free(result);
                result = new_result;
                pos = result + prefix_len + home_len;
            } else {
                break;
            }
        }

        // Also handle ~ at start or after /
        if (result[0] == '~') {
            char* expanded = paths_expand_home(result);
            free(result);
            result = expanded;
        }
    }

    // Expand $XDG_DATA_HOME (default: ~/.local/share)
    char* xdg_data = get_xdg_dir("XDG_DATA_HOME", ".local/share");
    if (xdg_data) {
        char* pos = result;
        while ((pos = strstr(pos, "$XDG_DATA_HOME")) != NULL) {
            size_t prefix_len = pos - result;
            size_t xdg_len = strlen(xdg_data);
            size_t suffix_len = strlen(pos + 13);  // 13 = strlen("$XDG_DATA_HOME")
            char* new_result = malloc(prefix_len + xdg_len + suffix_len + 1);
            if (new_result) {
                strncpy(new_result, result, prefix_len);
                strcpy(new_result + prefix_len, xdg_data);
                strcpy(new_result + prefix_len + xdg_len, pos + 13);
                free(result);
                result = new_result;
                pos = result + prefix_len + xdg_len;
            } else {
                break;
            }
        }
        free(xdg_data);
    }

    // Expand $XDG_BIN_HOME (default: ~/.local/bin)
    char* xdg_bin = get_xdg_dir("XDG_BIN_HOME", ".local/bin");
    if (xdg_bin) {
        char* pos = result;
        while ((pos = strstr(pos, "$XDG_BIN_HOME")) != NULL) {
            size_t prefix_len = pos - result;
            size_t xdg_len = strlen(xdg_bin);
            size_t suffix_len = strlen(pos + 12);  // 12 = strlen("$XDG_BIN_HOME")
            char* new_result = malloc(prefix_len + xdg_len + suffix_len + 1);
            if (new_result) {
                strncpy(new_result, result, prefix_len);
                strcpy(new_result + prefix_len, xdg_bin);
                strcpy(new_result + prefix_len + xdg_len, pos + 12);
                free(result);
                result = new_result;
                pos = result + prefix_len + xdg_len;
            } else {
                break;
            }
        }
        free(xdg_bin);
    }

    // Expand $XDG_STATE_HOME (default: ~/.local/state)
    char* xdg_state = get_xdg_dir("XDG_STATE_HOME", ".local/state");
    if (xdg_state) {
        char* pos = result;
        while ((pos = strstr(pos, "$XDG_STATE_HOME")) != NULL) {
            size_t prefix_len = pos - result;
            size_t xdg_len = strlen(xdg_state);
            size_t suffix_len = strlen(pos + 14);  // 14 = strlen("$XDG_STATE_HOME")
            char* new_result = malloc(prefix_len + xdg_len + suffix_len + 1);
            if (new_result) {
                strncpy(new_result, result, prefix_len);
                strcpy(new_result + prefix_len, xdg_state);
                strcpy(new_result + prefix_len + xdg_len, pos + 14);
                free(result);
                result = new_result;
                pos = result + prefix_len + xdg_len;
            } else {
                break;
            }
        }
        free(xdg_state);
    }

    // Expand $XDG_CACHE_HOME (default: ~/.cache)
    char* xdg_cache = get_xdg_dir("XDG_CACHE_HOME", ".cache");
    if (xdg_cache) {
        char* pos = result;
        while ((pos = strstr(pos, "$XDG_CACHE_HOME")) != NULL) {
            size_t prefix_len = pos - result;
            size_t xdg_len = strlen(xdg_cache);
            size_t suffix_len = strlen(pos + 13);  // 13 = strlen("$XDG_CACHE_HOME")
            char* new_result = malloc(prefix_len + xdg_len + suffix_len + 1);
            if (new_result) {
                strncpy(new_result, result, prefix_len);
                strcpy(new_result + prefix_len, xdg_cache);
                strcpy(new_result + prefix_len + xdg_len, pos + 13);
                free(result);
                result = new_result;
                pos = result + prefix_len + xdg_len;
            } else {
                break;
            }
        }
        free(xdg_cache);
    }

    if (home) free(home);
    return result;
}

/**
 * Resolve path (absolute, relative, or ~)
 * base_dir: Directory to resolve relative paths against
 */
char* paths_resolve(const char* path, const char* base_dir) {
    if (!path) return NULL;

    // Handle ~ expansion first
    char* expanded = paths_expand_home(path);

    // If absolute, return as-is
    if (expanded[0] == '/') {
        return expanded;
    }

    // Relative path - resolve against base_dir
    if (base_dir && strlen(base_dir) > 0) {
        char* resolved = path_join(base_dir, expanded);
        free(expanded);
        return resolved;
    }

    return expanded;
}

/**
 * Check if directory is a Kryon git repository
 */
static bool is_kryon_git_repo(const char* dir) {
    char* git_config = path_join(dir, ".git/config");

    if (!file_exists(git_config)) {
        free(git_config);
        return false;
    }

    // Read .git/config and check for kryon remote
    char* content = file_read(git_config);
    free(git_config);

    if (!content) return false;

    bool is_kryon = (strstr(content, "kryon") != NULL ||
                     strstr(content, "Kryon") != NULL);
    free(content);

    return is_kryon;
}

/**
 * Walk up directory tree looking for Kryon root
 */
static char* find_kryon_in_parents(const char* start_dir) {
    char current[PATH_MAX];
    strncpy(current, start_dir, PATH_MAX - 1);
    current[PATH_MAX - 1] = '\0';

    // Walk up to root
    for (int depth = 0; depth < 10; depth++) {
        // Check if current dir is Kryon root - be very strict
        // Must have BOTH ir/ directory AND cli/ directory
        // And ir/ must contain ir_core.h (definitive proof it's Kryon)
        char* check_ir = path_join(current, "ir");
        char* check_cli = path_join(current, "cli");
        char* check_ir_core = path_join(current, "ir/ir_core.h");

        bool is_kryon = file_is_directory(check_ir) &&
                        file_is_directory(check_cli) &&
                        file_exists(check_ir_core);

        free(check_ir);
        free(check_cli);
        free(check_ir_core);

        if (is_kryon) {
            return realpath(current, NULL);
        }

        // Move to parent
        char* parent = path_join(current, "..");
        char* resolved = realpath(parent, NULL);
        free(parent);

        if (!resolved || strcmp(resolved, current) == 0) {
            free(resolved);
            break;
        }

        strncpy(current, resolved, PATH_MAX - 1);
        free(resolved);
    }

    return NULL;
}

/**
 * Get Kryon root directory using fallback chain
 */
char* paths_get_kryon_root(void) {
    // 1. Check KRYON_ROOT environment variable
    const char* env_root = getenv("KRYON_ROOT");
    if (env_root && strlen(env_root) > 0) {
        char* root = paths_expand_home(env_root);
        if (file_exists(root)) {
            return root;
        }
        free(root);
    }

    // 2. Search parent directories from current working directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        char* parent_kryon = find_kryon_in_parents(cwd);
        if (parent_kryon) {
            return parent_kryon;
        }
    }

    // 3. Binary location discovery
    char* bin_path = paths_get_binary_path();
    if (bin_path) {
        // Get directory containing binary
        char* bin_dir = str_copy(bin_path);
        char* last_slash = strrchr(bin_dir, '/');
        if (last_slash) {
            *last_slash = '\0';

            // Try parent directory (for installed /usr/local/bin/kryon → /usr/local)
            char* parent = path_join(bin_dir, "..");

            // Check if it looks like Kryon root (has build/ or bindings/)
            char* check_build = path_join(parent, "build");
            char* check_bindings = path_join(parent, "bindings");

            bool is_kryon_root = file_is_directory(check_build) || file_is_directory(check_bindings);

            free(check_build);
            free(check_bindings);

            if (is_kryon_root) {
                // Resolve to canonical path
                char* canonical = realpath(parent, NULL);
                free(parent);
                free(bin_dir);
                free(bin_path);
                return canonical;
            }

            free(parent);
        }
        free(bin_dir);
        free(bin_path);
    }

    // 4. XDG user data directory
    char* home = paths_get_home_dir();
    if (home) {
        char* xdg_path = path_join(home, ".local/share/kryon");
        free(home);

        if (file_exists(xdg_path)) {
            return xdg_path;
        }
        free(xdg_path);
    }

    // 5. System-wide location
    if (file_exists("/usr/local/lib/kryon")) {
        return str_copy("/usr/local/lib/kryon");
    }

    // Not found
    return NULL;
}

/**
 * Find library in standard locations
 */
char* paths_find_library(const char* lib_name) {
    if (!lib_name) return NULL;

    // Build search paths
    char* search_paths[10];
    int idx = 0;

    char* kryon_root = paths_get_kryon_root();
    char* home = paths_get_home_dir();

    if (kryon_root) {
        search_paths[idx++] = path_join(kryon_root, "build");
    }
    if (home) {
        search_paths[idx++] = path_join(home, ".local/lib");
    }
    search_paths[idx++] = str_copy("/usr/local/lib");
    search_paths[idx++] = str_copy("/usr/lib");

    // Search for library
    char* result = NULL;
    for (int i = 0; i < idx; i++) {
        char* lib_path = path_join(search_paths[i], lib_name);
        if (file_exists(lib_path)) {
            result = lib_path;
            break;
        }
        free(lib_path);
    }

    // Cleanup
    for (int i = 0; i < idx; i++) {
        free(search_paths[i]);
    }
    if (kryon_root) free(kryon_root);
    if (home) free(home);

    return result;
}

/**
 * Find plugin directory
 * plugin_name: Name of plugin
 * explicit_path: Path from config (can be NULL)
 * config_dir: Directory containing kryon.toml (for relative path resolution)
 */
char* paths_find_plugin(const char* plugin_name, const char* explicit_path, const char* config_dir) {
    if (!plugin_name) return NULL;

    // If explicit path provided, use it (with resolution)
    if (explicit_path && strlen(explicit_path) > 0) {
        return paths_resolve(explicit_path, config_dir);
    }

    // No auto-discovery - return NULL if no explicit path
    // (Per user requirements: config file paths ONLY)
    return NULL;
}

/**
 * Get build directory (where .so files are located)
 * Checks: KRYON_ROOT/build, ~/.local/lib, /usr/local/lib
 */
char* paths_get_build_path(void) {
    char* kryon_root = paths_get_kryon_root();
    if (kryon_root) {
        char* build_path = path_join(kryon_root, "build");
        free(kryon_root);
        if (file_is_directory(build_path)) {
            return build_path;
        }
        free(build_path);
    }

    // Try user lib directory
    char* home = paths_get_home_dir();
    if (home) {
        char* user_lib = path_join(home, ".local/lib");
        free(home);
        if (file_is_directory(user_lib)) {
            return user_lib;
        }
        free(user_lib);
    }

    // Fallback to system lib
    return str_copy("/usr/local/lib");
}

/**
 * Get bindings directory (where C/Lua bindings are located)
 * Checks: KRYON_ROOT/bindings, ~/.local/share/kryon/bindings, /usr/local/share/kryon/bindings
 */
char* paths_get_bindings_path(void) {
    char* kryon_root = paths_get_kryon_root();
    if (kryon_root) {
        char* bindings_path = path_join(kryon_root, "bindings");
        free(kryon_root);
        if (file_is_directory(bindings_path)) {
            return bindings_path;
        }
        free(bindings_path);
    }

    // Try user share directory
    char* home = paths_get_home_dir();
    if (home) {
        char* user_share = path_join(home, ".local/share/kryon/bindings");
        free(home);
        if (file_is_directory(user_share)) {
            return user_share;
        }
        free(user_share);
    }

    // Fallback to system share
    return str_copy("/usr/local/share/kryon/bindings");
}

/**
 * Get scripts directory (where dev_server.ts is located)
 * Checks: KRYON_ROOT/cli/scripts, ~/.local/share/kryon/scripts, /usr/local/share/kryon/scripts
 */
char* paths_get_scripts_path(void) {
    char* kryon_root = paths_get_kryon_root();
    if (kryon_root) {
        char* scripts_path = path_join(kryon_root, "cli/scripts");
        if (file_is_directory(scripts_path)) {
            return scripts_path;
        }
        free(scripts_path);

        // Also try kryon_root/scripts (for installed structure)
        scripts_path = path_join(kryon_root, "scripts");
        if (file_is_directory(scripts_path)) {
            free(kryon_root);
            return scripts_path;
        }
        free(scripts_path);
        free(kryon_root);
    }

    // Try user share directory
    char* home = paths_get_home_dir();
    if (home) {
        char* user_share = path_join(home, ".local/share/kryon/scripts");
        free(home);
        if (file_is_directory(user_share)) {
            return user_share;
        }
        free(user_share);
    }

    // Fallback to system share
    return str_copy("/usr/local/share/kryon/scripts");
}

/**
 * Get TSX parser path (tsx_to_kir.ts)
 * Checks: KRYON_ROOT/ir/parsers/tsx/, ~/.local/share/kryon/tsx_parser, /usr/local/share/kryon/tsx_parser
 */
char* paths_get_tsx_parser_path(void) {
    // First try KRYON_ROOT
    char* kryon_root = paths_get_kryon_root();
    if (kryon_root) {
        char* tsx_path = path_join(kryon_root, "ir/parsers/tsx/tsx_to_kir.ts");
        if (file_exists(tsx_path)) {
            free(kryon_root);
            return tsx_path;
        }
        free(tsx_path);
        free(kryon_root);
    }

    // Try user share directory
    char* home = paths_get_home_dir();
    if (home) {
        char* user_tsx = path_join(home, ".local/share/kryon/tsx_parser/tsx_to_kir.ts");
        free(home);
        if (file_exists(user_tsx)) {
            return user_tsx;
        }
        free(user_tsx);
    }

    // Fallback to system share
    return str_copy("/usr/local/share/kryon/tsx_parser/tsx_to_kir.ts");
}
