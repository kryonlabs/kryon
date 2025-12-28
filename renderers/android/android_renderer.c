#include "android_renderer_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "KryonRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) fprintf(stdout, "[INFO] " __VA_ARGS__)
#define LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__)
#endif

// ============================================================================
// Utility Functions
// ============================================================================

static double get_current_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void android_renderer_set_error(AndroidRenderer* renderer, AndroidRendererError error) {
    if (renderer) {
        renderer->last_error = error;
    }
}

uint32_t android_color_to_rgba(uint32_t color) {
    // Convert ARGB to RGBA
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

void android_color_to_floats(uint32_t color, float* r, float* g, float* b, float* a) {
    *a = ((color >> 24) & 0xFF) / 255.0f;
    *r = ((color >> 16) & 0xFF) / 255.0f;
    *g = ((color >> 8) & 0xFF) / 255.0f;
    *b = (color & 0xFF) / 255.0f;
}

void android_renderer_update_projection(AndroidRenderer* renderer) {
    if (!renderer) return;

    // Create orthographic projection matrix
    float left = 0.0f;
    float right = (float)renderer->window_width;
    float bottom = (float)renderer->window_height;
    float top = 0.0f;
    float near_plane = -1.0f;
    float far_plane = 1.0f;

    float* m = renderer->projection_matrix;

    m[0] = 2.0f / (right - left);
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;

    m[4] = 0.0f;
    m[5] = 2.0f / (top - bottom);
    m[6] = 0.0f;
    m[7] = 0.0f;

    m[8] = 0.0f;
    m[9] = 0.0f;
    m[10] = -2.0f / (far_plane - near_plane);
    m[11] = 0.0f;

    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(far_plane + near_plane) / (far_plane - near_plane);
    m[15] = 1.0f;
}

// ============================================================================
// Renderer Lifecycle
// ============================================================================

AndroidRenderer* android_renderer_create(const AndroidRendererConfig* config) {
    if (!config) {
        LOGE("Invalid renderer configuration\n");
        return NULL;
    }

    AndroidRenderer* renderer = (AndroidRenderer*)calloc(1, sizeof(AndroidRenderer));
    if (!renderer) {
        LOGE("Failed to allocate renderer\n");
        return NULL;
    }

    // Copy configuration
    renderer->config = *config;

    // Set default values
    renderer->global_opacity = 1.0f;
    renderer->current_shader = SHADER_PROGRAM_COUNT;  // Invalid initially

    LOGI("Renderer created\n");

    return renderer;
}

bool android_renderer_initialize(AndroidRenderer* renderer, ANativeWindow* window) {
    if (!renderer) {
        LOGE("Renderer is NULL\n");
        return false;
    }

    if (renderer->initialized) {
        LOGI("Renderer already initialized\n");
        return true;
    }

    LOGI("Initializing Android renderer...\n");

    // Initialize EGL
    if (!android_egl_init(renderer, window)) {
        LOGE("Failed to initialize EGL\n");
        android_renderer_set_error(renderer, ANDROID_RENDERER_ERROR_EGL_FAILED);
        return false;
    }

    renderer->egl_initialized = true;

    // Initialize shaders
    if (!android_shader_init_all(renderer)) {
        LOGE("Failed to initialize shaders\n");
        android_renderer_set_error(renderer, ANDROID_RENDERER_ERROR_SHADER_COMPILE_FAILED);
        android_egl_shutdown(renderer);
        return false;
    }

#ifdef __ANDROID__
    // Create vertex array object
    glGenVertexArrays(1, &renderer->vao);
    glBindVertexArray(renderer->vao);

    // Create vertex buffer
    glGenBuffers(1, &renderer->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(renderer->vertices), NULL, GL_DYNAMIC_DRAW);

    // Create index buffer
    glGenBuffers(1, &renderer->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(renderer->indices), NULL, GL_DYNAMIC_DRAW);

    // Setup vertex attributes (for texture shader)
    // Position (vec2) - Location 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, x));

    // TexCoord (vec2) - Location 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, u));

    // Color (vec4) - Location 2
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                          (void*)offsetof(Vertex, r));

    glBindVertexArray(0);

    LOGI("OpenGL buffers created\n");
#endif

    // Update projection matrix
    android_renderer_update_projection(renderer);

    // Initialize font system
    if (!android_font_init(renderer)) {
        LOGE("Failed to initialize font system\n");
        android_renderer_set_error(renderer, ANDROID_RENDERER_ERROR_FONT_LOAD_FAILED);
    }

    // Initialize texture cache
    android_texture_cache_init(renderer);

    renderer->initialized = true;
    renderer->frame_start_time = get_current_time();

    LOGI("Renderer initialized successfully\n");
    android_renderer_print_info(renderer);

    return true;
}

