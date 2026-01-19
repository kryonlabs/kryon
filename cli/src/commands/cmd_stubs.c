/**
 * Command Stub Implementations
 * Placeholder implementations for all commands
 * Will be replaced with real implementations in later steps
 */

#include "kryon_cli.h"
#include "../utils/file_watcher.h"
#include "../utils/ws_reload.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>



// cmd_plugin moved to cmd_plugin.c

int cmd_test(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("test command not yet implemented\n");
    return 1;
}

// Global state for file watcher and reload server (accessed by callback)
static WSReloadServer* g_reload_server = NULL;

// Global WebSocket port for hot reload callback
static int g_ws_port = 0;

// File change callback for hot reloading
static void on_file_change(const char* filepath, void* user_data) {
    (void)user_data;

    printf("\n[hot reload] %s changed, rebuilding...\n", filepath);

    // Set environment variables for hot reload injection
    setenv("KRYON_DEV_MODE", "1", 1);
    char ws_port_str[16];
    snprintf(ws_port_str, sizeof(ws_port_str), "%d", g_ws_port);
    setenv("KRYON_WS_PORT", ws_port_str, 1);

    // Build the changed file
    char* filepath_copy = strdup(filepath);
    char* args[] = {filepath_copy};
    int result = cmd_build(1, args);
    free(filepath_copy);

    // Unset environment variables after build
    unsetenv("KRYON_DEV_MODE");
    unsetenv("KRYON_WS_PORT");

    if (result == 0) {
        printf("[hot reload] ✓ Rebuild complete\n");

        // Trigger browser reload
        if (g_reload_server) {
            ws_reload_trigger(g_reload_server);
            printf("[hot reload] → Browser notified\n");
        }
    } else {
        printf("[hot reload] ✗ Rebuild failed\n");
    }
}

// File watcher thread function
static void* file_watcher_thread(void* arg) {
    FileWatcher* watcher = (FileWatcher*)arg;
    file_watcher_start(watcher, on_file_change, NULL);
    return NULL;
}

/**
 * Dev Command
 * Start development server with automatic rebuilding and hot reloading
 */
int cmd_dev(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Load config
    KryonConfig* config = config_find_and_load();
    if (!config) {
        fprintf(stderr, "Error: No kryon.toml found\n");
        fprintf(stderr, "Run 'kryon new <name>' to create a new project\n");
        return 1;
    }

    // Validate config
    if (!config_validate(config)) {
        config_free(config);
        return 1;
    }

    // Ensure we have a web target
    bool has_web_target = false;
    for (int i = 0; i < config->build_targets_count; i++) {
        if (strcmp(config->build_targets[i], "web") == 0) {
            has_web_target = true;
            break;
        }
    }

    if (!has_web_target) {
        fprintf(stderr, "Error: dev server requires 'web' target in build.targets\n");
        fprintf(stderr, "Add to your kryon.toml:\n");
        fprintf(stderr, "  [build]\n");
        fprintf(stderr, "  targets = [\"web\"]\n");
        config_free(config);
        return 1;
    }

    // Build the project first (build all discovered files)
    printf("Building project...\n");

    // Get dev server port before setting env vars
    int port = config->dev_port > 0 ? config->dev_port : 3000;
    int ws_port = port + 1;

    // Store WebSocket port for hot reload callback
    g_ws_port = ws_port;

    // Set environment variables for hot reload injection
    setenv("KRYON_DEV_MODE", "1", 1);
    char ws_port_str[16];
    snprintf(ws_port_str, sizeof(ws_port_str), "%d", ws_port);
    setenv("KRYON_WS_PORT", ws_port_str, 1);

    // Call cmd_build with no arguments to build everything
    int build_result = cmd_build(0, NULL);

    // Unset environment variables after build
    unsetenv("KRYON_DEV_MODE");
    unsetenv("KRYON_WS_PORT");

    if (build_result != 0) {
        fprintf(stderr, "Build failed, cannot start dev server\n");
        config_free(config);
        return 1;
    }

    printf("\n");

    // Get dev server settings
    const char* output_dir = config->build_output_dir ? config->build_output_dir : "build";
    bool auto_open = config->dev_auto_open;

    // WebSocket port already calculated above
    g_reload_server = ws_reload_start(ws_port);
    if (!g_reload_server) {
        fprintf(stderr, "Warning: Could not start hot reload server\n");
    }

    // Create file watcher
    FileWatcher* watcher = NULL;
    pthread_t watcher_thread_id;

    if (config->dev_hot_reload) {
        watcher = file_watcher_create(".");
        if (watcher) {
            // Start file watcher in background thread
            if (pthread_create(&watcher_thread_id, NULL, file_watcher_thread, watcher) == 0) {
                pthread_detach(watcher_thread_id);
            } else {
                fprintf(stderr, "Warning: Could not start file watcher\n");
                file_watcher_destroy(watcher);
                watcher = NULL;
            }
        } else {
            fprintf(stderr, "Warning: Could not create file watcher\n");
        }
    }

    // Declare the function (implemented in dev_server.c)
    extern int start_dev_server(const char* document_root, int port, bool auto_open);

    // Start dev server (this blocks until Ctrl+C)
    int result = start_dev_server(output_dir, port, auto_open);

    // Cleanup
    if (watcher) {
        file_watcher_stop(watcher);
        file_watcher_destroy(watcher);
    }

    if (g_reload_server) {
        ws_reload_stop(g_reload_server);
        g_reload_server = NULL;
    }

    config_free(config);
    return result;
}
