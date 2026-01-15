/**
 * Plugin Installer - Git cloning and plugin installation
 */

#ifndef PLUGIN_INSTALLER_H
#define PLUGIN_INSTALLER_H

#include <stdbool.h>

/**
 * Plugin dependency descriptor for grouped installations
 */
typedef struct {
    char* name;           // Plugin name
    char* subdir;         // Subdirectory within the repo (e.g., "syntax")
    char* git;            // Git URL
    char* branch;         // Branch to checkout (can be NULL)
} PluginGitDep;

/**
 * Group of plugins from the same git repository
 */
typedef struct {
    char* git_url;        // Shared git URL
    char* branch;         // Shared branch (can be NULL)
    PluginGitDep* plugins; // Array of plugins from this repo
    int count;            // Number of plugins
} PluginGitGroup;

/**
 * Clone a git repository to the plugin cache
 *
 * @param git_url The git URL to clone
 * @param branch The branch to checkout (NULL for default)
 * @param plugin_name The name of the plugin (for cache directory)
 * @return The path to the cloned repository (must be freed), or NULL on failure
 */
char* plugin_clone_from_git(const char* git_url, const char* branch, const char* plugin_name);

/**
 * Clone a specific subdirectory from a git repository using sparse checkout
 *
 * @param git_url The git URL to clone
 * @param subdirectory The subdirectory to checkout (e.g., "syntax")
 * @param plugin_name The name of the plugin (for cache directory)
 * @param branch The branch to checkout (NULL for default "master")
 * @return The path to the plugin subdirectory (must be freed), or NULL on failure
 */
char* plugin_clone_from_git_sparse(const char* git_url, const char* subdirectory, const char* plugin_name, const char* branch);

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
bool plugin_clone_multi_sparse(const char* git_url, PluginGitDep* plugins, int count, const char* branch);

/**
 * Group plugins by git URL for efficient batch installation
 *
 * @param deps Array of plugin dependencies
 * @param count Number of dependencies
 * @param group_count Output: number of groups returned
 * @return Array of plugin groups (caller must free), or NULL on failure
 */
PluginGitGroup* plugin_group_by_git(PluginGitDep* deps, int count, int* group_count);

/**
 * Free plugin groups allocated by plugin_group_by_git
 *
 * @param groups Array of plugin groups
 * @param count Number of groups
 */
void plugin_free_groups(PluginGitGroup* groups, int count);

/**
 * Compile a plugin from source
 *
 * @param plugin_path Path to the plugin directory
 * @param plugin_name Name of the plugin
 * @return 0 on success, non-zero on failure
 */
int plugin_compile_from_source(const char* plugin_path, const char* plugin_name);

/**
 * Install a plugin from git URL
 * This clones the repo and compiles it
 *
 * @param git_url The git URL
 * @param branch The branch to checkout (can be NULL)
 * @param plugin_name The plugin name
 * @return The path to the installed plugin (must be freed), or NULL on failure
 */
char* plugin_install_from_git(const char* git_url, const char* branch, const char* plugin_name);

/**
 * Update a git plugin by pulling latest changes and recompiling
 *
 * @param plugin_name The plugin name
 * @return 0 on success, non-zero on failure
 */
int plugin_update_git_plugin(const char* plugin_name);

/**
 * Remove a git plugin from cache
 *
 * @param plugin_name The plugin name
 * @return 0 on success, non-zero on failure
 */
int plugin_remove_git_plugin(const char* plugin_name);

#endif // PLUGIN_INSTALLER_H
