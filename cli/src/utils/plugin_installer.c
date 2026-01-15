/**
 * Plugin Installer - Git cloning and plugin installation
 *
 * Handles automatic installation of plugins from:
 * - Git URLs (clone to cache)
 * - Local paths (compile and install)
 */

#include "kryon_cli.h"
#include "plugin_installer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

// Forward declaration
static char* extract_repo_name(const char* git_url);

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
        // Try to pull latest changes (use --ff-only to avoid divergent branch issues)
        char pull_cmd[1024];
        snprintf(pull_cmd, sizeof(pull_cmd),
            "cd \"%s\" && git fetch origin > /dev/null 2>&1 && "
            "git reset --hard origin/master > /dev/null 2>&1 && "
            "git clean -fdx > /dev/null 2>&1",
            cached_path);
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
 * Clone a specific subdirectory from a git repository using sparse checkout
 *
 * @param git_url The git URL to clone
 * @param subdirectory The subdirectory to checkout (e.g., "syntax")
 * @param plugin_name The name of the plugin (for logging)
 * @param branch The branch to checkout (NULL for default "master")
 * @return The path to the plugin subdirectory (must be freed), or NULL on failure
 *
 * Note: The cache directory is named after the repository, not the plugin name.
 * This allows multiple plugins from the same repository to share the same cache.
 */
char* plugin_clone_from_git_sparse(const char* git_url, const char* subdirectory, const char* plugin_name, const char* branch) {
    if (!git_url || !subdirectory || !plugin_name) {
        fprintf(stderr, "[kryon][plugin] Invalid git URL, subdirectory, or plugin name\n");
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

    // Use repository name for cache directory (not plugin name)
    // This allows multiple plugins from the same repo to share the cache
    char* repo_name = extract_repo_name(git_url);
    if (!repo_name) {
        return NULL;
    }

    char* cache_dir = get_plugin_cache_dir();
    char* cached_path = path_join(cache_dir, repo_name);
    free(cache_dir);
    free(repo_name);

    if (!cached_path) {
        fprintf(stderr, "[kryon][plugin] Failed to get cached path for plugin '%s'\n", plugin_name);
        return NULL;
    }

    // For sparse checkout, we need to:
    // 1. Create empty directory
    // 2. git init
    // 3. git remote add origin
    // 4. git config core.sparseCheckout true
    // 5. Add subdirectory to .git/info/sparse-checkout
    // 6. git pull --depth 1 origin <branch>

    char cmd[2048];
    const char* actual_branch = branch ? branch : "master";

    // Create and initialize directory
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" && cd \"%s\" && git init -q", cached_path, cached_path);
    system(cmd);

    // Add remote
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && git remote add origin \"%s\" 2>/dev/null", cached_path, git_url);
    system(cmd);

    // Enable sparse checkout
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && git config core.sparseCheckout true", cached_path);
    system(cmd);

    // Set sparse checkout path
    char sparse_file[1024];
    snprintf(sparse_file, sizeof(sparse_file), "%s/.git/info/sparse-checkout", cached_path);
    FILE* f = fopen(sparse_file, "w");
    if (f) {
        fprintf(f, "%s\n", subdirectory);
        fclose(f);
    } else {
        fprintf(stderr, "[kryon][plugin] Failed to create sparse-checkout file\n");
        free(cached_path);
        return NULL;
    }

    // Pull with depth 1 (fetch + reset to avoid divergent branch issues)
    printf("[kryon][plugin] Cloning plugin '%s' from %s (sparse: %s)\n", plugin_name, git_url, subdirectory);
    snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && git fetch origin %s --depth 1 --quiet 2>&1 && "
        "git reset --hard origin/%s --quiet 2>&1 && "
        "git clean -fdx --quiet 2>&1",
        cached_path, actual_branch, actual_branch);
    int result = system(cmd);

    if (result != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to sparse checkout (exit code %d)\n", result);
        free(cached_path);
        return NULL;
    }

    // The actual plugin is now in cached_path/subdirectory
    char* plugin_path = path_join(cached_path, subdirectory);

    // Verify the subdirectory exists
    if (!file_is_directory(plugin_path)) {
        fprintf(stderr, "[kryon][plugin] Subdirectory '%s' not found in cloned repository\n", subdirectory);
        free(plugin_path);
        free(cached_path);
        return NULL;
    }

    printf("[kryon][plugin] Successfully cloned plugin '%s' (sparse checkout)\n", plugin_name);
    free(cached_path);
    return plugin_path;
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
    snprintf(cmd, sizeof(cmd), "make -B -C \"%s\" 2>&1", plugin_path);

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

    // Pull latest changes (fetch + reset to avoid divergent branch issues)
    char pull_cmd[1024];
    snprintf(pull_cmd, sizeof(pull_cmd),
        "cd \"%s\" && git fetch origin > /dev/null 2>&1 && "
        "git reset --hard origin/master > /dev/null 2>&1 && "
        "git clean -fdx > /dev/null 2>&1",
        cached_path);
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

