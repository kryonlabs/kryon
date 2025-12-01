/*
 * Canvas Plugin for Kryon
 *
 * Provides 2D drawing primitives (circles, ellipses, arcs) that work
 * across all frontends and backends via the plugin system.
 */

#ifndef KRYON_CANVAS_PLUGIN_H
#define KRYON_CANVAS_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include "../../core/include/kryon.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Canvas Plugin Command IDs
// ============================================================================

// These match the command IDs in kryon.h
#define CANVAS_CMD_CIRCLE   100
#define CANVAS_CMD_ELLIPSE  101
#define CANVAS_CMD_ARC      102

// ============================================================================
// Canvas Drawing Functions
// ============================================================================

/**
 * Draw a circle on the canvas.
 *
 * @param cx Center X coordinate (fixed-point)
 * @param cy Center Y coordinate (fixed-point)
 * @param radius Radius (fixed-point)
 * @param color Color in RGBA8888 format
 * @param filled Whether to fill the circle
 */
void kryon_canvas_draw_circle(kryon_fp_t cx, kryon_fp_t cy, kryon_fp_t radius,
                              uint32_t color, bool filled);

/**
 * Draw an ellipse on the canvas.
 *
 * @param cx Center X coordinate (fixed-point)
 * @param cy Center Y coordinate (fixed-point)
 * @param rx Horizontal radius (fixed-point)
 * @param ry Vertical radius (fixed-point)
 * @param color Color in RGBA8888 format
 * @param filled Whether to fill the ellipse
 */
void kryon_canvas_draw_ellipse(kryon_fp_t cx, kryon_fp_t cy,
                               kryon_fp_t rx, kryon_fp_t ry,
                               uint32_t color, bool filled);

/**
 * Draw an arc on the canvas.
 *
 * @param cx Center X coordinate (fixed-point)
 * @param cy Center Y coordinate (fixed-point)
 * @param radius Radius (fixed-point)
 * @param start_angle Start angle in degrees (fixed-point)
 * @param end_angle End angle in degrees (fixed-point)
 * @param color Color in RGBA8888 format
 */
void kryon_canvas_draw_arc(kryon_fp_t cx, kryon_fp_t cy, kryon_fp_t radius,
                           kryon_fp_t start_angle, kryon_fp_t end_angle,
                           uint32_t color);

// ============================================================================
// Plugin Registration
// ============================================================================

/**
 * Register the canvas plugin with the backend.
 * Should be called during backend initialization.
 *
 * @param backend_ctx Backend-specific context (e.g., SDL_Renderer*)
 * @return true on success, false on failure
 */
bool kryon_canvas_plugin_init(void* backend_ctx);

/**
 * Unregister the canvas plugin.
 * Should be called during backend shutdown.
 */
void kryon_canvas_plugin_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_CANVAS_PLUGIN_H
