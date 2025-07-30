/**
 * @file image_loader.c
 * @brief Kryon Image Loader Implementation (Thin Abstraction Layer)
 */

#include "internal/graphics.h"
#include "internal/memory.h"
#include "internal/renderer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Include renderer-specific headers based on active renderer
#ifdef KRYON_RENDERER_RAYLIB
    #include <raylib.h>
#endif

#ifdef KRYON_RENDERER_SDL2
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#endif

// =============================================================================
// IMAGE FORMAT DETECTION
// =============================================================================

typedef enum {
    KRYON_IMAGE_FORMAT_UNKNOWN,
    KRYON_IMAGE_FORMAT_PNG,
    KRYON_IMAGE_FORMAT_JPEG,
    KRYON_IMAGE_FORMAT_BMP,
    KRYON_IMAGE_FORMAT_TGA,
    KRYON_IMAGE_FORMAT_GIF
} KryonImageFormat;

static KryonImageFormat detect_image_format(const uint8_t* data, size_t size) {
    if (size < 8) return KRYON_IMAGE_FORMAT_UNKNOWN;
    
    // PNG signature
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        return KRYON_IMAGE_FORMAT_PNG;
    }
    
    // JPEG signature
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return KRYON_IMAGE_FORMAT_JPEG;
    }
    
    // BMP signature
    if (data[0] == 0x42 && data[1] == 0x4D) {
        return KRYON_IMAGE_FORMAT_BMP;
    }
    
    // TGA signature (check footer)
    if (size >= 18) {
        // Check for TGA footer "TRUEVISION-XFILE"
        const char* tga_footer = "TRUEVISION-XFILE";
        if (size >= 18 + strlen(tga_footer) && 
            memcmp(data + size - 18, tga_footer, strlen(tga_footer)) == 0) {
            return KRYON_IMAGE_FORMAT_TGA;
        }
        
        // Check for basic TGA header structure
        uint8_t image_type = data[2];
        if (image_type == 1 || image_type == 2 || image_type == 3 || 
            image_type == 9 || image_type == 10 || image_type == 11) {
            return KRYON_IMAGE_FORMAT_TGA;
        }
    }
    
    // GIF signature
    if (size >= 6 && 
        ((data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && 
          data[3] == '8' && data[4] == '7' && data[5] == 'a') ||
         (data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && 
          data[3] == '8' && data[4] == '9' && data[5] == 'a'))) {
        return KRYON_IMAGE_FORMAT_GIF;
    }
    
    return KRYON_IMAGE_FORMAT_UNKNOWN;
}

// =============================================================================
// SIMPLE BMP LOADER
// =============================================================================

static KryonImage* load_bmp(const uint8_t* data, size_t size) {
    if (size < 54) return NULL; // Minimum BMP header size
    
    // Read BMP header
    uint32_t file_size = *(uint32_t*)(data + 2);
    uint32_t data_offset = *(uint32_t*)(data + 10);
    uint32_t header_size = *(uint32_t*)(data + 14);
    int32_t width = *(int32_t*)(data + 18);
    int32_t height = *(int32_t*)(data + 22);
    uint16_t planes = *(uint16_t*)(data + 26);
    uint16_t bits_per_pixel = *(uint16_t*)(data + 28);
    uint32_t compression = *(uint32_t*)(data + 30);
    
    (void)file_size; // Unused
    (void)planes;    // Unused
    
    // Validate header
    if (width <= 0 || height == 0 || compression != 0) {
        return NULL; // Unsupported format
    }
    
    bool flip_y = height > 0;
    if (height < 0) height = -height;
    
    // Only support 24-bit and 32-bit BMPs
    if (bits_per_pixel != 24 && bits_per_pixel != 32) {
        return NULL;
    }
    
    // Create image
    KryonImage* image = kryon_alloc(sizeof(KryonImage));
    if (!image) return NULL;
    
    memset(image, 0, sizeof(KryonImage));
    image->width = width;
    image->height = height;
    image->channels = (bits_per_pixel == 32) ? 4 : 3;
    image->format = (bits_per_pixel == 32) ? KRYON_PIXEL_FORMAT_RGBA8 : KRYON_PIXEL_FORMAT_RGB8;
    
    size_t image_data_size = width * height * image->channels;
    image->data = kryon_alloc(image_data_size);
    if (!image->data) {
        kryon_free(image);
        return NULL;
    }
    
    // Read pixel data
    const uint8_t* src = data + data_offset;
    uint8_t* dst = (uint8_t*)image->data;
    
    int bytes_per_pixel = bits_per_pixel / 8;
    int row_padding = (4 - (width * bytes_per_pixel) % 4) % 4;
    
    for (int y = 0; y < height; y++) {
        int dst_y = flip_y ? (height - 1 - y) : y;
        
        for (int x = 0; x < width; x++) {
            size_t src_idx = y * (width * bytes_per_pixel + row_padding) + x * bytes_per_pixel;
            size_t dst_idx = (dst_y * width + x) * image->channels;
            
            if (src_idx + bytes_per_pixel <= size - data_offset) {
                // BMP stores pixels as BGR(A), convert to RGB(A)
                dst[dst_idx] = src[src_idx + 2];     // R
                dst[dst_idx + 1] = src[src_idx + 1]; // G
                dst[dst_idx + 2] = src[src_idx];     // B
                if (image->channels == 4) {
                    dst[dst_idx + 3] = (bytes_per_pixel == 4) ? src[src_idx + 3] : 255; // A
                }
            }
        }
    }
    
    return image;
}