// ============================================================================
// Multi-Plugin Sparse Checkout (Grouped Installation)
// ============================================================================

/**
 * Extract repository name from git URL for cache directory naming
 * Returns allocated string that must be freed
 */
static char* extract_repo_name(const char* git_url) {
    if (!git_url) return NULL;

    // Find the last slash in the URL
    const char* last_slash = strrchr(git_url, '/');
    if (!last_slash) {
        // No slash, use full URL
        return strdup(git_url);
    }

    // Skip the slash
    const char* repo_name = last_slash + 1;

    // Remove .git suffix if present
    size_t len = strlen(repo_name);
    if (len > 4 && strcmp(repo_name + len - 4, ".git") == 0) {
        len -= 4;
    }

    // Copy the name
    char* result = malloc(len + 1);
    if (result) {
        memcpy(result, repo_name, len);
        result[len] = '\0';
    }

    return result;
}

/**
 * Group plugins by git URL for efficient batch installation
 *
 * @param deps Array of plugin dependencies
 * @param count Number of dependencies
 * @param group_count Output: number of groups returned
 * @return Array of plugin groups (caller must free), or NULL on failure
 */
PluginGitGroup* plugin_group_by_git(PluginGitDep* deps, int count, int* group_count) {
    if (!deps || count <= 0 || !group_count) {
        return NULL;
    }

    // Allocate maximum possible groups (each plugin could be from different repo)
    PluginGitGroup* groups = calloc(count, sizeof(PluginGitGroup));
    if (!groups) {
        return NULL;
    }

    int actual_groups = 0;

    for (int i = 0; i < count; i++) {
        PluginGitDep* dep = &deps[i];
        if (!dep->git) {
            continue;  // Skip non-git plugins
        }

        // Check if we already have a group for this git URL
        bool found = false;
        for (int j = 0; j < actual_groups; j++) {
            if (strcmp(groups[j].git_url, dep->git) == 0) {
                // Add to existing group
                // Expand plugins array
                int new_count = groups[j].count + 1;
                PluginGitDep* new_plugins = realloc(groups[j].plugins,
                                                      sizeof(PluginGitDep) * new_count);
                if (new_plugins) {
                    groups[j].plugins = new_plugins;
                    groups[j].plugins[groups[j].count].name = strdup(dep->name);
                    groups[j].plugins[groups[j].count].subdir = dep->subdir ? strdup(dep->subdir) : NULL;
                    groups[j].plugins[groups[j].count].git = NULL;  // Don't duplicate git URL
                    groups[j].plugins[groups[j].count].branch = dep->branch ? strdup(dep->branch) : NULL;
                    groups[j].count = new_count;
                }
                found = true;
                break;
            }
        }

        // Create new group
        if (!found) {
            groups[actual_groups].git_url = strdup(dep->git);
            groups[actual_groups].branch = dep->branch ? strdup(dep->branch) : NULL;
            groups[actual_groups].plugins = malloc(sizeof(PluginGitDep));
            if (groups[actual_groups].plugins) {
                groups[actual_groups].plugins[0].name = strdup(dep->name);
                groups[actual_groups].plugins[0].subdir = dep->subdir ? strdup(dep->subdir) : NULL;
                groups[actual_groups].plugins[0].git = NULL;
                groups[actual_groups].plugins[0].branch = NULL;
                groups[actual_groups].count = 1;
                actual_groups++;
            }
        }
    }

    *group_count = actual_groups;
    return groups;
}

/**
 * Clone multiple subdirectories from the same git repository using sparse checkout
 * This is more efficient than cloning each subdirectory separately.
 *
 * @param git_url The git URL to clone
 * @param plugins Array of plugin descriptors (name, subdir)
 * @param count Number of plugins
 * @param branch The branch to checkout (NULL for default)
 * @return true on success, false on failure
 */
