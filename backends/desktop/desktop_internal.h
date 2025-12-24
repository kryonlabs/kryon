#ifndef DESKTOP_INTERNAL_H
#define DESKTOP_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_animation.h"
#include "../../ir/ir_hot_reload.h"
#include "../../ir/ir_style_vars.h"
#include "../../ir/ir_serialization.h"
#include "ir_desktop_renderer.h"

// Platform-specific includes (conditional compilation)
#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif

// ============================================================================
// SHARED TYPE DEFINITIONS
// ============================================================================

// Desktop IR Renderer Context (main renderer structure)
struct DesktopIRRenderer {
    DesktopRendererConfig config;
    bool initialized;
    bool running;

#ifdef ENABLE_SDL3
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* default_font;
    SDL_Cursor* cursor_default;
    SDL_Cursor* cursor_hand;
    SDL_Cursor* current_cursor;
    SDL_Texture* white_texture;  // 1x1 white texture for vertex coloring
    bool blend_mode_set;  // Track if blend mode already set this frame
#endif

    // Event handling
    DesktopEventCallback event_callback;
    void* event_user_data;

    // Lua event callback (for Lua FFI bindings)
    void (*lua_event_callback)(uint32_t component_id, int event_type);

    // Performance tracking
    uint64_t frame_count;
    double last_frame_time;
    double fps;

    // Component rendering cache
    IRComponent* last_root;
    bool needs_relayout;

    // Animation system
    IRAnimationContext* animation_ctx;

    // Hot reload system
    IRHotReloadContext* hot_reload_ctx;
    bool hot_reload_enabled;

    // Screenshot capture
    char screenshot_path[512];
    int screenshot_after_frames;
    int frames_since_start;
    bool screenshot_taken;
    bool headless_mode;
};

// Font registry entry
typedef struct {
    char name[128];
    char path[512];
} RegisteredFont;

// Cached font instance
typedef struct {
    char path[512];
    int size;
    TTF_Font* font;
} CachedFont;

// Text texture cache entry with LRU eviction
typedef struct {
    char text[256];           // Cached text content
    char font_path[512];      // Font path
    int font_size;            // Font size
    uint32_t color;           // RGBA color packed as uint32
    SDL_Texture* texture;     // Cached texture
    int width;                // Texture width
    int height;               // Texture height
    uint64_t last_access;     // Frame counter for LRU
    bool valid;               // Entry is valid
    int cache_index;          // Index in g_text_texture_cache array
} TextTextureCache;

// Hash table for O(1) text cache lookup (Phase 1 optimization)
#define TEXT_CACHE_HASH_SIZE 256
typedef struct {
    int cache_index;  // Index into g_text_texture_cache, or -1 if empty
    int next_index;   // For collision chaining, or -1 if end of chain
} TextCacheHashBucket;

// Layout rectangle helper
typedef struct LayoutRect {
    float x, y, width, height;
} LayoutRect;

// Text input runtime state
typedef struct {
    uint32_t id;
    size_t cursor_index;
    float scroll_x;
    bool focused;
    uint32_t last_blink_ms;
    bool caret_visible;
} InputRuntimeState;

// ============================================================================
// SHARED CONSTANTS
// ============================================================================

#define TEXT_CHAR_WIDTH_RATIO 0.6f
#define TEXT_LINE_HEIGHT_RATIO 1.2f
#define TEXT_TEXTURE_CACHE_SIZE 128

// ============================================================================
// GLOBAL STATE (shared across modules)
// ============================================================================

// Global renderer pointer (for layout measurement)
extern struct DesktopIRRenderer* g_desktop_renderer;

// Frame counter for LRU cache
extern uint64_t g_frame_counter;

// Font management
extern RegisteredFont g_font_registry[32];
extern int g_font_registry_count;
extern CachedFont g_font_cache[64];
extern int g_font_cache_count;
extern char g_default_font_name[128];
extern char g_default_font_path[512];

// Font path resolution cache (family, weight, italic) → path
#define FONT_PATH_CACHE_SIZE 128
typedef struct {
    char family[128];      // Font family name
    uint16_t weight;       // 100-900
    bool italic;           // Italic flag
    char path[512];        // Resolved font file path
    bool valid;            // Entry is valid
} FontPathCacheEntry;

extern FontPathCacheEntry g_font_path_cache[FONT_PATH_CACHE_SIZE];
extern int g_font_path_cache_count;

// Text texture cache
extern TextTextureCache g_text_texture_cache[TEXT_TEXTURE_CACHE_SIZE];
extern TextCacheHashBucket g_text_cache_hash_table[TEXT_CACHE_HASH_SIZE];

// Input states
extern InputRuntimeState input_states[64];
extern size_t input_state_count;

// Debug logging flag
extern bool g_debug_renderer;
#define DEBUG_LOG(...) do { if (g_debug_renderer) fprintf(stderr, __VA_ARGS__); } while(0)

