/*
 * Kryon Sprite System Implementation
 *
 * Texture atlas management with shelf bin packing algorithm
 */

#include "ir_sprite.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// STB Image for loading images (single header library)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO  // We'll handle file I/O
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "third_party/stb_image.h"

// ============================================================================
// Global State
// ============================================================================

static struct {
    bool initialized;
    IRSpriteAtlas atlases[IR_MAX_ATLASES];
    uint32_t atlas_count;
    uint32_t next_sprite_id;
    uint32_t next_atlas_id;

    // Backend callbacks
    IRSpriteBackendCreateTexture backend_create_texture;
    IRSpriteBackendDestroyTexture backend_destroy_texture;
    IRSpriteBackendDrawSprite backend_draw_sprite;
} g_sprite_system = {0};

// ============================================================================
// Helper Functions
// ============================================================================

static IRSpriteAtlas* get_atlas(IRSpriteAtlasID atlas_id) {
    for (uint32_t i = 0; i < g_sprite_system.atlas_count; i++) {
        if (g_sprite_system.atlases[i].id == atlas_id) {
            return &g_sprite_system.atlases[i];
        }
    }
    return NULL;
}

static IRSprite* get_sprite(IRSpriteID sprite_id) {
    // Search all atlases for sprite
    for (uint32_t i = 0; i < g_sprite_system.atlas_count; i++) {
        IRSpriteAtlas* atlas = &g_sprite_system.atlases[i];
        for (uint16_t j = 0; j < atlas->sprite_count; j++) {
            if (atlas->sprites[j].id == sprite_id) {
                return &atlas->sprites[j];
            }
        }
    }
    return NULL;
}

// Load file into memory
static uint8_t* load_file(const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t* data = (uint8_t*)malloc(size);
    if (!data) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(data, 1, size, file);
    fclose(file);

    if (read != size) {
        free(data);
        return NULL;
    }

    *out_size = size;
    return data;
}

// ============================================================================
// Initialization
// ============================================================================

void ir_sprite_init(void) {
    memset(&g_sprite_system, 0, sizeof(g_sprite_system));
    g_sprite_system.initialized = true;
    g_sprite_system.next_sprite_id = 1;  // Start at 1, 0 is invalid
    g_sprite_system.next_atlas_id = 1;
}

void ir_sprite_shutdown(void) {
    // Destroy all atlases
    for (uint32_t i = 0; i < g_sprite_system.atlas_count; i++) {
        IRSpriteAtlas* atlas = &g_sprite_system.atlases[i];
        if (atlas->pixel_data) {
            free(atlas->pixel_data);
        }
        if (atlas->texture_id != IR_INVALID_TEXTURE && g_sprite_system.backend_destroy_texture) {
            g_sprite_system.backend_destroy_texture(atlas->texture_id);
        }
    }
    memset(&g_sprite_system, 0, sizeof(g_sprite_system));
}

void ir_sprite_register_backend(IRSpriteBackendCreateTexture create_texture,
                                IRSpriteBackendDestroyTexture destroy_texture,
                                IRSpriteBackendDrawSprite draw_sprite) {
    g_sprite_system.backend_create_texture = create_texture;
    g_sprite_system.backend_destroy_texture = destroy_texture;
    g_sprite_system.backend_draw_sprite = draw_sprite;
}

// ============================================================================
// Atlas Management
// ============================================================================

IRSpriteAtlasID ir_sprite_atlas_create(uint16_t width, uint16_t height) {
    if (!g_sprite_system.initialized) {
        ir_sprite_init();
    }

    if (g_sprite_system.atlas_count >= IR_MAX_ATLASES) {
        return IR_INVALID_SPRITE_ATLAS;
    }

    if (width > IR_MAX_ATLAS_SIZE || height > IR_MAX_ATLAS_SIZE) {
        return IR_INVALID_SPRITE_ATLAS;
    }

    IRSpriteAtlas* atlas = &g_sprite_system.atlases[g_sprite_system.atlas_count++];
    memset(atlas, 0, sizeof(IRSpriteAtlas));

    atlas->id = g_sprite_system.next_atlas_id++;
    atlas->width = width;
    atlas->height = height;
    atlas->is_packed = false;
    atlas->texture_id = IR_INVALID_TEXTURE;

    // Allocate pixel data (RGBA)
    atlas->pixel_data = (uint8_t*)calloc(width * height * 4, 1);
    if (!atlas->pixel_data) {
        g_sprite_system.atlas_count--;
        return IR_INVALID_SPRITE_ATLAS;
    }

    // Initialize shelf packing
    atlas->current_shelf_y = 0;
    atlas->current_shelf_x = 0;
    atlas->current_shelf_height = 0;

    return atlas->id;
}

