#define _GNU_SOURCE
#include "../../core/include/kryon.h"
#include "sdl3_effects.h"
#include "../../third_party/stb/stb_image_write.h"
// command_buf functions are now in libkryon_core.a - no need to include the .c file
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>

#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#endif

#ifndef KRYON_HAS_FONTCONFIG
#define KRYON_HAS_FONTCONFIG 0
#endif

#if KRYON_HAS_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define KRYON_MAX_FONT_PATH 1024

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

#include "sdl3.h"

// Forward declaration for input system
void kryon_sdl3_input_set_window(SDL_Window* window);

// ============================================================================
// SDL3 Backend Implementation
// ============================================================================

// SDL3-specific backend data
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* default_font;
    TTF_Font* monospace_font;  // Font for code (font_id == 1)
    uint32_t window_id;
    bool initialized;
    uint16_t last_width, last_height;

    // Window configuration from DSL
    uint16_t config_width, config_height;
    char* config_title;
    bool owns_window;            // Whether we own the window (vs wrapped existing)

    // Clipping stack
    SDL_Rect clip_stack[8];      // Stack of clip rectangles (max 8 nested levels)
    int clip_stack_depth;        // Current depth in the stack
    SDL_Rect current_clip;       // Currently pending clip rectangle
    bool clip_enabled;           // Whether clipping is currently active
} kryon_sdl3_backend_t;

// Forward declarations
static bool sdl3_init(kryon_renderer_t* renderer, void* native_window);
static void sdl3_shutdown(kryon_renderer_t* renderer);
static void sdl3_begin_frame(kryon_renderer_t* renderer);
static void sdl3_end_frame(kryon_renderer_t* renderer);
static void sdl3_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf);
static void sdl3_swap_buffers(kryon_renderer_t* renderer);
static void sdl3_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height);
static void sdl3_set_clear_color(kryon_renderer_t* renderer, uint32_t color);

// Trace helper --------------------------------------------------------------
static bool kryon_sdl3_should_trace(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
        const char* env = SDL_getenv("KRYON_TRACE_COMMANDS");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
    }
    return enabled;
}

static void kryon_sdl3_trace_command(const kryon_command_t* cmd) {
    if (cmd == NULL) {
        return;
    }

    switch (cmd->type) {
        case KRYON_CMD_DRAW_RECT:
            fprintf(stderr,
                    "[kryon][sdl3][trace] rect x=%d y=%d w=%u h=%u color=%08X\n",
                    cmd->data.draw_rect.x,
                    cmd->data.draw_rect.y,
                    cmd->data.draw_rect.w,
                    cmd->data.draw_rect.h,
                    cmd->data.draw_rect.color);
            break;

        case KRYON_CMD_DRAW_TEXT:
            fprintf(stderr,
                    "[kryon][sdl3][trace] text x=%d y=%d color=%08X text=\"%s\"\n",
                    cmd->data.draw_text.x,
                    cmd->data.draw_text.y,
                    cmd->data.draw_text.color,
                    cmd->data.draw_text.text ? cmd->data.draw_text.text : "(null)");
            break;

        case KRYON_CMD_DRAW_LINE:
            fprintf(stderr,
                    "[kryon][sdl3][trace] line (%d,%d)->(%d,%d) color=%08X\n",
                    cmd->data.draw_line.x1,
                    cmd->data.draw_line.y1,
                    cmd->data.draw_line.x2,
                    cmd->data.draw_line.y2,
                    cmd->data.draw_line.color);
            break;

        case KRYON_CMD_DRAW_ARC:
            fprintf(stderr,
                    "[kryon][sdl3][trace] arc center=(%d,%d) r=%u start=%d end=%d color=%08X\n",
                    cmd->data.draw_arc.cx,
                    cmd->data.draw_arc.cy,
                    cmd->data.draw_arc.radius,
                    cmd->data.draw_arc.start_angle,
                    cmd->data.draw_arc.end_angle,
                    cmd->data.draw_arc.color);
            break;

        case KRYON_CMD_DRAW_POLYGON:
            fprintf(stderr,
                    "[kryon][sdl3][trace] polygon vertices=%u color=%08X filled=%s\n",
                    cmd->data.draw_polygon.vertex_count,
                    cmd->data.draw_polygon.color,
                    cmd->data.draw_polygon.filled ? "true" : "false");
            break;

        default:
            fprintf(stderr, "[kryon][sdl3][trace] command type=%d\n", cmd->type);
            break;
    }
}

// ============================================================================
// Font discovery utilities
// ============================================================================

static bool kryon_file_exists(const char* path) {
    if (path == NULL || *path == '\0') {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (file != NULL) {
        fclose(file);
        return true;
    }
    return false;
}

static bool kryon_try_env_font(char* font_path, size_t path_len) {
    const char* env_font = SDL_getenv("KRYON_FONT_PATH");
    if (kryon_file_exists(env_font)) {
        SDL_strlcpy(font_path, env_font, path_len);
        return true;
    }
    return false;
}

#if KRYON_HAS_FONTCONFIG
static bool kryon_try_fontconfig(char* font_path, size_t path_len) {
    if (!FcInit()) {
        return false;
    }

    FcPattern* pattern = FcPatternCreate();
    if (pattern == NULL) {
        return false;
    }

    FcPatternAddString(pattern, FC_FAMILY, (const FcChar8*)"sans-serif");
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR);
    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);

    FcResult result;
    FcPattern* font = FcFontMatch(NULL, pattern, &result);
    bool success = false;

    if (font != NULL) {
        FcChar8* file = NULL;
        if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file != NULL) {
            SDL_strlcpy(font_path, (const char*)file, path_len);
            success = true;
        }
        FcPatternDestroy(font);
    }

    FcPatternDestroy(pattern);
    return success;
}
#else
static bool kryon_try_fontconfig(char* font_path, size_t path_len) {
    (void)font_path;
    (void)path_len;
    return false;
}
#endif

static void kryon_append_path(char* out, size_t len, const char* dir, const char* file) {
    if (dir == NULL || file == NULL) {
        out[0] = '\0';
        return;
    }

    SDL_strlcpy(out, dir, len);

    const size_t dir_len = SDL_strlen(out);
    if (dir_len > 0) {
        const char last = out[dir_len - 1];
        const bool needs_sep = last != '/' && last != '\\';
        if (needs_sep) {
#if defined(_WIN32)
            SDL_strlcat(out, "\\", len);
#else
            SDL_strlcat(out, "/", len);
#endif
        }
    }

    SDL_strlcat(out, file, len);
}

static bool kryon_try_directory_fonts(char* font_path, size_t path_len,
                                      const char* dir,
                                      const char* const* font_names,
                                      size_t font_count) {
    if (dir == NULL || *dir == '\0') {
        return false;
    }

    for (size_t i = 0; i < font_count; ++i) {
        char candidate[KRYON_MAX_FONT_PATH];
        kryon_append_path(candidate, sizeof(candidate), dir, font_names[i]);
        if (kryon_file_exists(candidate)) {
            SDL_strlcpy(font_path, candidate, path_len);
            return true;
        }
    }
    return false;
}

