#include "../../core/include/kryon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Try different SDL3 header locations
#if defined(__has_include)
  #if __has_include(<SDL3/SDL.h>)
    #include <SDL3/SDL.h>
    #include <SDL3_ttf/SDL_ttf.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
    #include <SDL_ttf.h>
  #else
    #error "SDL3 headers not found"
  #endif
#else
  // Fallback to standard locations
  #include <SDL3/SDL.h>
  #include <SDL3_ttf/SDL_ttf.h>
#endif

// ============================================================================
// SDL3 Font Management System
// ============================================================================

#define KRYON_MAX_FONTS 16
#define KRYON_FONT_PATH_BUFFER 512

typedef struct {
    TTF_Font* font;
    char name[64];
    uint16_t size;
    uint16_t font_id;
    uint8_t ref_count;
    bool loaded;
} kryon_sdl3_font_t;

typedef struct {
    kryon_sdl3_font_t fonts[KRYON_MAX_FONTS];
    uint8_t font_count;
    TTF_Font* default_font;
    char font_search_paths[8][KRYON_FONT_PATH_BUFFER];
    uint8_t search_path_count;
} kryon_sdl3_font_manager_t;

static kryon_sdl3_font_manager_t font_manager = {0};

// Font loading utilities
static void init_default_font_paths(void) {
    if (font_manager.search_path_count > 0) {
        return; // Already initialized
    }

    const char* default_paths[] = {
        "/usr/share/fonts/truetype/dejavu/",
        "/usr/share/fonts/truetype/liberation/",
        "/usr/share/fonts/truetype/ubuntu/",
        "/usr/share/fonts/TTF/",
        "/usr/share/fonts/truetype/",
        "/Windows/Fonts/",
        "/System/Library/Fonts/",
        "/Library/Fonts/",
        "./fonts/",
        "../fonts/"
    };

    int path_count = sizeof(default_paths) / sizeof(default_paths[0]);
    font_manager.search_path_count = 0;

    for (int i = 0; i < path_count && font_manager.search_path_count < 8; i++) {
        strncpy(font_manager.font_search_paths[font_manager.search_path_count],
                default_paths[i],
                KRYON_FONT_PATH_BUFFER - 1);
        font_manager.font_search_paths[font_manager.search_path_count][KRYON_FONT_PATH_BUFFER - 1] = '\0';
        font_manager.search_path_count++;
    }
}

static bool find_font_file(const char* font_name, char* out_path, size_t out_size) {
    init_default_font_paths();

    // First try direct path
    if (access(font_name, R_OK) == 0) {
        size_t len = strlen(font_name);
        if (len >= out_size) len = out_size - 1;
        memcpy(out_path, font_name, len);
        out_path[len] = '\0';
        return true;
    }

    // Try with .ttf extension
    char temp_path[KRYON_FONT_PATH_BUFFER];
    snprintf(temp_path, sizeof(temp_path), "%s.ttf", font_name);
    if (access(temp_path, R_OK) == 0) {
        size_t len = strlen(temp_path);
        if (len >= out_size) len = out_size - 1;
        memcpy(out_path, temp_path, len);
        out_path[len] = '\0';
        return true;
    }

    // Search in font directories
    for (int i = 0; i < font_manager.search_path_count; i++) {
        snprintf(temp_path, sizeof(temp_path), "%s%s", font_manager.font_search_paths[i], font_name);
        if (access(temp_path, R_OK) == 0) {
            size_t len = strlen(temp_path);
            if (len >= out_size) len = out_size - 1;
            memcpy(out_path, temp_path, len);
            out_path[len] = '\0';
            return true;
        }

        snprintf(temp_path, sizeof(temp_path), "%s%s.ttf", font_manager.font_search_paths[i], font_name);
        if (access(temp_path, R_OK) == 0) {
            size_t len = strlen(temp_path);
            if (len >= out_size) len = out_size - 1;
            memcpy(out_path, temp_path, len);
            out_path[len] = '\0';
            return true;
        }
    }

    return false;
}

static TTF_Font* load_font_from_name(const char* font_name, uint16_t size) {
    char font_path[KRYON_FONT_PATH_BUFFER];

    if (!find_font_file(font_name, font_path, sizeof(font_path))) {
        return NULL;
    }

    return TTF_OpenFont(font_path, size);
}

