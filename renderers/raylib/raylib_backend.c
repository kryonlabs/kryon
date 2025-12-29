#define _GNU_SOURCE
#include "../../core/include/kryon.h"
#include "../../ir/ir_native_canvas.h"
#include <raylib.h>
#include <rlgl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "raylib_backend.h"
#include "raylib_effects.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Raylib Backend Implementation
// ============================================================================

// Raylib-specific backend data
typedef struct {
    uint16_t width, height;
    const char* title;
    bool initialized;
    bool window_should_close;

    // Window configuration
    uint16_t config_width, config_height;
    char* config_title;
    int target_fps;
} kryon_raylib_backend_t;

// Forward declarations
static bool raylib_init(kryon_renderer_t* renderer, void* native_window);
static void raylib_shutdown(kryon_renderer_t* renderer);
static void raylib_begin_frame(kryon_renderer_t* renderer);
static void raylib_end_frame(kryon_renderer_t* renderer);
static void raylib_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf);
static void raylib_swap_buffers(kryon_renderer_t* renderer);
static void raylib_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height);
static void raylib_set_clear_color(kryon_renderer_t* renderer, uint32_t color);

// Trace helper --------------------------------------------------------------
static bool kryon_raylib_should_trace(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
        const char* env = getenv("KRYON_TRACE_COMMANDS");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
    }
    return enabled;
}

static void kryon_raylib_trace_command(const kryon_command_t* cmd) {
    if (!kryon_raylib_should_trace()) return;

    printf("[RAYLIB] Executing command type: %d\n", cmd->type);
}

// Utility: Convert RGBA to raylib Color
static Color rgba_to_color(uint32_t rgba) {
    return (Color){
        .r = (rgba >> 24) & 0xFF,
        .g = (rgba >> 16) & 0xFF,
        .b = (rgba >> 8) & 0xFF,
        .a = rgba & 0xFF
    };
}

// ============================================================================
// Renderer Operations Implementation
// ============================================================================

static bool raylib_init(kryon_renderer_t* renderer, void* native_window) {
    (void)native_window; // Unused - raylib manages its own window

    kryon_raylib_backend_t* backend = (kryon_raylib_backend_t*)malloc(sizeof(kryon_raylib_backend_t));
    if (!backend) {
        fprintf(stderr, "[RAYLIB] Failed to allocate backend data\n");
        return false;
    }

    backend->width = renderer->width;
    backend->height = renderer->height;
    backend->title = renderer->backend_data ? ((kryon_raylib_backend_t*)renderer->backend_data)->config_title : "Kryon App";
    backend->target_fps = KRYON_RAYLIB_DEFAULT_FPS;
    backend->initialized = false;
    backend->window_should_close = false;

    // Initialize raylib window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(backend->width, backend->height, backend->title);
    SetTargetFPS(backend->target_fps);

    if (!IsWindowReady()) {
        fprintf(stderr, "[RAYLIB] Failed to create window\n");
        free(backend);
        return false;
    }

    // Disable ESC key closing the window (let app handle it)
    SetExitKey(KEY_NULL);

    backend->initialized = true;
    renderer->backend_data = backend;

    printf("[RAYLIB] Initialized: %dx%d \"%s\"\n", backend->width, backend->height, backend->title);

    return true;
}

static void raylib_shutdown(kryon_renderer_t* renderer) {
    if (!renderer || !renderer->backend_data) return;

    kryon_raylib_backend_t* backend = (kryon_raylib_backend_t*)renderer->backend_data;

    if (backend->initialized) {
        CloseWindow();
    }

    if (backend->config_title) {
        free(backend->config_title);
    }

    free(backend);
    renderer->backend_data = NULL;

    printf("[RAYLIB] Shutdown complete\n");
}

