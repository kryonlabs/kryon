/**
 * @file texture_manager.c
 * @brief Kryon Texture Manager Implementation (Thin Abstraction Layer)
 */

#include "internal/graphics.h"
#include "internal/memory.h"
#include "internal/renderer_interface.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Include renderer-specific headers
#ifdef KRYON_RENDERER_RAYLIB
    #include <raylib.h>
#endif

#ifdef KRYON_RENDERER_SDL2
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#endif

// =============================================================================
// TEXTURE MANAGEMENT STRUCTURES
// =============================================================================

typedef struct KryonTextureCache {
    uint32_t id;
    char* source_path;
    void* renderer_texture; // Renderer-specific texture object
    KryonTexture kryon_texture; // Our abstracted texture
    uint32_t ref_count;
    struct KryonTextureCache* next;
} KryonTextureCache;

typedef struct {
    KryonTextureCache* cached_textures;
    uint32_t next_texture_id;
    size_t texture_count;
    size_t memory_usage; // Total texture memory in bytes
    
    // Default textures
    uint32_t white_texture_id;
    uint32_t black_texture_id;
    
    // Configuration
    size_t max_texture_size;
    bool auto_generate_mipmaps;
    
} KryonTextureManager;

static KryonTextureManager g_texture_manager = {0};

// =============================================================================
// RENDERER-SPECIFIC IMPLEMENTATIONS
// =============================================================================

#ifdef KRYON_RENDERER_RAYLIB
static KryonTexture* raylib_create_texture_from_image(const KryonImage* image) {
    if (!image || !image->data) return NULL;
    
    // Convert our image to Raylib image
    Image raylib_image = {0};
    raylib_image.data = image->data;
    raylib_image.width = image->width;
    raylib_image.height = image->height;
    raylib_image.mipmaps = 1;
    raylib_image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    
    // Create texture
    Texture2D raylib_texture = LoadTextureFromImage(raylib_image);
    if (raylib_texture.id == 0) return NULL;
    
    KryonTexture* texture = kryon_alloc(sizeof(KryonTexture));
    if (!texture) {
        UnloadTexture(raylib_texture);
        return NULL;
    }
    
    texture->width = raylib_texture.width;
    texture->height = raylib_texture.height;
    texture->format = KRYON_TEXTURE_FORMAT_RGBA8;
    texture->has_mipmaps = raylib_texture.mipmaps > 1;
    texture->renderer_data = kryon_alloc(sizeof(Texture2D));
    
    if (!texture->renderer_data) {
        UnloadTexture(raylib_texture);
        kryon_free(texture);
        return NULL;
    }
    
    memcpy(texture->renderer_data, &raylib_texture, sizeof(Texture2D));
    return texture;
}

static void raylib_destroy_texture(KryonTexture* texture) {
    if (!texture || !texture->renderer_data) return;
    
    Texture2D* raylib_texture = (Texture2D*)texture->renderer_data;
    UnloadTexture(*raylib_texture);
    kryon_free(texture->renderer_data);
}

static bool raylib_update_texture(KryonTexture* texture, const void* pixels, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!texture || !texture->renderer_data || !pixels) return false;
    
    Texture2D* raylib_texture = (Texture2D*)texture->renderer_data;
    
    // Create a temporary image for the update region
    Image update_image = {0};
    update_image.data = (void*)pixels;
    update_image.width = width;
    update_image.height = height;
    update_image.mipmaps = 1;
    update_image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    
    // Update texture region (Raylib doesn't have direct region update, so we use a workaround)
    if (x == 0 && y == 0 && width == texture->width && height == texture->height) {
        UpdateTexture(*raylib_texture, pixels);
    } else {
        // For partial updates, we'd need to implement a more complex solution
        // This is a simplified version
        UpdateTexture(*raylib_texture, pixels);
    }
    
    return true;
}
#endif

#ifdef KRYON_RENDERER_SDL2
static KryonTexture* sdl2_create_texture_from_image(const KryonImage* image) {
    if (!image || !image->data) return NULL;
    
    // Get current renderer (this would be stored in renderer context)
    SDL_Renderer* renderer = kryon_renderer_get_sdl_renderer();
    if (!renderer) return NULL;
    
    // Create SDL surface from image data
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        image->data, image->width, image->height, 32, image->width * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
    );
    
    if (!surface) return NULL;
    
    // Create texture from surface
    SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    if (!sdl_texture) return NULL;
    
    KryonTexture* texture = kryon_alloc(sizeof(KryonTexture));
    if (!texture) {
        SDL_DestroyTexture(sdl_texture);
        return NULL;
    }
    
    texture->width = image->width;
    texture->height = image->height;
    texture->format = KRYON_TEXTURE_FORMAT_RGBA8;
    texture->has_mipmaps = false; // SDL2 doesn't auto-generate mipmaps
    texture->renderer_data = sdl_texture;
    
    return texture;
}

