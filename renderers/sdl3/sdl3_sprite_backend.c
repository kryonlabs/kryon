/*
 * SDL3 Sprite Backend Integration
 *
 * Bridges the IR sprite system with SDL3 sprite batch renderer.
 * Implements texture creation/destruction callbacks and manages sprite batching.
 */

#include "../../ir/ir_sprite.h"
#include "sdl3_sprite_batch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Try different SDL3 header locations
#if defined(__has_include)
  #if __has_include(<SDL3/SDL.h>)
    #include <SDL3/SDL.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
  #else
    #error "SDL3 headers not found"
  #endif
#else
  #include <SDL3/SDL.h>
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

static void sdl3_sprite_backend_draw(IRSpriteID sprite_id, float x, float y,
                                     float rotation, float scale_x, float scale_y,
                                     float pivot_x, float pivot_y,
                                     bool flip_x, bool flip_y, uint32_t tint);

// ============================================================================
// Texture Management
// ============================================================================

// Maximum textures (matches IR_MAX_ATLASES * 2 for safety)
#define SDL3_MAX_TEXTURES 32

// Texture storage
typedef struct {
    SDL_Texture* texture;
    uint16_t width;
    uint16_t height;
    bool in_use;
} SDL3TextureHandle;

// Global texture registry
static struct {
    SDL_Renderer* renderer;
    SDL3TextureHandle textures[SDL3_MAX_TEXTURES];
    uint32_t next_texture_id;
    SDL3SpriteBatch sprite_batch;
    bool initialized;
} g_sdl3_sprite_backend = {0};

// ============================================================================
// Backend Callbacks
// ============================================================================

/**
 * Create texture from RGBA pixel data
 * Called by sprite atlas when packing is complete
 */