// Font manager functions
void kryon_sdl3_fonts_init(void) {
    memset(&font_manager, 0, sizeof(kryon_sdl3_font_manager_t));
    init_default_font_paths();

    // Try to load a default font
    const char* default_fonts[] = {
        "DejaVuSans",
        "LiberationSans",
        "Ubuntu",
        "Arial",
        "sans",
        "Helvetica"
    };

    for (size_t i = 0; i < sizeof(default_fonts) / sizeof(default_fonts[0]); i++) {
        font_manager.default_font = load_font_from_name(default_fonts[i], 16);
        if (font_manager.default_font != NULL) {
            break;
        }
    }
}

void kryon_sdl3_fonts_shutdown(void) {
    // Close all loaded fonts
    for (int i = 0; i < KRYON_MAX_FONTS; i++) {
        if (font_manager.fonts[i].loaded && font_manager.fonts[i].font != NULL) {
            TTF_CloseFont(font_manager.fonts[i].font);
            font_manager.fonts[i].font = NULL;
            font_manager.fonts[i].loaded = false;
        }
    }

    if (font_manager.default_font != NULL) {
        TTF_CloseFont(font_manager.default_font);
        font_manager.default_font = NULL;
    }

    memset(&font_manager, 0, sizeof(kryon_sdl3_font_manager_t));
}

uint16_t kryon_sdl3_load_font(const char* name, uint16_t size) {
    if (name == NULL || font_manager.font_count >= KRYON_MAX_FONTS) {
        return 0;
    }

    // Check if font already exists
    for (int i = 0; i < font_manager.font_count; i++) {
        if (font_manager.fonts[i].loaded &&
            strcmp(font_manager.fonts[i].name, name) == 0 &&
            font_manager.fonts[i].size == size) {
            font_manager.fonts[i].ref_count++;
            return font_manager.fonts[i].font_id;
        }
    }

    // Load new font
    TTF_Font* font = load_font_from_name(name, size);
    if (font == NULL) {
        return 0;
    }

    // Add to font manager
    kryon_sdl3_font_t* font_entry = &font_manager.fonts[font_manager.font_count];
    font_entry->font = font;
    strncpy(font_entry->name, name, sizeof(font_entry->name) - 1);
    font_entry->name[sizeof(font_entry->name) - 1] = '\0';
    font_entry->size = size;
    font_entry->font_id = font_manager.font_count + 1; // 1-based IDs
    font_entry->ref_count = 1;
    font_entry->loaded = true;

    return font_entry->font_id;
}

void kryon_sdl3_unload_font(uint16_t font_id) {
    if (font_id == 0 || font_id > KRYON_MAX_FONTS) {
        return;
    }

    kryon_sdl3_font_t* font_entry = &font_manager.fonts[font_id - 1];
    if (!font_entry->loaded) {
        return;
    }

    font_entry->ref_count--;
    if (font_entry->ref_count <= 0) {
        TTF_CloseFont(font_entry->font);
        font_entry->font = NULL;
        font_entry->loaded = false;

        // Move remaining fonts to fill the gap
        for (int i = font_id - 1; i < font_manager.font_count - 1; i++) {
            font_manager.fonts[i] = font_manager.fonts[i + 1];
        }
        font_manager.font_count--;
    }
}

TTF_Font* kryon_sdl3_get_font(uint16_t font_id) {
    if (font_id == 0) {
        return font_manager.default_font;
    }

    if (font_id > KRYON_MAX_FONTS) {
        return NULL;
    }

    kryon_sdl3_font_t* font_entry = &font_manager.fonts[font_id - 1];
    if (font_entry->loaded) {
        return font_entry->font;
    }

    return NULL;
}

// Text measurement utilities - simplified estimation for SDL3_ttf
void kryon_sdl3_measure_text(const char* text, uint16_t font_id, uint16_t* width, uint16_t* height) {
    TTF_Font* font = kryon_sdl3_get_font(font_id);
    if (font == NULL || text == NULL) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    // Use TTF_GetStringSize for accurate text measurement (SDL3_ttf function)
    int measured_width = 0;
    int measured_height = 0;

    if (TTF_GetStringSize(font, text, strlen(text), &measured_width, &measured_height)) {
        // Success - use actual measured dimensions
        if (width) *width = (uint16_t)measured_width;
        if (height) *height = (uint16_t)measured_height;
    } else {
        // Fallback to font height if measurement fails
        int font_height = TTF_GetFontHeight(font);
        size_t text_len = strlen(text);
        int estimated_width = (int)(text_len * font_height * 0.6f);

        if (width) *width = (uint16_t)estimated_width;
        if (height) *height = (uint16_t)font_height;
    }
}