static void sdl2_destroy_texture(KryonTexture* texture) {
    if (!texture || !texture->renderer_data) return;
    
    SDL_Texture* sdl_texture = (SDL_Texture*)texture->renderer_data;
    SDL_DestroyTexture(sdl_texture);
}

static bool sdl2_update_texture(KryonTexture* texture, const void* pixels, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!texture || !texture->renderer_data || !pixels) return false;
    
    SDL_Texture* sdl_texture = (SDL_Texture*)texture->renderer_data;
    
    SDL_Rect rect = {x, y, width, height};
    return SDL_UpdateTexture(sdl_texture, &rect, pixels, width * 4) == 0;
}
#endif


// =============================================================================
// TEXTURE CACHE MANAGEMENT
// =============================================================================

static KryonTextureCache* find_cached_texture(const char* source_path) {
    if (!source_path) return NULL;
    
    KryonTextureCache* cache = g_texture_manager.cached_textures;
    while (cache) {
        if (cache->source_path && strcmp(cache->source_path, source_path) == 0) {
            return cache;
        }
        cache = cache->next;
    }
    
    return NULL;
}

static KryonTextureCache* find_cached_texture_by_id(uint32_t texture_id) {
    KryonTextureCache* cache = g_texture_manager.cached_textures;
    while (cache) {
        if (cache->id == texture_id) {
            return cache;
        }
        cache = cache->next;
    }
    
    return NULL;
}

static uint32_t add_to_cache(const char* source_path, KryonTexture* texture, void* renderer_texture) {
    KryonTextureCache* cache = kryon_alloc(sizeof(KryonTextureCache));
    if (!cache) return 0;
    
    memset(cache, 0, sizeof(KryonTextureCache));
    
    cache->id = ++g_texture_manager.next_texture_id;
    
    if (source_path) {
        cache->source_path = kryon_alloc(strlen(source_path) + 1);
        if (cache->source_path) {
            strcpy(cache->source_path, source_path);
        }
    }
    
    cache->renderer_texture = renderer_texture;
    cache->kryon_texture = *texture;
    cache->ref_count = 1;
    cache->next = g_texture_manager.cached_textures;
    
    g_texture_manager.cached_textures = cache;
    g_texture_manager.texture_count++;
    
    // Update memory usage
    size_t texture_memory = texture->width * texture->height * 4; // Assume RGBA
    g_texture_manager.memory_usage += texture_memory;
    
    return cache->id;
}

static void remove_from_cache(uint32_t texture_id) {
    KryonTextureCache* cache = g_texture_manager.cached_textures;
    KryonTextureCache* prev = NULL;
    
    while (cache) {
        if (cache->id == texture_id) {
            if (prev) {
                prev->next = cache->next;
            } else {
                g_texture_manager.cached_textures = cache->next;
            }
            
            // Update memory usage
            size_t texture_memory = cache->kryon_texture.width * cache->kryon_texture.height * 4;
            g_texture_manager.memory_usage -= texture_memory;
            
            // Clean up
            kryon_free(cache->source_path);
            kryon_free(cache);
            g_texture_manager.texture_count--;
            return;
        }
        prev = cache;
        cache = cache->next;
    }
}

// =============================================================================
// DEFAULT TEXTURE CREATION
// =============================================================================

static uint32_t create_default_texture(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Create 1x1 texture with specified color
    KryonImage* image = kryon_image_create(1, 1, KRYON_PIXEL_FORMAT_RGBA8);
    if (!image) return 0;
    
    uint8_t* pixel = (uint8_t*)image->data;
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
    pixel[3] = a;
    
    uint32_t texture_id = kryon_texture_create_from_image(image);
    kryon_image_destroy(image);
    
    return texture_id;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_texture_manager_init(void) {
    memset(&g_texture_manager, 0, sizeof(g_texture_manager));
    
    g_texture_manager.next_texture_id = 1;
    g_texture_manager.max_texture_size = 4096; // 4K max texture size
    g_texture_manager.auto_generate_mipmaps = true;
    
    // Create default textures
    g_texture_manager.white_texture_id = create_default_texture(255, 255, 255, 255);
    g_texture_manager.black_texture_id = create_default_texture(0, 0, 0, 255);
    
    return g_texture_manager.white_texture_id && g_texture_manager.black_texture_id;
}

void kryon_texture_manager_shutdown(void) {
    // Clean up all cached textures
    KryonTextureCache* cache = g_texture_manager.cached_textures;
    while (cache) {
        KryonTextureCache* next = cache->next;
        
        // Destroy renderer-specific texture
        KryonRendererType renderer = kryon_renderer_get_type();
        switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
            case KRYON_RENDERER_RAYLIB:
                raylib_destroy_texture(&cache->kryon_texture);
                break;
#endif
                
#ifdef KRYON_RENDERER_SDL2
            case KRYON_RENDERER_SDL2:
                sdl2_destroy_texture(&cache->kryon_texture);
                break;
#endif
                
            case KRYON_RENDERER_HTML:
            default:
                // HTML renderer cleanup - nothing to do for now
                break;
        }
        
        kryon_free(cache->source_path);
        kryon_free(cache);
        cache = next;
    }
    
    // Clean up default textures
    if (g_texture_manager.white_texture_id) {
        kryon_texture_destroy(g_texture_manager.white_texture_id);
    }
    if (g_texture_manager.black_texture_id) {
        kryon_texture_destroy(g_texture_manager.black_texture_id);
    }
    
    memset(&g_texture_manager, 0, sizeof(g_texture_manager));
}

