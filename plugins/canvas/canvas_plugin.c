/*
 * Canvas Plugin Implementation
 */

#define _USE_MATH_DEFINES
#include "canvas_plugin.h"
#include "../../ir/ir_plugin.h"
#include "../../core/include/kryon_canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#endif

// ============================================================================
// Global State
// ============================================================================

static void* g_backend_ctx = NULL;

// ============================================================================
// Drawing Functions (Frontend API)
// ============================================================================

void kryon_canvas_draw_circle(kryon_fp_t cx, kryon_fp_t cy, kryon_fp_t radius,
                              uint32_t color, bool filled) {
    // Get canvas command buffer
    kryon_cmd_buf_t* buf = kryon_canvas_get_command_buffer();
    if (!buf) {
        fprintf(stderr, "[canvas_plugin] No canvas command buffer available\n");
        return;
    }

    // Create circle command
    kryon_command_t cmd;
    cmd.type = CANVAS_CMD_CIRCLE;
    cmd.data.canvas_circle.cx = cx;
    cmd.data.canvas_circle.cy = cy;
    cmd.data.canvas_circle.radius = radius;
    cmd.data.canvas_circle.color = color;
    cmd.data.canvas_circle.filled = filled;

    // Add to command buffer
    if (!kryon_cmd_buf_push(buf, &cmd)) {
        fprintf(stderr, "[canvas_plugin] Failed to push circle command\n");
    }
}

void kryon_canvas_draw_ellipse(kryon_fp_t cx, kryon_fp_t cy,
                               kryon_fp_t rx, kryon_fp_t ry,
                               uint32_t color, bool filled) {
    kryon_cmd_buf_t* buf = kryon_canvas_get_command_buffer();
    if (!buf) {
        fprintf(stderr, "[canvas_plugin] No canvas command buffer available\n");
        return;
    }

    kryon_command_t cmd;
    cmd.type = CANVAS_CMD_ELLIPSE;
    cmd.data.canvas_ellipse.cx = cx;
    cmd.data.canvas_ellipse.cy = cy;
    cmd.data.canvas_ellipse.rx = rx;
    cmd.data.canvas_ellipse.ry = ry;
    cmd.data.canvas_ellipse.color = color;
    cmd.data.canvas_ellipse.filled = filled;

    if (!kryon_cmd_buf_push(buf, &cmd)) {
        fprintf(stderr, "[canvas_plugin] Failed to push ellipse command\n");
    }
}

void kryon_canvas_draw_arc(kryon_fp_t cx, kryon_fp_t cy, kryon_fp_t radius,
                           kryon_fp_t start_angle, kryon_fp_t end_angle,
                           uint32_t color) {
    kryon_cmd_buf_t* buf = kryon_canvas_get_command_buffer();
    if (!buf) {
        fprintf(stderr, "[canvas_plugin] No canvas command buffer available\n");
        return;
    }

    kryon_command_t cmd;
    cmd.type = CANVAS_CMD_ARC;
    cmd.data.canvas_arc.cx = cx;
    cmd.data.canvas_arc.cy = cy;
    cmd.data.canvas_arc.radius = radius;
    cmd.data.canvas_arc.start_angle = start_angle;
    cmd.data.canvas_arc.end_angle = end_angle;
    cmd.data.canvas_arc.color = color;

    if (!kryon_cmd_buf_push(buf, &cmd)) {
        fprintf(stderr, "[canvas_plugin] Failed to push arc command\n");
    }
}

// ============================================================================
// Plugin Handlers (Backend Rendering)
// ============================================================================

#ifdef ENABLE_SDL3

static void handle_canvas_circle(void* backend_ctx, const void* cmd_ptr) {
    SDL_Renderer* renderer = (SDL_Renderer*)backend_ctx;
    const kryon_command_t* cmd = (const kryon_command_t*)cmd_ptr;

    kryon_fp_t cx = cmd->data.canvas_circle.cx;
    kryon_fp_t cy = cmd->data.canvas_circle.cy;
    kryon_fp_t radius = cmd->data.canvas_circle.radius;
    uint32_t color = cmd->data.canvas_circle.color;
    bool filled = cmd->data.canvas_circle.filled;

    // Extract color components
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    if (filled) {
        // Draw filled circle using triangle fan
        const int segments = 64;
        SDL_FPoint center = {(float)cx, (float)cy};

        for (int i = 0; i < segments; i++) {
            float angle1 = (2.0f * M_PI * i) / segments;
            float angle2 = (2.0f * M_PI * (i + 1)) / segments;

            SDL_FPoint p1 = {
                center.x + radius * cosf(angle1),
                center.y + radius * sinf(angle1)
            };
            SDL_FPoint p2 = {
                center.x + radius * cosf(angle2),
                center.y + radius * sinf(angle2)
            };

            SDL_FPoint points[3] = {center, p1, p2};
            SDL_RenderLines(renderer, points, 3);
        }
    } else {
        // Draw circle outline
        const int segments = 64;
        SDL_FPoint* points = malloc((segments + 1) * sizeof(SDL_FPoint));
        if (!points) return;

        for (int i = 0; i <= segments; i++) {
            float angle = (2.0f * M_PI * i) / segments;
            points[i].x = cx + radius * cosf(angle);
            points[i].y = cy + radius * sinf(angle);
        }

        SDL_RenderLines(renderer, points, segments + 1);
        free(points);
    }
}

