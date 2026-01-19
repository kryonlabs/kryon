/**
 * Raylib Font Management System
 *
 * Provides font loading, caching, and text measurement for Raylib backend.
 * Uses fontconfig for system font discovery (same as SDL3).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <raylib.h>

#include "../../desktop_internal.h"
#include "raylib_renderer.h"

// ============================================================================
// FONT CACHE
// ============================================================================

#define MAX_RAYLIB_CACHED_FONTS 32

typedef struct {
    char path[512];
    int size;
    Font font;
    bool loaded;
    uint64_t last_used_frame;  // For LRU eviction
} RaylibCachedFont;

static RaylibCachedFont g_raylib_font_cache[MAX_RAYLIB_CACHED_FONTS] = {0};
static uint64_t g_current_frame = 0;

/**
 * Find font in cache by path and size
 */
static RaylibCachedFont* find_cached_font(const char* path, int size) {
    for (int i = 0; i < MAX_RAYLIB_CACHED_FONTS; i++) {
        if (g_raylib_font_cache[i].loaded &&
            strcmp(g_raylib_font_cache[i].path, path) == 0 &&
            g_raylib_font_cache[i].size == size) {
            g_raylib_font_cache[i].last_used_frame = g_current_frame;
            return &g_raylib_font_cache[i];
        }
    }
    return NULL;
}

/**
 * Find empty cache slot or evict least recently used font
 */
static RaylibCachedFont* get_cache_slot() {
    // First, try to find empty slot
    for (int i = 0; i < MAX_RAYLIB_CACHED_FONTS; i++) {
        if (!g_raylib_font_cache[i].loaded) {
            return &g_raylib_font_cache[i];
        }
    }

    // No empty slots, evict LRU
    int lru_index = 0;
    uint64_t oldest_frame = g_raylib_font_cache[0].last_used_frame;

    for (int i = 1; i < MAX_RAYLIB_CACHED_FONTS; i++) {
        if (g_raylib_font_cache[i].last_used_frame < oldest_frame) {
            oldest_frame = g_raylib_font_cache[i].last_used_frame;
            lru_index = i;
        }
    }

    // Unload old font
    if (g_raylib_font_cache[lru_index].loaded) {
        UnloadFont(g_raylib_font_cache[lru_index].font);
        g_raylib_font_cache[lru_index].loaded = false;
    }

    return &g_raylib_font_cache[lru_index];
}

/**
 * Load and cache a font
 */
Font raylib_load_cached_font(const char* path, int size) {
    if (!path || path[0] == '\0') {
        return GetFontDefault();
    }

    g_current_frame++;

    // Check cache
    RaylibCachedFont* cached = find_cached_font(path, size);
    if (cached) {
        return cached->font;
    }

    // Load new font
    RaylibCachedFont* slot = get_cache_slot();
    if (!slot) {
        fprintf(stderr, "ERROR: Font cache full, returning default font\n");
        return GetFontDefault();
    }

    Font font = LoadFontEx(path, size, NULL, 0);

    // Verify font loaded correctly
    if (font.texture.id == 0) {
        fprintf(stderr, "ERROR: Failed to load font: %s (size %d), using default\n", path, size);
        return GetFontDefault();
    }

    // Cache the font
    strncpy(slot->path, path, sizeof(slot->path) - 1);
    slot->path[sizeof(slot->path) - 1] = '\0';
    slot->size = size;
    slot->font = font;
    slot->loaded = true;
    slot->last_used_frame = g_current_frame;

    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);  // Smooth text

    return font;
}

/**
 * Cleanup all cached fonts
 */
void raylib_fonts_cleanup() {
    for (int i = 0; i < MAX_RAYLIB_CACHED_FONTS; i++) {
        if (g_raylib_font_cache[i].loaded) {
            UnloadFont(g_raylib_font_cache[i].font);
            g_raylib_font_cache[i].loaded = false;
        }
    }
}

// ============================================================================
// FONTCONFIG INTEGRATION (reused from SDL3)
// ============================================================================

/**
 * Find system font using fontconfig
 * Returns path to font file, or empty string if not found
 */
static void find_system_font_fc(const char* family, bool bold, bool italic,
                                char* out_path, size_t max_len) {
    out_path[0] = '\0';

    // Build fc-match query
    char query[256];
    snprintf(query, sizeof(query), "%s:weight=%s:slant=%s",
             family,
             bold ? "bold" : "regular",
             italic ? "italic" : "roman");

    // Run fc-match to find font
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "fc-match -f '%%{file}' '%s' 2>/dev/null", query);

    FILE* fp = popen(cmd, "r");
    if (!fp) return;

    char result[512] = {0};
    if (fgets(result, sizeof(result), fp) != NULL) {
        // Remove trailing newline
        size_t len = strlen(result);
        if (len > 0 && result[len - 1] == '\n') {
            result[len - 1] = '\0';
        }

        // Verify file exists
        FILE* test = fopen(result, "r");
        if (test) {
            fclose(test);
            strncpy(out_path, result, max_len - 1);
            out_path[max_len - 1] = '\0';
        }
    }

    pclose(fp);
}

