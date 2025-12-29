#ifndef KRYON_ANDROID_RENDERER_H
#define KRYON_ANDROID_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __ANDROID__
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#else
// Mock types for non-Android builds
typedef void* ANativeWindow;
typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef unsigned int GLuint;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct IRComponent IRComponent;
typedef struct AndroidRenderer AndroidRenderer;

// ============================================================================
// Renderer Configuration
// ============================================================================

typedef struct {
    int window_width;
    int window_height;
    bool vsync_enabled;
    int target_fps;
    bool debug_mode;

    // Font configuration
    const char* default_font_path;
    int default_font_size;

    // Performance settings
    bool enable_texture_cache;
    size_t texture_cache_size_mb;
    bool enable_glyph_cache;
    size_t glyph_cache_size_mb;
} AndroidRendererConfig;

// ============================================================================
// Renderer Lifecycle
// ============================================================================

// Create renderer with configuration
AndroidRenderer* android_renderer_create(const AndroidRendererConfig* config);

// Initialize renderer with native window
bool android_renderer_initialize(AndroidRenderer* renderer, ANativeWindow* window);

// Initialize GL resources only (for GLSurfaceView where EGL context is pre-created)
bool android_renderer_initialize_gl_only(AndroidRenderer* renderer);

// Shutdown and cleanup renderer
void android_renderer_shutdown(AndroidRenderer* renderer);

// Destroy renderer (releases all resources)
void android_renderer_destroy(AndroidRenderer* renderer);

// ============================================================================
// Rendering
// ============================================================================

// Begin frame rendering
bool android_renderer_begin_frame(AndroidRenderer* renderer);

// End frame and present
bool android_renderer_end_frame(AndroidRenderer* renderer);

// Render component tree
bool android_renderer_render_component(AndroidRenderer* renderer, IRComponent* root);

// Clear screen with color
void android_renderer_clear(AndroidRenderer* renderer, uint32_t color);

// ============================================================================
// Surface Management
// ============================================================================

// Handle window resize
void android_renderer_resize(AndroidRenderer* renderer, int width, int height);

// Check if renderer is ready
bool android_renderer_is_ready(AndroidRenderer* renderer);

// Get current framebuffer size
void android_renderer_get_size(AndroidRenderer* renderer, int* width, int* height);

// ============================================================================
// Font Management
// ============================================================================

// Register font from file path
bool android_renderer_register_font(AndroidRenderer* renderer,
                                     const char* font_name,
                                     const char* font_path);

// Set default font
bool android_renderer_set_default_font(AndroidRenderer* renderer,
                                        const char* font_name,
                                        int size);

// Measure text dimensions
// Returns visual bounding box: width = horizontal advance,
// height = actual vertical space occupied by rendered glyphs
bool android_renderer_measure_text(AndroidRenderer* renderer,
                                    const char* text,
                                    const char* font_name,
                                    int font_size,
                                    float* width,
                                    float* height);

// ============================================================================
// Primitive Rendering
// ============================================================================

// Render rectangle (filled)
void android_renderer_draw_rect(AndroidRenderer* renderer,
                                 float x, float y,
                                 float width, float height,
                                 uint32_t color);

// Render rectangle with rounded corners
void android_renderer_draw_rounded_rect(AndroidRenderer* renderer,
                                         float x, float y,
                                         float width, float height,
                                         float radius,
                                         uint32_t color);

// Render rectangle outline
void android_renderer_draw_rect_outline(AndroidRenderer* renderer,
                                         float x, float y,
                                         float width, float height,
                                         float border_width,
                                         uint32_t color);

// Render text
void android_renderer_draw_text(AndroidRenderer* renderer,
                                 const char* text,
                                 float x, float y,
                                 const char* font_name,
                                 int font_size,
                                 uint32_t color);

// Render texture/image
void android_renderer_draw_texture(AndroidRenderer* renderer,
                                    GLuint texture_id,
                                    float x, float y,
                                    float width, float height,
                                    uint32_t tint_color);

