// ============================================================================
// Desktop Renderer - Font Management Module
// ============================================================================
// Handles font registration, caching, resolution, and text texture caching
// with LRU eviction for performance optimization.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "desktop_internal.h"

#ifdef ENABLE_SDL3

// ============================================================================
// FONT REGISTRATION AND RESOLUTION
// ============================================================================

void desktop_ir_register_font_internal(const char* name, const char* path) {
    if (!name || !path || g_font_registry_count >= (int)(sizeof(g_font_registry) / sizeof(g_font_registry[0]))) return;

    // Check if font already registered - update path if so
    for (int i = 0; i < g_font_registry_count; i++) {
        if (strcmp(g_font_registry[i].name, name) == 0) {
            strncpy(g_font_registry[i].path, path, sizeof(g_font_registry[i].path) - 1);
            g_font_registry[i].path[sizeof(g_font_registry[i].path) - 1] = '\0';
            return;
        }
    }

    // Add new font to registry
    strncpy(g_font_registry[g_font_registry_count].name, name, sizeof(g_font_registry[g_font_registry_count].name) - 1);
    strncpy(g_font_registry[g_font_registry_count].path, path, sizeof(g_font_registry[g_font_registry_count].path) - 1);
    g_font_registry[g_font_registry_count].name[sizeof(g_font_registry[g_font_registry_count].name) - 1] = '\0';
    g_font_registry[g_font_registry_count].path[sizeof(g_font_registry[g_font_registry_count].path) - 1] = '\0';
    g_font_registry_count++;

    // Set as default if no default exists
    if (g_default_font_path[0] == '\0') {
        strncpy(g_default_font_name, name, sizeof(g_default_font_name) - 1);
        strncpy(g_default_font_path, path, sizeof(g_default_font_path) - 1);
    }
}

const char* desktop_ir_find_font_path(const char* name_or_path) {
    if (!name_or_path) return NULL;

    // If it's an absolute/relative path that exists, use it
    if (access(name_or_path, R_OK) == 0) {
        return name_or_path;
    }

    // Otherwise look up by registered name
    for (int i = 0; i < g_font_registry_count; i++) {
        if (strcmp(g_font_registry[i].name, name_or_path) == 0) {
            return g_font_registry[i].path;
        }
    }

    return NULL;
}

TTF_Font* desktop_ir_get_cached_font(const char* path, int size) {
    if (!path) return NULL;

    // Check cache for existing font
    for (int i = 0; i < g_font_cache_count; i++) {
        if (g_font_cache[i].size == size && strcmp(g_font_cache[i].path, path) == 0) {
            return g_font_cache[i].font;
        }
    }

    // Cache full - cannot load more fonts
    if (g_font_cache_count >= (int)(sizeof(g_font_cache) / sizeof(g_font_cache[0]))) {
        return NULL;
    }

    // Load and cache new font
    TTF_Font* font = TTF_OpenFont(path, size);
    if (!font) return NULL;

    strncpy(g_font_cache[g_font_cache_count].path, path, sizeof(g_font_cache[g_font_cache_count].path) - 1);
    g_font_cache[g_font_cache_count].path[sizeof(g_font_cache[g_font_cache_count].path) - 1] = '\0';
    g_font_cache[g_font_cache_count].size = size;
    g_font_cache[g_font_cache_count].font = font;
    g_font_cache_count++;

    return font;
}

TTF_Font* desktop_ir_resolve_font(DesktopIRRenderer* renderer, IRComponent* component, float fallback_size) {
    float size = fallback_size > 0 ? fallback_size : 16.0f;
    const char* family = NULL;

    if (component && component->style) {
        if (component->style->font.size > 0) size = component->style->font.size;
        family = component->style->font.family;
    }

    const char* path = NULL;
    if (family) {
        path = desktop_ir_find_font_path(family);
    }
    if (!path && g_default_font_path[0] != '\0') {
        path = g_default_font_path;
    }

    if (path) {
        TTF_Font* f = desktop_ir_get_cached_font(path, (int)size);
        if (f) return f;
    }

    // Fallback to renderer default
    if (renderer && renderer->default_font) {
        return renderer->default_font;
    }

    return NULL;
}

// ============================================================================
// TEXT MEASUREMENT
// ============================================================================

float measure_text_width(TTF_Font* font, const char* text) {
    if (!font || !text || text[0] == '\0') return 0.0f;

    SDL_Color dummy = {255, 255, 255, 255};
    SDL_Surface* s = TTF_RenderText_Blended(font, text, strlen(text), dummy);
    if (!s) return 0.0f;

    float w = (float)s->w;
    SDL_DestroySurface(s);
    return w;
}

// ============================================================================
// TEXT TEXTURE CACHE (LRU with 40-60% speedup)
// ============================================================================

uint32_t pack_color(SDL_Color color) {
    return ((uint32_t)color.r << 24) | ((uint32_t)color.g << 16) | ((uint32_t)color.b << 8) | (uint32_t)color.a;
}

