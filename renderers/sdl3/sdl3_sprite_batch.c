/*
 * SDL3 GPU Sprite Batch Renderer Implementation
 *
 * High-performance sprite rendering using SDL_RenderGeometry
 */

#include "sdl3_sprite_batch.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to convert RGBA uint32 to SDL_FColor (normalized 0.0-1.0)
static SDL_FColor uint32_to_fcolor(uint32_t rgba) {
    SDL_FColor color;
    color.r = ((rgba >> 24) & 0xFF) / 255.0f;
    color.g = ((rgba >> 16) & 0xFF) / 255.0f;
    color.b = ((rgba >> 8) & 0xFF) / 255.0f;
    color.a = (rgba & 0xFF) / 255.0f;
    return color;
}

void sdl3_sprite_batch_init(SDL3SpriteBatch* batch, SDL_Renderer* renderer) {
    if (!batch) return;

    memset(batch, 0, sizeof(SDL3SpriteBatch));
    batch->renderer = renderer;
    batch->current_blend_mode = SDL_BLENDMODE_BLEND;
    batch->current_texture = NULL;

    // Pre-build index buffer (never changes)
    // Each sprite = 2 triangles = 6 indices
    // Pattern: 0,1,2, 2,3,0 for each quad
    for (uint32_t i = 0; i < SDL3_MAX_BATCH_SPRITES; i++) {
        uint32_t base_vertex = i * 4;
        uint32_t base_index = i * 6;

        batch->indices[base_index + 0] = base_vertex + 0;
        batch->indices[base_index + 1] = base_vertex + 1;
        batch->indices[base_index + 2] = base_vertex + 2;
        batch->indices[base_index + 3] = base_vertex + 2;
        batch->indices[base_index + 4] = base_vertex + 3;
        batch->indices[base_index + 5] = base_vertex + 0;
    }
}

void sdl3_sprite_batch_begin_frame(SDL3SpriteBatch* batch) {
    if (!batch) return;
    batch->frame_sprites = 0;
    batch->frame_batches = 0;
}

void sdl3_sprite_batch_end_frame(SDL3SpriteBatch* batch) {
    if (!batch) return;

    // Flush any remaining sprites
    if (batch->sprite_count > 0) {
        sdl3_sprite_batch_flush(batch);
    }

    // Optional: Print statistics (enable with environment variable)
    const char* debug = getenv("KRYON_SPRITE_BATCH_DEBUG");
    if (debug && strcmp(debug, "1") == 0) {
        printf("[Sprite Batch] Frame: %u sprites, %u batches (avg %.1f sprites/batch)\n",
               batch->frame_sprites,
               batch->frame_batches,
               batch->frame_batches > 0 ? (float)batch->frame_sprites / batch->frame_batches : 0.0f);
    }
}

void sdl3_sprite_batch_set_texture(SDL3SpriteBatch* batch, SDL_Texture* texture) {
    if (!batch) return;

    // If texture changed, flush current batch
    if (texture != batch->current_texture && batch->sprite_count > 0) {
        sdl3_sprite_batch_flush(batch);
    }

    batch->current_texture = texture;
}

void sdl3_sprite_batch_set_blend_mode(SDL3SpriteBatch* batch, SDL_BlendMode blend_mode) {
    if (!batch) return;

    // If blend mode changed, flush current batch
    if (blend_mode != batch->current_blend_mode && batch->sprite_count > 0) {
        sdl3_sprite_batch_flush(batch);
    }

    batch->current_blend_mode = blend_mode;
}

