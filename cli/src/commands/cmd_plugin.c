/**
 * Plugin Command Implementation
 *
 * Manages Kryon plugins: list, install, enable, disable, info
 */

#include "kryon_cli.h"
#include <ir_capability.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

/**
 * List all discovered and loaded plugins
 */
static int cmd_plugin_list(void) {
    printf("Kryon Plugins\n");
    printf("=============\n\n");

    // Get count of loaded plugins from capability system
    uint32_t loaded_count = ir_capability_get_plugin_count();

    // List loaded plugins
    if (loaded_count > 0) {
        printf("Loaded plugins (%u):\n\n", loaded_count);
        for (uint32_t i = 0; i < loaded_count; i++) {
            const char* name = ir_capability_get_plugin_name(i);
            if (name) {
                printf("  - %s\n", name);
            }
        }
        printf("\n");
    } else {
        printf("No plugins currently loaded.\n\n");
    }

    // Scan plugin directories for available plugins
    const char* plugin_paths[] = {
        getenv("HOME") ? : "",
        "/usr/local/lib/kryon/plugins",
        "/usr/lib/kryon/plugins",
        NULL
    };

    printf("Plugin search paths:\n");
    for (int i = 0; plugin_paths[i]; i++) {
        if (i == 0 && strlen(plugin_paths[i]) > 0) {
            char home_plugins[512];
            snprintf(home_plugins, sizeof(home_plugins), "%s/.kryon/plugins", plugin_paths[i]);
            printf("  - %s\n", home_plugins);
        } else {
            printf("  - %s\n", plugin_paths[i]);
        }
    }
    printf("\n");

    return 0;
}

/**
 * Load a plugin from a specific path
 */
static int cmd_plugin_load(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: kryon plugin load <plugin.so> [name]\n");
        return 1;
    }

    const char* path = argv[0];
    const char* name = (argc >= 2) ? argv[1] : NULL;

    if (!file_exists(path)) {
        fprintf(stderr, "Error: Plugin file not found: %s\n", path);
        return 1;
    }

    printf("Loading plugin from: %s\n", path);
    bool success = ir_capability_load_plugin(path, name);

    if (success) {
        printf("Plugin loaded successfully!\n");
        return 0;
    } else {
        fprintf(stderr, "Error: Failed to load plugin\n");
        return 1;
    }
}

/**
 * Unload a plugin by name
 */
static int cmd_plugin_unload(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: kryon plugin unload <name>\n");
        return 1;
    }

    const char* name = argv[0];
    printf("Unloading plugin: %s\n", name);

    bool success = ir_capability_unload_plugin(name);

    if (success) {
        printf("Plugin unloaded successfully!\n");
        return 0;
    } else {
        fprintf(stderr, "Error: Failed to unload plugin (not found?)\n");
        return 1;
    }
}

/**
 * Show detailed information about a specific plugin
 */
static int cmd_plugin_info(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: kryon plugin info <name>\n");
        return 1;
    }

    const char* name = argv[0];
    printf("Plugin Info: %s\n", name);
    printf("====================================\n\n");

    // Check if plugin is loaded
    bool is_loaded = false;
    uint32_t count = ir_capability_get_plugin_count();
    for (uint32_t i = 0; i < count; i++) {
        const char* plugin_name = ir_capability_get_plugin_name(i);
        if (plugin_name && strcmp(plugin_name, name) == 0) {
            is_loaded = true;
            break;
        }
    }

    printf("Status: %s\n", is_loaded ? "Loaded" : "Not loaded");
    printf("\n");

    return 0;
}

/**
 * Install a plugin from a source directory to the plugin directory
 */
