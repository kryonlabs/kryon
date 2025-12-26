#ifndef KRYON_RAYLIB_H
#define KRYON_RAYLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../core/include/kryon.h"
#include <raylib.h>

// ============================================================================
// Raylib Renderer API
// ============================================================================

// Raylib renderer creation and management
kryon_renderer_t* kryon_raylib_renderer_create(uint16_t width, uint16_t height, const char* title);
void kryon_raylib_renderer_destroy(kryon_renderer_t* renderer);

// Raylib event handling
bool kryon_raylib_poll_event(kryon_event_t* event);
bool kryon_raylib_process_event(kryon_event_t* kryon_event);

// ============================================================================
// Raylib Input System API
// ============================================================================

// Mouse state
bool kryon_raylib_is_mouse_button_down(uint8_t button);
void kryon_raylib_get_mouse_position(int16_t* x, int16_t* y);

// Keyboard state
bool kryon_raylib_is_key_down(int key);

// Text input management
void kryon_raylib_start_text_input(void);
void kryon_raylib_stop_text_input(void);

// ============================================================================
// Raylib High-Level API
// ============================================================================

// Complete Raylib application setup
typedef struct {
    const char* window_title;
    uint16_t window_width;
    uint16_t window_height;
    uint32_t background_color;
    int target_fps;
    bool resizable;
} kryon_raylib_app_config_t;

typedef struct {
    kryon_renderer_t* renderer;
    bool running;
    uint16_t width, height;
} kryon_raylib_app_t;

// High-level app management
kryon_raylib_app_t* kryon_raylib_app_create(const kryon_raylib_app_config_t* config);
void kryon_raylib_app_destroy(kryon_raylib_app_t* app);
bool kryon_raylib_app_handle_events(kryon_raylib_app_t* app);
void kryon_raylib_app_begin_frame(kryon_raylib_app_t* app);
void kryon_raylib_app_end_frame(kryon_raylib_app_t* app);
void kryon_raylib_app_render_component(kryon_raylib_app_t* app, kryon_component_t* component);

// ============================================================================
// Constants and Configuration
// ============================================================================

// Default values
#define KRYON_RAYLIB_DEFAULT_WIDTH    800
#define KRYON_RAYLIB_DEFAULT_HEIGHT   600
#define KRYON_RAYLIB_DEFAULT_FPS      60

#ifdef __cplusplus
}
#endif

#endif // KRYON_RAYLIB_H