// Render linear gradient rectangle
void android_renderer_draw_gradient_rect(AndroidRenderer* renderer,
                                          float x, float y,
                                          float width, float height,
                                          uint32_t color_start,
                                          uint32_t color_end,
                                          bool vertical);

// Render box shadow
void android_renderer_draw_box_shadow(AndroidRenderer* renderer,
                                       float x, float y,
                                       float width, float height,
                                       float radius,
                                       float offset_x, float offset_y,
                                       float blur,
                                       uint32_t shadow_color);

// ============================================================================
// Texture Management
// ============================================================================

// Load texture from memory (PNG/JPEG)
GLuint android_renderer_load_texture(AndroidRenderer* renderer,
                                      const void* data,
                                      size_t size,
                                      int* out_width,
                                      int* out_height);

// Load texture from file
GLuint android_renderer_load_texture_from_file(AndroidRenderer* renderer,
                                                const char* filepath,
                                                int* out_width,
                                                int* out_height);

// Delete texture
void android_renderer_delete_texture(AndroidRenderer* renderer, GLuint texture_id);

// ============================================================================
// Advanced Rendering
// ============================================================================

// Set clipping rectangle
void android_renderer_push_clip_rect(AndroidRenderer* renderer,
                                      float x, float y,
                                      float width, float height);

// Remove clipping rectangle
void android_renderer_pop_clip_rect(AndroidRenderer* renderer);

// Set global opacity/alpha
void android_renderer_set_opacity(AndroidRenderer* renderer, float opacity);

// Reset opacity to 1.0
void android_renderer_reset_opacity(AndroidRenderer* renderer);

// ============================================================================
// Performance & Debugging
// ============================================================================

// Get current FPS
float android_renderer_get_fps(AndroidRenderer* renderer);

// Get frame time in milliseconds
float android_renderer_get_frame_time(AndroidRenderer* renderer);

// Get render statistics
typedef struct {
    uint32_t frame_count;
    uint32_t draw_calls;
    uint32_t triangles;
    uint32_t texture_switches;
    uint32_t shader_switches;
    float fps;
    float frame_time_ms;
    size_t texture_memory_mb;
    size_t glyph_cache_entries;
} AndroidRendererStats;

void android_renderer_get_stats(AndroidRenderer* renderer, AndroidRendererStats* stats);

// Reset statistics
void android_renderer_reset_stats(AndroidRenderer* renderer);

// Print debug information
void android_renderer_print_info(AndroidRenderer* renderer);

// ============================================================================
// Error Handling
// ============================================================================

typedef enum {
    ANDROID_RENDERER_ERROR_NONE = 0,
    ANDROID_RENDERER_ERROR_NOT_INITIALIZED = 1,
    ANDROID_RENDERER_ERROR_EGL_FAILED = 2,
    ANDROID_RENDERER_ERROR_GLES_FAILED = 3,
    ANDROID_RENDERER_ERROR_SHADER_COMPILE_FAILED = 4,
    ANDROID_RENDERER_ERROR_TEXTURE_LOAD_FAILED = 5,
    ANDROID_RENDERER_ERROR_FONT_LOAD_FAILED = 6,
    ANDROID_RENDERER_ERROR_OUT_OF_MEMORY = 7,
    ANDROID_RENDERER_ERROR_INVALID_PARAM = 8
} AndroidRendererError;

AndroidRendererError android_renderer_get_last_error(AndroidRenderer* renderer);
const char* android_renderer_get_error_string(AndroidRendererError error);

// ============================================================================
// OpenGL ES Capabilities
// ============================================================================

typedef struct {
    bool has_gles3;
    bool has_gles31;
    bool has_gles32;
    int max_texture_size;
    int max_vertex_attribs;
    int max_texture_units;
    int max_renderbuffer_size;
    const char* vendor;
    const char* renderer;
    const char* version;
    const char* extensions;
} AndroidRendererCapabilities;

AndroidRendererCapabilities android_renderer_get_capabilities(AndroidRenderer* renderer);

#ifdef __cplusplus
}
#endif

#endif // KRYON_ANDROID_RENDERER_H