static void raylib_begin_frame(kryon_renderer_t* renderer) {
    if (!renderer || !renderer->backend_data) return;

    kryon_raylib_backend_t* backend = (kryon_raylib_backend_t*)renderer->backend_data;

    // Check if window should close
    backend->window_should_close = WindowShouldClose();

    // Update dimensions if window was resized
    int current_width = GetScreenWidth();
    int current_height = GetScreenHeight();

    if (current_width != backend->width || current_height != backend->height) {
        backend->width = current_width;
        backend->height = current_height;
        renderer->width = current_width;
        renderer->height = current_height;
    }

    BeginDrawing();

    // Clear with renderer's clear color
    Color clear_color = rgba_to_color(renderer->clear_color);
    ClearBackground(clear_color);
}

static void raylib_end_frame(kryon_renderer_t* renderer) {
    (void)renderer;
    EndDrawing();
}

static void raylib_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf) {
    if (!renderer || !buf) return;

    kryon_cmd_iterator_t iter = kryon_cmd_iter_create(buf);

    while (kryon_cmd_iter_has_next(&iter)) {
        kryon_command_t cmd;
        kryon_cmd_iter_next(&iter, &cmd);

        kryon_raylib_trace_command(&cmd);

        switch (cmd.type) {
            case KRYON_CMD_DRAW_RECT: {
                Color color = rgba_to_color(cmd.data.draw_rect.color);
                DrawRectangle(
                    (int)cmd.data.draw_rect.x,
                    (int)cmd.data.draw_rect.y,
                    (int)cmd.data.draw_rect.w,
                    (int)cmd.data.draw_rect.h,
                    color
                );
                break;
            }

            case KRYON_CMD_DRAW_TEXT: {
                Color color = rgba_to_color(cmd.data.draw_text.color);
                DrawText(
                    cmd.data.draw_text.text,
                    (int)cmd.data.draw_text.x,
                    (int)cmd.data.draw_text.y,
                    cmd.data.draw_text.font_size,
                    color
                );
                break;
            }

            case KRYON_CMD_DRAW_LINE: {
                Color color = rgba_to_color(cmd.data.draw_line.color);
                DrawLine(
                    (int)cmd.data.draw_line.x1,
                    (int)cmd.data.draw_line.y1,
                    (int)cmd.data.draw_line.x2,
                    (int)cmd.data.draw_line.y2,
                    color
                );
                break;
            }

            case KRYON_CMD_DRAW_ARC: {
                Color color = rgba_to_color(cmd.data.draw_arc.color);
                DrawCircleSector(
                    (Vector2){(float)cmd.data.draw_arc.cx, (float)cmd.data.draw_arc.cy},
                    (float)cmd.data.draw_arc.radius,
                    (float)cmd.data.draw_arc.start_angle,
                    (float)cmd.data.draw_arc.end_angle,
                    16, // segments
                    color
                );
                break;
            }

            case KRYON_CMD_SET_CLIP: {
                BeginScissorMode(
                    (int)cmd.data.set_clip.x,
                    (int)cmd.data.set_clip.y,
                    (int)cmd.data.set_clip.w,
                    (int)cmd.data.set_clip.h
                );
                break;
            }

            case KRYON_CMD_POP_CLIP: {
                EndScissorMode();
                break;
            }

            case KRYON_CMD_DRAW_POLYGON: {
                // Draw polygon using raylib
                // Note: raylib doesn't have a direct polygon fill, so we'll use triangles
                if (cmd.data.draw_polygon.filled && cmd.data.draw_polygon.vertex_count >= 3) {
                    Color color = rgba_to_color(cmd.data.draw_polygon.color);
                    // Simple triangle fan approach
                    for (uint16_t i = 1; i < cmd.data.draw_polygon.vertex_count - 1; i++) {
                        DrawTriangle(
                            (Vector2){cmd.data.draw_polygon.vertices[0], cmd.data.draw_polygon.vertices[1]},
                            (Vector2){cmd.data.draw_polygon.vertices[i*2], cmd.data.draw_polygon.vertices[i*2+1]},
                            (Vector2){cmd.data.draw_polygon.vertices[(i+1)*2], cmd.data.draw_polygon.vertices[(i+1)*2+1]},
                            color
                        );
                    }
                }
                break;
            }

            case KRYON_CMD_DRAW_ROUNDED_RECT: {
                Rectangle rect = {
                    .x = cmd.data.draw_rounded_rect.x,
                    .y = cmd.data.draw_rounded_rect.y,
                    .width = cmd.data.draw_rounded_rect.w,
                    .height = cmd.data.draw_rounded_rect.h
                };
                raylib_render_rounded_rect(rect,
                    cmd.data.draw_rounded_rect.radius,
                    cmd.data.draw_rounded_rect.color);
                break;
            }

            case KRYON_CMD_DRAW_GRADIENT: {
                Rectangle rect = {
                    .x = cmd.data.draw_gradient.x,
                    .y = cmd.data.draw_gradient.y,
                    .width = cmd.data.draw_gradient.w,
                    .height = cmd.data.draw_gradient.h
                };

                // Convert gradient stops
                Raylib_GradientStop stops[8];
                for (int i = 0; i < cmd.data.draw_gradient.stop_count && i < 8; i++) {
                    uint32_t color = cmd.data.draw_gradient.stops[i].color;
                    stops[i].position = cmd.data.draw_gradient.stops[i].position;
                    stops[i].r = (color >> 24) & 0xFF;
                    stops[i].g = (color >> 16) & 0xFF;
                    stops[i].b = (color >> 8) & 0xFF;
                    stops[i].a = color & 0xFF;
                }

                raylib_render_gradient(
                    rect,
                    (Raylib_GradientType)cmd.data.draw_gradient.gradient_type,
                    cmd.data.draw_gradient.angle,
                    stops,
                    cmd.data.draw_gradient.stop_count,
                    1.0f  // Opacity handled separately
                );
                break;
            }

            case KRYON_CMD_DRAW_SHADOW: {
                Rectangle rect = {
                    .x = cmd.data.draw_shadow.x,
                    .y = cmd.data.draw_shadow.y,
                    .width = cmd.data.draw_shadow.w,
                    .height = cmd.data.draw_shadow.h
                };
                raylib_render_shadow(
                    rect,
                    cmd.data.draw_shadow.offset_x,
                    cmd.data.draw_shadow.offset_y,
                    cmd.data.draw_shadow.blur_radius,
                    cmd.data.draw_shadow.spread_radius,
                    cmd.data.draw_shadow.color,
                    cmd.data.draw_shadow.inset
                );
                break;
            }

            case KRYON_CMD_DRAW_TEXT_SHADOW: {
                // TODO: Implement text shadow with blur
                // For now, render shadow as regular text with offset
                Color color = rgba_to_color(cmd.data.draw_text_shadow.color);
                DrawText(
                    cmd.data.draw_text_shadow.text,
                    (int)(cmd.data.draw_text_shadow.x + cmd.data.draw_text_shadow.offset_x),
                    (int)(cmd.data.draw_text_shadow.y + cmd.data.draw_text_shadow.offset_y),
                    20,  // Default font size for shadow
                    color
                );
                break;
            }

            case KRYON_CMD_DRAW_IMAGE: {
                // TODO: Implement image rendering
                // Requires image resource loading system
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] DRAW_IMAGE not yet implemented\n");
                }
                break;
            }

            case KRYON_CMD_SET_OPACITY: {
                // TODO: Implement global opacity
                // Raylib doesn't have built-in opacity stack - would need to apply to all colors
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] SET_OPACITY: %.2f\n", cmd.data.set_opacity.opacity);
                }
                break;
            }

            case KRYON_CMD_PUSH_OPACITY: {
                // TODO: Implement opacity stack
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] PUSH_OPACITY\n");
                }
                break;
            }

            case KRYON_CMD_POP_OPACITY: {
                // TODO: Implement opacity stack
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] POP_OPACITY\n");
                }
                break;
            }

            case KRYON_CMD_SET_BLEND_MODE: {
                // Map Kryon blend modes to Raylib blend modes
                switch (cmd.data.set_blend_mode.blend_mode) {
                    case 0: // Normal/Alpha
                        rlSetBlendMode(BLEND_ALPHA);
                        break;
                    case 1: // Additive
                        rlSetBlendMode(BLEND_ADDITIVE);
                        break;
                    case 2: // Multiply
                        rlSetBlendMode(BLEND_MULTIPLIED);
                        break;
                    default:
                        rlSetBlendMode(BLEND_ALPHA);
                        break;
                }
                break;
            }

            case KRYON_CMD_BEGIN_PASS: {
                // TODO: Implement multi-pass rendering
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] BEGIN_PASS: %d\n", cmd.data.begin_pass.pass_id);
                }
                break;
            }

            case KRYON_CMD_END_PASS: {
                // TODO: Implement multi-pass rendering
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] END_PASS\n");
                }
                break;
            }

            case KRYON_CMD_DRAW_TEXT_WRAPPED: {
                // TODO: Implement word-wrapped text
                // For now, render as regular text
                Color color = rgba_to_color(cmd.data.draw_text_wrapped.color);
                DrawText(
                    cmd.data.draw_text_wrapped.text,
                    (int)cmd.data.draw_text_wrapped.x,
                    (int)cmd.data.draw_text_wrapped.y,
                    cmd.data.draw_text_wrapped.font_size,
                    color
                );
                break;
            }

            case KRYON_CMD_DRAW_TEXT_FADE: {
                // TODO: Implement fade effect
                // For now, render as regular text
                Color color = rgba_to_color(cmd.data.draw_text_fade.color);
                DrawText(
                    cmd.data.draw_text_fade.text,
                    (int)cmd.data.draw_text_fade.x,
                    (int)cmd.data.draw_text_fade.y,
                    cmd.data.draw_text_fade.font_size,
                    color
                );
                break;
            }

            case KRYON_CMD_LOAD_FONT: {
                // TODO: Implement font resource loading
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] LOAD_FONT: %s (id: %d, size: %d)\n",
                        cmd.data.load_font.path,
                        cmd.data.load_font.font_id,
                        cmd.data.load_font.size);
                }
                break;
            }

            case KRYON_CMD_LOAD_IMAGE: {
                // TODO: Implement image resource loading
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] LOAD_IMAGE: %s (id: %d)\n",
                        cmd.data.load_image.path,
                        cmd.data.load_image.image_id);
                }
                break;
            }

            default:
                if (kryon_raylib_should_trace()) {
                    printf("[RAYLIB] Unhandled command type: %d\n", cmd.type);
                }
                break;
        }
    }
}