void ir_sprite_atlas_destroy(IRSpriteAtlasID atlas_id) {
    IRSpriteAtlas* atlas = get_atlas(atlas_id);
    if (!atlas) return;

    if (atlas->pixel_data) {
        free(atlas->pixel_data);
        atlas->pixel_data = NULL;
    }

    if (atlas->texture_id != IR_INVALID_TEXTURE && g_sprite_system.backend_destroy_texture) {
        g_sprite_system.backend_destroy_texture(atlas->texture_id);
        atlas->texture_id = IR_INVALID_TEXTURE;
    }

    // Remove from array by shifting
    uint32_t index = atlas - g_sprite_system.atlases;
    memmove(atlas, atlas + 1, (g_sprite_system.atlas_count - index - 1) * sizeof(IRSpriteAtlas));
    g_sprite_system.atlas_count--;
}

// Shelf bin packing algorithm
static bool pack_sprite_into_atlas(IRSpriteAtlas* atlas, uint8_t* pixels,
                                    uint16_t width, uint16_t height,
                                    IRSpriteRegion* out_region) {
    // Try to fit on current shelf
    if (atlas->current_shelf_x + width <= atlas->width &&
        atlas->current_shelf_y + height <= atlas->height) {

        // Fits on current shelf
        out_region->x = atlas->current_shelf_x;
        out_region->y = atlas->current_shelf_y;
        out_region->width = width;
        out_region->height = height;
        out_region->atlas_id = atlas->id;

        // Copy pixel data into atlas
        for (uint16_t y = 0; y < height; y++) {
            uint8_t* src = pixels + (y * width * 4);
            uint8_t* dst = atlas->pixel_data + ((out_region->y + y) * atlas->width + out_region->x) * 4;
            memcpy(dst, src, width * 4);
        }

        // Update shelf packing state
        atlas->current_shelf_x += width;
        if (height > atlas->current_shelf_height) {
            atlas->current_shelf_height = height;
        }

        return true;
    }

    // Move to next shelf
    atlas->current_shelf_y += atlas->current_shelf_height;
    atlas->current_shelf_x = 0;
    atlas->current_shelf_height = height;

    // Try again on new shelf
    if (atlas->current_shelf_x + width <= atlas->width &&
        atlas->current_shelf_y + height <= atlas->height) {

        out_region->x = atlas->current_shelf_x;
        out_region->y = atlas->current_shelf_y;
        out_region->width = width;
        out_region->height = height;
        out_region->atlas_id = atlas->id;

        // Copy pixel data
        for (uint16_t y = 0; y < height; y++) {
            uint8_t* src = pixels + (y * width * 4);
            uint8_t* dst = atlas->pixel_data + ((out_region->y + y) * atlas->width + out_region->x) * 4;
            memcpy(dst, src, width * 4);
        }

        atlas->current_shelf_x += width;
        return true;
    }

    // Doesn't fit
    return false;
}

IRSpriteID ir_sprite_atlas_add_image(IRSpriteAtlasID atlas_id, const char* path) {
    IRSpriteAtlas* atlas = get_atlas(atlas_id);
    if (!atlas || atlas->is_packed) {
        return IR_INVALID_SPRITE;
    }

    if (atlas->sprite_count >= IR_MAX_SPRITES_PER_ATLAS) {
        return IR_INVALID_SPRITE;
    }

    // Load image file
    size_t file_size;
    uint8_t* file_data = load_file(path, &file_size);
    if (!file_data) {
        return IR_INVALID_SPRITE;
    }

    // Decode image
    int width, height, channels;
    uint8_t* pixels = stbi_load_from_memory(file_data, file_size, &width, &height, &channels, 4);
    free(file_data);

    if (!pixels) {
        return IR_INVALID_SPRITE;
    }

    // Pack into atlas
    IRSpriteRegion region;
    if (!pack_sprite_into_atlas(atlas, pixels, width, height, &region)) {
        stbi_image_free(pixels);
        return IR_INVALID_SPRITE;
    }

    stbi_image_free(pixels);

    // Create sprite
    IRSprite* sprite = &atlas->sprites[atlas->sprite_count++];
    sprite->id = g_sprite_system.next_sprite_id++;
    sprite->region = region;
    sprite->region.orig_width = width;
    sprite->region.orig_height = height;
    sprite->pivot_x = 0.5f;
    sprite->pivot_y = 0.5f;

    // Extract filename for sprite name
    const char* filename = strrchr(path, '/');
    if (!filename) filename = strrchr(path, '\\');
    if (!filename) filename = path;
    else filename++;

    strncpy(sprite->name, filename, sizeof(sprite->name) - 1);
    sprite->name[sizeof(sprite->name) - 1] = '\0';

    return sprite->id;
}