// =============================================================================
// SIMPLE TGA LOADER
// =============================================================================

static KryonImage* load_tga(const uint8_t* data, size_t size) {
    if (size < 18) return NULL; // Minimum TGA header size
    
    // Read TGA header
    uint8_t id_length = data[0];
    uint8_t color_map_type = data[1];
    uint8_t image_type = data[2];
    uint16_t width = *(uint16_t*)(data + 12);
    uint16_t height = *(uint16_t*)(data + 14);
    uint8_t pixel_depth = data[16];
    uint8_t image_descriptor = data[17];
    
    (void)color_map_type;    // Unused for now
    (void)image_descriptor;  // Unused for now
    
    // Skip image ID
    const uint8_t* pixel_data = data + 18 + id_length;
    
    // Only support uncompressed true-color images
    if (image_type != 2) {
        return NULL;
    }
    
    // Only support 24-bit and 32-bit TGAs
    if (pixel_depth != 24 && pixel_depth != 32) {
        return NULL;
    }
    
    // Create image
    KryonImage* image = kryon_alloc(sizeof(KryonImage));
    if (!image) return NULL;
    
    memset(image, 0, sizeof(KryonImage));
    image->width = width;
    image->height = height;
    image->channels = (pixel_depth == 32) ? 4 : 3;
    image->format = (pixel_depth == 32) ? KRYON_PIXEL_FORMAT_RGBA8 : KRYON_PIXEL_FORMAT_RGB8;
    
    size_t image_data_size = width * height * image->channels;
    image->data = kryon_alloc(image_data_size);
    if (!image->data) {
        kryon_free(image);
        return NULL;
    }
    
    // Read pixel data
    uint8_t* dst = (uint8_t*)image->data;
    int bytes_per_pixel = pixel_depth / 8;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t src_idx = (y * width + x) * bytes_per_pixel;
            size_t dst_idx = (y * width + x) * image->channels;
            
            if (src_idx + bytes_per_pixel <= size - (pixel_data - data)) {
                // TGA stores pixels as BGR(A), convert to RGB(A)
                dst[dst_idx] = pixel_data[src_idx + 2];     // R
                dst[dst_idx + 1] = pixel_data[src_idx + 1]; // G
                dst[dst_idx + 2] = pixel_data[src_idx];     // B
                if (image->channels == 4) {
                    dst[dst_idx + 3] = (bytes_per_pixel == 4) ? pixel_data[src_idx + 3] : 255; // A
                }
            }
        }
    }
    
    return image;
}

// =============================================================================
// IMAGE PROCESSING UTILITIES
// =============================================================================