void android_renderer_shutdown(AndroidRenderer* renderer) {
    if (!renderer || !renderer->initialized) {
        return;
    }

    LOGI("Shutting down renderer...\n");

#ifdef __ANDROID__
    // Delete OpenGL buffers
    if (renderer->vao) {
        glDeleteVertexArrays(1, &renderer->vao);
        renderer->vao = 0;
    }

    if (renderer->vbo) {
        glDeleteBuffers(1, &renderer->vbo);
        renderer->vbo = 0;
    }

    if (renderer->ibo) {
        glDeleteBuffers(1, &renderer->ibo);
        renderer->ibo = 0;
    }
#endif

    // Cleanup font system
    android_font_cleanup(renderer);

    // Cleanup texture cache
    android_texture_cache_cleanup(renderer);

    // Cleanup shaders
    if (renderer->shaders_initialized) {
        android_shader_cleanup_all(renderer);
    }

    // Shutdown EGL
    if (renderer->egl_initialized) {
        android_egl_shutdown(renderer);
        renderer->egl_initialized = false;
    }

    renderer->initialized = false;

    LOGI("Renderer shutdown complete\n");
}

void android_renderer_destroy(AndroidRenderer* renderer) {
    if (!renderer) return;

    android_renderer_shutdown(renderer);
    free(renderer);

    LOGI("Renderer destroyed\n");
}

// ============================================================================
// Frame Rendering
// ============================================================================

bool android_renderer_begin_frame(AndroidRenderer* renderer) {
    if (!renderer || !renderer->initialized) {
        return false;
    }

    renderer->frame_start_time = get_current_time();

    // Clear screen
#ifdef __ANDROID__
    glClear(GL_COLOR_BUFFER_BIT);
#endif

    // Reset frame state
    renderer->vertex_count = 0;
    renderer->index_count = 0;
    renderer->clip_stack_depth = 0;
    renderer->current_texture = 0;

    // Reset stats for this frame
    renderer->stats.draw_calls = 0;
    renderer->stats.triangles = 0;
    renderer->stats.texture_switches = 0;
    renderer->stats.shader_switches = 0;

    return true;
}

bool android_renderer_end_frame(AndroidRenderer* renderer) {
    static int end_frame_count = 0;
    if (end_frame_count++ % 60 == 0) {
        LOGI("end_frame called: renderer=%p, initialized=%d, vertex_count=%d\n",
             renderer, renderer ? renderer->initialized : -1,
             renderer ? renderer->vertex_count : -1);
    }

    if (!renderer || !renderer->initialized) {
        LOGE("end_frame: renderer not initialized!\n");
        return false;
    }

    // Flush any pending batched geometry
    android_renderer_flush_batch(renderer);

    // Swap buffers
    if (!android_egl_swap_buffers(renderer)) {
        LOGE("Failed to swap buffers\n");
        return false;
    }

    // Update performance metrics
    double frame_end_time = get_current_time();
    renderer->frame_time_ms = (float)((frame_end_time - renderer->frame_start_time) * 1000.0);
    renderer->frame_count++;

    // Update FPS every second
    double time_since_last_update = frame_end_time - renderer->last_frame_time;
    if (time_since_last_update >= 1.0) {
        renderer->fps = renderer->frame_count / (float)time_since_last_update;
        renderer->stats.fps = renderer->fps;
        renderer->stats.frame_time_ms = renderer->frame_time_ms;
        renderer->stats.frame_count = (uint32_t)renderer->frame_count;
        renderer->last_frame_time = frame_end_time;
        renderer->frame_count = 0;
    }

    return true;
}

void android_renderer_clear(AndroidRenderer* renderer, uint32_t color) {
    if (!renderer) return;

    float r, g, b, a;
    android_color_to_floats(color, &r, &g, &b, &a);

#ifdef __ANDROID__
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
#endif
}

// ============================================================================
// Surface Management
// ============================================================================

void android_renderer_resize(AndroidRenderer* renderer, int width, int height) {
    if (!renderer || !renderer->initialized) {
        return;
    }

    LOGI("Renderer resize: %dx%d\n", width, height);

    android_egl_resize(renderer, width, height);
}

bool android_renderer_is_ready(AndroidRenderer* renderer) {
    return renderer && renderer->initialized && renderer->egl_initialized;
}

void android_renderer_get_size(AndroidRenderer* renderer, int* width, int* height) {
    if (!renderer) return;

    if (width) *width = renderer->window_width;
    if (height) *height = renderer->window_height;
}

// ============================================================================
// Performance & Debugging
// ============================================================================

float android_renderer_get_fps(AndroidRenderer* renderer) {
    return renderer ? renderer->fps : 0.0f;
}

float android_renderer_get_frame_time(AndroidRenderer* renderer) {
    return renderer ? renderer->frame_time_ms : 0.0f;
}