void sdl3_sprite_batch_draw(SDL3SpriteBatch* batch,
                            const SDL_FRect* src,
                            const SDL_FRect* dst,
                            float rotation,
                            float pivot_x, float pivot_y,
                            bool flip_x, bool flip_y,
                            uint32_t tint) {
    if (!batch || !src || !dst || !batch->current_texture) return;

    // Auto-flush if batch is full
    if (batch->sprite_count >= SDL3_MAX_BATCH_SPRITES) {
        sdl3_sprite_batch_flush(batch);
    }

    // Get texture size for UV calculations
    float tex_width, tex_height;
    SDL_GetTextureSize(batch->current_texture, &tex_width, &tex_height);

    // Calculate UV coordinates (normalized 0.0-1.0)
    float u0 = src->x / tex_width;
    float v0 = src->y / tex_height;
    float u1 = (src->x + src->w) / tex_width;
    float v1 = (src->y + src->h) / tex_height;

    // Handle flipping
    if (flip_x) {
        float temp = u0;
        u0 = u1;
        u1 = temp;
    }
    if (flip_y) {
        float temp = v0;
        v0 = v1;
        v1 = temp;
    }

    // Convert tint to SDL_FColor
    SDL_FColor color = uint32_to_fcolor(tint);

    // Calculate sprite corners (before rotation)
    float x0 = dst->x;
    float y0 = dst->y;
    float x1 = dst->x + dst->w;
    float y1 = dst->y + dst->h;

    // Apply rotation if needed
    if (fabs(rotation) > 0.001f) {
        // Convert degrees to radians
        float rad = rotation * (3.14159265359f / 180.0f);
        float cos_a = cosf(rad);
        float sin_a = sinf(rad);

        // Pivot point in world space
        float pivot_world_x = dst->x + dst->w * pivot_x;
        float pivot_world_y = dst->y + dst->h * pivot_y;

        // Rotate each corner around pivot
        SDL_FPoint corners[4] = {
            {x0, y0},  // Top-left
            {x1, y0},  // Top-right
            {x1, y1},  // Bottom-right
            {x0, y1}   // Bottom-left
        };

        for (int i = 0; i < 4; i++) {
            // Translate to pivot origin
            float dx = corners[i].x - pivot_world_x;
            float dy = corners[i].y - pivot_world_y;

            // Rotate
            float rotated_x = dx * cos_a - dy * sin_a;
            float rotated_y = dx * sin_a + dy * cos_a;

            // Translate back
            corners[i].x = rotated_x + pivot_world_x;
            corners[i].y = rotated_y + pivot_world_y;
        }

        // Add vertices with rotation
        uint32_t base_vertex = batch->vertex_count;

        batch->vertices[base_vertex + 0].position = corners[0];
        batch->vertices[base_vertex + 0].color = color;
        batch->vertices[base_vertex + 0].tex_coord = (SDL_FPoint){u0, v0};

        batch->vertices[base_vertex + 1].position = corners[1];
        batch->vertices[base_vertex + 1].color = color;
        batch->vertices[base_vertex + 1].tex_coord = (SDL_FPoint){u1, v0};

        batch->vertices[base_vertex + 2].position = corners[2];
        batch->vertices[base_vertex + 2].color = color;
        batch->vertices[base_vertex + 2].tex_coord = (SDL_FPoint){u1, v1};

        batch->vertices[base_vertex + 3].position = corners[3];
        batch->vertices[base_vertex + 3].color = color;
        batch->vertices[base_vertex + 3].tex_coord = (SDL_FPoint){u0, v1};
    } else {
        // No rotation - simpler path
        uint32_t base_vertex = batch->vertex_count;

        // Top-left
        batch->vertices[base_vertex + 0].position = (SDL_FPoint){x0, y0};
        batch->vertices[base_vertex + 0].color = color;
        batch->vertices[base_vertex + 0].tex_coord = (SDL_FPoint){u0, v0};

        // Top-right
        batch->vertices[base_vertex + 1].position = (SDL_FPoint){x1, y0};
        batch->vertices[base_vertex + 1].color = color;
        batch->vertices[base_vertex + 1].tex_coord = (SDL_FPoint){u1, v0};

        // Bottom-right
        batch->vertices[base_vertex + 2].position = (SDL_FPoint){x1, y1};
        batch->vertices[base_vertex + 2].color = color;
        batch->vertices[base_vertex + 2].tex_coord = (SDL_FPoint){u1, v1};

        // Bottom-left
        batch->vertices[base_vertex + 3].position = (SDL_FPoint){x0, y1};
        batch->vertices[base_vertex + 3].color = color;
        batch->vertices[base_vertex + 3].tex_coord = (SDL_FPoint){u0, v1};
    }

    batch->vertex_count += 4;
    batch->index_count += 6;
    batch->sprite_count++;
}

void sdl3_sprite_batch_flush(SDL3SpriteBatch* batch) {
    if (!batch || batch->sprite_count == 0 || !batch->current_texture) {
        return;
    }

    // Set texture blend mode
    SDL_SetTextureBlendMode(batch->current_texture, batch->current_blend_mode);

    // Render all sprites in a single draw call using SDL_RenderGeometry
    SDL_RenderGeometry(
        batch->renderer,
        batch->current_texture,
        batch->vertices,
        batch->vertex_count,
        batch->indices,
        batch->index_count
    );

    // Update statistics
    batch->total_sprites_drawn += batch->sprite_count;
    batch->total_batches++;
    batch->frame_sprites += batch->sprite_count;
    batch->frame_batches++;

    // Reset batch
    batch->vertex_count = 0;
    batch->index_count = 0;
    batch->sprite_count = 0;
}

void sdl3_sprite_batch_get_stats(const SDL3SpriteBatch* batch,
                                 uint32_t* out_sprites,
                                 uint32_t* out_batches) {
    if (!batch) return;
    if (out_sprites) *out_sprites = batch->frame_sprites;
    if (out_batches) *out_batches = batch->frame_batches;
}