static bool kryon_try_known_locations(char* font_path, size_t path_len) {
    static const char* font_names[] = {
        "DejaVuSans.ttf",
        "FreeSans.ttf",
        "LiberationSans-Regular.ttf",
        "Arial.ttf",
        "NotoSans-Regular.ttf",
        "Ubuntu-R.ttf",
        "Cantarell-VF.otf"
    };

    static const char* direct_files[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/local/share/fonts/DejaVuSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/SFNS.ttf"
    };

    for (size_t i = 0; i < SDL_arraysize(direct_files); ++i) {
        if (kryon_file_exists(direct_files[i])) {
            SDL_strlcpy(font_path, direct_files[i], path_len);
            return true;
        }
    }

#if defined(_WIN32)
    const char* win_dir = SDL_getenv("SystemRoot");
    if (win_dir == NULL) {
        win_dir = SDL_getenv("WINDIR");
    }
    char windows_fonts_dir[512];
    if (win_dir != NULL) {
        SDL_snprintf(windows_fonts_dir, sizeof(windows_fonts_dir), "%s\\Fonts", win_dir);
    } else {
        SDL_strlcpy(windows_fonts_dir, "C:\\\\Windows\\\\Fonts", sizeof(windows_fonts_dir));
    }

    static const char* windows_fonts[] = {
        "segoeui.ttf",
        "arial.ttf",
        "calibri.ttf"
    };

    if (kryon_try_directory_fonts(font_path, path_len, windows_fonts_dir,
                                  windows_fonts, SDL_arraysize(windows_fonts))) {
        return true;
    }
#endif

    static const char* linux_dirs[] = {
        "/usr/share/fonts",
        "/usr/local/share/fonts",
        "/usr/share/fonts/truetype",
        "/usr/share/fonts/truetype/dejavu",
        "/usr/share/fonts/truetype/noto",
        "/usr/share/fonts/opentype",
        NULL
    };

    for (size_t i = 0; linux_dirs[i] != NULL; ++i) {
        if (kryon_try_directory_fonts(font_path, path_len, linux_dirs[i],
                                      font_names, SDL_arraysize(font_names))) {
            return true;
        }
    }

#if defined(__APPLE__)
    static const char* mac_dirs[] = {
        "/Library/Fonts",
        "/System/Library/Fonts",
        NULL
    };

    if (kryon_try_directory_fonts(font_path, path_len, mac_dirs[0],
                                  font_names, SDL_arraysize(font_names)) ||
        kryon_try_directory_fonts(font_path, path_len, mac_dirs[1],
                                  font_names, SDL_arraysize(font_names))) {
        return true;
    }
#endif

    return false;
}

static bool kryon_try_xdg_dirs(char* font_path, size_t path_len) {
    const char* xdg_dirs = SDL_getenv("XDG_DATA_DIRS");
    if (xdg_dirs == NULL || *xdg_dirs == '\0') {
        return false;
    }

    char dirs_copy[2048];
    SDL_strlcpy(dirs_copy, xdg_dirs, sizeof(dirs_copy));

    char* saveptr = NULL;
    char* token = SDL_strtok_r(dirs_copy, ":", &saveptr);
    static const char* suffixes[] = {
        "fonts/truetype/dejavu/DejaVuSans.ttf",
        "fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "fonts/truetype/freefont/FreeSans.ttf",
        "fonts/TTF/DejaVuSans.ttf"
    };

    while (token != NULL) {
        for (size_t i = 0; i < SDL_arraysize(suffixes); ++i) {
            char candidate[KRYON_MAX_FONT_PATH];
            kryon_append_path(candidate, sizeof(candidate), token, suffixes[i]);
            if (kryon_file_exists(candidate)) {
                SDL_strlcpy(font_path, candidate, path_len);
                return true;
            }
        }
        token = SDL_strtok_r(NULL, ":", &saveptr);
    }

    return false;
}

#if defined(__linux__)
static bool kryon_try_nix_store(char* font_path, size_t path_len) {
    DIR* dir = opendir("/nix/store");
    if (dir == NULL) {
        return false;
    }

    static const char* font_names[] = {
        "DejaVuSans.ttf",
        "FreeSans.ttf",
        "LiberationSans-Regular.ttf",
        "NotoSans-Regular.ttf",
        "Ubuntu-R.ttf"
    };

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

#ifdef DT_DIR
        if (entry->d_type != DT_DIR && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN) {
            continue;
        }
#endif

        if (SDL_strstr(entry->d_name, "font") == NULL && SDL_strstr(entry->d_name, "Font") == NULL) {
            continue;
        }

        char base_dir[KRYON_MAX_FONT_PATH];
        SDL_snprintf(base_dir, sizeof(base_dir), "/nix/store/%s/share/fonts", entry->d_name);
        if (kryon_try_directory_fonts(font_path, path_len, base_dir,
                                      font_names, SDL_arraysize(font_names))) {
            closedir(dir);
            return true;
        }

        SDL_snprintf(base_dir, sizeof(base_dir), "/nix/store/%s/share/fonts/truetype", entry->d_name);
        if (kryon_try_directory_fonts(font_path, path_len, base_dir,
                                      font_names, SDL_arraysize(font_names))) {
            closedir(dir);
            return true;
        }
    }

    closedir(dir);
    return false;
}
#else
static bool kryon_try_nix_store(char* font_path, size_t path_len) {
    (void)font_path;
    (void)path_len;
    return false;
}
#endif

static const char* kryon_locate_default_font(void) {
    static bool resolved = false;
    static bool success = false;
    static char cached_path[KRYON_MAX_FONT_PATH];

    if (resolved) {
        return success ? cached_path : NULL;
    }

    resolved = true;

    if (kryon_try_env_font(cached_path, sizeof(cached_path))) {
        success = true;
        return cached_path;
    }

    if (kryon_try_fontconfig(cached_path, sizeof(cached_path))) {
        success = true;
        return cached_path;
    }

    if (kryon_try_known_locations(cached_path, sizeof(cached_path))) {
        success = true;
        return cached_path;
    }

    if (kryon_try_xdg_dirs(cached_path, sizeof(cached_path))) {
        success = true;
        return cached_path;
    }

    if (kryon_try_nix_store(cached_path, sizeof(cached_path))) {
        success = true;
        return cached_path;
    }

    success = false;
    return NULL;
}

