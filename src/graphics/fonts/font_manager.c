/**
 * @file font_manager.c
 * @brief Kryon Font Manager Implementation (Thin Abstraction Layer)
 */

#include "internal/graphics.h"
#include "internal/memory.h"
#include "internal/renderer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Include renderer-specific headers
#ifdef KRYON_RENDERER_RAYLIB
    #include <raylib.h>
#endif

#ifdef KRYON_RENDERER_SDL2
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_ttf.h>
#endif

// =============================================================================
// FONT MANAGEMENT STRUCTURES
// =============================================================================

typedef struct KryonFontCache {
    char* file_path;
    int size;
    void* renderer_font; // Renderer-specific font object
    KryonFont kryon_font; // Our abstracted font
    struct KryonFontCache* next;
} KryonFontCache;

typedef struct {
    KryonFontCache* cached_fonts;
    KryonFont* default_font;
    size_t font_count;
} KryonFontManager;

static KryonFontManager g_font_manager = {0};

// =============================================================================
// RENDERER-SPECIFIC IMPLEMENTATIONS
// =============================================================================

#ifdef KRYON_RENDERER_RAYLIB
static KryonFont* raylib_load_font(const char* file_path, int size) {
    Font raylib_font = LoadFontEx(file_path, size, NULL, 0);
    if (raylib_font.texture.id == 0) return NULL;
    
    KryonFont* font = kryon_alloc(sizeof(KryonFont));
    if (!font) {
        UnloadFont(raylib_font);
        return NULL;
    }
    
    font->size = size;
    font->line_height = size;
    font->renderer_data = kryon_alloc(sizeof(Font));
    if (!font->renderer_data) {
        UnloadFont(raylib_font);
        kryon_free(font);
        return NULL;
    }
    
    memcpy(font->renderer_data, &raylib_font, sizeof(Font));
    return font;
}

static void raylib_unload_font(KryonFont* font) {
    if (!font || !font->renderer_data) return;
    
    Font* raylib_font = (Font*)font->renderer_data;
    UnloadFont(*raylib_font);
    kryon_free(font->renderer_data);
    kryon_free(font);
}

static KryonTextMetrics raylib_measure_text(const KryonFont* font, const char* text) {
    KryonTextMetrics metrics = {0};
    if (!font || !text || !font->renderer_data) return metrics;
    
    Font* raylib_font = (Font*)font->renderer_data;
    Vector2 size = MeasureTextEx(*raylib_font, text, font->size, 1.0f);
    
    metrics.width = size.x;
    metrics.height = size.y;
    metrics.advance_x = size.x;
    
    return metrics;
}
#endif

#ifdef KRYON_RENDERER_SDL2
static KryonFont* sdl2_load_font(const char* file_path, int size) {
    TTF_Font* ttf_font = TTF_OpenFont(file_path, size);
    if (!ttf_font) return NULL;
    
    KryonFont* font = kryon_alloc(sizeof(KryonFont));
    if (!font) {
        TTF_CloseFont(ttf_font);
        return NULL;
    }
    
    font->size = size;
    font->line_height = TTF_FontHeight(ttf_font);
    font->renderer_data = ttf_font;
    
    return font;
}

static void sdl2_unload_font(KryonFont* font) {
    if (!font || !font->renderer_data) return;
    
    TTF_Font* ttf_font = (TTF_Font*)font->renderer_data;
    TTF_CloseFont(ttf_font);
    kryon_free(font);
}

static KryonTextMetrics sdl2_measure_text(const KryonFont* font, const char* text) {
    KryonTextMetrics metrics = {0};
    if (!font || !text || !font->renderer_data) return metrics;
    
    TTF_Font* ttf_font = (TTF_Font*)font->renderer_data;
    
    int width, height;
    if (TTF_SizeText(ttf_font, text, &width, &height) == 0) {
        metrics.width = width;
        metrics.height = height;
        metrics.advance_x = width;
    }
    
    return metrics;
}
#endif

// Software font implementation (bitmap fonts)
static KryonFont* software_create_default_font(void) {
    KryonFont* font = kryon_alloc(sizeof(KryonFont));
    if (!font) return NULL;
    
    font->size = 16;
    font->line_height = 16;
    font->renderer_data = NULL; // No specific data for software renderer
    
    return font;
}

static KryonFont* software_load_font(const char* file_path, int size) {
    (void)file_path; // Software renderer doesn't support custom fonts yet
    (void)size;
    
    return software_create_default_font();
}

static void software_unload_font(KryonFont* font) {
    if (!font) return;
    kryon_free(font);
}

static KryonTextMetrics software_measure_text(const KryonFont* font, const char* text) {
    KryonTextMetrics metrics = {0};
    if (!font || !text) return metrics;
    
    // Simple monospace calculation for software renderer
    size_t text_length = strlen(text);
    int char_width = font->size * 0.6f; // Approximate character width
    
    metrics.width = text_length * char_width;
    metrics.height = font->line_height;
    metrics.advance_x = metrics.width;
    
    return metrics;
}

// =============================================================================
// FONT CACHE MANAGEMENT
// =============================================================================

