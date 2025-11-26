// TypeScript FFI Wrapper - Helper functions for Bun FFI compatibility
// Some C functions return structs by value, which Bun FFI doesn't support directly.
// This wrapper provides pointer-returning versions.

#include <stdlib.h>
#include <string.h>
#include "../../backends/desktop/ir_desktop_renderer.h"

// Allocate and return a DesktopRendererConfig* instead of by-value
DesktopRendererConfig* ts_desktop_renderer_config_sdl3(int width, int height, const char* title) {
    DesktopRendererConfig* config = (DesktopRendererConfig*)malloc(sizeof(DesktopRendererConfig));
    if (!config) return NULL;

    *config = desktop_renderer_config_sdl3(width, height, title);
    return config;
}

// Free a config allocated by ts_desktop_renderer_config_sdl3
void ts_desktop_renderer_config_destroy(DesktopRendererConfig* config) {
    if (config) {
        free(config);
    }
}

// Allocate and return a default config
DesktopRendererConfig* ts_desktop_renderer_config_default(void) {
    DesktopRendererConfig* config = (DesktopRendererConfig*)malloc(sizeof(DesktopRendererConfig));
    if (!config) return NULL;

    *config = desktop_renderer_config_default();
    return config;
}

// Set config properties (since we can't access struct fields directly from FFI)
void ts_config_set_resizable(DesktopRendererConfig* config, int resizable) {
    if (config) config->resizable = resizable != 0;
}

void ts_config_set_fullscreen(DesktopRendererConfig* config, int fullscreen) {
    if (config) config->fullscreen = fullscreen != 0;
}

void ts_config_set_vsync(DesktopRendererConfig* config, int vsync) {
    if (config) config->vsync_enabled = vsync != 0;
}

void ts_config_set_target_fps(DesktopRendererConfig* config, int fps) {
    if (config) config->target_fps = fps;
}