IRSpriteID ir_sprite_atlas_add_image_from_memory(IRSpriteAtlasID atlas_id,
                                                  const char* name,
                                                  uint8_t* pixels,
                                                  uint16_t width, uint16_t height) {
    IRSpriteAtlas* atlas = get_atlas(atlas_id);
    if (!atlas || atlas->is_packed || !pixels) {
        return IR_INVALID_SPRITE;
    }

    if (atlas->sprite_count >= IR_MAX_SPRITES_PER_ATLAS) {
        return IR_INVALID_SPRITE;
    }

    // Pack into atlas
    IRSpriteRegion region;
    if (!pack_sprite_into_atlas(atlas, pixels, width, height, &region)) {
        return IR_INVALID_SPRITE;
    }

    // Create sprite
    IRSprite* sprite = &atlas->sprites[atlas->sprite_count++];
    sprite->id = g_sprite_system.next_sprite_id++;
    sprite->region = region;
    sprite->region.orig_width = width;
    sprite->region.orig_height = height;
    sprite->pivot_x = 0.5f;
    sprite->pivot_y = 0.5f;

    strncpy(sprite->name, name, sizeof(sprite->name) - 1);
    sprite->name[sizeof(sprite->name) - 1] = '\0';

    return sprite->id;
}

bool ir_sprite_atlas_pack(IRSpriteAtlasID atlas_id) {
    IRSpriteAtlas* atlas = get_atlas(atlas_id);
    if (!atlas || atlas->is_packed) {
        return false;
    }

    // Upload to GPU via backend
    if (g_sprite_system.backend_create_texture) {
        atlas->texture_id = g_sprite_system.backend_create_texture(
            atlas->width, atlas->height, atlas->pixel_data);

        if (atlas->texture_id == IR_INVALID_TEXTURE) {
            return false;
        }
    }

    // Free pixel data after uploading
    free(atlas->pixel_data);
    atlas->pixel_data = NULL;
    atlas->is_packed = true;

    return true;
}

IRSprite* ir_sprite_get(IRSpriteID sprite_id) {
    return get_sprite(sprite_id);
}

IRSprite* ir_sprite_get_by_name(const char* name) {
    for (uint32_t i = 0; i < g_sprite_system.atlas_count; i++) {
        IRSpriteAtlas* atlas = &g_sprite_system.atlases[i];
        for (uint16_t j = 0; j < atlas->sprite_count; j++) {
            if (strcmp(atlas->sprites[j].name, name) == 0) {
                return &atlas->sprites[j];
            }
        }
    }
    return NULL;
}

IRSpriteAtlas* ir_sprite_atlas_get(IRSpriteAtlasID atlas_id) {
    return get_atlas(atlas_id);
}

// ============================================================================
// Sprite Drawing (Stub - will integrate with command buffer)
// ============================================================================

// TODO: These will be implemented to push commands to IR command buffer
// For now, they're stubs that backends can hook into

void ir_sprite_draw(IRSpriteID sprite_id, float x, float y) {
    ir_sprite_draw_ex(sprite_id, x, y, 0.0f, 1.0f, 1.0f, 0xFFFFFFFF);
}

void ir_sprite_draw_ex(IRSpriteID sprite_id, float x, float y,
                       float rotation, float scale_x, float scale_y,
                       uint32_t tint) {
    IRSprite* sprite = get_sprite(sprite_id);
    if (!sprite) return;

    ir_sprite_draw_advanced(sprite_id, x, y, rotation, scale_x, scale_y,
                           sprite->pivot_x, sprite->pivot_y, false, false, tint);
}

void ir_sprite_draw_advanced(IRSpriteID sprite_id, float x, float y,
                             float rotation, float scale_x, float scale_y,
                             float pivot_x, float pivot_y,
                             bool flip_x, bool flip_y, uint32_t tint) {
    IRSprite* sprite = get_sprite(sprite_id);
    if (!sprite) return;

    // Call backend draw function if registered
    if (g_sprite_system.backend_draw_sprite) {
        g_sprite_system.backend_draw_sprite(sprite_id, x, y, rotation,
                                           scale_x, scale_y, pivot_x, pivot_y,
                                           flip_x, flip_y, tint);
    }
}

void ir_sprite_draw_region(IRSpriteID sprite_id, float x, float y,
                           uint16_t src_x, uint16_t src_y,
                           uint16_t src_width, uint16_t src_height) {
    // TODO: Implement region drawing
    (void)sprite_id; (void)x; (void)y; (void)src_x; (void)src_y;
    (void)src_width; (void)src_height;
}

// ============================================================================
// Sprite Animation
// ============================================================================

