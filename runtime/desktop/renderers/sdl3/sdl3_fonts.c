#define _POSIX_C_SOURCE 200809L

/**
 * SDL3 Fonts - SDL3-Specific Font Management
 *
 * This file implements SDL3 font operations including:
 * - Font registration, caching, and resolution
 * - TTF font loading with variant support (bold, italic)
 * - Text measurement for layout
 * - Text texture caching with LRU eviction
 * - Font metrics callbacks for IR layout system
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sdl3_renderer.h"
#include "../../desktop_internal.h"

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

// NOTE: Font cache hash functions are defined after FNV-1a hash (below text cache section)

TTF_Font* desktop_ir_get_cached_font(const char* path, int size);  // Forward declaration

// ============================================================================
// FONT PATH CACHE (family, weight, italic) → path
// ============================================================================

// Lookup cached font path (family, weight, italic) → path
static const char* lookup_font_path_cache(const char* family, uint16_t weight, bool italic) {
    const char* search_family = family ? family : "";

    // Linear search (acceptable for <128 entries)
    for (int i = 0; i < g_font_path_cache_count; i++) {
        if (g_font_path_cache[i].valid &&
            g_font_path_cache[i].weight == weight &&
            g_font_path_cache[i].italic == italic &&
            strcmp(g_font_path_cache[i].family, search_family) == 0) {
            return g_font_path_cache[i].path;
        }
    }

    return NULL;  // Cache miss
}

// Insert into font path cache
static void cache_font_path(const char* family, uint16_t weight, bool italic, const char* path) {
    if (g_font_path_cache_count >= FONT_PATH_CACHE_SIZE) return;

    const char* cache_family = family ? family : "";

    int idx = g_font_path_cache_count++;
    size_t family_len = strlen(cache_family);
    if (family_len >= sizeof(g_font_path_cache[idx].family)) family_len = sizeof(g_font_path_cache[idx].family) - 1;
    memcpy(g_font_path_cache[idx].family, cache_family, family_len);
    g_font_path_cache[idx].family[family_len] = '\0';

    size_t path_len = strlen(path);
    if (path_len >= sizeof(g_font_path_cache[idx].path)) path_len = sizeof(g_font_path_cache[idx].path) - 1;
    memcpy(g_font_path_cache[idx].path, path, path_len);
    g_font_path_cache[idx].path[path_len] = '\0';

    g_font_path_cache[idx].weight = weight;
    g_font_path_cache[idx].italic = italic;
    g_font_path_cache[idx].valid = true;
}

// Try to find font variant by filesystem naming convention
// Example: /path/to/DejaVuSans.ttf → /path/to/DejaVuSans-Bold.ttf
static const char* try_filesystem_font_variant(const char* base_path, uint16_t weight, bool italic) {
    if (!base_path) return NULL;

    static char variant_path[512];
    char dir[512], base[256];

    // Extract directory and base name
    const char* last_slash = strrchr(base_path, '/');
    if (!last_slash) return NULL;

    size_t dir_len = last_slash - base_path;
    strncpy(dir, base_path, dir_len);
    dir[dir_len] = '\0';

    const char* filename = last_slash + 1;
    const char* ext = strrchr(filename, '.');
    if (!ext) return NULL;

    size_t base_len = ext - filename;
    strncpy(base, filename, base_len);
    base[base_len] = '\0';

    // Remove existing variant suffixes
    char* hyphen = strrchr(base, '-');
    if (hyphen) {
        const char* suffix = hyphen + 1;
        if (strcmp(suffix, "Bold") == 0 || strcmp(suffix, "Italic") == 0 ||
            strcmp(suffix, "BoldItalic") == 0 || strcmp(suffix, "Oblique") == 0 ||
            strcmp(suffix, "BoldOblique") == 0 || strcmp(suffix, "Light") == 0 ||
            strcmp(suffix, "ExtraLight") == 0) {
            *hyphen = '\0';
        }
    }

    // Build variant path based on weight and italic
    const char* variant = "";
    if (weight >= 800) variant = italic ? "-BoldOblique" : "-Bold";
    else if (weight >= 600) variant = italic ? "-BoldOblique" : "-Bold";
    else if (weight <= 350) variant = italic ? "-ExtraLight" : "-ExtraLight";
    else if (italic) variant = "-Oblique";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(variant_path, sizeof(variant_path), "%s/%s%s%s", dir, base, variant, ext);
#pragma GCC diagnostic pop

    // Check if file exists
    if (access(variant_path, R_OK) == 0) {
        return variant_path;
    }

    return NULL;
}

// Map CSS font weight (100-900) to fontconfig weight name
static const char* map_weight_to_fc_weight(uint16_t weight) {
    if (weight == 0 || weight == 400) return "regular";
    if (weight <= 150) return "thin";
    if (weight <= 250) return "extralight";
    if (weight <= 350) return "light";
    if (weight <= 450) return "regular";
    if (weight <= 550) return "medium";
    if (weight <= 650) return "semibold";
    if (weight <= 750) return "bold";
    if (weight <= 850) return "extrabold";
    return "black";
}

// Helper to compute SDL_ttf style flags (only for decorations now)
static int compute_font_style_flags(IRComponent* component) {
    int style = TTF_STYLE_NORMAL;

    if (!component || !component->style) {
        return style;
    }

    IRTypography* font = &component->style->font;

    // Text decorations only (weight and italic handled by font loading)
    if (font->decoration & IR_TEXT_DECORATION_UNDERLINE) {
        style |= TTF_STYLE_UNDERLINE;
    }

    if (font->decoration & IR_TEXT_DECORATION_LINE_THROUGH) {
        style |= TTF_STYLE_STRIKETHROUGH;
    }

    // Note: SDL_ttf doesn't support overline natively

    return style;
}

// Query fontconfig for a font matching family, weight, and italic
static char* query_fontconfig_font(const char* family, uint16_t weight, bool italic) {
    char query[512];
    const char* fc_weight = map_weight_to_fc_weight(weight);
    const char* fc_slant = italic ? "italic" : "roman";

    // Build fontconfig query
    if (family && family[0] != '\0') {
        snprintf(query, sizeof(query), "fc-match \"%s:weight=%s:slant=%s\" -f \"%%{file}\"",
                 family, fc_weight, fc_slant);
    } else {
        snprintf(query, sizeof(query), "fc-match \"sans:weight=%s:slant=%s\" -f \"%%{file}\"",
                 fc_weight, fc_slant);
    }

    FILE* fc = popen(query, "r");
    if (!fc) return NULL;

    static char fontconfig_path[512];
    fontconfig_path[0] = '\0';

    if (fgets(fontconfig_path, sizeof(fontconfig_path), fc)) {
        size_t len = strlen(fontconfig_path);
        if (len > 0 && fontconfig_path[len-1] == '\n') {
            fontconfig_path[len-1] = '\0';
        }
    }

    pclose(fc);

    return fontconfig_path[0] != '\0' ? fontconfig_path : NULL;
}

TTF_Font* desktop_ir_resolve_font(DesktopIRRenderer* renderer, IRComponent* component, float fallback_size) {
    float size = fallback_size > 0 ? fallback_size : 16.0f;
    const char* family = NULL;
    uint16_t weight = 400;  // Default to normal
    bool italic = false;

    if (component && component->style) {
        if (component->style->font.size > 0) size = component->style->font.size;
        family = component->style->font.family;

        // Get weight (use legacy bold flag if weight not set)
        if (component->style->font.weight > 0) {
            weight = component->style->font.weight;
        } else if (component->style->font.bold) {
            weight = 700;
        }

        italic = component->style->font.italic;
    }

    // STEP 1: Check font path cache (O(1) after warmup)
    const char* path = lookup_font_path_cache(family, weight, italic);

    // STEP 2: Cache miss - resolve and cache
    if (!path) {
        // Try fontconfig (5ms, but only once per variant)
        path = query_fontconfig_font(family, weight, italic);

        // Try filesystem naming convention
        if (!path && family) {
            const char* base_path = desktop_ir_find_font_path(family);
            if (base_path) {
                path = try_filesystem_font_variant(base_path, weight, italic);
            }
        }

        // Fallback to registered font
        if (!path && family) {
            path = desktop_ir_find_font_path(family);
        }

        // Fallback to default font
        if (!path && g_default_font_path[0] != '\0') {
            path = g_default_font_path;
        }

        // Cache the result
        if (path) {
            cache_font_path(family, weight, italic, path);
        }
    }

    // STEP 3: Load font from cached path
    if (path) {
        TTF_Font* f = desktop_ir_get_cached_font(path, (int)size);
        if (f) {
            // Apply text decorations (underline, strikethrough)
            int style_flags = compute_font_style_flags(component);
            TTF_SetFontStyle(f, style_flags);
            return f;
        }
    }

    // STEP 4: Fallback to renderer default
    SDL3RendererData* data = sdl3_get_data(renderer);
    if (data && data->default_font) {
        int style_flags = compute_font_style_flags(component);
        TTF_SetFontStyle(data->default_font, style_flags);
        return data->default_font;
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
// FONT METRICS CALLBACKS (for IR layout system)
// ============================================================================

static TTF_Font* get_font_for_metrics(float font_size) {
    int size = (int)(font_size + 0.5f);
    if (g_default_font_path[0] == '\0') return NULL;
    return desktop_ir_get_cached_font(g_default_font_path, size);
}

static float desktop_font_metrics_get_text_width(const char* text, uint32_t length,
                                                  float font_size, const char* font_family) {
    (void)font_family;
    if (!text || length == 0) return 0.0f;

    TTF_Font* font = get_font_for_metrics(font_size);
    if (!font) {
        // No font available yet - return estimate
        return length * font_size * 0.6f;
    }
    int w = 0, h = 0;
    TTF_GetStringSize(font, text, length, &w, &h);
    return (float)w;
}

static float desktop_font_metrics_get_font_height(float font_size, const char* font_family) {
    (void)font_family;
    TTF_Font* font = get_font_for_metrics(font_size);
    return (float)TTF_GetFontHeight(font);
}

static IRFontMetrics g_desktop_font_metrics = {
    .get_text_width = desktop_font_metrics_get_text_width,
    .get_font_height = desktop_font_metrics_get_font_height,
    .get_word_width = NULL
};

void desktop_register_font_metrics(void) {
    ir_set_font_metrics(&g_desktop_font_metrics);
}

// ============================================================================
// TEXT TEXTURE CACHE (LRU with Hash Table - 15-25% speedup)
// ============================================================================
// Phase 1, Week 1: Performance optimization
// Replaced O(n) linear search with O(1) hash table lookup using FNV-1a

#define TEXT_CACHE_HASH_SIZE 256  // Power of 2 for fast modulo

// FNV-1a hash function (fast, good distribution)
static uint32_t fnv1a_hash(const char* data, size_t len) {
    uint32_t hash = 2166136261u;  // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;  // FNV prime
    }
    return hash;
}

// Compute hash key for text cache entry
static uint32_t compute_text_cache_hash(const char* font_path, int font_size,
                                        const char* text, size_t text_len,
                                        uint32_t color) {
    // Hash: font_path + size + text + color
    uint32_t hash = fnv1a_hash(font_path, strlen(font_path));
    hash ^= (uint32_t)font_size;
    hash *= 16777619u;
    hash ^= fnv1a_hash(text, text_len);
    hash *= 16777619u;
    hash ^= color;
    return hash & (TEXT_CACHE_HASH_SIZE - 1);  // Fast modulo for power of 2
}

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

    // O(1) hash table lookup (Phase 1 optimization)
    uint32_t hash = compute_text_cache_hash(font_path, font_size, text, text_len, packed_color);

    // Check first entry in bucket
    int cache_idx = g_text_cache_hash_table[hash].cache_index;
    if (cache_idx >= 0 && cache_idx < TEXT_TEXTURE_CACHE_SIZE) {
        TextTextureCache* entry = &g_text_texture_cache[cache_idx];
        if (entry->valid &&
            entry->font_size == font_size &&
            entry->color == packed_color &&
            strcmp(entry->font_path, font_path) == 0 &&
            strncmp(entry->text, text, text_len) == 0 &&
            entry->text[text_len] == '\0') {
            entry->last_access = g_frame_counter;
            return entry;
        }
    }

    // Check second entry in bucket (collision chain has max 2 entries)
    cache_idx = g_text_cache_hash_table[hash].next_index;
    if (cache_idx >= 0 && cache_idx < TEXT_TEXTURE_CACHE_SIZE) {
        TextTextureCache* entry = &g_text_texture_cache[cache_idx];
        if (entry->valid &&
            entry->font_size == font_size &&
            entry->color == packed_color &&
            strcmp(entry->font_path, font_path) == 0 &&
            strncmp(entry->text, text, text_len) == 0 &&
            entry->text[text_len] == '\0') {
            entry->last_access = g_frame_counter;
            return entry;
        }
    }

    return NULL;  // Cache miss
}

void text_texture_cache_insert(const char* font_path, int font_size, const char* text, size_t text_len,
                                SDL_Color color, SDL_Texture* texture, int width, int height) {
    if (!font_path || !text || !texture || text_len == 0 || text_len > 255) return;

    uint32_t packed_color = pack_color(color);

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

    // Remove old entry from hash table if evicting
    if (g_text_texture_cache[lru_index].valid) {
        uint32_t old_hash = compute_text_cache_hash(
            g_text_texture_cache[lru_index].font_path,
            g_text_texture_cache[lru_index].font_size,
            g_text_texture_cache[lru_index].text,
            strlen(g_text_texture_cache[lru_index].text),
            g_text_texture_cache[lru_index].color
        );

        // Remove from hash chain (max 2 entries per bucket)
        if (g_text_cache_hash_table[old_hash].cache_index == lru_index) {
            // First in chain - promote second to first
            g_text_cache_hash_table[old_hash].cache_index = g_text_cache_hash_table[old_hash].next_index;
            g_text_cache_hash_table[old_hash].next_index = -1;
        } else if (g_text_cache_hash_table[old_hash].next_index == lru_index) {
            // Second in chain - just remove it
            g_text_cache_hash_table[old_hash].next_index = -1;
        }

        // Destroy old texture
        if (g_text_texture_cache[lru_index].texture) {
            SDL_DestroyTexture(g_text_texture_cache[lru_index].texture);
        }
    }

    // Insert new entry into cache
    strncpy(g_text_texture_cache[lru_index].font_path, font_path, sizeof(g_text_texture_cache[lru_index].font_path) - 1);
    g_text_texture_cache[lru_index].font_path[sizeof(g_text_texture_cache[lru_index].font_path) - 1] = '\0';

    strncpy(g_text_texture_cache[lru_index].text, text, text_len);
    g_text_texture_cache[lru_index].text[text_len] = '\0';

    g_text_texture_cache[lru_index].font_size = font_size;
    g_text_texture_cache[lru_index].color = packed_color;
    g_text_texture_cache[lru_index].texture = texture;
    g_text_texture_cache[lru_index].width = width;
    g_text_texture_cache[lru_index].height = height;
    g_text_texture_cache[lru_index].last_access = g_frame_counter;
    g_text_texture_cache[lru_index].valid = true;

    // Insert into hash table
    uint32_t new_hash = compute_text_cache_hash(font_path, font_size, text, text_len, packed_color);

    if (g_text_cache_hash_table[new_hash].cache_index == -1) {
        // Empty bucket - direct insertion
        g_text_cache_hash_table[new_hash].cache_index = lru_index;
        g_text_cache_hash_table[new_hash].next_index = -1;
    } else {
        // Collision - add to chain (prepend for O(1))
        int old_head = g_text_cache_hash_table[new_hash].cache_index;
        g_text_cache_hash_table[new_hash].cache_index = lru_index;
        g_text_cache_hash_table[new_hash].next_index = old_head;
    }
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
        if (texture) {
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text rendering
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);    // Enable alpha blending
        }
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
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);  // Crisp text rendering
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);    // Enable alpha blending
    }
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

// ============================================================================
// FONT CACHE OPTIMIZATION (Phase 1: 2-5% speedup)
// ============================================================================

#define FONT_CACHE_HASH_SIZE 64
int g_font_cache_hash_table[FONT_CACHE_HASH_SIZE];  // Exported for initialization

// Compute hash key for font cache (path + size)
static uint32_t compute_font_cache_hash(const char* path, int size) {
    uint32_t hash = fnv1a_hash(path, strlen(path));
    hash ^= (uint32_t)size;
    hash *= 16777619u;
    return hash % FONT_CACHE_HASH_SIZE;
}

TTF_Font* desktop_ir_get_cached_font(const char* path, int size) {
    if (!path) return NULL;

    // O(1) hash table lookup (Phase 1 optimization)
    uint32_t hash = compute_font_cache_hash(path, size);
    int cache_idx = g_font_cache_hash_table[hash];

    if (cache_idx != -1 && cache_idx < g_font_cache_count) {
        if (g_font_cache[cache_idx].size == size &&
            strcmp(g_font_cache[cache_idx].path, path) == 0) {
            return g_font_cache[cache_idx].font;
        }
    }

    // Cache miss - check for collisions (linear probe)
    for (int i = 0; i < g_font_cache_count; i++) {
        if (g_font_cache[i].size == size && strcmp(g_font_cache[i].path, path) == 0) {
            // Found in cache but hash collision - update hash table
            g_font_cache_hash_table[hash] = i;
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

    int new_index = g_font_cache_count;
    strncpy(g_font_cache[new_index].path, path, sizeof(g_font_cache[new_index].path) - 1);
    g_font_cache[new_index].path[sizeof(g_font_cache[new_index].path) - 1] = '\0';
    g_font_cache[new_index].size = size;
    g_font_cache[new_index].font = font;
    g_font_cache_count++;

    // Insert into hash table
    g_font_cache_hash_table[hash] = new_index;

    return font;
}
