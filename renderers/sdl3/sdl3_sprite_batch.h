#ifndef SDL3_SPRITE_BATCH_H
#define SDL3_SPRITE_BATCH_H

#include <stdbool.h>
#include <stdint.h>

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SDL3 GPU Sprite Batch Renderer
 *
 * High-performance sprite batching using SDL_RenderGeometry
 * Features:
 * - Batches up to 1024 sprites in a single draw call
 * - Automatic flush on texture/blend mode change
 * - Transform support (rotation, scale, pivot)
 * - Color tinting
 * - Sprite flipping
 *
 * Performance target: 1000+ sprites at 60 FPS with < 5 draw calls
 */

// Maximum sprites per batch (1024 sprites = 4096 vertices, 6144 indices)
#define SDL3_MAX_BATCH_SPRITES 1024
#define SDL3_MAX_BATCH_VERTICES (SDL3_MAX_BATCH_SPRITES * 4)
#define SDL3_MAX_BATCH_INDICES (SDL3_MAX_BATCH_SPRITES * 6)

// Sprite batch state
typedef struct {
    // Vertex/index buffers
    SDL_Vertex vertices[SDL3_MAX_BATCH_VERTICES];
    int indices[SDL3_MAX_BATCH_INDICES];

    // Current batch state
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t sprite_count;

    // Rendering state
    SDL_Texture* current_texture;
    SDL_BlendMode current_blend_mode;

    // Statistics
    uint32_t total_sprites_drawn;
    uint32_t total_batches;
    uint32_t frame_sprites;
    uint32_t frame_batches;

    // SDL renderer reference
    SDL_Renderer* renderer;
} SDL3SpriteBatch;

/**
 * Initialize sprite batch
 */
void sdl3_sprite_batch_init(SDL3SpriteBatch* batch, SDL_Renderer* renderer);

/**
 * Begin a new frame
 * Resets frame statistics
 */
void sdl3_sprite_batch_begin_frame(SDL3SpriteBatch* batch);

/**
 * End frame and print statistics (if enabled)
 */
void sdl3_sprite_batch_end_frame(SDL3SpriteBatch* batch);

/**
 * Begin batching with a texture
 * If texture differs from current, flushes previous batch
 */
void sdl3_sprite_batch_set_texture(SDL3SpriteBatch* batch, SDL_Texture* texture);

/**
 * Set blend mode
 * If blend mode differs from current, flushes previous batch
 */
void sdl3_sprite_batch_set_blend_mode(SDL3SpriteBatch* batch, SDL_BlendMode blend_mode);

/**
 * Add sprite to batch
 * Automatically flushes if batch is full
 *
 * Parameters:
 * - src: Source rectangle in texture (sprite region)
 * - dst: Destination rectangle on screen
 * - rotation: Rotation in degrees
 * - pivot_x, pivot_y: Rotation pivot (0.0-1.0 relative to sprite size)
 * - flip_x, flip_y: Flip sprite horizontally/vertically
 * - tint: RGBA color tint (0xFFFFFFFF = white/no tint)
 */
void sdl3_sprite_batch_draw(SDL3SpriteBatch* batch,
                            const SDL_FRect* src,
                            const SDL_FRect* dst,
                            float rotation,
                            float pivot_x, float pivot_y,
                            bool flip_x, bool flip_y,
                            uint32_t tint);

/**
 * Flush current batch to GPU
 * Renders all queued sprites in a single SDL_RenderGeometry call
 */
void sdl3_sprite_batch_flush(SDL3SpriteBatch* batch);

/**
 * Get frame statistics
 */
void sdl3_sprite_batch_get_stats(const SDL3SpriteBatch* batch,
                                 uint32_t* out_sprites,
                                 uint32_t* out_batches);

#ifdef __cplusplus
}
#endif

#endif // SDL3_SPRITE_BATCH_H