bool plugin_clone_multi_sparse(const char* git_url, PluginGitDep* plugins, int count, const char* branch) {
    if (!git_url || !plugins || count <= 0) {
        fprintf(stderr, "[kryon][plugin] Invalid parameters for multi-sparse checkout\n");
        return false;
    }

    if (!git_is_available()) {
        fprintf(stderr, "[kryon][plugin] Git is not available. Please install git.\n");
        return false;
    }

    // Ensure cache directory exists
    if (!ensure_plugin_cache_dir()) {
        fprintf(stderr, "[kryon][plugin] Failed to create plugin cache directory\n");
        return false;
    }

    // Get repository name for cache directory
    char* repo_name = extract_repo_name(git_url);
    if (!repo_name) {
        return false;
    }

    // Create shared cache directory for all plugins from this repo
    char* cache_dir = get_plugin_cache_dir();
    char* shared_cache_path = path_join(cache_dir, repo_name);
    free(cache_dir);
    free(repo_name);

    if (!shared_cache_path) {
        return false;
    }

    char cmd[4096];
    const char* actual_branch = branch ? branch : "master";

    // Check if already initialized
    if (file_is_directory(shared_cache_path) &&
        file_is_directory(path_join(shared_cache_path, ".git"))) {
        printf("[kryon][plugin] Repository already cached at %s\n", shared_cache_path);

        // Update existing repo - fetch + reset to avoid divergent branch issues
        snprintf(cmd, sizeof(cmd),
            "cd \"%s\" && git fetch origin %s --depth 1 --quiet 2>&1 && "
            "git reset --hard origin/%s --quiet 2>&1 && "
            "git clean -fdx --quiet 2>&1",
            shared_cache_path, actual_branch, actual_branch);
        system(cmd);

        // Note: We don't return here because we still need to verify/update sparse checkout
    } else {
        // Create and initialize directory
        mkdir_p(shared_cache_path);
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && git init -q", shared_cache_path);
        system(cmd);

        // Add remote
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && git remote add origin \"%s\" 2>/dev/null",
                 shared_cache_path, git_url);
        system(cmd);

        // Enable sparse checkout
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && git config core.sparseCheckout true",
                 shared_cache_path);
        system(cmd);
    }

    // Update sparse-checkout file with all subdirectories
    char sparse_file[1024];
    snprintf(sparse_file, sizeof(sparse_file), "%s/.git/info/sparse-checkout", shared_cache_path);
    FILE* f = fopen(sparse_file, "w");
    if (!f) {
        fprintf(stderr, "[kryon][plugin] Failed to create sparse-checkout file\n");
        free(shared_cache_path);
        return false;
    }

    // Add all subdirectories to sparse-checkout
    for (int i = 0; i < count; i++) {
        if (plugins[i].subdir) {
            fprintf(f, "%s\n", plugins[i].subdir);
        }
    }
    fclose(f);

    // Pull with depth 1 (fetch + reset to avoid divergent branch issues)
    printf("[kryon][plugin] Cloning %d plugin(s) from %s\n", count, git_url);
    snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && git fetch origin %s --depth 1 --quiet 2>&1 && "
        "git reset --hard origin/%s --quiet 2>&1 && "
        "git clean -fdx --quiet 2>&1",
        shared_cache_path, actual_branch, actual_branch);
    int result = system(cmd);

    if (result != 0) {
        fprintf(stderr, "[kryon][plugin] Failed to sparse checkout (exit code %d)\n", result);
        free(shared_cache_path);
        return false;
    }

    // Verify all subdirectories exist
    for (int i = 0; i < count; i++) {
        if (plugins[i].subdir) {
            char* subdir_path = path_join(shared_cache_path, plugins[i].subdir);
            if (!file_is_directory(subdir_path)) {
                fprintf(stderr, "[kryon][plugin] Subdirectory '%s' not found in cloned repository\n",
                        plugins[i].subdir);
                free(subdir_path);
                free(shared_cache_path);
                return false;
            }
            free(subdir_path);
        }
    }

    printf("[kryon][plugin] Successfully cloned %d plugin(s) from shared repository\n", count);
    free(shared_cache_path);
    return true;
}

/**
 * Free plugin groups allocated by plugin_group_by_git
 *
 * @param groups Array of plugin groups
 * @param count Number of groups
 */
void plugin_free_groups(PluginGitGroup* groups, int count) {
    if (!groups) return;

    for (int i = 0; i < count; i++) {
        free(groups[i].git_url);
        free(groups[i].branch);

        for (int j = 0; j < groups[i].count; j++) {
            free(groups[i].plugins[j].name);
            free(groups[i].plugins[j].subdir);
            free(groups[i].plugins[j].branch);
        }
        free(groups[i].plugins);
    }

    free(groups);
}

