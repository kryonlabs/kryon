/*
 * Tilemap Plugin for Kryon
 *
 * Provides 2D tile-based rendering for games and grid-based UIs.
 * Supports multiple layers, tile flipping, and efficient rendering.
 */

#ifndef KRYON_TILEMAP_PLUGIN_H
#define KRYON_TILEMAP_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include "../../core/include/kryon.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Tilemap Plugin Command IDs
// ============================================================================

#define TILEMAP_CMD_CREATE    103
#define TILEMAP_CMD_SET_TILE  104
#define TILEMAP_CMD_RENDER    105

// ============================================================================
// Tilemap Constants
// ============================================================================

#define TILEMAP_MAX_LAYERS    4
#define TILEMAP_FLIP_H        0x01  // Flip horizontally
#define TILEMAP_FLIP_V        0x02  // Flip vertically
#define TILEMAP_FLIP_D        0x04  // Flip diagonally

// ============================================================================
// Tilemap Functions
// ============================================================================

/**
 * Create a new tilemap.
 *
 * @param width Width in tiles
 * @param height Height in tiles
 * @param tile_width Width of each tile in pixels
 * @param tile_height Height of each tile in pixels
 * @param layer Layer index (0-3)
 */
void kryon_tilemap_create(uint16_t width, uint16_t height,
                          uint16_t tile_width, uint16_t tile_height,
                          uint8_t layer);

/**
 * Set a tile at a specific position.
 *
 * @param x X position in tiles
 * @param y Y position in tiles
 * @param tile_id Tile ID (index into tileset)
 * @param flags Flip flags (TILEMAP_FLIP_*)
 * @param layer Layer index (0-3)
 */
void kryon_tilemap_set_tile(uint16_t x, uint16_t y, uint16_t tile_id,
                            uint8_t flags, uint8_t layer);

/**
 * Render the tilemap.
 *
 * @param x X position to render at (in pixels)
 * @param y Y position to render at (in pixels)
 * @param layer Layer index (0-3)
 */
void kryon_tilemap_render(int32_t x, int32_t y, uint8_t layer);

// ============================================================================
// Plugin Registration
// ============================================================================

/**
 * Initialize the tilemap plugin.
 * Should be called during backend initialization.
 *
 * @param backend_ctx Backend-specific context
 * @return true on success, false on failure
 */
bool kryon_tilemap_plugin_init(void* backend_ctx);

/**
 * Shutdown the tilemap plugin.
 * Should be called during backend cleanup.
 */
void kryon_tilemap_plugin_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_TILEMAP_PLUGIN_H
