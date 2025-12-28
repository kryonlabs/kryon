#ifndef KRYON_SDL3_H
#define KRYON_SDL3_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../core/include/kryon.h"

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
// SDL3 Renderer API
// ============================================================================

// SDL3 renderer creation and management
kryon_renderer_t* kryon_sdl3_renderer_create(uint16_t width, uint16_t height, const char* title);
kryon_renderer_t* kryon_sdl3_renderer_wrap_existing(SDL_Renderer* sdl_renderer, SDL_Window* window);
void kryon_sdl3_renderer_destroy(kryon_renderer_t* renderer);

// SDL3 event handling
bool kryon_sdl3_poll_event(kryon_event_t* event);
bool kryon_sdl3_process_event(const SDL_Event* sdl_event, kryon_event_t* kryon_event);

// ============================================================================
// SDL3 Input System API
// ============================================================================

// Input system initialization
void kryon_sdl3_input_init(void);
void kryon_sdl3_input_shutdown(void);
void kryon_sdl3_input_set_window(SDL_Window* window);

// Mouse state
bool kryon_sdl3_is_mouse_button_down(uint8_t button);
void kryon_sdl3_get_mouse_position(int16_t* x, int16_t* y);

// Keyboard state
bool kryon_sdl3_is_key_down(SDL_Scancode scancode);
uint32_t kryon_sdl3_get_key_timestamp(SDL_Scancode scancode);

// Text input management
void kryon_sdl3_start_text_input(void);
void kryon_sdl3_stop_text_input(void);
const char* kryon_sdl3_get_composing_text(void);
void kryon_sdl3_set_composing_text(const char* text);
void kryon_sdl3_clear_composing_text(void);

// Advanced input features
bool kryon_sdl3_is_key_combo_down(SDL_Scancode key1, SDL_Scancode key2);
bool kryon_sdl3_is_key_combo_pressed(SDL_Scancode key1, SDL_Scancode key2,
                                      uint32_t current_time, uint32_t threshold_ms);

// Cursor management
void kryon_sdl3_set_cursor(SDL_SystemCursor cursor);
void kryon_sdl3_show_cursor(bool show);
void kryon_sdl3_apply_cursor_shape(uint8_t shape);

// Clipboard operations
bool kryon_sdl3_has_clipboard_text(void);
char* kryon_sdl3_get_clipboard_text(void);
bool kryon_sdl3_set_clipboard_text(const char* text);

// ============================================================================
// SDL3 Font Management API
// ============================================================================

// Font system initialization
void kryon_sdl3_fonts_init(void);
void kryon_sdl3_fonts_shutdown(void);

// Font loading and management
uint16_t kryon_sdl3_load_font(const char* name, uint16_t size);
void kryon_sdl3_unload_font(uint16_t font_id);
TTF_Font* kryon_sdl3_get_font(uint16_t font_id);

// Text measurement
void kryon_sdl3_measure_text(const char* text, uint16_t font_id, uint16_t* width, uint16_t* height);
void kryon_sdl3_measure_text_utf8(const char* text, uint16_t font_id, uint16_t* width, uint16_t* height);
void kryon_sdl3_measure_text_styled(const char* text, uint16_t font_size,
                                   uint8_t font_weight, uint8_t font_style,
                                   uint16_t font_id,
                                   uint16_t* width, uint16_t* height, void* user_data);

// Text rendering utilities
SDL_Texture* kryon_sdl3_render_text(SDL_Renderer* renderer, const char* text, uint16_t font_id, uint32_t color);
SDL_Texture* kryon_sdl3_render_text_wrapped(SDL_Renderer* renderer, const char* text, uint16_t font_id, uint32_t color, uint16_t wrap_width);

// Font information
bool kryon_sdl3_get_font_metrics(uint16_t font_id, uint16_t* ascent, uint16_t* descent,
                                 uint16_t* height, uint16_t* line_skip);
uint16_t kryon_sdl3_get_font_height(uint16_t font_id);
bool kryon_sdl3_glyph_provided(uint16_t font_id, uint16_t ch);

// Font path management
void kryon_sdl3_add_font_search_path(const char* path);
const char* kryon_sdl3_get_font_name(uint16_t font_id);
uint16_t kryon_sdl3_get_font_size(uint16_t font_id);

// ============================================================================
// High-Level Convenience Functions
// ============================================================================

// Complete SDL3 application setup
typedef struct {
    const char* window_title;
    uint16_t window_width;
    uint16_t window_height;
    uint32_t background_color;
    bool vsync_enabled;
    bool resizable;
} kryon_sdl3_app_config_t;

typedef struct {
    kryon_renderer_t* renderer;
    SDL_Window* window;
    bool running;
    uint16_t width, height;
} kryon_sdl3_app_t;

// High-level app management
kryon_sdl3_app_t* kryon_sdl3_app_create(const kryon_sdl3_app_config_t* config);
void kryon_sdl3_app_destroy(kryon_sdl3_app_t* app);
bool kryon_sdl3_app_handle_events(kryon_sdl3_app_t* app);
void kryon_sdl3_app_begin_frame(kryon_sdl3_app_t* app);
void kryon_sdl3_app_end_frame(kryon_sdl3_app_t* app);
void kryon_sdl3_app_render_component(kryon_sdl3_app_t* app, kryon_component_t* component);

// ============================================================================
// Constants and Configuration
// ============================================================================

// Default values
#define KRYON_SDL3_DEFAULT_WIDTH    800
#define KRYON_SDL3_DEFAULT_HEIGHT   600
#define KRYON_SDL3_DEFAULT_FPS      60
#define KRYON_SDL3_MAX_FONTS        16
#define KRYON_SDL3_FONT_PATH_BUFFER 512

// Event conversion utilities
#define KRYON_SDL3_KEY_TO_KRYON(sdl_key) ((uint32_t)(sdl_key))
#define KRYON_SDL3_MOUSE_TO_KRYON(sdl_button) ((uint32_t)(sdl_button))

#ifdef __cplusplus
}
#endif

#endif // KRYON_SDL3_H