// Nim bridge declarations (weak symbols for Lua compatibility)
__attribute__((weak)) void nimProcessReactiveUpdates(void);

// ============================================================================
// FONT MANAGEMENT (desktop_fonts.c)
// ============================================================================

#ifdef ENABLE_SDL3

// Font registration and resolution
void desktop_ir_register_font_internal(const char* name, const char* path);
const char* desktop_ir_find_font_path(const char* name_or_path);
TTF_Font* desktop_ir_get_cached_font(const char* path, int size);
TTF_Font* desktop_ir_resolve_font(DesktopIRRenderer* renderer, IRComponent* component, float fallback_size);

// Text measurement
float measure_text_width(TTF_Font* font, const char* text);

// Font metrics for IR layout system
void desktop_register_font_metrics(void);

// Text texture cache
uint32_t pack_color(SDL_Color color);
bool get_font_info(TTF_Font* font, const char** out_path, int* out_size);
TextTextureCache* text_texture_cache_lookup(const char* font_path, int font_size, const char* text, size_t text_len, SDL_Color color);
void text_texture_cache_insert(const char* font_path, int font_size, const char* text, size_t text_len,
                                SDL_Color color, SDL_Texture* texture, int width, int height);
SDL_Texture* get_text_texture_cached(SDL_Renderer* sdl_renderer, TTF_Font* font,
                                      const char* text, SDL_Color color, int* out_width, int* out_height);

// ============================================================================
// VISUAL EFFECTS (desktop_effects.c)
// ============================================================================

// Color utilities
SDL_Color ir_color_to_sdl(IRColor color);
void interpolate_color(IRGradientStop* from, IRGradientStop* to, float t, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
SDL_Color apply_opacity_to_color(SDL_Color color, float opacity);
void ensure_blend_mode(SDL_Renderer* renderer);

// Gradient rendering
int find_gradient_stop_binary(IRGradient* gradient, float t);
void render_linear_gradient(SDL_Renderer* renderer, IRGradient* gradient, SDL_FRect* rect, float opacity);
void render_radial_gradient(SDL_Renderer* renderer, IRGradient* gradient, SDL_FRect* rect, float opacity);
void render_conic_gradient(SDL_Renderer* renderer, IRGradient* gradient, SDL_FRect* rect, float opacity);
void render_gradient(SDL_Renderer* renderer, IRGradient* gradient, SDL_FRect* rect, float opacity);

// Shape rendering
void render_rounded_rect(SDL_Renderer* renderer, SDL_FRect* rect, float radius, SDL_Color color);

// Background rendering
void render_background_solid(SDL_Renderer* renderer, SDL_FRect* rect, IRColor bg_color, float opacity);
void render_background(SDL_Renderer* renderer, IRComponent* comp, SDL_FRect* rect, float opacity);

// Text effects
void render_text_with_shadow(SDL_Renderer* sdl_renderer, TTF_Font* font, const char* text,
                             SDL_Color color, IRComponent* comp, float x, float y, float max_width);
void render_text_with_fade(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                           SDL_FRect* rect, SDL_Color color, float fade_start_ratio);

// ============================================================================
// LAYOUT UTILITIES (desktop_layout.c)
// ============================================================================
// All layout computation is now in ir/ir_layout.c (two-pass system)

// Dimension conversion
float ir_dimension_to_pixels(IRDimension dimension, float container_size);

// Text measurement callback for IR layout system
#ifdef ENABLE_SDL3
void desktop_text_measure_callback(const char* text, float font_size,
                                   float max_width, float* out_width, float* out_height);
int wrap_text_into_lines(const char* text, float max_width, TTF_Font* font,
                         float font_size, char*** out_lines);
#endif

// ============================================================================
// INPUT HANDLING (desktop_input.c)
// ============================================================================

// Input utilities
void ensure_caret_visible(DesktopIRRenderer* renderer, IRComponent* input,
                          InputRuntimeState* istate, TTF_Font* font,
                          float pad_left, float pad_right);

// Input state management
InputRuntimeState* get_input_state(uint32_t id);

// Dropdown hit testing
void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count);
IRComponent* find_dropdown_menu_at_point(IRComponent* root, float x, float y);

// Dropdown menu rendering (rendered in second pass)
void render_dropdown_menu_sdl3(DesktopIRRenderer* renderer, IRComponent* component);

// Main event handling
void handle_sdl3_events(DesktopIRRenderer* desktop_renderer);

// ============================================================================
// MAIN RENDERING (desktop_rendering.c)
// ============================================================================

// Component rendering
bool render_component_sdl3(DesktopIRRenderer* renderer, IRComponent* component,
                           LayoutRect rect, float inherited_opacity);

// Screenshot capture
bool desktop_save_screenshot(DesktopIRRenderer* renderer, const char* path);

// Debug overlay
void desktop_render_debug_overlay(DesktopIRRenderer* renderer, IRComponent* root);

#endif // ENABLE_SDL3

#endif // DESKTOP_INTERNAL_H