static int cmd_plugin_install(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: kryon plugin install <plugin-dir>\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Installs a plugin from a source directory to ~/.kryon/plugins/\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Example:\n");
        fprintf(stderr, "  kryon plugin install ./kryon-plugins/example\n");
        return 1;
    }

    const char* src_dir = argv[0];
    char install_dir[512];

    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        return 1;
    }

    // Determine plugin name from source directory
    const char* plugin_name = strrchr(src_dir, '/');
    if (plugin_name) {
        plugin_name++;  // Skip the slash
    } else {
        plugin_name = src_dir;
    }

    // Create install directory path
    snprintf(install_dir, sizeof(install_dir), "%s/.kryon/plugins/%s", home, plugin_name);

    // Check if source directory exists
    if (!file_is_directory(src_dir)) {
        fprintf(stderr, "Error: Source directory not found: %s\n", src_dir);
        return 1;
    }

    printf("Installing plugin '%s'...\n", plugin_name);
    printf("  Source: %s\n", src_dir);
    printf("  Target: %s\n", install_dir);

    // Create install directory
    if (!dir_create(install_dir) && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create install directory: %s\n", strerror(errno));
        return 1;
    }

    // Find the .so file and copy it
    DIR* dir = opendir(src_dir);
    if (!dir) {
        fprintf(stderr, "Error: Failed to open source directory\n");
        return 1;
    }

    bool found_so = false;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".so") != NULL) {
            char src_path[1024];
            char dst_path[1024];
            snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", install_dir, entry->d_name);

            printf("  Copying: %s\n", entry->d_name);

            // Copy file using system command
            // Buffer is 4096 bytes: worst case is 3 + 1024 + 2 + 1024 + 1 = 2054, well within limit
            char cmd[4096];
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", src_path, dst_path);
            if (system(cmd) != 0) {
                fprintf(stderr, "Warning: Failed to copy %s\n", entry->d_name);
            } else {
                found_so = true;
            }
        }
    }
    closedir(dir);

    // Copy plugin.toml if it exists
    char src_toml[1024];
    char dst_toml[1024];
    snprintf(src_toml, sizeof(src_toml), "%s/plugin.toml", src_dir);
    snprintf(dst_toml, sizeof(dst_toml), "%s/plugin.toml", install_dir);
    if (file_exists(src_toml)) {
        printf("  Copying: plugin.toml\n");
        // Buffer is 4096 bytes: worst case is 3 + 1024 + 2 + 1024 + 1 = 2054, well within limit
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", src_toml, dst_toml);
        int result = system(cmd);
        if (result != 0) {
            fprintf(stderr, "Warning: Failed to copy plugin.toml (code %d)\n", result);
        }
    }

    if (!found_so) {
        fprintf(stderr, "Error: No .so file found in source directory\n");
        return 1;
    }

    printf("\nPlugin installed successfully!\n");
    printf("To load the plugin:\n");
    printf("  kryon plugin load %s/lib%s.so\n", install_dir, plugin_name);
    printf("Or restart kryon to auto-load plugins\n");

    return 0;
}

/**
 * Show help for plugin commands
 */
static int cmd_plugin_help(void) {
    printf("Kryon Plugin Commands\n");
    printf("=====================\n\n");
    printf("Usage: kryon plugin <command> [options]\n\n");
    printf("Commands:\n");
    printf("  list                    List all discovered and loaded plugins\n");
    printf("  load <path> [name]      Load a plugin from a .so file\n");
    printf("  unload <name>           Unload a loaded plugin\n");
    printf("  info <name>             Show information about a plugin\n");
    printf("  install <dir>           Install a plugin from a directory\n");
    printf("  help                   Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  kryon plugin list\n");
    printf("  kryon plugin load ~/.kryon/plugins/example/libexample.so\n");
    printf("  kryon plugin info syntax\n");
    printf("  kryon plugin install ./kryon-plugins/example\n");
    printf("\n");

    return 0;
}

/**
 * Main plugin command dispatcher
 */
int cmd_plugin(int argc, char** argv) {
    if (argc < 1) {
        return cmd_plugin_help();
    }

    const char* subcommand = argv[0];

    if (strcmp(subcommand, "list") == 0 || strcmp(subcommand, "ls") == 0) {
        return cmd_plugin_list();
    }
    else if (strcmp(subcommand, "load") == 0) {
        return cmd_plugin_load(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "unload") == 0) {
        return cmd_plugin_unload(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "info") == 0) {
        return cmd_plugin_info(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "install") == 0) {
        return cmd_plugin_install(argc - 1, argv + 1);
    }
    else if (strcmp(subcommand, "help") == 0 || strcmp(subcommand, "--help") == 0 || strcmp(subcommand, "-h") == 0) {
        return cmd_plugin_help();
    }
    else {
        fprintf(stderr, "Error: Unknown plugin command '%s'\n", subcommand);
        fprintf(stderr, "Run 'kryon plugin help' for usage\n");
        return 1;
    }
}
