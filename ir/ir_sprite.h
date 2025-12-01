#ifndef IR_SPRITE_H
#define IR_SPRITE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Kryon Sprite System
 *
 * Provides texture atlas management and sprite rendering for games.
 * Features:
 * - Texture atlas with shelf bin packing algorithm
 * - Sprite sheet support with frame metadata
 * - Sprite animation system
 * - Integration with IR command buffer for batching
 *
 * Usage:
 *   // Create atlas
 *   IRSpriteAtlasID atlas = ir_sprite_atlas_create(2048, 2048);
 *
 *   // Add sprites
 *   IRSpriteID player = ir_sprite_atlas_add_image(atlas, "player.png");
 *   IRSpriteID enemy = ir_sprite_atlas_add_image(atlas, "enemy.png");
 *
 *   // Finalize atlas (packs and uploads to GPU)
 *   ir_sprite_atlas_pack(atlas);
 *
 *   // Draw sprites
 *   ir_sprite_draw(player, 100, 100);
 *   ir_sprite_draw_ex(enemy, 200, 150, 45.0f, 1.5f, 1.5f, 0xFFFFFFFF);
 */

// ============================================================================
// Type Definitions
// ============================================================================

typedef uint32_t IRSpriteAtlasID;
typedef uint32_t IRSpriteID;
typedef uint32_t IRTextureID;

#define IR_INVALID_SPRITE_ATLAS 0
#define IR_INVALID_SPRITE 0
#define IR_INVALID_TEXTURE 0

// Maximum atlas size (4K texture)
#define IR_MAX_ATLAS_SIZE 4096

// Maximum sprites per atlas
#define IR_MAX_SPRITES_PER_ATLAS 1024

// Maximum atlases
#define IR_MAX_ATLASES 16

// Sprite region within atlas
typedef struct {
    uint16_t x, y;           // Position in atlas
    uint16_t width, height;  // Size in pixels
    uint16_t orig_width, orig_height;  // Original size before packing
    IRSpriteAtlasID atlas_id;
} IRSpriteRegion;

// Sprite metadata
typedef struct {
    IRSpriteID id;
    IRSpriteRegion region;
    char name[64];           // Sprite name
    float pivot_x, pivot_y;  // Pivot point (0.0-1.0, default 0.5,0.5)
} IRSprite;

// Sprite atlas
typedef struct {
    IRSpriteAtlasID id;
    uint16_t width, height;
    IRTextureID texture_id;  // Backend texture handle
    bool is_packed;          // Atlas has been finalized

    // Shelf packing state
    uint16_t current_shelf_y;
    uint16_t current_shelf_x;
    uint16_t current_shelf_height;

    // Sprites in this atlas
    IRSprite sprites[IR_MAX_SPRITES_PER_ATLAS];
    uint16_t sprite_count;

    // Raw pixel data (RGBA, freed after packing)
    uint8_t* pixel_data;
} IRSpriteAtlas;

// Sprite animation
typedef struct {
    IRSpriteID* frames;      // Array of sprite IDs
    uint16_t frame_count;
    float fps;               // Frames per second
    bool loop;               // Loop animation

    // Animation state
    float current_time;
    uint16_t current_frame;
    bool playing;
} IRSpriteAnimation;

// ============================================================================
// Atlas Management
// ============================================================================

/**
 * Create a new sprite atlas
 * Returns atlas ID or IR_INVALID_SPRITE_ATLAS on failure
 */
IRSpriteAtlasID ir_sprite_atlas_create(uint16_t width, uint16_t height);

/**
 * Destroy sprite atlas and free resources
 */
void ir_sprite_atlas_destroy(IRSpriteAtlasID atlas_id);

/**
 * Add image to atlas from file path
 * Image is loaded and packed into atlas
 * Returns sprite ID or IR_INVALID_SPRITE on failure
 */
IRSpriteID ir_sprite_atlas_add_image(IRSpriteAtlasID atlas_id, const char* path);

/**
 * Add image to atlas from memory (RGBA format)
 * Useful for procedurally generated sprites
 */
IRSpriteID ir_sprite_atlas_add_image_from_memory(IRSpriteAtlasID atlas_id,
                                                  const char* name,
                                                  uint8_t* pixels,
                                                  uint16_t width, uint16_t height);

/**
 * Finalize atlas packing and upload to GPU
 * Must be called before drawing sprites
 * After packing, no more sprites can be added
 */
bool ir_sprite_atlas_pack(IRSpriteAtlasID atlas_id);

/**
 * Get sprite by ID
 */
IRSprite* ir_sprite_get(IRSpriteID sprite_id);

/**
 * Get sprite by name
 */
IRSprite* ir_sprite_get_by_name(const char* name);

/**
 * Get atlas by ID
 */
IRSpriteAtlas* ir_sprite_atlas_get(IRSpriteAtlasID atlas_id);

// ============================================================================
// Sprite Drawing (IR Command Buffer Integration)
// ============================================================================

/**
 * Draw sprite at position
 * Uses default scale (1.0), rotation (0.0), and tint (white)
 */
void ir_sprite_draw(IRSpriteID sprite_id, float x, float y);

