/**
 * Plugin Command Implementation
 *
 * Manages Kryon plugins: list, install, enable, disable, info
 */

#include "kryon_cli.h"
#include <ir_plugin.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

// Helper: Copy file
static int copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return -1;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    char buffer[8192];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

// Helper: Create directory recursively
static int mkdir_p(const char* path) {
    char tmp[512];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }

    return mkdir(tmp, 0755);
}

/**
 * List all discovered and loaded plugins
 */
static int cmd_plugin_list(void) {
    printf("Kryon Plugins\n");
    printf("=============\n\n");

    // Get plugin stats
    IRPluginStats stats;
    ir_plugin_get_stats(&stats);

    // Discover plugins
    uint32_t count = 0;
    IRPluginDiscoveryInfo** plugins = ir_plugin_discover(NULL, &count);

    if (count == 0) {
        printf("No plugins found.\n\n");
        printf("Plugin search paths:\n");
        printf("  - ~/.kryon/plugins/\n");
        printf("  - /usr/local/lib/kryon/plugins/\n");
        printf("  - /usr/lib/kryon/plugins/\n\n");
        printf("To install a plugin:\n");
        printf("  kryon plugin install <path-to-plugin-dir>\n");
        return 0;
    }

    printf("Found %u plugin(s):\n\n", count);

    for (uint32_t i = 0; i < count; i++) {
        IRPluginDiscoveryInfo* info = plugins[i];
        bool is_loaded = ir_plugin_is_loaded(info->name);

        printf("  %s v%s", info->name, info->version);
        if (is_loaded) {
            printf(" [loaded]");
        }
        printf("\n");
        printf("    Path: %s\n", info->path);

        // Show backends
        if (info->backend_count > 0) {
            printf("    Backends: ");
            for (uint32_t j = 0; j < info->backend_count; j++) {
                printf("%s%s", info->backends[j],
                       (j < info->backend_count - 1) ? ", " : "");
            }
            printf("\n");
        }
        printf("\n");
    }

    ir_plugin_free_discovery(plugins, count);
    return 0;
}

/**
 * Show detailed info about a plugin
 */
static int cmd_plugin_info(const char* plugin_name) {
    if (!plugin_name) {
        fprintf(stderr, "Usage: kryon plugin info <name>\n");
        return 1;
    }

    // Discover plugins
    uint32_t count = 0;
    IRPluginDiscoveryInfo** plugins = ir_plugin_discover(NULL, &count);

    IRPluginDiscoveryInfo* found = NULL;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(plugins[i]->name, plugin_name) == 0) {
            found = plugins[i];
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Plugin '%s' not found\n", plugin_name);
        ir_plugin_free_discovery(plugins, count);
        return 1;
    }

    printf("Plugin: %s\n", found->name);
    printf("Version: %s\n", found->version);
    printf("Library: %s\n", found->path);
    printf("Loaded: %s\n", ir_plugin_is_loaded(found->name) ? "yes" : "no");

    if (found->command_count > 0) {
        printf("Command IDs: ");
        for (uint32_t i = 0; i < found->command_count; i++) {
            printf("%u%s", found->command_ids[i],
                   (i < found->command_count - 1) ? ", " : "");
        }
        printf("\n");
    }

    if (found->backend_count > 0) {
        printf("Backends: ");
        for (uint32_t i = 0; i < found->backend_count; i++) {
            printf("%s%s", found->backends[i],
                   (i < found->backend_count - 1) ? ", " : "");
        }
        printf("\n");
    }

    ir_plugin_free_discovery(plugins, count);
    return 0;
}

/**
 * Install a plugin from a directory
 */
static int cmd_plugin_install(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: kryon plugin install <path-to-plugin-dir>\n");
        fprintf(stderr, "\nThe plugin directory should contain:\n");
        fprintf(stderr, "  - plugin.toml (plugin metadata)\n");
        fprintf(stderr, "  - libkryon_<name>.so (shared library)\n");
        return 1;
    }

    const char* source_dir = argv[0];

    // Check for plugin.toml
    char toml_path[512];
    snprintf(toml_path, sizeof(toml_path), "%s/plugin.toml", source_dir);

    struct stat st;
    if (stat(toml_path, &st) != 0) {
        fprintf(stderr, "Error: No plugin.toml found in %s\n", source_dir);
        return 1;
    }

    // Read plugin name from toml (simplified - just extract name)
    FILE* f = fopen(toml_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot read plugin.toml\n");
        return 1;
    }

    char plugin_name[64] = {0};
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "name", 4) == 0) {
            char* eq = strchr(line, '=');
            if (eq) {
                eq++;
                while (*eq == ' ' || *eq == '"') eq++;
                char* end = eq;
                while (*end && *end != '"' && *end != '\n') end++;
                size_t len = end - eq;
                if (len < sizeof(plugin_name)) {
                    memcpy(plugin_name, eq, len);
                    plugin_name[len] = '\0';
                }
                break;
            }
        }
    }
    fclose(f);

    if (plugin_name[0] == '\0') {
        fprintf(stderr, "Error: Cannot determine plugin name from plugin.toml\n");
        return 1;
    }

    // Check for .so file
    char so_path[512];
    snprintf(so_path, sizeof(so_path), "%s/libkryon_%s.so", source_dir, plugin_name);
    if (stat(so_path, &st) != 0) {
        fprintf(stderr, "Error: No libkryon_%s.so found in %s\n", plugin_name, source_dir);
        return 1;
    }

    // Create plugin directory
    char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        return 1;
    }

    char dest_dir[512];
    snprintf(dest_dir, sizeof(dest_dir), "%s/.kryon/plugins/%s", home, plugin_name);
    mkdir_p(dest_dir);

    // Copy files
    char dest_toml[512], dest_so[512];
    snprintf(dest_toml, sizeof(dest_toml), "%s/plugin.toml", dest_dir);
    snprintf(dest_so, sizeof(dest_so), "%s/libkryon_%s.so", dest_dir, plugin_name);

    if (copy_file(toml_path, dest_toml) != 0) {
        fprintf(stderr, "Error: Failed to copy plugin.toml\n");
        return 1;
    }

    if (copy_file(so_path, dest_so) != 0) {
        fprintf(stderr, "Error: Failed to copy shared library\n");
        return 1;
    }

    printf("Installed plugin '%s' to %s\n", plugin_name, dest_dir);
    printf("Restart kryon to load the new plugin.\n");
    return 0;
}

/**
 * Plugin command dispatcher
 */
int cmd_plugin(int argc, char** argv) {
    if (argc < 1) {
        return cmd_plugin_list();
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "list") == 0) {
        return cmd_plugin_list();
    }
    else if (strcmp(subcmd, "info") == 0) {
        return cmd_plugin_info(argc > 1 ? argv[1] : NULL);
    }
    else if (strcmp(subcmd, "install") == 0) {
        return cmd_plugin_install(argc - 1, argv + 1);
    }
    else {
        fprintf(stderr, "Unknown plugin command: %s\n", subcmd);
        fprintf(stderr, "\nUsage: kryon plugin <command>\n");
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  list              List installed plugins\n");
        fprintf(stderr, "  info <name>       Show plugin details\n");
        fprintf(stderr, "  install <path>    Install plugin from directory\n");
        return 1;
    }
}
