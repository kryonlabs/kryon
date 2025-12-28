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

// stb_image for PNG/JPEG loading
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb_image.h"

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
// Texture Loading
// ============================================================================

GLuint android_renderer_load_texture(AndroidRenderer* renderer,
                                      const void* data,
                                      size_t size,
                                      int* out_width,
                                      int* out_height) {
    if (!renderer || !data || size == 0) return 0;

    // Decode image using stb_image
    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory((const stbi_uc*)data, (int)size,
                                                   &width, &height, &channels, 4);

    if (!pixels) {
        LOGE("Failed to decode image: %s\n", stbi_failure_reason());
        return 0;
    }

    LOGI("Decoded image: %dx%d with %d channels\n", width, height, channels);

#ifdef __ANDROID__
    // Create OpenGL texture
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Upload pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Generate mipmaps
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
#else
    GLuint texture_id = 1; // Dummy value for non-Android builds
#endif

    // Free pixel data
    stbi_image_free(pixels);

    // Return dimensions
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;

    LOGI("Created texture %u (%dx%d)\n", texture_id, width, height);
    return texture_id;
}

GLuint android_renderer_load_texture_from_file(AndroidRenderer* renderer,
                                                const char* filepath,
                                                int* out_width,
                                                int* out_height) {
    if (!renderer || !filepath) return 0;

    LOGI("Loading texture from file: %s\n", filepath);

    // Read file into memory
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        LOGE("Failed to open file: %s\n", filepath);
        return 0;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        LOGE("Invalid file size: %ld\n", file_size);
        fclose(file);
        return 0;
    }

    // Allocate buffer
    unsigned char* buffer = (unsigned char*)malloc(file_size);
    if (!buffer) {
        LOGE("Failed to allocate memory for file: %ld bytes\n", file_size);
        fclose(file);
        return 0;
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        LOGE("Failed to read file: read %zu of %ld bytes\n", bytes_read, file_size);
        free(buffer);
        return 0;
    }

    // Load texture from memory
    GLuint texture_id = android_renderer_load_texture(renderer, buffer, file_size,
                                                       out_width, out_height);

    free(buffer);
    return texture_id;
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

    // Check if we need to flush batch
    if (renderer->vertex_count + 4 > MAX_VERTICES ||
        renderer->index_count + 6 > MAX_INDICES) {
        android_renderer_flush_batch(renderer);
    }

    // Switch to texture shader
    android_shader_use(renderer, SHADER_PROGRAM_TEXTURE);

#ifdef __ANDROID__
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glUniform1i(renderer->shader_programs[SHADER_PROGRAM_TEXTURE].u_texture, 0);
#endif

    // Extract tint color components
    uint8_t a = (tint_color >> 24) & 0xFF;
    uint8_t r = (tint_color >> 16) & 0xFF;
    uint8_t g = (tint_color >> 8) & 0xFF;
    uint8_t b = tint_color & 0xFF;

    // Apply global opacity
    a = (uint8_t)(a * renderer->global_opacity);

    // Add 4 vertices for textured quad
    uint32_t base_index = renderer->vertex_count;

    // Top-left
    renderer->vertices[renderer->vertex_count].x = x;
    renderer->vertices[renderer->vertex_count].y = y;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Top-right
    renderer->vertices[renderer->vertex_count].x = x + width;
    renderer->vertices[renderer->vertex_count].y = y;
    renderer->vertices[renderer->vertex_count].u = 1.0f;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Bottom-right
    renderer->vertices[renderer->vertex_count].x = x + width;
    renderer->vertices[renderer->vertex_count].y = y + height;
    renderer->vertices[renderer->vertex_count].u = 1.0f;
    renderer->vertices[renderer->vertex_count].v = 1.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Bottom-left
    renderer->vertices[renderer->vertex_count].x = x;
    renderer->vertices[renderer->vertex_count].y = y + height;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = 1.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Add indices for 2 triangles (6 indices)
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 1;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 3;
}
