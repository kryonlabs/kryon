/**
 * Plugin Installer - Git cloning and plugin installation
 *
 * Handles automatic installation of plugins from:
 * - Git URLs (clone to cache)
 * - Local paths (compile and install)
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

// ============================================================================
// Plugin Cache Directory
// ============================================================================

/**
 * Get the plugin cache directory
 * Returns allocated string that must be freed by caller
 */
static char* get_plugin_cache_dir(void) {
    // Try XDG_CACHE_HOME first, then ~/.cache
    char* xdg_cache = getenv("XDG_CACHE_HOME");
    char* cache_dir;

    if (xdg_cache && xdg_cache[0] != '\0') {
        cache_dir = path_join(xdg_cache, "kryon/plugins");
    } else {
        char* home = getenv("HOME");
        if (!home) {
            home = getenv("USERPROFILE");
        }
        if (!home) {
            return NULL;
        }
        // ~/.cache/kryon/plugins
        char* cache_base = path_join(home, ".cache/kryon");
        cache_dir = path_join(cache_base, "plugins");
        free(cache_base);
    }

    return cache_dir;
}

/**
 * Ensure plugin cache directory exists
 * Returns true on success, false on failure
 */
static bool ensure_plugin_cache_dir(void) {
    char* cache_dir = get_plugin_cache_dir();
    if (!cache_dir) return false;

    // Create directory if it doesn't exist
    int result = mkdir(cache_dir, 0755);
    bool success = (result == 0 || errno == EEXIST);

    free(cache_dir);
    return success;
}

/**
 * Get the cached path for a plugin
 * Returns allocated string that must be freed by caller
 */
static char* get_plugin_cached_path(const char* plugin_name) {
    char* cache_dir = get_plugin_cache_dir();
    if (!cache_dir) return NULL;

    char* cached_path = path_join(cache_dir, plugin_name);
    free(cache_dir);

    return cached_path;
}

// ============================================================================
// File Utilities
// ============================================================================

/**
 * Copy a file from src to dst
 */
static int copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return -1;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

/**
 * Create a directory and all parents (like mkdir -p)
 */
static int mkdir_p(const char* path) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!file_is_directory(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }

    if (!file_is_directory(tmp)) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    return 0;
}

// ============================================================================
// Git Cloning
// ============================================================================

/**
 * Check if git is available on the system
 */
static bool git_is_available(void) {
    return (system("git --version > /dev/null 2>&1") == 0);
}

/**
 * Clone a git repository to the plugin cache
 *
 * @param git_url The git URL to clone
 * @param branch The branch to checkout (NULL for default)
 * @param plugin_name The name of the plugin (for cache directory)
 * @return The path to the cloned repository (must be freed), or NULL on failure
 */
char* plugin_clone_from_git(const char* git_url, const char* branch, const char* plugin_name) {
    if (!git_url || !plugin_name) {
        fprintf(stderr, "[kryon][plugin] Invalid git URL or plugin name\n");
        return NULL;
    }

    if (!git_is_available()) {
        fprintf(stderr, "[kryon][plugin] Git is not available. Please install git.\n");
        return NULL;
    }

    // Ensure cache directory exists
    if (!ensure_plugin_cache_dir()) {
        fprintf(stderr, "[kryon][plugin] Failed to create plugin cache directory\n");
        return NULL;
    }

    char* cached_path = get_plugin_cached_path(plugin_name);
    if (!cached_path) {
        fprintf(stderr, "[kryon][plugin] Failed to get cached path for plugin '%s'\n", plugin_name);
        return NULL;
    }

    // Check if already cloned
    if (file_is_directory(cached_path)) {
        printf("[kryon][plugin] Plugin '%s' already cached at %s\n", plugin_name, cached_path);
        // Try to pull latest changes
        char pull_cmd[1024];
        snprintf(pull_cmd, sizeof(pull_cmd), "cd \"%s\" && git pull > /dev/null 2>&1", cached_path);
        system(pull_cmd);
        return cached_path;
    }

    // Clone the repository
    printf("[kryon][plugin] Cloning plugin '%s' from %s\n", plugin_name, git_url);

    char clone_cmd[1024];
    if (branch && branch[0] != '\0') {
        snprintf(clone_cmd, sizeof(clone_cmd),
                 "git clone --depth 1 --branch %s \"%s\" \"%s\" 2>&1",
                 branch, git_url, cached_path);
    } else {
        snprintf(clone_cmd, sizeof(clone_cmd),
                 "git clone --depth 1 \"%s\" \"%s\" 2>&1",
                 git_url, cached_path);
    }

    int result = system(clone_cmd);
    if (result != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to clone plugin '%s' (exit code %d)\n",
                plugin_name, result);
        free(cached_path);
        return NULL;
    }

    printf("[kryon][plugin] Successfully cloned plugin '%s'\n", plugin_name);
    return cached_path;
}

/**
 * Compile a plugin from source
 *
 * @param plugin_path Path to the plugin directory
 * @param plugin_name Name of the plugin
 * @return 0 on success, non-zero on failure
 */