static bool kryon_try_known_monospace_locations(char* font_path, size_t path_len) {
    static const char* mono_font_names[] = {
        "DejaVuSansMono.ttf",
        "FreeMono.ttf",
        "LiberationMono-Regular.ttf",
        "UbuntuMono-R.ttf",
        "CascadiaCode.ttf",
        "SourceCodePro-Regular.ttf",
        "FiraCode-Regular.ttf",
        "JetBrainsMono-Regular.ttf"
    };

    static const char* direct_files[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/local/share/fonts/DejaVuSansMono.ttf",
        "/Library/Fonts/Courier New.ttf",
        "/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Menlo.ttc"
    };

    // Try direct file paths first
    for (size_t i = 0; i < SDL_arraysize(direct_files); ++i) {
        if (kryon_file_exists(direct_files[i])) {
            SDL_strlcpy(font_path, direct_files[i], path_len);
            return true;
        }
    }

#if defined(_WIN32)
    const char* win_dir = SDL_getenv("SystemRoot");
    if (win_dir == NULL) {
        win_dir = SDL_getenv("WINDIR");
    }
    char windows_fonts_dir[512];
    if (win_dir != NULL) {
        SDL_snprintf(windows_fonts_dir, sizeof(windows_fonts_dir), "%s\\Fonts", win_dir);
    } else {
        SDL_strlcpy(windows_fonts_dir, "C:\\Windows\\Fonts", sizeof(windows_fonts_dir));
    }

    static const char* windows_mono_fonts[] = {
        "consola.ttf",
        "cour.ttf",
        "CascadiaCode.ttf"
    };

    if (kryon_try_directory_fonts(font_path, path_len, windows_fonts_dir,
                                  windows_mono_fonts, SDL_arraysize(windows_mono_fonts))) {
        return true;
    }
#endif

    // Try common Linux font directories
    static const char* linux_dirs[] = {
        "/usr/share/fonts/truetype/dejavu",
        "/usr/share/fonts/truetype/liberation",
        "/usr/share/fonts/truetype/ubuntu",
        "/usr/share/fonts",
        "/usr/local/share/fonts",
        "/usr/share/fonts/truetype",
        "/usr/share/fonts/opentype",
        NULL
    };

    for (size_t i = 0; linux_dirs[i] != NULL; ++i) {
        if (kryon_try_directory_fonts(font_path, path_len, linux_dirs[i],
                                      mono_font_names, SDL_arraysize(mono_font_names))) {
            return true;
        }
    }

#if defined(__APPLE__)
    static const char* mac_dirs[] = {
        "/Library/Fonts",
        "/System/Library/Fonts",
        NULL
    };

    static const char* mac_mono_fonts[] = {
        "Monaco.ttf",
        "Menlo.ttc",
        "Courier New.ttf"
    };

    for (size_t i = 0; mac_dirs[i] != NULL; ++i) {
        if (kryon_try_directory_fonts(font_path, path_len, mac_dirs[i],
                                      mac_mono_fonts, SDL_arraysize(mac_mono_fonts))) {
            return true;
        }
    }

    // Also try the generic monospace font names
    if (kryon_try_directory_fonts(font_path, path_len, mac_dirs[0],
                                  mono_font_names, SDL_arraysize(mono_font_names))) {
        return true;
    }
#endif

    return false;
}

static const char* kryon_locate_monospace_font(void) {
    static bool resolved = false;
    static bool success = false;
    static char cached_path[KRYON_MAX_FONT_PATH];

    if (resolved) {
        return success ? cached_path : NULL;
    }

    resolved = true;

    // Check for KRYON_MONO_FONT_PATH environment variable
    const char* env_mono_font = SDL_getenv("KRYON_MONO_FONT_PATH");
    if (kryon_file_exists(env_mono_font)) {
        SDL_strlcpy(cached_path, env_mono_font, sizeof(cached_path));
        success = true;
        return cached_path;
    }

#if KRYON_HAS_FONTCONFIG
    // Try fontconfig for monospace font
    if (!FcInit()) {
        goto try_known_locations;
    }

    FcPattern* pattern = FcPatternCreate();
    if (pattern != NULL) {
        FcPatternAddString(pattern, FC_FAMILY, (const FcChar8*)"monospace");
        FcPatternAddInteger(pattern, FC_SPACING, FC_MONO);

        FcResult result;
        FcPattern* font = FcFontMatch(NULL, pattern, &result);

        if (font != NULL) {
            FcChar8* file = NULL;
            if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file != NULL) {
                SDL_strlcpy(cached_path, (const char*)file, sizeof(cached_path));
                success = true;
                FcPatternDestroy(font);
                FcPatternDestroy(pattern);
                return cached_path;
            }
            FcPatternDestroy(font);
        }
        FcPatternDestroy(pattern);
    }

try_known_locations:
#endif

    if (kryon_try_known_monospace_locations(cached_path, sizeof(cached_path))) {
        success = true;
        return cached_path;
    }

    success = false;
    return NULL;
}

// SDL3 color conversion helper
static SDL_Color kryon_color_to_sdl(uint32_t kryon_color) {
    SDL_Color sdl_color;
    kryon_color_get_components(kryon_color, &sdl_color.r, &sdl_color.g, &sdl_color.b, &sdl_color.a);
    return sdl_color;
}

// Text measurement callback for markdown renderer
void kryon_sdl3_measure_text_styled(const char* text, uint16_t font_size,
                                    uint8_t font_weight, uint8_t font_style,
                                    uint16_t font_id,
                                    uint16_t* width, uint16_t* height, void* user_data) {
    if (text == NULL || width == NULL || height == NULL) {
        return;
    }

    // Select font path based on font_id
    const char* font_path = NULL;
    if (font_id == 1) {
        // Monospace font for code
        font_path = kryon_locate_monospace_font();
    }

    if (font_path == NULL) {
        // Default font (font_id == 0 or monospace not found)
        font_path = kryon_locate_default_font();
    }

    if (font_path == NULL) {
        // Fallback to approximation
        if (width) *width = (uint16_t)(strlen(text) * font_size * 0.5f);
        if (height) *height = font_size;
        return;
    }

    TTF_Font* font = TTF_OpenFont(font_path, font_size);
    if (font == NULL) {
        // Fallback to approximation
        if (width) *width = (uint16_t)(strlen(text) * font_size * 0.5f);
        if (height) *height = font_size;
        return;
    }

    // Apply font style
    int ttf_style = TTF_STYLE_NORMAL;
    if (font_weight == KRYON_FONT_WEIGHT_BOLD) {
        ttf_style |= TTF_STYLE_BOLD;
    }
    if (font_style == KRYON_FONT_STYLE_ITALIC) {
        ttf_style |= TTF_STYLE_ITALIC;
    }
    TTF_SetFontStyle(font, ttf_style);

    // CRITICAL FIX: Render text to a surface to get the ACTUAL pixel dimensions
    // This ensures measurement matches what sdl3_draw_text() will render
    // TTF_GetStringSize returns logical width, but actual rendered width can differ
    SDL_Color dummy_color = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, strlen(text), dummy_color);
    
    if (surface != NULL) {
        // Use the actual rendered surface dimensions
        *width = (uint16_t)surface->w;
        *height = (uint16_t)surface->h;
        SDL_DestroySurface(surface);
    } else {
        // Fallback to TTF_GetStringSize if rendering fails
        int measured_width = 0;
        int measured_height = 0;
        
        if (TTF_GetStringSize(font, text, strlen(text), &measured_width, &measured_height)) {
            *width = (uint16_t)measured_width;
            *height = (uint16_t)measured_height;
        } else {
            // Final fallback to approximation
            *width = (uint16_t)(strlen(text) * font_size * 0.5f);
            *height = font_size;
        }
    }

    // Clean up
    TTF_CloseFont(font);
}

