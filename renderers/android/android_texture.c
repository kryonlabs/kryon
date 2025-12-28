#include "android_renderer_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "KryonTexture"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) fprintf(stdout, "[INFO] " __VA_ARGS__)
#define LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__)
#endif

// ============================================================================
// Texture Cache Management
// ============================================================================

void android_texture_cache_init(AndroidRenderer* renderer) {
    if (!renderer) return;

    memset(renderer->texture_cache, 0, sizeof(renderer->texture_cache));
    renderer->texture_cache_count = 0;

    LOGI("Texture cache initialized\n");
}

void android_texture_cache_cleanup(AndroidRenderer* renderer) {
    if (!renderer) return;

#ifdef __ANDROID__
    for (int i = 0; i < MAX_TEXTURE_CACHE; i++) {
        if (renderer->texture_cache[i].used && renderer->texture_cache[i].texture_id) {
            glDeleteTextures(1, &renderer->texture_cache[i].texture_id);
        }
    }
#endif

    memset(renderer->texture_cache, 0, sizeof(renderer->texture_cache));
    renderer->texture_cache_count = 0;

    LOGI("Texture cache cleaned up\n");
}

GLuint android_texture_cache_get(AndroidRenderer* renderer, const char* key) {
    if (!renderer || !key) return 0;

    for (int i = 0; i < MAX_TEXTURE_CACHE; i++) {
        if (renderer->texture_cache[i].used &&
            strcmp(renderer->texture_cache[i].key, key) == 0) {
            renderer->texture_cache[i].last_used_frame = renderer->frame_count;
            return renderer->texture_cache[i].texture_id;
        }
    }

    return 0;
}

void android_texture_cache_set(AndroidRenderer* renderer, const char* key,
                                GLuint texture_id, int width, int height) {
    if (!renderer || !key || !texture_id) return;

    // Find free slot or LRU
    int slot = -1;
    uint64_t oldest_frame = renderer->frame_count;

    for (int i = 0; i < MAX_TEXTURE_CACHE; i++) {
        if (!renderer->texture_cache[i].used) {
            slot = i;
            break;
        }

        if (renderer->texture_cache[i].last_used_frame < oldest_frame) {
            oldest_frame = renderer->texture_cache[i].last_used_frame;
            slot = i;
        }
    }

    if (slot < 0) return;

    // Evict old texture if needed
    if (renderer->texture_cache[slot].used) {
#ifdef __ANDROID__
        glDeleteTextures(1, &renderer->texture_cache[slot].texture_id);
#endif
    }

    // Store new texture
    renderer->texture_cache[slot].texture_id = texture_id;
    renderer->texture_cache[slot].width = width;
    renderer->texture_cache[slot].height = height;
    renderer->texture_cache[slot].last_used_frame = renderer->frame_count;
    strncpy(renderer->texture_cache[slot].key, key, sizeof(renderer->texture_cache[slot].key) - 1);
    renderer->texture_cache[slot].used = true;

    if (slot >= renderer->texture_cache_count) {
        renderer->texture_cache_count = slot + 1;
    }
}

// ============================================================================
// Texture Loading (Stub Implementation)
// ============================================================================
// TODO: Integrate stb_image or similar for PNG/JPEG loading

GLuint android_renderer_load_texture(AndroidRenderer* renderer,
                                      const void* data,
                                      size_t size,
                                      int* out_width,
                                      int* out_height) {
    if (!renderer || !data || size == 0) return 0;

    // TODO: Decode image data and upload to GPU
    // 1. Use stb_image to decode PNG/JPEG
    // 2. Create OpenGL texture
    // 3. Upload pixel data
    // 4. Generate mipmaps

    (void)out_width;
    (void)out_height;

    LOGI("Load texture from memory (stub): %zu bytes\n", size);
    return 0;
}

GLuint android_renderer_load_texture_from_file(AndroidRenderer* renderer,
                                                const char* filepath,
                                                int* out_width,
                                                int* out_height) {
    if (!renderer || !filepath) return 0;

    // TODO: Load file and call android_renderer_load_texture

    (void)out_width;
    (void)out_height;

    LOGI("Load texture from file (stub): %s\n", filepath);
    return 0;
}

void android_renderer_delete_texture(AndroidRenderer* renderer, GLuint texture_id) {
    if (!renderer || !texture_id) return;

#ifdef __ANDROID__
    glDeleteTextures(1, &texture_id);
#endif

    // Remove from cache
    for (int i = 0; i < MAX_TEXTURE_CACHE; i++) {
        if (renderer->texture_cache[i].used &&
            renderer->texture_cache[i].texture_id == texture_id) {
            renderer->texture_cache[i].used = false;
            break;
        }
    }
}

void android_renderer_draw_texture(AndroidRenderer* renderer,
                                    GLuint texture_id,
                                    float x, float y,
                                    float width, float height,
                                    uint32_t tint_color) {
    if (!renderer || !texture_id) return;

    // TODO: Implement texture rendering
    // 1. Switch to texture shader
    // 2. Bind texture
    // 3. Add textured quad to batch

    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)tint_color;
}