bool get_font_info(TTF_Font* font, const char** out_path, int* out_size) {
    if (!font || !out_path || !out_size) return false;

    // Look up font in cache to get path and size
    for (int i = 0; i < g_font_cache_count; i++) {
        if (g_font_cache[i].font == font) {
            *out_path = g_font_cache[i].path;
            *out_size = g_font_cache[i].size;
            return true;
        }
    }

    return false;
}

TextTextureCache* text_texture_cache_lookup(const char* font_path, int font_size, const char* text, size_t text_len, SDL_Color color) {
    if (!font_path || !text || text_len == 0 || text_len > 255) return NULL;

    uint32_t packed_color = pack_color(color);

    // Linear search through cache (simple but effective for 128 entries)
    for (int i = 0; i < TEXT_TEXTURE_CACHE_SIZE; i++) {
        if (!g_text_texture_cache[i].valid) continue;

        if (g_text_texture_cache[i].font_size == font_size &&
            g_text_texture_cache[i].color == packed_color &&
            strcmp(g_text_texture_cache[i].font_path, font_path) == 0 &&
            strncmp(g_text_texture_cache[i].text, text, text_len) == 0 &&
            g_text_texture_cache[i].text[text_len] == '\0') {

            // Update LRU timestamp
            g_text_texture_cache[i].last_access = g_frame_counter;
            return &g_text_texture_cache[i];
        }
    }

    return NULL;  // Cache miss
}

void text_texture_cache_insert(const char* font_path, int font_size, const char* text, size_t text_len,
                                SDL_Color color, SDL_Texture* texture, int width, int height) {
    if (!font_path || !text || !texture || text_len == 0 || text_len > 255) return;

    // Find LRU entry (oldest last_access, or first invalid entry)
    int lru_index = 0;
    uint64_t oldest_access = g_text_texture_cache[0].valid ? g_text_texture_cache[0].last_access : 0;

    for (int i = 0; i < TEXT_TEXTURE_CACHE_SIZE; i++) {
        if (!g_text_texture_cache[i].valid) {
            lru_index = i;
            break;
        }
        if (g_text_texture_cache[i].last_access < oldest_access) {
            oldest_access = g_text_texture_cache[i].last_access;
            lru_index = i;
        }
    }

    // Evict old texture if needed
    if (g_text_texture_cache[lru_index].valid && g_text_texture_cache[lru_index].texture) {
        SDL_DestroyTexture(g_text_texture_cache[lru_index].texture);
    }

    // Insert new entry
    strncpy(g_text_texture_cache[lru_index].font_path, font_path, sizeof(g_text_texture_cache[lru_index].font_path) - 1);
    g_text_texture_cache[lru_index].font_path[sizeof(g_text_texture_cache[lru_index].font_path) - 1] = '\0';

    strncpy(g_text_texture_cache[lru_index].text, text, text_len);
    g_text_texture_cache[lru_index].text[text_len] = '\0';

    g_text_texture_cache[lru_index].font_size = font_size;
    g_text_texture_cache[lru_index].color = pack_color(color);
    g_text_texture_cache[lru_index].texture = texture;
    g_text_texture_cache[lru_index].width = width;
    g_text_texture_cache[lru_index].height = height;
    g_text_texture_cache[lru_index].last_access = g_frame_counter;
    g_text_texture_cache[lru_index].valid = true;
}

SDL_Texture* get_text_texture_cached(SDL_Renderer* sdl_renderer, TTF_Font* font,
                                     const char* text, SDL_Color color,
                                     int* out_width, int* out_height) {
    size_t text_len = text ? strlen(text) : 0;
    if (!sdl_renderer || !font || !text || text_len == 0 || text_len > 255) return NULL;

    // Get font info for cache key
    const char* font_path;
    int font_size;
    if (!get_font_info(font, &font_path, &font_size)) {
        // Font not in cache - render without caching
        SDL_Surface* surface = TTF_RenderText_Blended(font, text, text_len, color);
        if (!surface) return NULL;

        if (out_width) *out_width = surface->w;
        if (out_height) *out_height = surface->h;

        SDL_Texture* texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
        SDL_DestroySurface(surface);
        return texture;
    }

    // Check cache
    TextTextureCache* cached = text_texture_cache_lookup(font_path, font_size, text, text_len, color);
    if (cached) {
        // Cache hit! Return cached texture
        if (out_width) *out_width = cached->width;
        if (out_height) *out_height = cached->height;

        // Caller must NOT destroy this texture - it's managed by the cache
        return cached->texture;
    }

    // Cache miss - render new texture
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, text_len, color);
    if (!surface) return NULL;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
    if (out_width) *out_width = surface->w;
    if (out_height) *out_height = surface->h;

    if (texture) {
        // Insert into cache (cache takes ownership)
        text_texture_cache_insert(font_path, font_size, text, text_len, color,
                                 texture, surface->w, surface->h);
    }

    SDL_DestroySurface(surface);

    // Return texture (now owned by cache, caller should NOT destroy)
    return texture;
}

#endif // ENABLE_SDL3
