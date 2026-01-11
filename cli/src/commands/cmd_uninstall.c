/**
 * Uninstall Command
 * Uninstalls a Kryon application from the system
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "cJSON.h"
#include <limits.h>

/**
 * Find and parse install manifest
 */
static cJSON* find_install_manifest(const char* project_name) {
    // Try XDG state directory first
    const char* xdg_state = getenv("XDG_STATE_HOME");
    char* manifest_path = NULL;

    if (xdg_state && strlen(xdg_state) > 0) {
        manifest_path = path_join(xdg_state, project_name);
        char* tmp = manifest_path;
        manifest_path = path_join(tmp, "install-manifest.json");
        free(tmp);
    } else {
        char* home = paths_get_home_dir();
        if (home) {
            char* state_dir = path_join(home, ".local/state");
            char* project_dir = path_join(state_dir, project_name);
            manifest_path = path_join(project_dir, "install-manifest.json");
            free(project_dir);
            free(state_dir);
            free(home);
        }
    }

    if (!manifest_path) return NULL;

    if (!file_exists(manifest_path)) {
        free(manifest_path);
        return NULL;
    }

    char* content = file_read(manifest_path);
    free(manifest_path);

    if (!content) return NULL;

    cJSON* manifest = cJSON_Parse(content);
    free(content);

    return manifest;
}

/**
 * Remove installed binary
 */
static bool remove_binary(const cJSON* manifest, bool dry_run) {
    cJSON* binary = cJSON_GetObjectItem(manifest, "binary");
    if (!binary) return false;

    cJSON* path_obj = cJSON_GetObjectItem(binary, "path");
    if (!path_obj || !cJSON_IsString(path_obj)) return false;

    const char* path = cJSON_GetStringValue(path_obj);

    if (dry_run) {
        printf("  [DRY RUN] Would remove: %s\n", path);
        return true;
    }

    if (unlink(path) == 0) {
        printf("  Removed: %s\n", path);
        return true;
    } else {
        fprintf(stderr, "  Warning: Failed to remove %s: %s\n", path, strerror(errno));
        return false;
    }
}

/**
 * Remove desktop entry
 */
static bool remove_desktop_entry(const char* project_name, bool dry_run) {
    const char* xdg_data = getenv("XDG_DATA_HOME");
    char* apps_dir;
    if (xdg_data && strlen(xdg_data) > 0) {
        apps_dir = path_join(xdg_data, "applications");
    } else {
        char* home = paths_get_home_dir();
        apps_dir = path_join(home, ".local/share/applications");
        free(home);
    }

    char desktop_file[PATH_MAX];
    snprintf(desktop_file, sizeof(desktop_file), "%s/%s.desktop", apps_dir, project_name);
    free(apps_dir);

    if (!file_exists(desktop_file)) {
        return false;
    }

    if (dry_run) {
        printf("  [DRY RUN] Would remove desktop entry: %s\n", desktop_file);
        return true;
    }

    if (unlink(desktop_file) == 0) {
        printf("  Removed desktop entry: %s\n", desktop_file);
        return true;
    } else {
        fprintf(stderr, "  Warning: Failed to remove desktop entry: %s\n", strerror(errno));
        return false;
    }
}

/**
 * Remove manifest file
 */
static void remove_manifest(const char* project_name, bool dry_run) {
    const char* xdg_state = getenv("XDG_STATE_HOME");
    char* state_dir;
    if (xdg_state && strlen(xdg_state) > 0) {
        state_dir = path_join(xdg_state, project_name);
    } else {
        char* home = paths_get_home_dir();
        char* tmp = path_join(home, ".local/state");
        state_dir = path_join(tmp, project_name);
        free(tmp);
        free(home);
    }

    char manifest_path[PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), "%s/install-manifest.json", state_dir);
    free(state_dir);

    if (!file_exists(manifest_path)) {
        return;
    }

    if (dry_run) {
        printf("  [DRY RUN] Would remove manifest: %s\n", manifest_path);
        return;
    }

    unlink(manifest_path);
}

int cmd_uninstall(int argc, char** argv) {
    bool dry_run = false;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: kryon uninstall [options]\n\n");
            printf("Options:\n");
            printf("  --dry-run        Show what would be removed without removing\n");
            printf("  --help, -h       Show this help message\n");
            return 0;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
        }
    }

    // Load config to get project name
    KryonConfig* config = config_find_and_load();
    if (!config) {
        fprintf(stderr, "Error: No kryon.toml found in current directory\n");
        return 1;
    }

    const char* project_name = config->project_name;
    if (!project_name) {
        fprintf(stderr, "Error: No project name in kryon.toml\n");
        config_free(config);
        return 1;
    }

    printf("Uninstalling %s...\n", project_name);

    // Find and parse manifest
    cJSON* manifest = find_install_manifest(project_name);

    if (!manifest) {
        fprintf(stderr, "Warning: No install manifest found for '%s'\n", project_name);
        fprintf(stderr, "         Attempting manual removal...\n");

        // Try to remove from default location
        char* home = paths_get_home_dir();
        if (home) {
            char bin_path[PATH_MAX];
            snprintf(bin_path, sizeof(bin_path), "%s/.local/bin/%s", home, project_name);

            if (file_exists(bin_path)) {
                if (dry_run) {
                    printf("  [DRY RUN] Would remove: %s\n", bin_path);
                } else {
                    if (unlink(bin_path) == 0) {
                        printf("  Removed: %s\n", bin_path);
                    }
                }
            }

            free(home);
        }

        // Try to remove desktop entry
        remove_desktop_entry(project_name, dry_run);

        config_free(config);
        printf("Uninstall complete!\n");
        return 0;
    }

    // Remove binary
    cJSON* binary = cJSON_GetObjectItem(manifest, "binary");
    if (binary) {
        remove_binary(manifest, dry_run);
    }

    // Remove desktop entry
    cJSON* desktop = cJSON_GetObjectItem(manifest, "desktop");
    bool has_desktop = cJSON_IsBool(desktop) && cJSON_IsTrue(desktop);
    if (has_desktop) {
        remove_desktop_entry(project_name, dry_run);
    }

    // Remove manifest
    remove_manifest(project_name, dry_run);

    // TODO: Remove config/data files if --all is specified

    cJSON_Delete(manifest);
    config_free(config);

    printf("Uninstall complete!\n");
    return 0;
}
