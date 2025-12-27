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