static void convert_to_rgba(KryonImage* image) {
    if (!image || image->channels == 4) return;
    
    size_t old_size = image->width * image->height * image->channels;
    size_t new_size = image->width * image->height * 4;
    
    uint8_t* new_data = kryon_alloc(new_size);
    if (!new_data) return;
    
    uint8_t* old_data = (uint8_t*)image->data;
    
    for (size_t i = 0; i < image->width * image->height; i++) {
        size_t old_idx = i * image->channels;
        size_t new_idx = i * 4;
        
        if (image->channels == 1) {
            // Grayscale to RGBA
            new_data[new_idx] = old_data[old_idx];     // R
            new_data[new_idx + 1] = old_data[old_idx]; // G
            new_data[new_idx + 2] = old_data[old_idx]; // B
            new_data[new_idx + 3] = 255;               // A
        } else if (image->channels == 3) {
            // RGB to RGBA
            new_data[new_idx] = old_data[old_idx];     // R
            new_data[new_idx + 1] = old_data[old_idx + 1]; // G
            new_data[new_idx + 2] = old_data[old_idx + 2]; // B
            new_data[new_idx + 3] = 255;               // A
        }
    }
    
    kryon_free(image->data);
    image->data = new_data;
    image->channels = 4;
    image->format = KRYON_PIXEL_FORMAT_RGBA8;
}

// =============================================================================
// RENDERER-SPECIFIC IMPLEMENTATIONS
// =============================================================================

#ifdef KRYON_RENDERER_RAYLIB
static KryonImage* raylib_load_image_from_file(const char* file_path) {
    Image raylib_image = LoadImage(file_path);
    if (raylib_image.data == NULL) return NULL;
    
    KryonImage* image = kryon_alloc(sizeof(KryonImage));
    if (!image) {
        UnloadImage(raylib_image);
        return NULL;
    }
    
    // Convert Raylib image to our format
    image->width = raylib_image.width;
    image->height = raylib_image.height;
    image->channels = 4; // Raylib typically uses RGBA
    image->format = KRYON_PIXEL_FORMAT_RGBA8;
    
    // Copy image data
    size_t data_size = image->width * image->height * image->channels;
    image->data = kryon_alloc(data_size);
    if (!image->data) {
        UnloadImage(raylib_image);
        kryon_free(image);
        return NULL;
    }
    
    // Ensure RGBA format
    if (raylib_image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&raylib_image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    
    memcpy(image->data, raylib_image.data, data_size);
    UnloadImage(raylib_image);
    
    return image;
}
#endif

#ifdef KRYON_RENDERER_SDL2
static KryonImage* sdl2_load_image_from_file(const char* file_path) {
    SDL_Surface* surface = IMG_Load(file_path);
    if (!surface) return NULL;
    
    KryonImage* image = kryon_alloc(sizeof(KryonImage));
    if (!image) {
        SDL_FreeSurface(surface);
        return NULL;
    }
    
    // Convert to RGBA format
    SDL_Surface* rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surface);
    
    if (!rgba_surface) {
        kryon_free(image);
        return NULL;
    }
    
    image->width = rgba_surface->w;
    image->height = rgba_surface->h;
    image->channels = 4;
    image->format = KRYON_PIXEL_FORMAT_RGBA8;
    
    // Copy pixel data
    size_t data_size = image->width * image->height * image->channels;
    image->data = kryon_alloc(data_size);
    if (!image->data) {
        SDL_FreeSurface(rgba_surface);
        kryon_free(image);
        return NULL;
    }
    
    memcpy(image->data, rgba_surface->pixels, data_size);
    SDL_FreeSurface(rgba_surface);
    
    return image;
}
#endif

static KryonImage* software_load_image_from_file(const char* file_path) {
    if (!file_path) return NULL;
    
    FILE* file = fopen(file_path, "rb");
    if (!file) return NULL;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    // Read file data
    uint8_t* file_data = kryon_alloc(file_size);
    if (!file_data) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(file_data, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        kryon_free(file_data);
        return NULL;
    }
    
    KryonImage* image = software_load_image_from_memory(file_data, file_size);
    kryon_free(file_data);
    
    return image;
}

// =============================================================================
// PUBLIC API
// =============================================================================

KryonImage* kryon_image_load_from_file(const char* file_path) {
    if (!file_path) return NULL;
    
    // Delegate to appropriate renderer implementation
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            return raylib_load_image_from_file(file_path);
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            return sdl2_load_image_from_file(file_path);
#endif
            
        case KRYON_RENDERER_SOFTWARE:
        case KRYON_RENDERER_HTML:
        default:
            return software_load_image_from_file(file_path);
    }
}

