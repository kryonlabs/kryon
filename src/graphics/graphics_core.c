/**
 * @file graphics_core.c
 * @brief Kryon Graphics Core Implementation
 */

#include "graphics.h"
#include <string.h>

static KryonRendererType g_current_renderer = KRYON_RENDERER_RAYLIB;

bool kryon_graphics_init(void) {
    return true; // TODO: Initialize graphics subsystem
}

void kryon_graphics_cleanup(void) {
    // TODO: Cleanup graphics subsystem
}

void kryon_graphics_set_renderer_type(const char* renderer_type) {
    if (!renderer_type) return;
    
    if (strcmp(renderer_type, "raylib") == 0) {
        g_current_renderer = KRYON_RENDERER_RAYLIB;
    } else if (strcmp(renderer_type, "sdl2") == 0) {
        g_current_renderer = KRYON_RENDERER_SDL2;
    } else if (strcmp(renderer_type, "html") == 0) {
        g_current_renderer = KRYON_RENDERER_HTML;
    }
}

KryonRendererType kryon_renderer_get_type(void) {
    return g_current_renderer;
}