static void raylib_swap_buffers(kryon_renderer_t* renderer) {
    // Raylib handles buffer swapping in EndDrawing()
    // This is a no-op
    (void)renderer;
}

static void raylib_get_dimensions(kryon_renderer_t* renderer, uint16_t* w, uint16_t* h) {
    if (!renderer) return;

    *w = GetScreenWidth();
    *h = GetScreenHeight();
}

static void raylib_set_clear_color(kryon_renderer_t* renderer, uint32_t color) {
    if (!renderer) return;
    renderer->clear_color = color;
}

// ============================================================================
// Renderer Operations Table
// ============================================================================

static const kryon_renderer_ops_t kryon_raylib_ops = {
    .init = raylib_init,
    .shutdown = raylib_shutdown,
    .begin_frame = raylib_begin_frame,
    .end_frame = raylib_end_frame,
    .execute_commands = raylib_execute_commands,
    .swap_buffers = raylib_swap_buffers,
    .get_dimensions = raylib_get_dimensions,
    .set_clear_color = raylib_set_clear_color
};

// ============================================================================
// Public API Implementation
// ============================================================================

kryon_renderer_t* kryon_raylib_renderer_create(uint16_t width, uint16_t height, const char* title) {
    kryon_renderer_t* renderer = (kryon_renderer_t*)malloc(sizeof(kryon_renderer_t));
    if (!renderer) {
        fprintf(stderr, "[RAYLIB] Failed to allocate renderer\n");
        return NULL;
    }

    memset(renderer, 0, sizeof(kryon_renderer_t));

    renderer->ops = &kryon_raylib_ops;
    renderer->width = width;
    renderer->height = height;
    renderer->clear_color = 0x000000FF; // Black with full alpha

    // Store config in temporary backend data
    kryon_raylib_backend_t* temp_backend = (kryon_raylib_backend_t*)malloc(sizeof(kryon_raylib_backend_t));
    temp_backend->config_width = width;
    temp_backend->config_height = height;
    temp_backend->config_title = title ? strdup(title) : strdup("Kryon App");
    renderer->backend_data = temp_backend;

    return renderer;
}