static KryonImage* software_load_image_from_memory(const void* data, size_t size) {
    if (!data || size == 0) return NULL;
    
    const uint8_t* byte_data = (const uint8_t*)data;
    KryonImageFormat format = detect_image_format(byte_data, size);
    
    KryonImage* image = NULL;
    
    switch (format) {
        case KRYON_IMAGE_FORMAT_BMP:
            image = load_bmp(byte_data, size);
            break;
            
        case KRYON_IMAGE_FORMAT_TGA:
            image = load_tga(byte_data, size);
            break;
            
        case KRYON_IMAGE_FORMAT_PNG:
        case KRYON_IMAGE_FORMAT_JPEG:
        case KRYON_IMAGE_FORMAT_GIF:
            // These would require external libraries (stb_image, etc.)
            printf("Image format not supported in basic implementation\n");
            break;
            
        default:
            printf("Unknown image format\n");
            break;
    }
    
    return image;
}

KryonImage* kryon_image_load_from_memory(const void* data, size_t size) {
    if (!data || size == 0) return NULL;
    
    // For memory loading, most renderers don't have direct support
    // So we primarily use our software implementation
    // TODO: Add renderer-specific memory loading if available
    
    return software_load_image_from_memory(data, size);
}

KryonImage* kryon_image_create(uint32_t width, uint32_t height, KryonPixelFormat format) {
    if (width == 0 || height == 0) return NULL;
    
    KryonImage* image = kryon_alloc(sizeof(KryonImage));
    if (!image) return NULL;
    
    memset(image, 0, sizeof(KryonImage));
    image->width = width;
    image->height = height;
    image->format = format;
    
    // Determine channels based on format
    switch (format) {
        case KRYON_PIXEL_FORMAT_R8:
            image->channels = 1;
            break;
        case KRYON_PIXEL_FORMAT_RGB8:
            image->channels = 3;
            break;
        case KRYON_PIXEL_FORMAT_RGBA8:
            image->channels = 4;
            break;
        default:
            image->channels = 4; // Default to RGBA
            break;
    }
    
    size_t data_size = width * height * image->channels;
    image->data = kryon_alloc(data_size);
    if (!image->data) {
        kryon_free(image);
        return NULL;
    }
    
    // Initialize to transparent black
    memset(image->data, 0, data_size);
    
    return image;
}

void kryon_image_destroy(KryonImage* image) {
    if (!image) return;
    
    kryon_free(image->data);
    kryon_free(image);
}

bool kryon_image_save_to_file(const KryonImage* image, const char* file_path) {
    if (!image || !file_path || !image->data) return false;
    
    // Simple BMP writer
    FILE* file = fopen(file_path, "wb");
    if (!file) return false;
    
    uint32_t width = image->width;
    uint32_t height = image->height;
    uint32_t channels = image->channels;
    
    // Convert to 24-bit for BMP if needed
    bool needs_conversion = (channels != 3 && channels != 4);
    uint32_t bmp_channels = needs_conversion ? 3 : channels;
    
    // Calculate padding
    uint32_t row_size = (width * bmp_channels + 3) & ~3; // Round up to multiple of 4
    uint32_t padding = row_size - width * bmp_channels;
    uint32_t image_size = row_size * height;
    uint32_t file_size = 54 + image_size;
    
    // Write BMP header
    uint8_t header[54] = {0};
    
    // File header
    header[0] = 'B';
    header[1] = 'M';
    *(uint32_t*)(header + 2) = file_size;
    *(uint32_t*)(header + 10) = 54; // Data offset
    
    // Info header
    *(uint32_t*)(header + 14) = 40;    // Header size
    *(int32_t*)(header + 18) = width;  // Width
    *(int32_t*)(header + 22) = height; // Height
    *(uint16_t*)(header + 26) = 1;     // Planes
    *(uint16_t*)(header + 28) = bmp_channels * 8; // Bits per pixel
    *(uint32_t*)(header + 34) = image_size; // Image size
    
    fwrite(header, 1, 54, file);
    
    // Write pixel data (bottom-up, BGR format)
    uint8_t* src = (uint8_t*)image->data;
    uint8_t padding_bytes[4] = {0};
    
    for (int y = height - 1; y >= 0; y--) {
        for (uint32_t x = 0; x < width; x++) {
            size_t src_idx = (y * width + x) * channels;
            
            if (channels == 1) {
                // Grayscale to BGR
                uint8_t gray = src[src_idx];
                fwrite(&gray, 1, 1, file); // B
                fwrite(&gray, 1, 1, file); // G  
                fwrite(&gray, 1, 1, file); // R
            } else if (channels >= 3) {
                // RGB(A) to BGR
                fwrite(&src[src_idx + 2], 1, 1, file); // B
                fwrite(&src[src_idx + 1], 1, 1, file); // G
                fwrite(&src[src_idx], 1, 1, file);     // R
                if (bmp_channels == 4) {
                    uint8_t alpha = (channels == 4) ? src[src_idx + 3] : 255;
                    fwrite(&alpha, 1, 1, file); // A
                }
            }
        }
        
        // Write padding
        if (padding > 0) {
            fwrite(padding_bytes, 1, padding, file);
        }
    }
    
    fclose(file);
    return true;
}