// Command execution functions
static void sdl3_draw_rect(kryon_cmd_buf_t* buf, const kryon_command_t* cmd, SDL_Renderer* renderer) {
    SDL_FRect frect = {
        (float)cmd->data.draw_rect.x,
        (float)cmd->data.draw_rect.y,
        (float)cmd->data.draw_rect.w,
        (float)cmd->data.draw_rect.h
    };

    SDL_Color color = kryon_color_to_sdl(cmd->data.draw_rect.color);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &frect);
}

static void sdl3_draw_text(kryon_cmd_buf_t* buf, const kryon_command_t* cmd, SDL_Renderer* renderer, TTF_Font* default_font) {
    TTF_Font* font_to_use = default_font;
    TTF_Font* dynamic_font = NULL;

    /* Use text_storage if text pointer is NULL */
    const char* text_to_render = cmd->data.draw_text.text ?
                                   cmd->data.draw_text.text :
                                   cmd->data.draw_text.text_storage;

    if (text_to_render == NULL || text_to_render[0] == '\0') {
        return;
    }

    // If a specific font size is requested (non-zero), load font at that size
    if (cmd->data.draw_text.font_size > 0) {
        // Select font based on font_id
        const char* font_path = NULL;
        if (cmd->data.draw_text.font_id == 1) {
            // Use monospace font for code
            font_path = kryon_locate_monospace_font();
        }

        if (font_path == NULL) {
            // Fallback to default font
            font_path = kryon_locate_default_font();
        }

        if (font_path != NULL) {
            dynamic_font = TTF_OpenFont(font_path, cmd->data.draw_text.font_size);
            if (dynamic_font != NULL) {
                font_to_use = dynamic_font;
            }
        }
    }

    if (font_to_use == NULL) {
        if (dynamic_font != NULL) {
            TTF_CloseFont(dynamic_font);
        }
        return;
    }

    // Apply font style based on weight and style parameters
    int ttf_style = TTF_STYLE_NORMAL;
    if (cmd->data.draw_text.font_weight == KRYON_FONT_WEIGHT_BOLD) {
        ttf_style |= TTF_STYLE_BOLD;
    }
    if (cmd->data.draw_text.font_style == KRYON_FONT_STYLE_ITALIC) {
        ttf_style |= TTF_STYLE_ITALIC;
    }
    TTF_SetFontStyle(font_to_use, ttf_style);

    SDL_Color color = kryon_color_to_sdl(cmd->data.draw_text.color);
    SDL_Surface* surface = TTF_RenderText_Blended(font_to_use, text_to_render, strlen(text_to_render), color);
    if (surface == NULL) {
        if (dynamic_font != NULL) {
            TTF_CloseFont(dynamic_font);
        }
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        SDL_DestroySurface(surface);
        if (dynamic_font != NULL) {
            TTF_CloseFont(dynamic_font);
        }
        return;
    }

    float text_width, text_height;
    SDL_GetTextureSize(texture, &text_width, &text_height);

    SDL_FRect dst_rect = {
        (float)cmd->data.draw_text.x,
        (float)cmd->data.draw_text.y,
        (float)text_width,
        (float)text_height
    };

    SDL_RenderTexture(renderer, texture, NULL, &dst_rect);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);

    // Reset font style back to normal to avoid affecting subsequent text
    TTF_SetFontStyle(font_to_use, TTF_STYLE_NORMAL);

    // Clean up dynamically loaded font
    if (dynamic_font != NULL) {
        TTF_CloseFont(dynamic_font);
    }
}

static void sdl3_draw_line(kryon_cmd_buf_t* buf, const kryon_command_t* cmd, SDL_Renderer* renderer) {
    SDL_Color color = kryon_color_to_sdl(cmd->data.draw_line.color);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(renderer, cmd->data.draw_line.x1, cmd->data.draw_line.y1,
                   cmd->data.draw_line.x2, cmd->data.draw_line.y2);
}

