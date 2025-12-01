#ifndef SDL3_SPRITE_BACKEND_H
#define SDL3_SPRITE_BACKEND_H

#include <stdbool.h>
#include <stdint.h>
#include "../../ir/ir_sprite.h"

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
 * SDL3 Sprite Backend Integration
 *
 * Provides the bridge between Kryon's IR sprite system and SDL3 rendering.
 * Features:
 * - Automatic texture atlas management
 * - GPU-accelerated sprite batching (1000+ sprites at 60 FPS)
 * - Texture creation from sprite atlases
 * - Integrated with SDL3SpriteBatch for optimal performance
 *
 * Usage:
 *   1. Call sdl3_sprite_backend_init() after creating SDL renderer
 *   2. Load sprites using ir_sprite_atlas_* functions
 *   3. Call sdl3_sprite_backend_begin_frame() at frame start
 *   4. Draw sprites using ir_sprite_draw*() functions
 *   5. Call sdl3_sprite_backend_end_frame() at frame end
 */

/**
 * Initialize SDL3 sprite backend
 * Must be called after SDL renderer is created
 *
 * This:
 * - Initializes the sprite batch renderer
 * - Initializes the IR sprite system
 * - Registers backend callbacks for texture management
 */
void sdl3_sprite_backend_init(SDL_Renderer* renderer);

/**
 * Shutdown SDL3 sprite backend
 * Destroys all textures and frees resources
 */
void sdl3_sprite_backend_shutdown(void);

/**
 * Begin sprite rendering frame
 * Call at the start of each frame before drawing sprites
 */
void sdl3_sprite_backend_begin_frame(void);

/**
 * End sprite rendering frame
 * Call at the end of each frame to flush remaining sprites
 * Prints statistics if KRYON_SPRITE_BATCH_DEBUG=1
 */
void sdl3_sprite_backend_end_frame(void);

/**
 * Draw sprite using batch renderer
 *
 * Parameters:
 * - sprite_id: Sprite to draw
 * - x, y: Screen position
 * - rotation: Rotation in degrees
 * - scale_x, scale_y: Scale factors (1.0 = normal size)
 * - pivot_x, pivot_y: Rotation/scale pivot (0.0-1.0 relative to sprite)
 * - flip_x, flip_y: Flip sprite horizontally/vertically
 * - tint: RGBA color tint (0xFFFFFFFF = white/no tint)
 *
 * Note: This is called internally by ir_sprite_draw*() functions.
 * You typically don't need to call this directly.
 */
void sdl3_sprite_backend_draw(IRSpriteID sprite_id, float x, float y,
                               float rotation, float scale_x, float scale_y,
                               float pivot_x, float pivot_y,
                               bool flip_x, bool flip_y, uint32_t tint);

/**
 * Get sprite batch statistics for current frame
 * Useful for performance monitoring and debugging
 */
void sdl3_sprite_backend_get_stats(uint32_t* out_sprites, uint32_t* out_batches);

#ifdef __cplusplus
}
#endif

#endif // SDL3_SPRITE_BACKEND_H