IRSpriteAnimation* ir_sprite_animation_create(IRSpriteID* frames, uint16_t frame_count,
                                               float fps, bool loop) {
    if (!frames || frame_count == 0) return NULL;

    IRSpriteAnimation* anim = (IRSpriteAnimation*)malloc(sizeof(IRSpriteAnimation));
    if (!anim) return NULL;

    anim->frames = (IRSpriteID*)malloc(sizeof(IRSpriteID) * frame_count);
    if (!anim->frames) {
        free(anim);
        return NULL;
    }

    memcpy(anim->frames, frames, sizeof(IRSpriteID) * frame_count);
    anim->frame_count = frame_count;
    anim->fps = fps;
    anim->loop = loop;
    anim->current_time = 0.0f;
    anim->current_frame = 0;
    anim->playing = false;

    return anim;
}

void ir_sprite_animation_destroy(IRSpriteAnimation* anim) {
    if (!anim) return;
    if (anim->frames) {
        free(anim->frames);
    }
    free(anim);
}

void ir_sprite_animation_update(IRSpriteAnimation* anim, float dt) {
    if (!anim || !anim->playing) return;

    anim->current_time += dt;
    float frame_time = 1.0f / anim->fps;

    while (anim->current_time >= frame_time) {
        anim->current_time -= frame_time;
        anim->current_frame++;

        if (anim->current_frame >= anim->frame_count) {
            if (anim->loop) {
                anim->current_frame = 0;
            } else {
                anim->current_frame = anim->frame_count - 1;
                anim->playing = false;
            }
        }
    }
}

IRSpriteID ir_sprite_animation_current_frame(IRSpriteAnimation* anim) {
    if (!anim || anim->current_frame >= anim->frame_count) {
        return IR_INVALID_SPRITE;
    }
    return anim->frames[anim->current_frame];
}

void ir_sprite_animation_play(IRSpriteAnimation* anim) {
    if (!anim) return;
    anim->playing = true;
    anim->current_time = 0.0f;
    anim->current_frame = 0;
}

void ir_sprite_animation_stop(IRSpriteAnimation* anim) {
    if (!anim) return;
    anim->playing = false;
    anim->current_time = 0.0f;
    anim->current_frame = 0;
}

void ir_sprite_animation_pause(IRSpriteAnimation* anim) {
    if (!anim) return;
    anim->playing = false;
}

void ir_sprite_animation_reset(IRSpriteAnimation* anim) {
    if (!anim) return;
    anim->current_time = 0.0f;
    anim->current_frame = 0;
}

bool ir_sprite_animation_is_playing(IRSpriteAnimation* anim) {
    return anim ? anim->playing : false;
}

bool ir_sprite_animation_is_finished(IRSpriteAnimation* anim) {
    if (!anim || anim->loop) return false;
    return anim->current_frame >= anim->frame_count - 1 && !anim->playing;
}

// ============================================================================
// Utility Functions
// ============================================================================

uint32_t ir_sprite_get_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_sprite_system.atlas_count; i++) {
        count += g_sprite_system.atlases[i].sprite_count;
    }
    return count;
}

uint32_t ir_sprite_atlas_get_count(void) {
    return g_sprite_system.atlas_count;
}

size_t ir_sprite_get_memory_usage(void) {
    size_t total = 0;
    for (uint32_t i = 0; i < g_sprite_system.atlas_count; i++) {
        IRSpriteAtlas* atlas = &g_sprite_system.atlases[i];
        if (atlas->pixel_data) {
            total += atlas->width * atlas->height * 4;
        }
    }
    return total;
}

void ir_sprite_set_pivot(IRSpriteID sprite_id, float pivot_x, float pivot_y) {
    IRSprite* sprite = get_sprite(sprite_id);
    if (!sprite) return;
    sprite->pivot_x = pivot_x;
    sprite->pivot_y = pivot_y;
}

void ir_sprite_get_size(IRSpriteID sprite_id, uint16_t* width, uint16_t* height) {
    IRSprite* sprite = get_sprite(sprite_id);
    if (!sprite) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = sprite->region.orig_width;
    if (height) *height = sprite->region.orig_height;
}

// ============================================================================
// Sprite Sheet Support (Basic implementation)
// ============================================================================

IRSpriteID* ir_sprite_load_sheet(IRSpriteAtlasID atlas_id,
                                  const char* path,
                                  uint16_t frame_width, uint16_t frame_height,
                                  uint16_t* out_frame_count) {
    // TODO: Implement sprite sheet loading
    // For now, return NULL
    (void)atlas_id; (void)path; (void)frame_width; (void)frame_height;
    if (out_frame_count) *out_frame_count = 0;
    return NULL;
}

IRSpriteID* ir_sprite_load_sheet_with_metadata(IRSpriteAtlasID atlas_id,
                                                const char* image_path,
                                                const char* metadata_path,
                                                uint16_t* out_frame_count) {
    // TODO: Implement metadata-based sprite sheet loading
    (void)atlas_id; (void)image_path; (void)metadata_path;
    if (out_frame_count) *out_frame_count = 0;
    return NULL;
}