/**
 * Draw sprite with transformation
 * rotation: in degrees
 * scale_x, scale_y: scale factors (1.0 = normal size)
 * tint: RGBA color (0xFFFFFFFF = white/no tint)
 */
void ir_sprite_draw_ex(IRSpriteID sprite_id, float x, float y,
                       float rotation, float scale_x, float scale_y,
                       uint32_t tint);

/**
 * Draw sprite with full control
 * pivot_x, pivot_y: rotation/scale pivot (0.0-1.0, default from sprite)
 * flip_x, flip_y: flip sprite horizontally/vertically
 */
void ir_sprite_draw_advanced(IRSpriteID sprite_id, float x, float y,
                             float rotation, float scale_x, float scale_y,
                             float pivot_x, float pivot_y,
                             bool flip_x, bool flip_y, uint32_t tint);

/**
 * Draw sub-region of sprite (for sprite sheets)
 */
void ir_sprite_draw_region(IRSpriteID sprite_id, float x, float y,
                           uint16_t src_x, uint16_t src_y,
                           uint16_t src_width, uint16_t src_height);

// ============================================================================
// Sprite Sheet Support
// ============================================================================

/**
 * Load sprite sheet with uniform grid
 * Automatically creates sprites for each frame
 * Returns array of sprite IDs (must be freed by caller)
 */
IRSpriteID* ir_sprite_load_sheet(IRSpriteAtlasID atlas_id,
                                  const char* path,
                                  uint16_t frame_width, uint16_t frame_height,
                                  uint16_t* out_frame_count);

/**
 * Load sprite sheet with metadata file (JSON)
 * Format: { "frames": [{"name": "sprite_01", "x": 0, "y": 0, "w": 32, "h": 32}, ...] }
 */
IRSpriteID* ir_sprite_load_sheet_with_metadata(IRSpriteAtlasID atlas_id,
                                                const char* image_path,
                                                const char* metadata_path,
                                                uint16_t* out_frame_count);

// ============================================================================
// Sprite Animation
// ============================================================================

/**
 * Create sprite animation from array of sprite IDs
 */
IRSpriteAnimation* ir_sprite_animation_create(IRSpriteID* frames, uint16_t frame_count,
                                               float fps, bool loop);

/**
 * Destroy sprite animation
 */
void ir_sprite_animation_destroy(IRSpriteAnimation* anim);

/**
 * Update animation (call each frame)
 */
void ir_sprite_animation_update(IRSpriteAnimation* anim, float dt);

/**
 * Get current frame sprite ID
 */
IRSpriteID ir_sprite_animation_current_frame(IRSpriteAnimation* anim);

/**
 * Play animation from start
 */
void ir_sprite_animation_play(IRSpriteAnimation* anim);

/**
 * Stop animation
 */
void ir_sprite_animation_stop(IRSpriteAnimation* anim);

/**
 * Pause animation
 */
void ir_sprite_animation_pause(IRSpriteAnimation* anim);

/**
 * Reset animation to first frame
 */
void ir_sprite_animation_reset(IRSpriteAnimation* anim);

/**
 * Check if animation is playing
 */
bool ir_sprite_animation_is_playing(IRSpriteAnimation* anim);

/**
 * Check if animation has finished (non-looping only)
 */
bool ir_sprite_animation_is_finished(IRSpriteAnimation* anim);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Initialize sprite system
 */
void ir_sprite_init(void);

/**
 * Shutdown sprite system and free all resources
 */
void ir_sprite_shutdown(void);

/**
 * Get total number of sprites loaded
 */
uint32_t ir_sprite_get_count(void);

/**
 * Get total number of atlases created
 */
uint32_t ir_sprite_atlas_get_count(void);

/**
 * Get memory usage in bytes
 */
size_t ir_sprite_get_memory_usage(void);

/**
 * Set sprite pivot point (for rotation/scaling)
 */
void ir_sprite_set_pivot(IRSpriteID sprite_id, float pivot_x, float pivot_y);

/**
 * Get sprite dimensions
 */
void ir_sprite_get_size(IRSpriteID sprite_id, uint16_t* width, uint16_t* height);

// ============================================================================
// Backend Integration (implemented by backends)
// ============================================================================

/**
 * Backend callback to create texture from pixel data
 * Called by ir_sprite_atlas_pack()
 */
typedef IRTextureID (*IRSpriteBackendCreateTexture)(uint16_t width, uint16_t height,
                                                     uint8_t* pixels);

/**
 * Backend callback to destroy texture
 */
typedef void (*IRSpriteBackendDestroyTexture)(IRTextureID texture_id);

/**
 * Backend callback to draw sprite
 * Called by ir_sprite_draw*() functions
 */
typedef void (*IRSpriteBackendDrawSprite)(IRSpriteID sprite_id, float x, float y,
                                          float rotation, float scale_x, float scale_y,
                                          float pivot_x, float pivot_y,
                                          bool flip_x, bool flip_y, uint32_t tint);

/**
 * Register backend callbacks
 * Must be called before using sprite system
 */
void ir_sprite_register_backend(IRSpriteBackendCreateTexture create_texture,
                                IRSpriteBackendDestroyTexture destroy_texture,
                                IRSpriteBackendDrawSprite draw_sprite);

#ifdef __cplusplus
}
#endif

#endif // IR_SPRITE_H