void android_renderer_get_stats(AndroidRenderer* renderer, AndroidRendererStats* stats) {
    if (!renderer || !stats) return;

    *stats = renderer->stats;
}

void android_renderer_reset_stats(AndroidRenderer* renderer) {
    if (!renderer) return;

    memset(&renderer->stats, 0, sizeof(AndroidRendererStats));
}

void android_renderer_print_info(AndroidRenderer* renderer) {
    if (!renderer) return;

    printf("=== Android Renderer Info ===\n");
    printf("Window Size: %dx%d\n", renderer->window_width, renderer->window_height);
    printf("VSync: %s\n", renderer->config.vsync_enabled ? "Enabled" : "Disabled");
    printf("Target FPS: %d\n", renderer->config.target_fps);

    if (renderer->capabilities.vendor) {
        printf("OpenGL Vendor: %s\n", renderer->capabilities.vendor);
        printf("OpenGL Renderer: %s\n", renderer->capabilities.renderer);
        printf("OpenGL Version: %s\n", renderer->capabilities.version);
        printf("GLES 3.0: %s\n", renderer->capabilities.has_gles3 ? "Yes" : "No");
        printf("GLES 3.1: %s\n", renderer->capabilities.has_gles31 ? "Yes" : "No");
        printf("GLES 3.2: %s\n", renderer->capabilities.has_gles32 ? "Yes" : "No");
        printf("Max Texture Size: %d\n", renderer->capabilities.max_texture_size);
        printf("Max Texture Units: %d\n", renderer->capabilities.max_texture_units);
    }

    printf("============================\n");
}

// ============================================================================
// Error Handling
// ============================================================================

AndroidRendererError android_renderer_get_last_error(AndroidRenderer* renderer) {
    return renderer ? renderer->last_error : ANDROID_RENDERER_ERROR_INVALID_PARAM;
}

const char* android_renderer_get_error_string(AndroidRendererError error) {
    switch (error) {
        case ANDROID_RENDERER_ERROR_NONE:
            return "No error";
        case ANDROID_RENDERER_ERROR_NOT_INITIALIZED:
            return "Renderer not initialized";
        case ANDROID_RENDERER_ERROR_EGL_FAILED:
            return "EGL initialization failed";
        case ANDROID_RENDERER_ERROR_GLES_FAILED:
            return "OpenGL ES operation failed";
        case ANDROID_RENDERER_ERROR_SHADER_COMPILE_FAILED:
            return "Shader compilation failed";
        case ANDROID_RENDERER_ERROR_TEXTURE_LOAD_FAILED:
            return "Texture loading failed";
        case ANDROID_RENDERER_ERROR_FONT_LOAD_FAILED:
            return "Font loading failed";
        case ANDROID_RENDERER_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case ANDROID_RENDERER_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        default:
            return "Unknown error";
    }
}

AndroidRendererCapabilities android_renderer_get_capabilities(AndroidRenderer* renderer) {
    if (!renderer) {
        AndroidRendererCapabilities empty = {0};
        return empty;
    }

    return renderer->capabilities;
}

// ============================================================================
// Batch Rendering
// ============================================================================

void android_renderer_flush_batch(AndroidRenderer* renderer) {
    if (!renderer || renderer->vertex_count == 0) {
        return;
    }

    // DEBUG: Log flush
    static int flush_count = 0;
    if (flush_count++ < 5 || flush_count % 60 == 0) {
        LOGI("Flushing batch: %d vertices, %d indices (%d triangles)\n",
             renderer->vertex_count, renderer->index_count, renderer->index_count / 3);
        // Log first few vertex colors
        for (int i = 0; i < renderer->vertex_count && i < 8; i++) {
            if (i % 4 == 0) {  // Log one vertex per quad
                LOGI("  Vertex[%d]: pos=(%.0f,%.0f) color=(%d,%d,%d,%d)\n",
                     i, renderer->vertices[i].x, renderer->vertices[i].y,
                     renderer->vertices[i].r, renderer->vertices[i].g,
                     renderer->vertices[i].b, renderer->vertices[i].a);
            }
        }
    }

#ifdef __ANDROID__
    // Update vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    renderer->vertex_count * sizeof(Vertex),
                    renderer->vertices);

    // Update index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ibo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                    renderer->index_count * sizeof(uint16_t),
                    renderer->indices);

    // Draw
    glBindVertexArray(renderer->vao);
    glDrawElements(GL_TRIANGLES, renderer->index_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);

    // Update stats
    renderer->stats.draw_calls++;
    renderer->stats.triangles += renderer->index_count / 3;
#endif

    // Reset counters
    renderer->vertex_count = 0;
    renderer->index_count = 0;
}

// Stub implementations for functions that will be implemented later
bool android_renderer_render_component(AndroidRenderer* renderer, IRComponent* root) {
    (void)renderer;
    (void)root;
    // TODO: Implement component tree rendering
    return false;
}