static void sdl3_draw_arc(kryon_cmd_buf_t* buf, const kryon_command_t* cmd, SDL_Renderer* renderer) {
    SDL_Color color = kryon_color_to_sdl(cmd->data.draw_arc.color);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    // Draw arc as a series of line segments (approximation)
    const int segments = 32;
    const float angle_step = (cmd->data.draw_arc.end_angle - cmd->data.draw_arc.start_angle) / (float)segments;

    float prev_x = cmd->data.draw_arc.cx + cmd->data.draw_arc.radius * cosf(cmd->data.draw_arc.start_angle * M_PI / 180.0f);
    float prev_y = cmd->data.draw_arc.cy + cmd->data.draw_arc.radius * sinf(cmd->data.draw_arc.start_angle * M_PI / 180.0f);

    for (int i = 1; i <= segments; i++) {
        float angle = cmd->data.draw_arc.start_angle + i * angle_step;
        float x = cmd->data.draw_arc.cx + cmd->data.draw_arc.radius * cosf(angle * M_PI / 180.0f);
        float y = cmd->data.draw_arc.cy + cmd->data.draw_arc.radius * sinf(angle * M_PI / 180.0f);
        SDL_RenderLine(renderer, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

static void sdl3_draw_polygon(kryon_cmd_buf_t* buf, const kryon_command_t* cmd, SDL_Renderer* renderer) {
    fprintf(stderr, "[SDL3] POLYGON RENDER CALLED: vertices=%d color=0x%08x filled=%d\n",
            cmd->data.draw_polygon.vertex_count, cmd->data.draw_polygon.color, cmd->data.draw_polygon.filled);

    if (cmd->data.draw_polygon.vertex_count < 3) {
        fprintf(stderr, "[SDL3] POLYGON SKIPPED: Not enough vertices\n");
        return; // Need at least 3 vertices for a polygon
    }

    SDL_Color color = kryon_color_to_sdl(cmd->data.draw_polygon.color);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    fprintf(stderr, "[SDL3] POLYGON: Set color RGB(%d,%d,%d,%d)\n", color.r, color.g, color.b, color.a);

    // Use vertex_storage instead of vertices pointer to avoid dangling pointer issues
    const kryon_fp_t* vertices = cmd->data.draw_polygon.vertex_storage;
    uint16_t vertex_count = cmd->data.draw_polygon.vertex_count;

    // Debug: Log vertex data as received by renderer
    if (getenv("KRYON_TRACE_POLYGON")) {
        fprintf(stderr, "[kryon][sdl3] POLYGON RECEIVED: vertex_count=%d, vertices_ptr=%p\n",
                vertex_count, (void*)vertices);
        for (uint16_t i = 0; i < vertex_count; i++) {
            fprintf(stderr, "[kryon][sdl3]   received_vertex[%d]: raw=%d float=%.2f\n",
                    i, (int32_t)vertices[i * 2], kryon_fp_to_float(vertices[i * 2]));
            fprintf(stderr, "[kryon][sdl3]   received_vertex[%d]: raw=%d float=%.2f\n",
                    i, (int32_t)vertices[i * 2 + 1], kryon_fp_to_float(vertices[i * 2 + 1]));
        }
    }

    if (cmd->data.draw_polygon.filled) {
        fprintf(stderr, "[SDL3] POLYGON: Filled polygon path, will triangulate %d vertices\n", vertex_count);
        // Filled polygon - convert to triangles and render using SDL's geometry
        // Simple fan triangulation from vertex 0
        for (uint16_t i = 1; i < vertex_count - 1; i++) {
            fprintf(stderr, "[SDL3] POLYGON: Triangle %d/%d\n", i, vertex_count - 2);
            // Get triangle vertices with proper coordinate conversion
            // Use rounding for better precision
            float x0f = kryon_fp_to_float(vertices[0]);
            float y0f = kryon_fp_to_float(vertices[1]);
            float x1f = kryon_fp_to_float(vertices[i * 2]);
            float y1f = kryon_fp_to_float(vertices[i * 2 + 1]);
            float x2f = kryon_fp_to_float(vertices[(i + 1) * 2]);
            float y2f = kryon_fp_to_float(vertices[(i + 1) * 2 + 1]);
            fprintf(stderr, "[SDL3] POLYGON: Triangle coords: (%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f)\n",
                    x0f, y0f, x1f, y1f, x2f, y2f);

            // Convert to screen coordinates with rounding
            int16_t x0 = (int16_t)(x0f + 0.5f);
            int16_t y0 = (int16_t)(y0f + 0.5f);
            int16_t x1 = (int16_t)(x1f + 0.5f);
            int16_t y1 = (int16_t)(y1f + 0.5f);
            int16_t x2 = (int16_t)(x2f + 0.5f);
            int16_t y2 = (int16_t)(y2f + 0.5f);

            // Debug trace coordinates to help verify positioning
            if (getenv("KRYON_TRACE_POLYGON")) {
                fprintf(stderr, "[kryon][polygon] Triangle %d: (%d,%d), (%d,%d), (%d,%d) raw=[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]\n",
                        i - 1, x0, y0, x1, y1, x2, y2,
                        kryon_fp_to_float(vertices[0]), kryon_fp_to_float(vertices[1]),
                        kryon_fp_to_float(vertices[i * 2]), kryon_fp_to_float(vertices[i * 2 + 1]),
                        kryon_fp_to_float(vertices[(i + 1) * 2]), kryon_fp_to_float(vertices[(i + 1) * 2 + 1]));
            }

            // Fill the triangle using SDL_RenderGeometry with vertex data
            SDL_FPoint points[3] = {
                {(float)x0, (float)y0},
                {(float)x1, (float)y1},
                {(float)x2, (float)y2}
            };

            // Use SDL_RenderGeometry for filled triangles
            SDL_Vertex sdl_vertices[3] = {
                {{points[0].x, points[0].y}, {color.r, color.g, color.b, color.a}, {0, 0}},
                {{points[1].x, points[1].y}, {color.r, color.g, color.b, color.a}, {0, 0}},
                {{points[2].x, points[2].y}, {color.r, color.g, color.b, color.a}, {0, 0}}
            };

            fprintf(stderr, "[SDL3] POLYGON: Calling SDL_RenderGeometry with 3 vertices\n");
            // Draw filled triangle using geometry rendering
            int result = SDL_RenderGeometry(renderer, NULL, sdl_vertices, 3, NULL, 0);
            fprintf(stderr, "[SDL3] POLYGON: SDL_RenderGeometry returned %d\n", result);
        }
        fprintf(stderr, "[SDL3] POLYGON: Finished rendering filled polygon\n");
    } else {
        // Outline polygon - draw lines between consecutive vertices
        for (uint16_t i = 0; i < vertex_count; i++) {
            float x1f = kryon_fp_to_float(vertices[i * 2]);
            float y1f = kryon_fp_to_float(vertices[i * 2 + 1]);
            float x2f = kryon_fp_to_float(vertices[((i + 1) % vertex_count) * 2]);
            float y2f = kryon_fp_to_float(vertices[((i + 1) % vertex_count) * 2 + 1]);

            int16_t x1 = (int16_t)(x1f + 0.5f);
            int16_t y1 = (int16_t)(y1f + 0.5f);
            int16_t x2 = (int16_t)(x2f + 0.5f);
            int16_t y2 = (int16_t)(y2f + 0.5f);

            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }
    }
}

// ============================================================================
// SDL3 Renderer Operations
// ============================================================================

static bool sdl3_init(kryon_renderer_t* renderer, void* native_window) {
    if (renderer == NULL || renderer->backend_data == NULL) {
        return false;
    }

    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[kryon][sdl3] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    // Initialize SDL_ttf
    if (!TTF_Init()) {
        fprintf(stderr, "[kryon][sdl3] TTF_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // Initialize font system
    kryon_sdl3_fonts_init();

    // Create window if not provided
    if (native_window == NULL) {
        backend->window = SDL_CreateWindow(
            backend->config_title != NULL ? backend->config_title : "Kryon UI Framework",
            backend->config_width > 0 ? (int)backend->config_width : 800,
            backend->config_height > 0 ? (int)backend->config_height : 600,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );
        if (backend->window == NULL) {
            fprintf(stderr, "[kryon][sdl3] SDL_CreateWindow failed: %s\n", SDL_GetError());
            TTF_Quit();
            SDL_Quit();
            return false;
        }
    } else {
        backend->window = (SDL_Window*)native_window;
    }

    // Create renderer
    backend->renderer = SDL_CreateRenderer(backend->window, NULL);
    if (backend->renderer != NULL) {
        SDL_SetRenderVSync(backend->renderer, 1);  // Enable vsync
        SDL_SetRenderDrawBlendMode(backend->renderer, SDL_BLENDMODE_BLEND);  // Enable alpha blending for SDL_RenderGeometry
    }
    if (backend->renderer == NULL) {
        fprintf(stderr, "[kryon][sdl3] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        if (native_window == NULL) {
            SDL_DestroyWindow(backend->window);
        }
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Load default font
    const char* default_font_path = kryon_locate_default_font();
    if (default_font_path != NULL) {
        backend->default_font = TTF_OpenFont(default_font_path, 16);
        if (backend->default_font != NULL) {
            fprintf(stderr, "[kryon][sdl3] Loaded system font: %s\n", default_font_path);
        } else {
            fprintf(stderr, "[kryon][sdl3] Failed to load system font '%s': %s\n",
                    default_font_path, SDL_GetError());
        }
    }

    if (backend->default_font == NULL) {
        static const char* fallback_fonts[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "arial.ttf",
            "sans.ttf"
        };

        for (size_t i = 0; i < SDL_arraysize(fallback_fonts); ++i) {
            backend->default_font = TTF_OpenFont(fallback_fonts[i], 16);
            if (backend->default_font != NULL) {
                fprintf(stderr, "[kryon][sdl3] Loaded fallback font: %s\n", fallback_fonts[i]);
                break;
            }
        }

        if (backend->default_font == NULL) {
            fprintf(stderr, "[kryon][sdl3] Failed to load default font: %s\n", SDL_GetError());
        }
    }

    // Load monospace font for code
    const char* mono_font_path = kryon_locate_monospace_font();
    if (mono_font_path != NULL) {
        backend->monospace_font = TTF_OpenFont(mono_font_path, 16);
        if (backend->monospace_font != NULL) {
            fprintf(stderr, "[kryon][sdl3] Loaded monospace font: %s\n", mono_font_path);
        } else {
            fprintf(stderr, "[kryon][sdl3] Failed to load monospace font '%s': %s\n",
                    mono_font_path, SDL_GetError());
        }
    }

    if (backend->monospace_font == NULL) {
        // Fallback to default font for code if no monospace found
        fprintf(stderr, "[kryon][sdl3] Warning: No monospace font found, code will use default font\n");
    }

    // Get window dimensions
    int width, height;
    SDL_GetWindowSize(backend->window, &width, &height);
    backend->last_width = (uint16_t)width;
    backend->last_height = (uint16_t)height;
    renderer->width = backend->last_width;
    renderer->height = backend->last_height;

    // Set window reference for input system
    kryon_sdl3_input_set_window(backend->window);

    backend->window_id = SDL_GetWindowID(backend->window);
    backend->initialized = true;

    // Initialize clipping stack
    backend->clip_stack_depth = 0;
    backend->clip_enabled = false;

    // Set text measurement callback for accurate rich text layout
    renderer->measure_text = kryon_sdl3_measure_text_styled;
    fprintf(stderr, "[kryon][sdl3] Set measure_text callback: renderer=%p callback=%p\n",
            (void*)renderer, (void*)kryon_sdl3_measure_text_styled);

    return true;
}

static void sdl3_shutdown(kryon_renderer_t* renderer) {
    if (renderer == NULL || renderer->backend_data == NULL) {
        return;
    }

    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;

    if (backend->default_font != NULL) {
        TTF_CloseFont(backend->default_font);
        backend->default_font = NULL;
    }

    if (backend->monospace_font != NULL) {
        TTF_CloseFont(backend->monospace_font);
        backend->monospace_font = NULL;
    }

    if (backend->renderer != NULL) {
        SDL_DestroyRenderer(backend->renderer);
        backend->renderer = NULL;
    }

    if (backend->window != NULL) {
        SDL_DestroyWindow(backend->window);
        backend->window = NULL;
    }

    TTF_Quit();
    SDL_Quit();
    backend->initialized = false;
}

static void sdl3_begin_frame(kryon_renderer_t* renderer) {
    if (renderer == NULL || renderer->backend_data == NULL) {
        return;
    }

    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;
    if (backend->renderer == NULL) {
        return;
    }

    // Clear screen with clear color
    SDL_Color clear_color = kryon_color_to_sdl(renderer->clear_color);
    SDL_SetRenderDrawColor(backend->renderer, clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    SDL_RenderClear(backend->renderer);
}

static void sdl3_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf) {
    if (renderer == NULL || buf == NULL || renderer->backend_data == NULL) {
        return;
    }

    // Check if we have commands to execute
    int cmd_count = kryon_cmd_buf_count(buf);
    if (cmd_count == 0) {
        return;
    }

    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;
    if (backend->renderer == NULL) {
        return;
    }

    // Update dimensions if window changed
    uint16_t current_width, current_height;
    sdl3_get_dimensions(renderer, &current_width, &current_height);
    if (current_width != backend->last_width || current_height != backend->last_height) {
        backend->last_width = current_width;
        backend->last_height = current_height;
        renderer->width = current_width;
        renderer->height = current_height;
    }

    // Execute all commands in the buffer
    kryon_cmd_iterator_t iter = kryon_cmd_iter_create(buf);
    kryon_command_t cmd;

    const bool trace_enabled = kryon_sdl3_should_trace();
    int executed_count = 0;

    while (kryon_cmd_iter_has_next(&iter)) {
        executed_count++;
        if (kryon_cmd_iter_next(&iter, &cmd)) {
            if (trace_enabled) {
                kryon_sdl3_trace_command(&cmd);
            }

            switch (cmd.type) {
                case KRYON_CMD_DRAW_RECT:
                    sdl3_draw_rect(buf, &cmd, backend->renderer);
                    break;

                case KRYON_CMD_DRAW_TEXT:
                    sdl3_draw_text(buf, &cmd, backend->renderer, backend->default_font);
                    break;

                case KRYON_CMD_DRAW_LINE:
                    sdl3_draw_line(buf, &cmd, backend->renderer);
                    break;

                case KRYON_CMD_DRAW_ARC:
                    sdl3_draw_arc(buf, &cmd, backend->renderer);
                    break;

                case KRYON_CMD_DRAW_POLYGON:
                    fprintf(stderr, "[SDL3] EXECUTE: Processing POLYGON command\n");
                    sdl3_draw_polygon(buf, &cmd, backend->renderer);
                    break;

                case KRYON_CMD_SET_CLIP:
                    // Store the clip rectangle to be pushed later
                    backend->current_clip.x = cmd.data.set_clip.x;
                    backend->current_clip.y = cmd.data.set_clip.y;
                    backend->current_clip.w = cmd.data.set_clip.w;
                    backend->current_clip.h = cmd.data.set_clip.h;
                    break;

                case KRYON_CMD_PUSH_CLIP:
                    // Push current clip onto stack and activate it
                    if (backend->clip_stack_depth < 8) {
                        backend->clip_stack[backend->clip_stack_depth] = backend->current_clip;
                        backend->clip_stack_depth++;
                        SDL_SetRenderClipRect(backend->renderer, &backend->current_clip);
                        backend->clip_enabled = true;
                    } else {
                        fprintf(stderr, "[kryon][sdl3] Warning: Clip stack overflow (max 8 levels)\n");
                    }
                    break;

                case KRYON_CMD_POP_CLIP:
                    // Pop clip from stack and restore previous or disable
                    if (backend->clip_stack_depth > 0) {
                        backend->clip_stack_depth--;
                        if (backend->clip_stack_depth > 0) {
                            // Restore previous clip
                            SDL_Rect* prev_clip = &backend->clip_stack[backend->clip_stack_depth - 1];
                            SDL_SetRenderClipRect(backend->renderer, prev_clip);
                        } else {
                            // No more clips, disable clipping
                            SDL_SetRenderClipRect(backend->renderer, NULL);
                            backend->clip_enabled = false;
                        }
                    } else {
                        fprintf(stderr, "[kryon][sdl3] Warning: Clip stack underflow\n");
                    }
                    break;

                case KRYON_CMD_DRAW_ROUNDED_RECT: {
                    SDL_FRect rect = {
                        .x = cmd.data.draw_rounded_rect.x,
                        .y = cmd.data.draw_rounded_rect.y,
                        .w = cmd.data.draw_rounded_rect.w,
                        .h = cmd.data.draw_rounded_rect.h
                    };
                    sdl3_render_rounded_rect(backend->renderer, &rect,
                        cmd.data.draw_rounded_rect.radius,
                        cmd.data.draw_rounded_rect.color);
                    break;
                }

                case KRYON_CMD_DRAW_GRADIENT: {
                    SDL_FRect rect = {
                        .x = cmd.data.draw_gradient.x,
                        .y = cmd.data.draw_gradient.y,
                        .w = cmd.data.draw_gradient.w,
                        .h = cmd.data.draw_gradient.h
                    };

                    /* Convert gradient stops */
                    SDL3_GradientStop stops[8];
                    for (int i = 0; i < cmd.data.draw_gradient.stop_count && i < 8; i++) {
                        uint32_t color = cmd.data.draw_gradient.stops[i].color;
                        stops[i].position = cmd.data.draw_gradient.stops[i].position;
                        stops[i].r = (color >> 24) & 0xFF;
                        stops[i].g = (color >> 16) & 0xFF;
                        stops[i].b = (color >> 8) & 0xFF;
                        stops[i].a = color & 0xFF;
                    }

                    sdl3_render_gradient(backend->renderer, &rect,
                        (SDL3_GradientType)cmd.data.draw_gradient.gradient_type,
                        cmd.data.draw_gradient.angle,
                        stops, cmd.data.draw_gradient.stop_count, 1.0f);
                    break;
                }

                case KRYON_CMD_DRAW_SHADOW: {
                    SDL_FRect rect = {
                        .x = cmd.data.draw_shadow.x,
                        .y = cmd.data.draw_shadow.y,
                        .w = cmd.data.draw_shadow.w,
                        .h = cmd.data.draw_shadow.h
                    };
                    sdl3_render_shadow(backend->renderer, &rect,
                        cmd.data.draw_shadow.offset_x,
                        cmd.data.draw_shadow.offset_y,
                        cmd.data.draw_shadow.blur_radius,
                        cmd.data.draw_shadow.spread_radius,
                        cmd.data.draw_shadow.color,
                        cmd.data.draw_shadow.inset);
                    break;
                }

                case KRYON_CMD_DRAW_TEXT_SHADOW:
                    /* TODO: Implement text shadow */
                    break;

                case KRYON_CMD_DRAW_IMAGE:
                    /* TODO: Implement image rendering */
                    break;

                case KRYON_CMD_SET_OPACITY:
                    /* Opacity applied via render draw color alpha */
                    break;

                case KRYON_CMD_PUSH_OPACITY:
                    /* TODO: Implement opacity stack */
                    break;

                case KRYON_CMD_POP_OPACITY:
                    /* TODO: Implement opacity stack */
                    break;

                case KRYON_CMD_SET_BLEND_MODE:
                    /* TODO: Implement blend mode switching */
                    break;

                case KRYON_CMD_BEGIN_PASS:
                    /* Multi-pass rendering - currently no-op */
                    break;

                case KRYON_CMD_END_PASS:
                    /* Multi-pass rendering - currently no-op */
                    break;

                case KRYON_CMD_DRAW_TEXT_WRAPPED:
                    /* TODO: Implement wrapped text */
                    break;

                case KRYON_CMD_DRAW_TEXT_FADE:
                    /* TODO: Implement text fade effect */
                    break;

                case KRYON_CMD_DRAW_TEXT_STYLED:
                    /* TODO: Implement styled text */
                    break;

                case KRYON_CMD_LOAD_FONT:
                    /* TODO: Implement font loading */
                    break;

                case KRYON_CMD_LOAD_IMAGE:
                    /* TODO: Implement image loading */
                    break;

                default:
                    // Unsupported command type
                    break;
            }
        }
    }

    fprintf(stderr, "[SDL3] Executed %d commands\n", executed_count);
}

static void sdl3_end_frame(kryon_renderer_t* renderer) {
    // SDL3 doesn't need explicit end frame operations
    // Everything is handled in swap_buffers
}

static void sdl3_swap_buffers(kryon_renderer_t* renderer) {
    if (renderer == NULL || renderer->backend_data == NULL) {
        return;
    }

    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;
    if (backend->renderer != NULL) {
        SDL_RenderPresent(backend->renderer);
    }
}

static void sdl3_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height) {
    if (renderer == NULL || renderer->backend_data == NULL) {
        return;
    }

    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;
    if (backend->window != NULL) {
        int w, h;
        SDL_GetWindowSize(backend->window, &w, &h);
        if (width != NULL) *width = (uint16_t)w;
        if (height != NULL) *height = (uint16_t)h;
    }
}

static void sdl3_set_clear_color(kryon_renderer_t* renderer, uint32_t color) {
    if (renderer == NULL) {
        return;
    }
    renderer->clear_color = color;
}

// ============================================================================
// SDL3 Backend Operations Table
// ============================================================================

static const kryon_renderer_ops_t kryon_sdl3_ops = {
    .init = sdl3_init,
    .shutdown = sdl3_shutdown,
    .begin_frame = sdl3_begin_frame,
    .end_frame = sdl3_end_frame,
    .execute_commands = sdl3_execute_commands,
    .swap_buffers = sdl3_swap_buffers,
    .get_dimensions = sdl3_get_dimensions,
    .set_clear_color = sdl3_set_clear_color
};

// ============================================================================
// Public SDL3 Backend Functions
// ============================================================================

kryon_renderer_t* kryon_sdl3_renderer_create(uint16_t width, uint16_t height, const char* title) {
    // Allocate backend-specific data
    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)malloc(sizeof(kryon_sdl3_backend_t));
    if (backend == NULL) {
        return NULL;
    }

    memset(backend, 0, sizeof(kryon_sdl3_backend_t));

    // Store window configuration from DSL
    backend->config_width = width;
    backend->config_height = height;
    backend->owns_window = true;  // We create and own the window
    if (title != NULL) {
        backend->config_title = strdup(title);
    } else {
        backend->config_title = strdup("Kryon UI Framework");
    }

    // Create renderer with SDL3 operations
    kryon_renderer_t* renderer = kryon_renderer_create(&kryon_sdl3_ops);
    if (renderer == NULL) {
        if (backend->config_title != NULL) {
            free(backend->config_title);
        }
        free(backend);
        return NULL;
    }

    renderer->backend_data = backend;
    return renderer;
}

void kryon_sdl3_renderer_destroy(kryon_renderer_t* renderer) {
    if (renderer == NULL) {
        return;
    }

    if (renderer->backend_data != NULL) {
        kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;
        if (backend->config_title != NULL) {
            free(backend->config_title);
        }
        free(renderer->backend_data);
        renderer->backend_data = NULL;
    }

    kryon_renderer_destroy(renderer);
}

/**
 * Wrap an existing SDL_Renderer into a kryon_renderer_t.
 * This is used by the desktop renderer which manages its own SDL context.
 * The kryon_renderer_t does NOT take ownership of the SDL_Renderer*.
 */
kryon_renderer_t* kryon_sdl3_renderer_wrap_existing(SDL_Renderer* sdl_renderer, SDL_Window* window) {
    if (!sdl_renderer) return NULL;

    /* Allocate backend data */
    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)malloc(sizeof(kryon_sdl3_backend_t));
    if (!backend) return NULL;

    memset(backend, 0, sizeof(kryon_sdl3_backend_t));

    /* Store the existing SDL renderer (we don't own it) */
    backend->renderer = sdl_renderer;
    backend->window = window;
    backend->owns_window = false;  /* Important: we don't own this */

    /* Get dimensions from window if available */
    if (window) {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        backend->config_width = w;
        backend->config_height = h;
    }

    /* Create kryon renderer wrapper */
    kryon_renderer_t* renderer = kryon_renderer_create(&kryon_sdl3_ops);
    if (!renderer) {
        free(backend);
        return NULL;
    }

    renderer->backend_data = backend;
    return renderer;
}

// SDL3 event handling
bool kryon_sdl3_poll_event(kryon_event_t* event) {
    if (event == NULL) {
        return false;
    }

    SDL_Event sdl_event;
    if (!SDL_PollEvent(&sdl_event)) {
        return false;
    }

    // Update shared input state (mouse buttons, cursor) even if we don't emit an event.
    kryon_event_t internal_event = {0};
    kryon_sdl3_process_event(&sdl_event, &internal_event);

    // Convert SDL3 event to Kryon event
    switch (sdl_event.type) {
        case SDL_EVENT_QUIT:
            event->type = KRYON_EVT_CUSTOM;
            event->param = 0; // Quit event
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            event->type = KRYON_EVT_CLICK;
            event->x = (int16_t)sdl_event.button.x;
            event->y = (int16_t)sdl_event.button.y;
            uint32_t button = (uint32_t)(sdl_event.button.button & 0xFFFFu);
            event->param = button; // Removed state_flag since we only handle mouse up
            event->data = NULL;
            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
            event->type = KRYON_EVT_HOVER;
            event->x = (int16_t)sdl_event.motion.x;
            event->y = (int16_t)sdl_event.motion.y;
            event->param = 0;
            event->data = NULL;
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            event->type = KRYON_EVT_KEY;
            event->param = sdl_event.key.key;
            // Store key state in data pointer (simple encoding: pressed=1, released=0)
            event->data = (void*)(uintptr_t)(sdl_event.type == SDL_EVENT_KEY_DOWN ? 1 : 0);
            break;

        case SDL_EVENT_MOUSE_WHEEL: {
            // Get current mouse position
            float mouse_x, mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);

            event->type = KRYON_EVT_SCROLL;
            event->x = (int16_t)mouse_x;
            event->y = (int16_t)mouse_y;

            // Pack scroll delta into param (delta_x in upper 16 bits, delta_y in lower 16 bits)
            int16_t delta_x = (int16_t)sdl_event.wheel.x;
            int16_t delta_y = (int16_t)sdl_event.wheel.y;
            event->param = ((uint32_t)(uint16_t)delta_x << 16) | (uint16_t)delta_y;
            event->data = NULL;
            break;
        }

        default:
            return false; // Unhandled event type
    }

    event->timestamp = SDL_GetTicks();
    return true;
}

// ============================================================================
// Generic Platform Abstraction API Implementations
// ============================================================================

// Generic Event System Implementation
bool kryon_poll_event(kryon_event_t* event) {
    return kryon_sdl3_poll_event(event);
}

bool kryon_is_mouse_button_down(uint8_t button) {
    Uint32 state = SDL_GetMouseState(NULL, NULL);
    return (state & (1 << (button - 1))) != 0;  /* SDL3: Manual button bit checking */
}

void kryon_get_mouse_position(int16_t* x, int16_t* y) {
    float fx, fy;
    SDL_GetMouseState(&fx, &fy);
    *x = (int16_t)fx;
    *y = (int16_t)fy;
}

bool kryon_is_key_down(uint32_t key_code) {
    const bool* state = SDL_GetKeyboardState(NULL);  /* SDL3: Returns bool* not Uint8* */
    SDL_Keymod modstate;
    SDL_Scancode scancode = SDL_GetScancodeFromKey(key_code, &modstate);
    return state[scancode];
}

// Generic Font Management Implementation
void kryon_add_font_search_path(const char* path) {
    if (!path) return;

    // Add to SDL3 font search path
    // SDL_AddPath(path);  // TODO: SDL_AddPath doesn't exist in SDL3 - needs alternative solution

#if KRYON_HAS_FONTCONFIG
    // Also add to FontConfig if available
    FcConfigAppFontAddDir(FcConfigGetCurrent(), (FcChar8*)path);
#endif
}

uint16_t kryon_load_font(const char* name, uint16_t size) {
    // This is a simplified implementation - in a full system we'd need proper font loading/caching
    // For now, we return a basic font ID (0 = default, 1+ = loaded fonts)
    if (!name || strlen(name) == 0) {
        return 0; // Default font
    }

    // Try to load the font with SDL3_ttf
    TTF_Font* font = TTF_OpenFont(name, size);
    if (font) {
        TTF_CloseFont(font); // We don't keep the font open, just validate it can be loaded
        return 1; // Return font ID 1 for any successfully loaded font
    }

    return 0; // Fall back to default font
}

void kryon_get_font_metrics(uint16_t font_id, uint16_t* width, uint16_t* height) {
    // Simplified font metrics - in a full implementation we'd cache actual font data
    if (width) *width = 8;  // Approximate monospace width
    if (height) *height = 16; // Approximate height
}

// Screenshot Implementation
bool kryon_sdl3_save_screenshot(kryon_renderer_t* renderer, const char* path) {
    if (!renderer || !renderer->backend_data || !path) {
        return false;
    }

    kryon_sdl3_backend_t* backend = (kryon_sdl3_backend_t*)renderer->backend_data;
    if (!backend->renderer) {
        return false;
    }

    // Get renderer output size
    int width, height;
    if (!SDL_GetRenderOutputSize(backend->renderer, &width, &height)) {
        fprintf(stderr, "Failed to get render output size: %s\n", SDL_GetError());
        return false;
    }

    // Read pixels from renderer
    SDL_Surface* surface = SDL_RenderReadPixels(backend->renderer, NULL);
    if (!surface) {
        fprintf(stderr, "Failed to read pixels: %s\n", SDL_GetError());
        return false;
    }

    // Check file extension to determine format
    const char* ext = strrchr(path, '.');
    bool is_png = (ext && strcasecmp(ext, ".png") == 0);

    bool success = false;
    if (is_png) {
        // Use stb_image_write for PNG
        SDL_Surface* rgba_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (rgba_surface) {
            int stride = rgba_surface->w * 4;
            success = stbi_write_png(path, rgba_surface->w, rgba_surface->h, 4,
                                      rgba_surface->pixels, stride) != 0;
            SDL_DestroySurface(rgba_surface);
        }
        if (!success) {
            fprintf(stderr, "Failed to save PNG screenshot: %s\n", path);
        }
    } else {
        // Use SDL_SaveBMP for BMP files
        success = SDL_SaveBMP(surface, path);
        if (!success) {
            fprintf(stderr, "Failed to save screenshot: %s\n", SDL_GetError());
        }
    }

    SDL_DestroySurface(surface);
    return success;
}

// Generic Renderer Factory Implementation
kryon_renderer_t* kryon_create_renderer(uint16_t width, uint16_t height, const char* title) {
    return kryon_sdl3_renderer_create(width, height, title);
}
