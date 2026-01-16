/**
 * Dev Server for Web Target
 * Uses Bun's built-in HTTP server for static file serving
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * Start development server using Bun
 *
 * @param document_root Path to serve files from (e.g., "./build")
 * @param port Port to listen on
 * @param auto_open Auto-open browser
 * @return 0 on success, non-zero on error
 */
int start_dev_server(const char* document_root, int port, bool auto_open) {
    char cmd[4096];

    // Auto-open browser before starting server
    if (auto_open) {
        char open_cmd[512];
        #ifdef __APPLE__
        snprintf(open_cmd, sizeof(open_cmd), "open http://127.0.0.1:%d 2>/dev/null &", port);
        #elif __linux__
        snprintf(open_cmd, sizeof(open_cmd), "xdg-open http://127.0.0.1:%d 2>/dev/null &", port);
        #elif _WIN32
        snprintf(open_cmd, sizeof(open_cmd), "start http://127.0.0.1:%d", port);
        #endif
        int open_result = system(open_cmd);
        if (open_result != 0) {
            fprintf(stderr, "Warning: Failed to open browser (code %d)\n", open_result);
        }
    }

    // Run Bun dev server (blocks until Ctrl+C)
    // Use installed dev server script
    char* home = getenv("HOME");
    if (home) {
        snprintf(cmd, sizeof(cmd),
                 "bun run %s/.local/share/kryon/scripts/dev_server.ts %d \"%s\"",
                 home, port, document_root);
    } else {
        // Fallback to system location
        snprintf(cmd, sizeof(cmd),
                 "bun run /usr/local/share/kryon/scripts/dev_server.ts %d \"%s\"",
                 port, document_root);
    }

    return system(cmd);
}
