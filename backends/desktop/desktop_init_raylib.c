/**
 * Raylib Backend Initialization
 * Handles window creation, setup, and cleanup for the raylib renderer
 */

#ifdef ENABLE_RAYLIB

#include "desktop_internal.h"
#include <raylib.h>
#include <stdio.h>

/**
 * Initialize raylib backend
 * Creates window, sets up rendering context
 */
bool initialize_raylib_backend(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    printf("[raylib] Initializing raylib renderer\n");

    // Configure raylib window flags
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

    // Initialize window
    InitWindow(
        renderer->config.window_width,
        renderer->config.window_height,
        renderer->config.window_title
    );

    if (!IsWindowReady()) {
        fprintf(stderr, "Error: Failed to create raylib window\n");
        return false;
    }

    // Set target FPS
    SetTargetFPS(renderer->config.target_fps);

    // Store window dimensions
    renderer->window_width = renderer->config.window_width;
    renderer->window_height = renderer->config.window_height;

    // Load default font (raylib provides built-in font)
    renderer->default_font_raylib = GetFontDefault();

    renderer->initialized = true;

    printf("[raylib] Initialized: %dx%d \"%s\"\n",
           renderer->window_width,
           renderer->window_height,
           renderer->config.window_title);

    return true;
}

/**
 * Shutdown raylib backend
 * Closes window and cleans up resources
 */
void shutdown_raylib_backend(DesktopIRRenderer* renderer) {
    if (!renderer || !renderer->initialized) return;

    printf("[raylib] Shutting down\n");

    CloseWindow();

    renderer->initialized = false;
}

#endif // ENABLE_RAYLIB
