/**
 * Kryon DSL Runtime for Desktop Target
 *
 * Implements the bridge between C DSL macros and the desktop IRRenderer.
 */

#include "kryon_dsl_runtime.h"
#include "kryon.h"
#include "../../runtime/desktop/ir_desktop_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Run the desktop application using the IRRenderer
 */
int kryon_run_desktop(void) {
    // Get the root component built by the DSL
    IRComponent* root = kryon_get_root();
    if (!root) {
        fprintf(stderr, "Error: No root component found. Did you call kryon_init()?\n");
        return 1;
    }

    // Get window configuration from global app state
    extern KryonAppState g_app_state;

    // Create desktop renderer configuration
    DesktopRendererConfig config = desktop_renderer_config_default();

    if (g_app_state.window_title) {
        config.window_title = g_app_state.window_title;
    } else {
        config.window_title = "Kryon App";
    }

    config.window_width = (g_app_state.window_width > 0) ? g_app_state.window_width : 800;
    config.window_height = (g_app_state.window_height > 0) ? g_app_state.window_height : 600;
    config.backend_type = DESKTOP_BACKEND_SDL3;

    printf("Starting desktop app: %s (%dx%d)\n",
           config.window_title, config.window_width, config.window_height);

    // Create the desktop renderer
    DesktopIRRenderer* renderer = desktop_ir_renderer_create(&config);
    if (!renderer) {
        fprintf(stderr, "Error: Failed to create desktop renderer\n");
        kryon_cleanup();
        return 1;
    }

    // Run the main event loop
    printf("Running main loop...\n");
    bool success = desktop_ir_renderer_run_main_loop(renderer, root);

    // Cleanup
    printf("Shutting down...\n");
    desktop_ir_renderer_destroy(renderer);
    kryon_cleanup();

    return success ? 0 : 1;
}