void kryon_raylib_renderer_destroy(kryon_renderer_t* renderer) {
    if (!renderer) return;

    if (renderer->ops && renderer->ops->shutdown) {
        renderer->ops->shutdown(renderer);
    }

    free(renderer);
}

// ============================================================================
// Event Handling
// ============================================================================

bool kryon_raylib_poll_event(kryon_event_t* event) {
    if (!event) return false;

    // Check if window should close
    if (WindowShouldClose()) {
        event->type = 0; // KRYON_EVENT_QUIT equivalent
        return true;
    }

    // TODO: Implement full event handling
    // For now, just return false to indicate no events
    return false;
}

bool kryon_raylib_process_event(kryon_event_t* kryon_event) {
    // TODO: Implement event processing
    (void)kryon_event;
    return false;
}

// ============================================================================
// Input System
// ============================================================================

bool kryon_raylib_is_mouse_button_down(uint8_t button) {
    return IsMouseButtonDown(button);
}

void kryon_raylib_get_mouse_position(int16_t* x, int16_t* y) {
    Vector2 pos = GetMousePosition();
    *x = (int16_t)pos.x;
    *y = (int16_t)pos.y;
}

bool kryon_raylib_is_key_down(int key) {
    return IsKeyDown(key);
}

void kryon_raylib_start_text_input(void) {
    // Raylib doesn't have explicit text input start/stop
    // Text input is always active
}