uint32_t kryon_texture_load_from_file(const char* file_path) {
    if (!file_path) return 0;
    
    // Check cache first
    KryonTextureCache* cached = find_cached_texture(file_path);
    if (cached) {
        cached->ref_count++;
        return cached->id;
    }
    
    // Load image first
    KryonImage* image = kryon_image_load_from_file(file_path);
    if (!image) return 0;
    
    // Create texture from image
    uint32_t texture_id = kryon_texture_create_from_image(image);
    kryon_image_destroy(image);
    
    if (!texture_id) return 0;
    
    return texture_id;
}

uint32_t kryon_texture_create_from_image(const KryonImage* image) {
    if (!image) return 0;
    
    // Validate texture size
    if (image->width > g_texture_manager.max_texture_size || 
        image->height > g_texture_manager.max_texture_size) {
        return 0;
    }
    
    KryonRendererType renderer = kryon_renderer_get_type();
    KryonTexture* texture = NULL;
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            texture = raylib_create_texture_from_image(image);
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            texture = sdl2_create_texture_from_image(image);
            break;
#endif
            
        case KRYON_RENDERER_HTML:
        default:
            // For HTML renderer, we'll handle textures differently
            texture = NULL; // TODO: Implement HTML texture creation
            break;
    }
    
    if (!texture) return 0;
    
    // Add to cache and return ID
    uint32_t texture_id = add_to_cache(NULL, texture, texture->renderer_data);
    kryon_free(texture); // Cache holds the actual data
    
    return texture_id;
}

uint32_t kryon_texture_create(uint32_t width, uint32_t height, KryonTextureFormat format) {
    if (width == 0 || height == 0) return 0;
    
    // Create blank image
    KryonPixelFormat pixel_format = (format == KRYON_TEXTURE_FORMAT_RGB8) ? 
        KRYON_PIXEL_FORMAT_RGB8 : KRYON_PIXEL_FORMAT_RGBA8;
    
    KryonImage* image = kryon_image_create(width, height, pixel_format);
    if (!image) return 0;
    
    uint32_t texture_id = kryon_texture_create_from_image(image);
    kryon_image_destroy(image);
    
    return texture_id;
}

void kryon_texture_destroy(uint32_t texture_id) {
    if (texture_id == 0) return;
    
    KryonTextureCache* cache = find_cached_texture_by_id(texture_id);
    if (!cache) return;
    
    cache->ref_count--;
    if (cache->ref_count <= 0) {
        // Destroy renderer-specific texture
        KryonRendererType renderer = kryon_renderer_get_type();
        switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
            case KRYON_RENDERER_RAYLIB:
                raylib_destroy_texture(&cache->kryon_texture);
                break;
#endif
                
#ifdef KRYON_RENDERER_SDL2
            case KRYON_RENDERER_SDL2:
                sdl2_destroy_texture(&cache->kryon_texture);
                break;
#endif
                
            case KRYON_RENDERER_HTML:
            default:
                // HTML renderer cleanup - nothing to do for now
                break;
        }
        
        remove_from_cache(texture_id);
    }
}

const KryonTexture* kryon_texture_get(uint32_t texture_id) {
    if (texture_id == 0) return NULL;
    
    KryonTextureCache* cache = find_cached_texture_by_id(texture_id);
    return cache ? &cache->kryon_texture : NULL;
}

bool kryon_texture_update(uint32_t texture_id, const void* pixels, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (texture_id == 0 || !pixels) return false;
    
    KryonTextureCache* cache = find_cached_texture_by_id(texture_id);
    if (!cache) return false;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            return raylib_update_texture(&cache->kryon_texture, pixels, x, y, width, height);
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            return sdl2_update_texture(&cache->kryon_texture, pixels, x, y, width, height);
#endif
            
        case KRYON_RENDERER_HTML:
        default:
            // HTML renderer update - not implemented yet
            return false;
    }
}

uint32_t kryon_texture_get_white(void) {
    return g_texture_manager.white_texture_id;
}

uint32_t kryon_texture_get_black(void) {
    return g_texture_manager.black_texture_id;
}

size_t kryon_texture_get_memory_usage(void) {
    return g_texture_manager.memory_usage;
}

size_t kryon_texture_get_count(void) {
    return g_texture_manager.texture_count;
}

void kryon_texture_set_max_size(size_t max_size) {
    g_texture_manager.max_texture_size = max_size;
}

size_t kryon_texture_get_max_size(void) {
    return g_texture_manager.max_texture_size;
}