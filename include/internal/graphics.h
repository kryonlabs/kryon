/**
 * @file graphics.h
 * @brief Central Graphics Header for Kryon
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * This header consolidates all graphics-related types and includes for
 * the Kryon graphics subsystem, providing a unified interface for 
 * image loading, texture management, font handling, and shader compilation.
 */

#ifndef KRYON_GRAPHICS_H
#define KRYON_GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Include the proper headers we actually need
#include "internal/memory.h"
#include "internal/color_utils.h"
#include "internal/renderer_interface.h"
#include "internal/shader_compiler.h"

// Renderer types for specific implementations
typedef enum {
    KRYON_RENDERER_RAYLIB,
    KRYON_RENDERER_SDL2,
    KRYON_RENDERER_HTML
} KryonRendererType;


// Texture formats
typedef enum {
    KRYON_TEXTURE_FORMAT_RGBA8,
    KRYON_TEXTURE_FORMAT_RGB8,
    KRYON_TEXTURE_FORMAT_RG8,
    KRYON_TEXTURE_FORMAT_R8,
    KRYON_TEXTURE_FORMAT_DEPTH24_STENCIL8
} KryonTextureFormat;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonImage KryonImage;
typedef struct KryonTexture KryonTexture;
typedef struct KryonShader KryonShader;
typedef struct KryonTextMetrics KryonTextMetrics;
typedef struct KryonFont KryonFont;

// =============================================================================
// PIXEL FORMATS
// =============================================================================

typedef enum {
    KRYON_PIXEL_FORMAT_UNKNOWN = 0,
    KRYON_PIXEL_FORMAT_R8,          // 8-bit red
    KRYON_PIXEL_FORMAT_RG8,         // 8-bit red, green
    KRYON_PIXEL_FORMAT_RGB8,        // 8-bit red, green, blue
    KRYON_PIXEL_FORMAT_RGBA8,       // 8-bit red, green, blue, alpha
    KRYON_PIXEL_FORMAT_R16,         // 16-bit red
    KRYON_PIXEL_FORMAT_RG16,        // 16-bit red, green
    KRYON_PIXEL_FORMAT_RGB16,       // 16-bit red, green, blue
    KRYON_PIXEL_FORMAT_RGBA16,      // 16-bit red, green, blue, alpha
    KRYON_PIXEL_FORMAT_R32F,        // 32-bit float red
    KRYON_PIXEL_FORMAT_RG32F,       // 32-bit float red, green
    KRYON_PIXEL_FORMAT_RGB32F,      // 32-bit float red, green, blue
    KRYON_PIXEL_FORMAT_RGBA32F      // 32-bit float red, green, blue, alpha
} KryonPixelFormat;

// =============================================================================
// IMAGE STRUCTURE
// =============================================================================

struct KryonImage {
    uint32_t width;                 // Image width in pixels
    uint32_t height;                // Image height in pixels
    KryonPixelFormat format;        // Pixel format
    uint8_t *data;                  // Raw pixel data
    size_t data_size;               // Size of pixel data in bytes
    uint32_t channels;              // Number of color channels
    uint32_t bit_depth;             // Bits per channel
};

// =============================================================================
// TEXTURE STRUCTURE
// =============================================================================

struct KryonTexture {
    uint32_t id;                    // Texture ID for renderer
    uint32_t width;                 // Texture width
    uint32_t height;                // Texture height
    KryonTextureFormat format;      // Texture format
    void *renderer_data;            // Renderer-specific data
    bool is_render_target;          // Can be used as render target
    uint32_t mip_levels;            // Number of mipmap levels
    bool has_mipmaps;               // Has mipmaps generated
};

// =============================================================================
// SHADER STRUCTURE (Simple version for graphics module)
// =============================================================================

struct KryonShader {
    uint32_t id;                    // Shader program ID
    KryonShaderType type;           // Shader type
    void *renderer_data;            // Renderer-specific data
    char *name;                     // Shader name for debugging
    bool is_compiled;               // Compilation status
};

// =============================================================================
// FONT STRUCTURE
// =============================================================================

struct KryonFont {
    uint32_t id;                    // Font ID for renderer
    int size;                       // Font size in pixels
    float line_height;              // Line height
    void *renderer_data;            // Renderer-specific font data
    char *name;                     // Font name for debugging
    bool is_default;                // Is this the default font
};

// =============================================================================
// TEXT METRICS STRUCTURE
// =============================================================================

struct KryonTextMetrics {
    float width;                    // Text width in pixels
    float height;                   // Text height in pixels
    float ascent;                   // Distance from baseline to top
    float descent;                  // Distance from baseline to bottom
    float line_gap;                 // Gap between lines
};

// =============================================================================
// GRAPHICS SUBSYSTEM FUNCTIONS
// =============================================================================

// Image functions
KryonImage* kryon_image_create(uint32_t width, uint32_t height, KryonPixelFormat format);
KryonImage* kryon_image_load_from_file(const char* file_path);
KryonImage* kryon_image_load_from_memory(const void* data, size_t size);
void kryon_image_destroy(KryonImage* image);
bool kryon_image_save_to_file(const KryonImage* image, const char* file_path);
bool kryon_image_resize(KryonImage* image, uint32_t new_width, uint32_t new_height);
bool kryon_image_convert_format(KryonImage* image, KryonPixelFormat new_format);
void kryon_image_fill(KryonImage* image, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void kryon_image_copy_region(const KryonImage* src, KryonImage* dst,
                            uint32_t src_x, uint32_t src_y,
                            uint32_t dst_x, uint32_t dst_y,
                            uint32_t width, uint32_t height);

// Texture functions  
uint32_t kryon_texture_load_from_file(const char* file_path);
uint32_t kryon_texture_create_from_image(const KryonImage* image);
uint32_t kryon_texture_create(uint32_t width, uint32_t height, KryonTextureFormat format);
void kryon_texture_destroy(uint32_t texture_id);
bool kryon_texture_update(uint32_t texture_id, const void* pixels, 
                         uint32_t x, uint32_t y, uint32_t width, uint32_t height);
const KryonTexture* kryon_texture_get(uint32_t texture_id);
bool kryon_texture_get_size(uint32_t texture_id, uint32_t* width, uint32_t* height);

// Font functions
KryonFont* kryon_font_load(const char* file_path, int size);
void kryon_font_unload(KryonFont* font);
KryonTextMetrics kryon_font_measure_text(const KryonFont* font, const char* text);
KryonFont* kryon_font_get_default(void);

// Shader functions (simplified interface)
uint32_t kryon_shader_compile(const char* source, KryonShaderType type, const char* name);
uint32_t kryon_shader_create_program(uint32_t vertex_shader, uint32_t fragment_shader, const char* name);
void kryon_shader_destroy(uint32_t shader_id);
bool kryon_shader_use(uint32_t program_id);
bool kryon_shader_set_uniform_int(uint32_t program_id, const char* name, int value);
bool kryon_shader_set_uniform_float(uint32_t program_id, const char* name, float value);
bool kryon_shader_set_uniform_vec3(uint32_t program_id, const char* name, float x, float y, float z);
bool kryon_shader_set_uniform_vec4(uint32_t program_id, const char* name, float x, float y, float z, float w);
bool kryon_shader_set_uniform_matrix4(uint32_t program_id, const char* name, const float* matrix);

// Initialization functions
bool kryon_graphics_init(void);
void kryon_graphics_cleanup(void);
void kryon_graphics_set_renderer_type(const char* renderer_type);
KryonRendererType kryon_renderer_get_type(void);

#endif // KRYON_GRAPHICS_H