void kryon_raylib_stop_text_input(void) {
    // Raylib doesn't have explicit text input start/stop
}

// ============================================================================
// High-Level Application API
// ============================================================================

kryon_raylib_app_t* kryon_raylib_app_create(const kryon_raylib_app_config_t* config) {
    if (!config) return NULL;

    kryon_raylib_app_t* app = (kryon_raylib_app_t*)malloc(sizeof(kryon_raylib_app_t));
    if (!app) return NULL;

    app->width = config->window_width;
    app->height = config->window_height;
    app->running = true;

    // Create renderer
    app->renderer = kryon_raylib_renderer_create(
        config->window_width,
        config->window_height,
        config->window_title
    );

    if (!app->renderer) {
        free(app);
        return NULL;
    }

    // Initialize renderer
    if (!app->renderer->ops->init(app->renderer, NULL)) {
        free(app->renderer);
        free(app);
        return NULL;
    }

    // Set clear color
    app->renderer->ops->set_clear_color(app->renderer, config->background_color);

    // Set target FPS
    kryon_raylib_backend_t* backend = (kryon_raylib_backend_t*)app->renderer->backend_data;
    backend->target_fps = config->target_fps;
    SetTargetFPS(config->target_fps);

    return app;
}

void kryon_raylib_app_destroy(kryon_raylib_app_t* app) {
    if (!app) return;

    if (app->renderer) {
        kryon_raylib_renderer_destroy(app->renderer);
    }

    free(app);
}

bool kryon_raylib_app_handle_events(kryon_raylib_app_t* app) {
    if (!app) return false;

    kryon_raylib_backend_t* backend = (kryon_raylib_backend_t*)app->renderer->backend_data;

    if (backend->window_should_close || WindowShouldClose()) {
        app->running = false;
        return false;
    }

    return true;
}

void kryon_raylib_app_begin_frame(kryon_raylib_app_t* app) {
    if (!app || !app->renderer) return;
    app->renderer->ops->begin_frame(app->renderer);
}

void kryon_raylib_app_end_frame(kryon_raylib_app_t* app) {
    if (!app || !app->renderer) return;
    app->renderer->ops->end_frame(app->renderer);
}

void kryon_raylib_app_render_component(kryon_raylib_app_t* app, kryon_component_t* component) {
    // TODO: Implement component rendering
    (void)app;
    (void)component;
}