static KryonFontCache* find_cached_font(const char* file_path, int size) {
    KryonFontCache* cache = g_font_manager.cached_fonts;
    
    while (cache) {
        if (cache->size == size && 
            ((file_path == NULL && cache->file_path == NULL) ||
             (file_path && cache->file_path && strcmp(cache->file_path, file_path) == 0))) {
            return cache;
        }
        cache = cache->next;
    }
    
    return NULL;
}

static void add_to_cache(const char* file_path, int size, KryonFont* font, void* renderer_font) {
    KryonFontCache* cache = kryon_alloc(sizeof(KryonFontCache));
    if (!cache) return;
    
    memset(cache, 0, sizeof(KryonFontCache));
    
    if (file_path) {
        cache->file_path = kryon_alloc(strlen(file_path) + 1);
        if (cache->file_path) {
            strcpy(cache->file_path, file_path);
        }
    }
    
    cache->size = size;
    cache->renderer_font = renderer_font;
    cache->kryon_font = *font;
    cache->next = g_font_manager.cached_fonts;
    
    g_font_manager.cached_fonts = cache;
    g_font_manager.font_count++;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_font_manager_init(void) {
    memset(&g_font_manager, 0, sizeof(g_font_manager));
    
    // Initialize renderer-specific font systems
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            if (TTF_Init() == -1) {
                return false;
            }
            break;
#endif
        default:
            break;
    }
    
    // Create default font
    g_font_manager.default_font = kryon_font_load(NULL, 16);
    
    return g_font_manager.default_font != NULL;
}

void kryon_font_manager_shutdown(void) {
    // Clean up cached fonts
    KryonFontCache* cache = g_font_manager.cached_fonts;
    while (cache) {
        KryonFontCache* next = cache->next;
        
        // Unload renderer-specific font
        KryonRendererType renderer = kryon_renderer_get_type();
        switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
            case KRYON_RENDERER_RAYLIB:
                if (cache->renderer_font) {
                    Font* raylib_font = (Font*)cache->renderer_font;
                    UnloadFont(*raylib_font);
                    kryon_free(cache->renderer_font);
                }
                break;
#endif
                
#ifdef KRYON_RENDERER_SDL2
            case KRYON_RENDERER_SDL2:
                if (cache->renderer_font) {
                    TTF_CloseFont((TTF_Font*)cache->renderer_font);
                }
                break;
#endif
                
            default:
                break;
        }
        
        kryon_free(cache->file_path);
        kryon_free(cache);
        cache = next;
    }
    
    // Shutdown renderer-specific font systems
    KryonRendererType renderer = kryon_renderer_get_type();
    switch (renderer) {
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            TTF_Quit();
            break;
#endif
        default:
            break;
    }
    
    memset(&g_font_manager, 0, sizeof(g_font_manager));
}

KryonFont* kryon_font_load(const char* file_path, int size) {
    if (size <= 0) size = 16;
    
    // Check cache first
    KryonFontCache* cached = find_cached_font(file_path, size);
    if (cached) {
        // Return a copy of the cached font
        KryonFont* font = kryon_alloc(sizeof(KryonFont));
        if (font) {
            *font = cached->kryon_font;
        }
        return font;
    }
    
    // Load new font based on renderer
    KryonRendererType renderer = kryon_renderer_get_type();
    KryonFont* font = NULL;
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            if (file_path) {
                font = raylib_load_font(file_path, size);
            } else {
                // Use Raylib's default font
                font = kryon_alloc(sizeof(KryonFont));
                if (font) {
                    font->size = size;
                    font->line_height = size;
                    font->renderer_data = NULL; // Use default font
                }
            }
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            if (file_path) {
                font = sdl2_load_font(file_path, size);
            } else {
                // SDL2 doesn't have a built-in font, fall back to software
                font = software_create_default_font();
            }
            break;
#endif
            
        case KRYON_RENDERER_SOFTWARE:
        case KRYON_RENDERER_HTML:
        default:
            font = software_load_font(file_path, size);
            break;
    }
    
    if (font) {
        add_to_cache(file_path, size, font, font->renderer_data);
    }
    
    return font;
}

void kryon_font_unload(KryonFont* font) {
    if (!font) return;
    
    // Note: We don't actually unload from cache here to allow reuse
    // The cache is cleaned up during shutdown
    // Just free the font wrapper
    kryon_free(font);
}

KryonTextMetrics kryon_font_measure_text(const KryonFont* font, const char* text) {
    KryonTextMetrics metrics = {0};
    if (!font || !text) return metrics;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            return raylib_measure_text(font, text);
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            return sdl2_measure_text(font, text);
#endif
            
        case KRYON_RENDERER_SOFTWARE:
        case KRYON_RENDERER_HTML:
        default:
            return software_measure_text(font, text);
    }
}

KryonFont* kryon_font_get_default(void) {
    return g_font_manager.default_font;
}

size_t kryon_font_get_loaded_count(void) {
    return g_font_manager.font_count;
}

bool kryon_font_is_loaded(const char* file_path, int size) {
    return find_cached_font(file_path, size) != NULL;
}

void kryon_font_clear_cache(void) {
    // This could be implemented to free unused fonts from cache
    // For now, cache persists until shutdown
}