static void handle_canvas_ellipse(void* backend_ctx, const void* cmd_ptr) {
    SDL_Renderer* renderer = (SDL_Renderer*)backend_ctx;
    const kryon_command_t* cmd = (const kryon_command_t*)cmd_ptr;

    kryon_fp_t cx = cmd->data.canvas_ellipse.cx;
    kryon_fp_t cy = cmd->data.canvas_ellipse.cy;
    kryon_fp_t rx = cmd->data.canvas_ellipse.rx;
    kryon_fp_t ry = cmd->data.canvas_ellipse.ry;
    uint32_t color = cmd->data.canvas_ellipse.color;
    bool filled = cmd->data.canvas_ellipse.filled;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    const int segments = 64;

    if (filled) {
        // Draw filled ellipse using triangle fan
        SDL_FPoint center = {(float)cx, (float)cy};

        for (int i = 0; i < segments; i++) {
            float angle1 = (2.0f * M_PI * i) / segments;
            float angle2 = (2.0f * M_PI * (i + 1)) / segments;

            SDL_FPoint p1 = {
                center.x + rx * cosf(angle1),
                center.y + ry * sinf(angle1)
            };
            SDL_FPoint p2 = {
                center.x + rx * cosf(angle2),
                center.y + ry * sinf(angle2)
            };

            SDL_FPoint points[3] = {center, p1, p2};
            SDL_RenderLines(renderer, points, 3);
        }
    } else {
        // Draw ellipse outline
        SDL_FPoint* points = malloc((segments + 1) * sizeof(SDL_FPoint));
        if (!points) return;

        for (int i = 0; i <= segments; i++) {
            float angle = (2.0f * M_PI * i) / segments;
            points[i].x = cx + rx * cosf(angle);
            points[i].y = cy + ry * sinf(angle);
        }

        SDL_RenderLines(renderer, points, segments + 1);
        free(points);
    }
}

static void handle_canvas_arc(void* backend_ctx, const void* cmd_ptr) {
    SDL_Renderer* renderer = (SDL_Renderer*)backend_ctx;
    const kryon_command_t* cmd = (const kryon_command_t*)cmd_ptr;

    kryon_fp_t cx = cmd->data.canvas_arc.cx;
    kryon_fp_t cy = cmd->data.canvas_arc.cy;
    kryon_fp_t radius = cmd->data.canvas_arc.radius;
    kryon_fp_t start_angle = cmd->data.canvas_arc.start_angle;
    kryon_fp_t end_angle = cmd->data.canvas_arc.end_angle;
    uint32_t color = cmd->data.canvas_arc.color;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    // Draw arc as line segments
    const int segments = 32;
    SDL_FPoint* points = malloc((segments + 1) * sizeof(SDL_FPoint));
    if (!points) return;

    float angle_span = end_angle - start_angle;

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float angle_deg = start_angle + angle_span * t;
        float angle_rad = angle_deg * M_PI / 180.0f;

        points[i].x = cx + radius * cosf(angle_rad);
        points[i].y = cy + radius * sinf(angle_rad);
    }

    SDL_RenderLines(renderer, points, segments + 1);
    free(points);
}

#endif // ENABLE_SDL3

// ============================================================================
// Plugin Registration
// ============================================================================

bool kryon_canvas_plugin_init(void* backend_ctx) {
    g_backend_ctx = backend_ctx;

#ifdef ENABLE_SDL3
    // Register plugin handlers
    if (!ir_plugin_register_handler(CANVAS_CMD_CIRCLE, handle_canvas_circle)) {
        fprintf(stderr, "[canvas_plugin] Failed to register circle handler\n");
        return false;
    }

    if (!ir_plugin_register_handler(CANVAS_CMD_ELLIPSE, handle_canvas_ellipse)) {
        fprintf(stderr, "[canvas_plugin] Failed to register ellipse handler\n");
        return false;
    }

    if (!ir_plugin_register_handler(CANVAS_CMD_ARC, handle_canvas_arc)) {
        fprintf(stderr, "[canvas_plugin] Failed to register arc handler\n");
        return false;
    }

    // Set backend capabilities
    IRBackendCapabilities caps = {
        .has_2d_shapes = true,
        .has_transforms = false,
        .has_hardware_accel = true,
        .has_blend_modes = true,
        .has_antialiasing = true,
        .has_gradients = false,
        .has_text_rendering = true,
        .has_3d_rendering = false
    };
    ir_plugin_set_backend_capabilities(&caps);

    fprintf(stderr, "[canvas_plugin] Canvas plugin initialized for SDL3 backend\n");
    return true;
#else
    fprintf(stderr, "[canvas_plugin] Canvas plugin requires SDL3 backend\n");
    return false;
#endif
}

void kryon_canvas_plugin_shutdown(void) {
    ir_plugin_unregister_handler(CANVAS_CMD_CIRCLE);
    ir_plugin_unregister_handler(CANVAS_CMD_ELLIPSE);
    ir_plugin_unregister_handler(CANVAS_CMD_ARC);

    g_backend_ctx = NULL;

    fprintf(stderr, "[canvas_plugin] Canvas plugin shutdown\n");
}