bool kryon_image_resize(KryonImage* image, uint32_t new_width, uint32_t new_height) {
    if (!image || !image->data || new_width == 0 || new_height == 0) return false;
    
    if (new_width == image->width && new_height == image->height) {
        return true; // No resize needed
    }
    
    size_t new_data_size = new_width * new_height * image->channels;
    uint8_t* new_data = kryon_alloc(new_data_size);
    if (!new_data) return false;
    
    uint8_t* src = (uint8_t*)image->data;
    
    // Simple nearest-neighbor scaling
    for (uint32_t y = 0; y < new_height; y++) {
        for (uint32_t x = 0; x < new_width; x++) {
            uint32_t src_x = (x * image->width) / new_width;
            uint32_t src_y = (y * image->height) / new_height;
            
            // Clamp to bounds
            if (src_x >= image->width) src_x = image->width - 1;
            if (src_y >= image->height) src_y = image->height - 1;
            
            size_t src_idx = (src_y * image->width + src_x) * image->channels;
            size_t dst_idx = (y * new_width + x) * image->channels;
            
            for (uint32_t c = 0; c < image->channels; c++) {
                new_data[dst_idx + c] = src[src_idx + c];
            }
        }
    }
    
    kryon_free(image->data);
    image->data = new_data;
    image->width = new_width;
    image->height = new_height;
    
    return true;
}

bool kryon_image_convert_format(KryonImage* image, KryonPixelFormat new_format) {
    if (!image || image->format == new_format) return true;
    
    // For now, just support conversion to RGBA
    if (new_format == KRYON_PIXEL_FORMAT_RGBA8) {
        convert_to_rgba(image);
        return true;
    }
    
    return false; // Other conversions not implemented
}

void kryon_image_fill(KryonImage* image, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!image || !image->data) return;
    
    uint8_t* data = (uint8_t*)image->data;
    
    for (size_t i = 0; i < image->width * image->height; i++) {
        size_t idx = i * image->channels;
        
        if (image->channels == 1) {
            // Convert to grayscale
            data[idx] = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
        } else if (image->channels >= 3) {
            data[idx] = r;
            data[idx + 1] = g;
            data[idx + 2] = b;
            if (image->channels == 4) {
                data[idx + 3] = a;
            }
        }
    }
}

void kryon_image_copy_region(const KryonImage* src, KryonImage* dst,
                            uint32_t src_x, uint32_t src_y,
                            uint32_t dst_x, uint32_t dst_y,
                            uint32_t width, uint32_t height) {
    if (!src || !dst || !src->data || !dst->data) return;
    if (src->channels != dst->channels) return;
    
    // Clamp dimensions
    if (src_x + width > src->width) width = src->width - src_x;
    if (src_y + height > src->height) height = src->height - src_y;
    if (dst_x + width > dst->width) width = dst->width - dst_x;
    if (dst_y + height > dst->height) height = dst->height - dst_y;
    
    uint8_t* src_data = (uint8_t*)src->data;
    uint8_t* dst_data = (uint8_t*)dst->data;
    
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            size_t src_idx = ((src_y + y) * src->width + (src_x + x)) * src->channels;
            size_t dst_idx = ((dst_y + y) * dst->width + (dst_x + x)) * dst->channels;
            
            for (uint32_t c = 0; c < src->channels; c++) {
                dst_data[dst_idx + c] = src_data[src_idx + c];
            }
        }
    }
}