void kryon_sdl3_measure_text_utf8(const char* text, uint16_t font_id, uint16_t* width, uint16_t* height) {
    // For now, treat UTF-8 the same as regular text measurement
    kryon_sdl3_measure_text(text, font_id, width, height);
}

// Text rendering utilities
SDL_Texture* kryon_sdl3_render_text(SDL_Renderer* renderer, const char* text, uint16_t font_id, uint32_t color) {
    TTF_Font* font = kryon_sdl3_get_font(font_id);
    if (font == NULL || text == NULL || renderer == NULL) {
        return NULL;
    }

    SDL_Color sdl_color;
    kryon_color_get_components(color, &sdl_color.r, &sdl_color.g, &sdl_color.b, &sdl_color.a);

    SDL_Surface* surface = TTF_RenderText_Blended(font, text, strlen(text), sdl_color);
    if (surface == NULL) {
        return NULL;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    return texture;
}

SDL_Texture* kryon_sdl3_render_text_wrapped(SDL_Renderer* renderer, const char* text, uint16_t font_id, uint32_t color, uint16_t wrap_width) {
    TTF_Font* font = kryon_sdl3_get_font(font_id);
    if (font == NULL || text == NULL || renderer == NULL) {
        return NULL;
    }

    SDL_Color sdl_color;
    kryon_color_get_components(color, &sdl_color.r, &sdl_color.g, &sdl_color.b, &sdl_color.a);

    SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text, strlen(text), sdl_color, wrap_width);
    if (surface == NULL) {
        return NULL;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    return texture;
}

// Font information utilities
bool kryon_sdl3_get_font_metrics(uint16_t font_id, uint16_t* ascent, uint16_t* descent, uint16_t* height, uint16_t* line_skip) {
    TTF_Font* font = kryon_sdl3_get_font(font_id);
    if (font == NULL) {
        return false;
    }

    if (ascent) *ascent = (uint16_t)TTF_GetFontAscent(font);
    if (descent) *descent = (uint16_t)TTF_GetFontDescent(font);
    if (height) *height = (uint16_t)TTF_GetFontHeight(font);
    if (line_skip) *line_skip = (uint16_t)TTF_GetFontLineSkip(font);

    return true;
}

uint16_t kryon_sdl3_get_font_height(uint16_t font_id) {
    uint16_t height = 0;
    kryon_sdl3_get_font_metrics(font_id, NULL, NULL, &height, NULL);
    return height;
}

bool kryon_sdl3_glyph_provided(uint16_t font_id, uint16_t ch) {
    TTF_Font* font = kryon_sdl3_get_font(font_id);
    if (font == NULL) {
        return false;
    }

    return TTF_FontHasGlyph(font, ch);
}

// Font path management
void kryon_sdl3_add_font_search_path(const char* path) {
    if (path == NULL || font_manager.search_path_count >= 8) {
        return;
    }

    strncpy(font_manager.font_search_paths[font_manager.search_path_count],
            path,
            KRYON_FONT_PATH_BUFFER - 1);
    font_manager.font_search_paths[font_manager.search_path_count][KRYON_FONT_PATH_BUFFER - 1] = '\0';
    font_manager.search_path_count++;
}

const char* kryon_sdl3_get_font_name(uint16_t font_id) {
    if (font_id == 0) {
        return "default";
    }

    if (font_id > KRYON_MAX_FONTS) {
        return NULL;
    }

    kryon_sdl3_font_t* font_entry = &font_manager.fonts[font_id - 1];
    if (font_entry->loaded) {
        return font_entry->name;
    }

    return NULL;
}

uint16_t kryon_sdl3_get_font_size(uint16_t font_id) {
    if (font_id == 0) {
        return 16; // Default size
    }

    if (font_id > KRYON_MAX_FONTS) {
        return 0;
    }

    kryon_sdl3_font_t* font_entry = &font_manager.fonts[font_id - 1];
    if (font_entry->loaded) {
        return font_entry->size;
    }

    return 0;
}