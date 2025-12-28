#ifndef KRYON_ANDROID_RENDERER_INTERNAL_H
#define KRYON_ANDROID_RENDERER_INTERNAL_H

#include "android_renderer.h"
#include "android_shaders.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#else
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef float GLfloat;
#endif

// ============================================================================
// Internal Constants
// ============================================================================

#define MAX_CLIP_STACK_DEPTH 16
#define MAX_VERTICES 10000
#define MAX_INDICES 15000
#define MAX_FONT_REGISTRY 32
#define MAX_TEXTURE_CACHE 256
#define MAX_GLYPH_CACHE 1024

// ============================================================================
// Vertex Structures
// ============================================================================

typedef struct {
    float x, y;
    float u, v;
    uint8_t r, g, b, a;
} Vertex;

typedef struct {
    float x, y;
    uint8_t r, g, b, a;
} ColorVertex;

// ============================================================================
// Shader Program
// ============================================================================

typedef struct {
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;

    // Uniform locations
    GLint u_projection;
    GLint u_texture;
    GLint u_color_start;
    GLint u_color_end;
    GLint u_size;
    GLint u_radius;
    GLint u_shadow_offset;
    GLint u_shadow_blur;
    GLint u_shadow_color;
    GLint u_box_size;
    GLint u_box_radius;

    bool compiled;
} ShaderProgram;

// ============================================================================
// Font & Glyph Management
// ============================================================================

typedef struct {
    GLuint texture_id;
    int width;
    int height;
    int bearing_x;
    int bearing_y;
    int advance;
    float u0, v0, u1, v1;  // Atlas texture coordinates
} GlyphInfo;

typedef struct {
    char name[64];
    char path[256];
    void* font_info;      // stbtt_fontinfo*
    void* font_buffer;    // Font file data
    int size;
    GLuint atlas_texture; // Glyph atlas texture
    int atlas_width;
    int atlas_height;
    GlyphInfo glyphs[256];  // ASCII only for now
    float scale;
    int ascent;
    int descent;
    int line_gap;
    bool loaded;
} FontInfo;

// ============================================================================
// Texture Cache
// ============================================================================

typedef struct {
    GLuint texture_id;
    int width;
    int height;
    uint64_t last_used_frame;
    char key[256];
    bool used;
} TextureCacheEntry;

// ============================================================================
// Clipping Stack
// ============================================================================

typedef struct {
    float x, y;
    float width, height;
} ClipRect;

// ============================================================================
// Renderer State
// ============================================================================

struct AndroidRenderer {
    // Configuration
    AndroidRendererConfig config;

    // EGL state
#ifdef __ANDROID__
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLConfig egl_config;
#else
    void* egl_display;
    void* egl_surface;
    void* egl_context;
    void* egl_config;
#endif

    // Window state
    int window_width;
    int window_height;

    // Initialization flags
    bool initialized;
    bool egl_initialized;
    bool shaders_initialized;

    // Shader programs
    ShaderProgram shader_programs[SHADER_PROGRAM_COUNT];
    ShaderProgramType current_shader;

    // Projection matrix (orthographic)
    float projection_matrix[16];

    // Vertex batch buffers
    Vertex vertices[MAX_VERTICES];
    uint16_t indices[MAX_INDICES];
    uint32_t vertex_count;
    uint32_t index_count;

    // OpenGL buffers
    GLuint vao;
    GLuint vbo;
    GLuint ibo;

    // Clipping stack
    ClipRect clip_stack[MAX_CLIP_STACK_DEPTH];
    int clip_stack_depth;

    // Rendering state
    float global_opacity;
    GLuint current_texture;

    // Font management
#ifdef __ANDROID__
    FT_Library ft_library;
#else
    void* ft_library;
#endif
    FontInfo font_registry[MAX_FONT_REGISTRY];
    int font_registry_count;
    char default_font_name[64];
    int default_font_size;

    // Texture cache
    TextureCacheEntry texture_cache[MAX_TEXTURE_CACHE];
    int texture_cache_count;

    // Performance tracking
    uint64_t frame_count;
    double last_frame_time;
    double frame_start_time;
    float fps;
    float frame_time_ms;

    // Statistics
    AndroidRendererStats stats;

    // Error handling
    AndroidRendererError last_error;

    // Capabilities
    AndroidRendererCapabilities capabilities;
};

// ============================================================================
// Internal Function Declarations
// ============================================================================

// EGL functions
bool android_egl_init(AndroidRenderer* renderer, ANativeWindow* window);
void android_egl_shutdown(AndroidRenderer* renderer);
bool android_egl_swap_buffers(AndroidRenderer* renderer);
bool android_egl_resize(AndroidRenderer* renderer, int width, int height);

// Shader functions
bool android_shader_init_all(AndroidRenderer* renderer);
void android_shader_cleanup_all(AndroidRenderer* renderer);
bool android_shader_compile(ShaderProgram* program,
                             const char* vertex_src,
                             const char* fragment_src);
void android_shader_use(AndroidRenderer* renderer, ShaderProgramType type);

// Rendering functions
void android_renderer_update_projection(AndroidRenderer* renderer);
void android_renderer_flush_batch(AndroidRenderer* renderer);
void android_renderer_add_rect(AndroidRenderer* renderer,
                                float x, float y,
                                float width, float height,
                                uint32_t color);

// Font functions
bool android_font_init(AndroidRenderer* renderer);
void android_font_cleanup(AndroidRenderer* renderer);
FontInfo* android_font_get(AndroidRenderer* renderer, const char* name, int size);
GlyphInfo* android_font_get_glyph(FontInfo* font, char c);

// Texture functions
void android_texture_cache_init(AndroidRenderer* renderer);
void android_texture_cache_cleanup(AndroidRenderer* renderer);
GLuint android_texture_cache_get(AndroidRenderer* renderer, const char* key);
void android_texture_cache_set(AndroidRenderer* renderer, const char* key, GLuint texture_id, int width, int height);

// Utility functions
void android_renderer_set_error(AndroidRenderer* renderer, AndroidRendererError error);
uint32_t android_color_to_rgba(uint32_t color);
void android_color_to_floats(uint32_t color, float* r, float* g, float* b, float* a);

#endif // KRYON_ANDROID_RENDERER_INTERNAL_H