int plugin_compile_from_source(const char* plugin_path, const char* plugin_name) {
    // Check if .so already exists
    char* so_path = path_join(plugin_path, "libkryon_");
    size_t base_len = strlen(so_path);
    char* full_so_path = (char*)malloc(base_len + strlen(plugin_name) + 4);
    if (full_so_path) {
        sprintf(full_so_path, "%s%s.so", so_path, plugin_name);
        free(so_path);

        if (file_exists(full_so_path)) {
            free(full_so_path);
            return 0;  // Already compiled
        }
        free(full_so_path);
    } else {
        free(so_path);
    }

    // Compile the plugin
    printf("[kryon][plugin] Compiling plugin '%s' from source...\n", plugin_name);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "make -C \"%s\" 2>&1", plugin_path);

    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to compile plugin '%s' (exit code %d)\n",
                plugin_name, result);
        return -1;
    }

    printf("[kryon][plugin] Successfully compiled plugin '%s'\n", plugin_name);
    return 0;
}

/**
 * Install a git-cloned plugin from cache to discovery path.
 * This copies the compiled .so and plugin.toml to ~/.local/share/kryon/plugins/<name>/
 *
 * @param plugin_name The plugin name
 * @param cache_dir The cache directory where the plugin was cloned
 * @return 0 on success, non-zero on failure
 */
static int plugin_install_from_cache(const char* plugin_name, const char* cache_dir) {
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[kryon][plugin] HOME not set\n");
        return -1;
    }

    /* Source paths in cache */
    char src_so[1024], src_toml[1024];
    snprintf(src_so, sizeof(src_so), "%s/build/libkryon_%s.so", cache_dir, plugin_name);
    snprintf(src_toml, sizeof(src_toml), "%s/plugin.toml", cache_dir);

    /* Destination: ~/.local/share/kryon/plugins/<plugin_name>/ */
    char dest_dir[1024];
    snprintf(dest_dir, sizeof(dest_dir), "%s/.local/share/kryon/plugins/%s", home, plugin_name);

    /* Create destination directory */
    if (mkdir_p(dest_dir) != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to create directory %s\n", dest_dir);
        return -1;
    }

    /* Copy .so file */
    char dest_so[1024];
    snprintf(dest_so, sizeof(dest_so), "%s/libkryon_%s.so", dest_dir, plugin_name);
    if (copy_file(src_so, dest_so) != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to copy .so file\n");
        return -1;
    }

    /* Copy plugin.toml */
    char dest_toml[1024];
    snprintf(dest_toml, sizeof(dest_toml), "%s/plugin.toml", dest_dir);
    if (file_exists(src_toml)) {
        if (copy_file(src_toml, dest_toml) != 0) {
            fprintf(stderr, "[kryon][plugin] Warning: Failed to copy plugin.toml\n");
        }
    }

    printf("[kryon][plugin] Installed %s to discovery path: %s/\n", plugin_name, dest_dir);
    return 0;
}

// ============================================================================
// Combined Installation
// ============================================================================

/**
 * Install a plugin from git URL
 * This clones the repo, compiles it, and installs to discovery path
 *
 * @param git_url The git URL
 * @param branch The branch to checkout (can be NULL)
 * @param plugin_name The plugin name
 * @return The path to the installed plugin (must be freed), or NULL on failure
 */
char* plugin_install_from_git(const char* git_url, const char* branch, const char* plugin_name) {
    // Clone the repository
    char* cached_path = plugin_clone_from_git(git_url, branch, plugin_name);
    if (!cached_path) {
        return NULL;
    }

    // Compile the plugin
    if (plugin_compile_from_source(cached_path, plugin_name) != 0) {
        free(cached_path);
        return NULL;
    }

    // Install to discovery path for runtime loading
    if (plugin_install_from_cache(plugin_name, cached_path) != 0) {
        fprintf(stderr, "[kryon][plugin] Warning: Failed to install plugin to discovery path\n");
        /* Continue anyway - plugin may still be usable from cache */
    }

    return cached_path;
}

/**
 * Update a git plugin by pulling latest changes and recompiling
 *
 * @param plugin_name The plugin name
 * @return 0 on success, non-zero on failure
 */
int plugin_update_git_plugin(const char* plugin_name) {
    char* cached_path = get_plugin_cached_path(plugin_name);
    if (!cached_path) {
        fprintf(stderr, "[kryon][plugin] Failed to get cached path for plugin '%s'\n", plugin_name);
        return -1;
    }

    if (!file_is_directory(cached_path)) {
        fprintf(stderr, "[kryon][plugin] Plugin '%s' is not installed from git\n", plugin_name);
        free(cached_path);
        return -1;
    }

    printf("[kryon][plugin] Updating plugin '%s'...\n", plugin_name);

    // Pull latest changes
    char pull_cmd[1024];
    snprintf(pull_cmd, sizeof(pull_cmd), "cd \"%s\" && git pull 2>&1", cached_path);
    int result = system(pull_cmd);
    if (result != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to update plugin '%s'\n", plugin_name);
        free(cached_path);
        return -1;
    }

    // Recompile
    result = plugin_compile_from_source(cached_path, plugin_name);

    free(cached_path);
    return result;
}

/**
 * Remove a git plugin from cache
 *
 * @param plugin_name The plugin name
 * @return 0 on success, non-zero on failure
 */
int plugin_remove_git_plugin(const char* plugin_name) {
    char* cached_path = get_plugin_cached_path(plugin_name);
    if (!cached_path) {
        return -1;
    }

    printf("[kryon][plugin] Removing plugin '%s' from cache...\n", plugin_name);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", cached_path);
    int result = system(cmd);

    free(cached_path);
    return result;
}