/**
 * Try filesystem font variant (e.g., Regular -> Bold, Italic)
 */
static bool try_filesystem_variant(const char* base_path, bool bold, bool italic,
                                   char* out_path, size_t max_len) {
    // Extract directory and base name
    char dir[512] = {0};
    char base[256] = {0};
    char ext[16] = {0};

    const char* last_slash = strrchr(base_path, '/');
    if (last_slash) {
        size_t dir_len = last_slash - base_path;
        strncpy(dir, base_path, dir_len);
        dir[dir_len] = '\0';

        const char* filename = last_slash + 1;
        const char* last_dot = strrchr(filename, '.');
        if (last_dot) {
            size_t base_len = last_dot - filename;
            strncpy(base, filename, base_len);
            base[base_len] = '\0';
            strncpy(ext, last_dot, sizeof(ext) - 1);
        } else {
            strncpy(base, filename, sizeof(base) - 1);
        }
    }

    // Try common variant patterns
    const char* variants[] = {
        bold && italic ? "-BoldItalic" : bold ? "-Bold" : italic ? "-Italic" : "",
        bold && italic ? "BoldItalic" : bold ? "Bold" : italic ? "Italic" : "",
        bold && italic ? "_Bold_Italic" : bold ? "_Bold" : italic ? "_Italic" : "",
        NULL
    };

    for (int i = 0; variants[i]; i++) {
        char variant_path[512];
        snprintf(variant_path, sizeof(variant_path), "%s/%s%s%s",
                 dir, base, variants[i], ext);

        FILE* test = fopen(variant_path, "r");
        if (test) {
            fclose(test);
            strncpy(out_path, variant_path, max_len - 1);
            out_path[max_len - 1] = '\0';
            return true;
        }
    }

    return false;
}

/**
 * Resolve font path from family name and style
 * This is the main entry point for font resolution
 */
bool raylib_resolve_font_path(const char* family, bool bold, bool italic,
                              char* out_path, size_t max_len) {
    if (!family || family[0] == '\0') {
        out_path[0] = '\0';
        return false;
    }

    // First, try fontconfig
    find_system_font_fc(family, bold, italic, out_path, max_len);

    if (out_path[0] != '\0') {
        return true;
    }

    // If fontconfig failed, try common font locations
    const char* font_dirs[] = {
        "/usr/share/fonts",
        "/usr/local/share/fonts",
        "~/.fonts",
        "~/.local/share/fonts",
        NULL
    };

    char search_cmd[1024];
    snprintf(search_cmd, sizeof(search_cmd),
             "find %s -iname '*%s*.ttf' -o -iname '*%s*.otf' 2>/dev/null | head -1",
             font_dirs[0], family, family);

    FILE* fp = popen(search_cmd, "r");
    if (fp) {
        char result[512] = {0};
        if (fgets(result, sizeof(result), fp) != NULL) {
            size_t len = strlen(result);
            if (len > 0 && result[len - 1] == '\n') {
                result[len - 1] = '\0';
            }

            // Try to find variant
            if (!try_filesystem_variant(result, bold, italic, out_path, max_len)) {
                strncpy(out_path, result, max_len - 1);
                out_path[max_len - 1] = '\0';
            }
        }
        pclose(fp);
    }

    return out_path[0] != '\0';
}

// ============================================================================
// TEXT MEASUREMENT
// ============================================================================

/**
 * Measure text width using Raylib
 */
float raylib_measure_text_width(Font font, const char* text, float font_size) {
    if (!text || text[0] == '\0') return 0.0f;

    Vector2 size = MeasureTextEx(font, text, font_size, 0);
    return size.x;
}

/**
 * Get font height (ascent + descent)
 */
float raylib_get_font_height(Font font, float font_size) {
    // Raylib font height is baseSize, scale to requested size
    return font_size;
}

/**
 * Get font ascent (distance from baseline to top)
 */
float raylib_get_font_ascent(Font font, float font_size) {
    // Approximation: 80% of font height
    return font_size * 0.8f;
}

/**
 * Get font descent (distance from baseline to bottom)
 */
float raylib_get_font_descent(Font font, float font_size) {
    // Approximation: 20% of font height
    return font_size * 0.2f;
}
