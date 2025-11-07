#define _GNU_SOURCE
#include "../common/command_buf.c"
#include "../../core/include/kryon.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

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
    uint32_t window_id;
    bool initialized;
    uint16_t last_width, last_height;

    // Window configuration from DSL
    uint16_t config_width, config_height;
    char* config_title;
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

// SDL3 color conversion helper
static SDL_Color kryon_color_to_sdl(uint32_t kryon_color) {
    SDL_Color sdl_color;
    kryon_color_get_components(kryon_color, &sdl_color.r, &sdl_color.g, &sdl_color.b, &sdl_color.a);
    return sdl_color;
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

static void sdl3_draw_text(kryon_cmd_buf_t* buf, const kryon_command_t* cmd, SDL_Renderer* renderer, TTF_Font* font) {

    if (font == NULL || cmd->data.draw_text.text == NULL) {
        return;
    }

    SDL_Color color = kryon_color_to_sdl(cmd->data.draw_text.color);
    SDL_Surface* surface = TTF_RenderText_Blended(font, cmd->data.draw_text.text, strlen(cmd->data.draw_text.text), color);
    if (surface == NULL) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        SDL_DestroySurface(surface);
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

    while (kryon_cmd_iter_has_next(&iter)) {
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

                default:
                    // Unsupported command type
                    break;
            }
        }
    }
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

// SDL3 event handling
bool kryon_sdl3_poll_event(kryon_event_t* event) {
    if (event == NULL) {
        return false;
    }

    SDL_Event sdl_event;
    if (!SDL_PollEvent(&sdl_event)) {
        return false;
    }

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

        default:
            return false; // Unhandled event type
    }

    event->timestamp = SDL_GetTicks();
    return true;
}
