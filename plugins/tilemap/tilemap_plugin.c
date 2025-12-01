/*
 * Tilemap Plugin Implementation
 */

#include "tilemap_plugin.h"
#include "../../ir/ir_plugin.h"
#include "../../core/include/kryon_canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Tilemap Data Structures
// ============================================================================

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t tile_width;
    uint16_t tile_height;
    uint16_t* tiles;      // Tile IDs
    uint8_t* flags;       // Flip flags per tile
    bool active;
} TilemapLayer;

typedef struct {
    TilemapLayer layers[TILEMAP_MAX_LAYERS];
    void* backend_ctx;
} TilemapSystem;

static TilemapSystem g_tilemap_system = {0};

// ============================================================================
// Frontend API Functions
// ============================================================================

void kryon_tilemap_create(uint16_t width, uint16_t height,
                          uint16_t tile_width, uint16_t tile_height,
                          uint8_t layer) {
    if (layer >= TILEMAP_MAX_LAYERS) {
        fprintf(stderr, "[tilemap] Invalid layer: %u\n", layer);
        return;
    }

    TilemapLayer* lyr = &g_tilemap_system.layers[layer];

    // Free existing data if any
    if (lyr->tiles) free(lyr->tiles);
    if (lyr->flags) free(lyr->flags);

    // Allocate new tilemap
    size_t tile_count = width * height;
    lyr->tiles = (uint16_t*)calloc(tile_count, sizeof(uint16_t));
    lyr->flags = (uint8_t*)calloc(tile_count, sizeof(uint8_t));

    if (!lyr->tiles || !lyr->flags) {
        fprintf(stderr, "[tilemap] Failed to allocate tilemap\n");
        if (lyr->tiles) free(lyr->tiles);
        if (lyr->flags) free(lyr->flags);
        lyr->tiles = NULL;
        lyr->flags = NULL;
        return;
    }

    lyr->width = width;
    lyr->height = height;
    lyr->tile_width = tile_width;
    lyr->tile_height = tile_height;
    lyr->active = true;

    fprintf(stderr, "[tilemap] Created tilemap layer %u: %ux%u tiles (%ux%u pixels)\n",
            layer, width, height, tile_width, tile_height);
}

void kryon_tilemap_set_tile(uint16_t x, uint16_t y, uint16_t tile_id,
                            uint8_t flags, uint8_t layer) {
    if (layer >= TILEMAP_MAX_LAYERS) return;

    TilemapLayer* lyr = &g_tilemap_system.layers[layer];
    if (!lyr->active || !lyr->tiles) return;

    if (x >= lyr->width || y >= lyr->height) {
        fprintf(stderr, "[tilemap] Tile position out of bounds: (%u, %u)\n", x, y);
        return;
    }

    size_t idx = y * lyr->width + x;
    lyr->tiles[idx] = tile_id;
    lyr->flags[idx] = flags;
}

void kryon_tilemap_render(int32_t x, int32_t y, uint8_t layer) {
    if (layer >= TILEMAP_MAX_LAYERS) return;

    TilemapLayer* lyr = &g_tilemap_system.layers[layer];
    if (!lyr->active || !lyr->tiles) return;

    // Get canvas command buffer
    kryon_cmd_buf_t* buf = kryon_canvas_get_command_buffer();
    if (!buf) return;

    // Create render command
    kryon_command_t cmd;
    cmd.type = TILEMAP_CMD_RENDER;
    cmd.data.tilemap_render.x = x;
    cmd.data.tilemap_render.y = y;
    cmd.data.tilemap_render.layer = layer;
    cmd.data.tilemap_render.width = lyr->width;
    cmd.data.tilemap_render.height = lyr->height;
    cmd.data.tilemap_render.tile_width = lyr->tile_width;
    cmd.data.tilemap_render.tile_height = lyr->tile_height;

    if (!kryon_cmd_buf_push(buf, &cmd)) {
        fprintf(stderr, "[tilemap] Failed to push render command\n");
    }
}

// ============================================================================
// Plugin Handlers (Backend Rendering)
// ============================================================================

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>

static void handle_tilemap_render(void* backend_ctx, const void* cmd_ptr) {
    SDL_Renderer* renderer = (SDL_Renderer*)backend_ctx;
    const kryon_command_t* cmd = (const kryon_command_t*)cmd_ptr;

    uint8_t layer = cmd->data.tilemap_render.layer;
    if (layer >= TILEMAP_MAX_LAYERS) return;

    TilemapLayer* lyr = &g_tilemap_system.layers[layer];
    if (!lyr->active || !lyr->tiles) return;

    int32_t render_x = cmd->data.tilemap_render.x;
    int32_t render_y = cmd->data.tilemap_render.y;

    // For now, render tiles as colored rectangles
    // In a real implementation, this would use a tileset texture
    for (uint16_t ty = 0; ty < lyr->height; ty++) {
        for (uint16_t tx = 0; tx < lyr->width; tx++) {
            size_t idx = ty * lyr->width + tx;
            uint16_t tile_id = lyr->tiles[idx];

            if (tile_id == 0) continue;  // Skip empty tiles

            // Calculate tile position
            int32_t px = render_x + (tx * lyr->tile_width);
            int32_t py = render_y + (ty * lyr->tile_height);

            // Generate color from tile ID (for visualization)
            uint8_t r = (tile_id * 50) % 256;
            uint8_t g = (tile_id * 100) % 256;
            uint8_t b = (tile_id * 150) % 256;

            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_FRect rect = {
                (float)px,
                (float)py,
                (float)lyr->tile_width,
                (float)lyr->tile_height
            };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

#endif // ENABLE_SDL3

// ============================================================================
// Plugin Registration
// ============================================================================

bool kryon_tilemap_plugin_init(void* backend_ctx) {
    g_tilemap_system.backend_ctx = backend_ctx;

    // Clear layers
    memset(&g_tilemap_system.layers, 0, sizeof(g_tilemap_system.layers));

#ifdef ENABLE_SDL3
    // Register plugin handlers
    if (!ir_plugin_register_handler(TILEMAP_CMD_RENDER, handle_tilemap_render)) {
        fprintf(stderr, "[tilemap] Failed to register render handler\n");
        return false;
    }

    fprintf(stderr, "[tilemap] Tilemap plugin initialized for SDL3 backend\n");
    return true;
#else
    fprintf(stderr, "[tilemap] Tilemap plugin requires SDL3 backend\n");
    return false;
#endif
}

void kryon_tilemap_plugin_shutdown(void) {
    // Free all layer data
    for (int i = 0; i < TILEMAP_MAX_LAYERS; i++) {
        TilemapLayer* lyr = &g_tilemap_system.layers[i];
        if (lyr->tiles) {
            free(lyr->tiles);
            lyr->tiles = NULL;
        }
        if (lyr->flags) {
            free(lyr->flags);
            lyr->flags = NULL;
        }
        lyr->active = false;
    }

    ir_plugin_unregister_handler(TILEMAP_CMD_RENDER);
    g_tilemap_system.backend_ctx = NULL;

    fprintf(stderr, "[tilemap] Tilemap plugin shutdown\n");
}
