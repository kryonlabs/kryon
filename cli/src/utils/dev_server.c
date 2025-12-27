/**
 * Dev Server for Web Target
 * Embeds CivetWeb to serve generated HTML/CSS/JS files
 */

#include "kryon_cli.h"
#include "../../third_party/civetweb/civetweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

static struct mg_context* server_ctx = NULL;
static volatile bool server_running = false;

/**
 * Signal handler for Ctrl+C
 */
static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\nShutting down dev server...\n");
        server_running = false;
        if (server_ctx) {
            mg_stop(server_ctx);
            server_ctx = NULL;
        }
    }
}

/**
 * Start development server
 *
 * @param document_root Path to serve files from (e.g., "./build")
 * @param port Port to listen on
 * @param auto_open Auto-open browser
 * @return 0 on success, non-zero on error
 */
int start_dev_server(const char* document_root, int port, bool auto_open) {
    // Initialize CivetWeb
    mg_init_library(0);

    // Server options
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    const char* options[] = {
        "document_root", document_root,
        "listening_ports", port_str,
        "enable_directory_listing", "no",
        "index_files", "index.html",
        NULL
    };

    // Start server
    server_ctx = mg_start(NULL, NULL, options);
    if (!server_ctx) {
        fprintf(stderr, "Error: Failed to start dev server on port %d\n", port);
        fprintf(stderr, "       Port may already be in use.\n");
        mg_exit_library();
        return 1;
    }

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Kryon Dev Server                                            ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Local:   http://localhost:%-5d                            ║\n", port);
    printf("║  Root:    %-50s║\n", document_root);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Press Ctrl+C to stop                                        ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Auto-open browser
    if (auto_open) {
        char url[256];
        snprintf(url, sizeof(url), "http://localhost:%d", port);

        #ifdef __APPLE__
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "open \"%s\" 2>/dev/null &", url);
            system(cmd);
        #elif __linux__
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" 2>/dev/null &", url);
            system(cmd);
        #elif _WIN32
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "start \"%s\"", url);
            system(cmd);
        #endif

        printf("Opening browser at %s\n\n", url);
    }

    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Run server loop
    server_running = true;
    while (server_running) {
        sleep(1);  // Sleep to avoid busy-waiting
    }

    // Cleanup
    if (server_ctx) {
        mg_stop(server_ctx);
        server_ctx = NULL;
    }
    mg_exit_library();

    printf("Dev server stopped.\n");
    return 0;
}