static IRTextureID sdl3_sprite_create_texture(uint16_t width, uint16_t height, uint8_t* pixels) {
    if (!g_sdl3_sprite_backend.initialized || !g_sdl3_sprite_backend.renderer || !pixels) {
        fprintf(stderr, "[SDL3 Sprite] Cannot create texture: backend not initialized\n");
        return IR_INVALID_TEXTURE;
    }

    // Find free texture slot
    uint32_t texture_id = IR_INVALID_TEXTURE;
    for (uint32_t i = 0; i < SDL3_MAX_TEXTURES; i++) {
        if (!g_sdl3_sprite_backend.textures[i].in_use) {
            texture_id = i + 1;  // IDs start at 1 (0 is invalid)
            break;
        }
    }

    if (texture_id == IR_INVALID_TEXTURE) {
        fprintf(stderr, "[SDL3 Sprite] Out of texture slots (max %d)\n", SDL3_MAX_TEXTURES);
        return IR_INVALID_TEXTURE;
    }

    // Create SDL texture (ABGR8888 is standard 32-bit RGBA format in SDL3)
    SDL_Texture* texture = SDL_CreateTexture(
        g_sdl3_sprite_backend.renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STATIC,
        width,
        height
    );

    if (!texture) {
        fprintf(stderr, "[SDL3 Sprite] Failed to create texture: %s\n", SDL_GetError());
        return IR_INVALID_TEXTURE;
    }

    // Upload pixel data (SDL3 returns bool: true on success, false on failure)
    bool result = SDL_UpdateTexture(texture, NULL, pixels, width * 4);
    if (!result) {
        fprintf(stderr, "[SDL3 Sprite] Failed to upload texture data: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        return IR_INVALID_TEXTURE;
    }

    // Enable blending for transparency
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    // Store texture handle
    uint32_t index = texture_id - 1;
    g_sdl3_sprite_backend.textures[index].texture = texture;
    g_sdl3_sprite_backend.textures[index].width = width;
    g_sdl3_sprite_backend.textures[index].height = height;
    g_sdl3_sprite_backend.textures[index].in_use = true;

    return texture_id;
}

/**
 * Destroy texture
 * Called when sprite atlas is destroyed
 */
static void sdl3_sprite_destroy_texture(IRTextureID texture_id) {
    if (texture_id == IR_INVALID_TEXTURE || texture_id > SDL3_MAX_TEXTURES) {
        return;
    }

    uint32_t index = texture_id - 1;
    SDL3TextureHandle* handle = &g_sdl3_sprite_backend.textures[index];

    if (handle->in_use && handle->texture) {
        SDL_DestroyTexture(handle->texture);
        memset(handle, 0, sizeof(SDL3TextureHandle));
    }
}

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize SDL3 sprite backend
 * Must be called after SDL renderer is created
 */
void sdl3_sprite_backend_init(SDL_Renderer* renderer) {
    if (!renderer) {
        fprintf(stderr, "[SDL3 Sprite] Cannot initialize: NULL renderer\n");
        return;
    }

    if (g_sdl3_sprite_backend.initialized) {
        fprintf(stderr, "[SDL3 Sprite] Already initialized\n");
        return;
    }

    memset(&g_sdl3_sprite_backend, 0, sizeof(g_sdl3_sprite_backend));
    g_sdl3_sprite_backend.renderer = renderer;
    g_sdl3_sprite_backend.next_texture_id = 1;

    // Initialize sprite batch
    sdl3_sprite_batch_init(&g_sdl3_sprite_backend.sprite_batch, renderer);

    // Initialize IR sprite system
    ir_sprite_init();

    // Register backend callbacks
    ir_sprite_register_backend(sdl3_sprite_create_texture, sdl3_sprite_destroy_texture,
                               sdl3_sprite_backend_draw);

    g_sdl3_sprite_backend.initialized = true;

    printf("[SDL3 Sprite] Backend initialized\n");
}

/**
 * Shutdown SDL3 sprite backend
 */
void sdl3_sprite_backend_shutdown(void) {
    if (!g_sdl3_sprite_backend.initialized) {
        return;
    }

    // Destroy all textures
    for (uint32_t i = 0; i < SDL3_MAX_TEXTURES; i++) {
        if (g_sdl3_sprite_backend.textures[i].in_use) {
            sdl3_sprite_destroy_texture(i + 1);
        }
    }

    // Shutdown IR sprite system
    ir_sprite_shutdown();

    memset(&g_sdl3_sprite_backend, 0, sizeof(g_sdl3_sprite_backend));

    printf("[SDL3 Sprite] Backend shutdown\n");
}

/**
 * Begin sprite rendering frame
 * Call at the start of each frame
 */
void sdl3_sprite_backend_begin_frame(void) {
    if (!g_sdl3_sprite_backend.initialized) {
        return;
    }

    sdl3_sprite_batch_begin_frame(&g_sdl3_sprite_backend.sprite_batch);
}

/**
 * End sprite rendering frame
 * Call at the end of each frame to flush remaining sprites
 */
void sdl3_sprite_backend_end_frame(void) {
    if (!g_sdl3_sprite_backend.initialized) {
        return;
    }

    sdl3_sprite_batch_end_frame(&g_sdl3_sprite_backend.sprite_batch);
}

/**
 * Draw sprite using batch renderer
 * This is called by the IR sprite system's draw functions
 */
void sdl3_sprite_backend_draw(IRSpriteID sprite_id, float x, float y,
                               float rotation, float scale_x, float scale_y,
                               float pivot_x, float pivot_y,
                               bool flip_x, bool flip_y, uint32_t tint) {
    if (!g_sdl3_sprite_backend.initialized) {
        return;
    }

    // Get sprite info
    IRSprite* sprite = ir_sprite_get(sprite_id);
    if (!sprite) {
        return;
    }

    // Get atlas texture
    IRSpriteAtlas* atlas = ir_sprite_atlas_get(sprite->region.atlas_id);
    if (!atlas || atlas->texture_id == IR_INVALID_TEXTURE) {
        return;
    }

    // Get SDL texture
    uint32_t texture_index = atlas->texture_id - 1;
    if (texture_index >= SDL3_MAX_TEXTURES) {
        return;
    }

    SDL3TextureHandle* tex_handle = &g_sdl3_sprite_backend.textures[texture_index];
    if (!tex_handle->in_use || !tex_handle->texture) {
        return;
    }

    // Set texture for batch
    sdl3_sprite_batch_set_texture(&g_sdl3_sprite_backend.sprite_batch, tex_handle->texture);

    // Prepare source rectangle (region in atlas)
    SDL_FRect src = {
        .x = (float)sprite->region.x,
        .y = (float)sprite->region.y,
        .w = (float)sprite->region.width,
        .h = (float)sprite->region.height
    };

    // Prepare destination rectangle (screen position with scale)
    SDL_FRect dst = {
        .x = x,
        .y = y,
        .w = sprite->region.width * scale_x,
        .h = sprite->region.height * scale_y
    };

    // Draw sprite with batch renderer
    sdl3_sprite_batch_draw(
        &g_sdl3_sprite_backend.sprite_batch,
        &src,
        &dst,
        rotation,
        pivot_x,
        pivot_y,
        flip_x,
        flip_y,
        tint
    );
}

/**
 * Get sprite batch statistics
 */
void sdl3_sprite_backend_get_stats(uint32_t* out_sprites, uint32_t* out_batches) {
    if (!g_sdl3_sprite_backend.initialized) {
        if (out_sprites) *out_sprites = 0;
        if (out_batches) *out_batches = 0;
        return;
    }

    sdl3_sprite_batch_get_stats(&g_sdl3_sprite_backend.sprite_batch, out_sprites, out_batches);
}
