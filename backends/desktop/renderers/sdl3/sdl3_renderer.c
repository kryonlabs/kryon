#define _POSIX_C_SOURCE 200809L

/**
 * SDL3 Renderer - SDL3 Backend Implementation
 *
 * This file implements the SDL3 backend for the desktop renderer.
 * It provides lifecycle management, event handling, and frame rendering using SDL3.
 */

#include "sdl3_renderer.h"
#include "../../desktop_internal.h"
#include "../../ir_to_commands.h"
#include "../../../../renderers/sdl3/sdl3.h"
#include "../../../../ir/ir_core.h"
#include "../../../../ir/ir_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations from other SDL3 modules
extern void handle_sdl3_events(DesktopIRRenderer* renderer);
extern void desktop_register_font_metrics(void);
extern void desktop_text_measure_callback(const char* text, float font_size,
                                          float max_width, float* out_width, float* out_height);

// Forward declarations from desktop_platform.c
extern char g_default_font_name[128];
extern char g_default_font_path[512];

// External globals from desktop_globals.c
extern TextTextureCache g_text_texture_cache[TEXT_TEXTURE_CACHE_SIZE];
extern TextCacheHashBucket g_text_cache_hash_table[TEXT_CACHE_HASH_SIZE];
extern int g_font_cache_hash_table[];  // Defined in sdl3_fonts.c

// ============================================================================
// Helper: Get SDL3 Data from Renderer
// ============================================================================

SDL3RendererData* sdl3_get_data(DesktopIRRenderer* renderer) {
    if (!renderer || !renderer->ops) return NULL;
    return (SDL3RendererData*)renderer->ops->backend_data;
}

// ============================================================================
// Lifecycle Operations
// ============================================================================

static bool sdl3_initialize(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    // Allocate SDL3-specific data
    SDL3RendererData* data = calloc(1, sizeof(SDL3RendererData));
    if (!data) {
        fprintf(stderr, "[sdl3] Failed to allocate renderer data\n");
        return false;
    }
    renderer->ops->backend_data = data;

    // Initialize SDL3 and TTF
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║         SDL3 Initialization Failed                       ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n\n");
        fprintf(stderr, "Error: %s\n\n", SDL_GetError());
        fprintf(stderr, "Possible solutions:\n\n");
        fprintf(stderr, "  1. Install SDL3 library:\n");
        fprintf(stderr, "     • Ubuntu/Debian: sudo apt install libsdl3-dev\n");
        fprintf(stderr, "     • Fedora:        sudo dnf install SDL3-devel\n");
        fprintf(stderr, "     • Arch Linux:    sudo pacman -S sdl3\n");
        fprintf(stderr, "     • macOS:         brew install sdl3\n\n");
        fprintf(stderr, "  2. Check display environment:\n");
        fprintf(stderr, "     • X11:    echo $DISPLAY     (should show :0 or similar)\n");
        fprintf(stderr, "     • Wayland: echo $WAYLAND_DISPLAY\n\n");
        fprintf(stderr, "  3. For Wayland, try:\n");
        fprintf(stderr, "     export SDL_VIDEODRIVER=wayland\n\n");
        fprintf(stderr, "  4. For headless/CI environments:\n");
        fprintf(stderr, "     xvfb-run kryon run <file>\n\n");
        free(data);
        return false;
    }

    if (!TTF_Init()) {
        fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║         SDL_ttf Initialization Failed                    ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n\n");
        fprintf(stderr, "Error: %s\n\n", SDL_GetError());
        fprintf(stderr, "Possible solutions:\n\n");
        fprintf(stderr, "  1. Install SDL3_ttf library:\n");
        fprintf(stderr, "     • Ubuntu/Debian: sudo apt install libsdl3-ttf-dev\n");
        fprintf(stderr, "     • Fedora:        sudo dnf install SDL3_ttf-devel\n");
        fprintf(stderr, "     • Arch Linux:    sudo pacman -S sdl3_ttf\n");
        fprintf(stderr, "     • macOS:         brew install sdl3_ttf\n\n");
        SDL_Quit();
        free(data);
        return false;
    }

    printf("[sdl3] SDL3 initialized successfully\n");

    // Flush stale events
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    // Create window
    Uint32 window_flags = SDL_WINDOW_OPENGL;
    if (renderer->config.resizable) window_flags |= SDL_WINDOW_RESIZABLE;
    if (renderer->config.fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;

    data->window = SDL_CreateWindow(
        renderer->config.window_title,
        renderer->config.window_width,
        renderer->config.window_height,
        window_flags
    );

    if (!data->window) {
        fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║         Window Creation Failed                           ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n\n");
        fprintf(stderr, "Error: %s\n\n", SDL_GetError());
        fprintf(stderr, "Possible causes:\n");
        fprintf(stderr, "  • Display not available (check $DISPLAY)\n");
        fprintf(stderr, "  • Running in headless environment without Xvfb\n");
        fprintf(stderr, "  • Video driver issues\n\n");
        TTF_Quit();
        SDL_Quit();
        free(data);
        return false;
    }

    printf("[sdl3] Window created: %dx%d\n", renderer->config.window_width, renderer->config.window_height);

    // Create renderer
    data->renderer = SDL_CreateRenderer(data->window, NULL);
    if (!data->renderer) {
        fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║         Renderer Creation Failed                         ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n\n");
        fprintf(stderr, "Error: %s\n\n", SDL_GetError());
        fprintf(stderr, "Possible causes:\n");
        fprintf(stderr, "  • Graphics driver issues\n");
        fprintf(stderr, "  • Incompatible OpenGL version\n");
        fprintf(stderr, "  • Try software rendering: export SDL_RENDER_DRIVER=software\n\n");
        SDL_DestroyWindow(data->window);
        TTF_Quit();
        SDL_Quit();
        free(data);
        return false;
    }

    printf("[sdl3] SDL renderer created successfully\n");

    // Wrap SDL renderer in kryon_renderer_t for command buffer execution
    data->kryon_renderer = kryon_sdl3_renderer_wrap_existing(data->renderer, data->window);
    if (!data->kryon_renderer) {
        fprintf(stderr, "[sdl3] Failed to create kryon renderer wrapper\n");
        SDL_DestroyRenderer(data->renderer);
        SDL_DestroyWindow(data->window);
        TTF_Quit();
        SDL_Quit();
        free(data);
        return false;
    }
    printf("[sdl3] Kryon renderer wrapper created\n");

    // Configure rendering
    if (renderer->config.vsync_enabled) {
        SDL_SetRenderVSync(data->renderer, 1);
    }

    // Create 1x1 white texture for vertex coloring
    data->white_texture = SDL_CreateTexture(data->renderer, SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (data->white_texture) {
        uint32_t white_pixel = 0xFFFFFFFF;
        SDL_UpdateTexture(data->white_texture, NULL, &white_pixel, 4);
        SDL_SetTextureBlendMode(data->white_texture, SDL_BLENDMODE_BLEND);
    }

    // Load default font
    if (g_default_font_path[0] != '\0') {
        data->default_font = TTF_OpenFont(g_default_font_path, 16);
        if (data->default_font) {
            printf("[sdl3] Loaded default font (registered): %s\n", g_default_font_path);
        }
    }

    // Try fontconfig if no default font yet
    if (!data->default_font) {
        FILE* fc = popen("fc-match sans:style=Regular -f \"%{file}\"", "r");
        if (fc) {
            char fontconfig_path[512] = {0};
            if (fgets(fontconfig_path, sizeof(fontconfig_path), fc)) {
                size_t len = strlen(fontconfig_path);
                if (len > 0 && fontconfig_path[len-1] == '\n') {
                    fontconfig_path[len-1] = '\0';
                }
                data->default_font = TTF_OpenFont(fontconfig_path, 16);
                if (data->default_font) {
                    printf("[sdl3] Loaded font via fontconfig: %s\n", fontconfig_path);
                    strncpy(g_default_font_path, fontconfig_path, sizeof(g_default_font_path) - 1);
                    g_default_font_path[sizeof(g_default_font_path) - 1] = '\0';
                }
            }
            pclose(fc);
        }
    }

    // Fallback font paths
    if (!data->default_font) {
        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/System/Library/Fonts/Arial.ttf",
            "C:/Windows/Fonts/arial.ttf",
            NULL
        };

        for (int i = 0; font_paths[i]; i++) {
            data->default_font = TTF_OpenFont(font_paths[i], 16);
            if (data->default_font) {
                printf("[sdl3] Loaded font: %s\n", font_paths[i]);
                strncpy(g_default_font_path, font_paths[i], sizeof(g_default_font_path) - 1);
                g_default_font_path[sizeof(g_default_font_path) - 1] = '\0';
                break;
            }
        }
    }

    if (!data->default_font) {
        printf("[sdl3] Warning: Could not load any system font\n");
    }

    // Initialize cursors
    data->cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    data->cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    data->current_cursor = data->cursor_default;
    SDL_SetCursor(data->cursor_default);

    // Initialize text cache hash table
    for (int i = 0; i < TEXT_CACHE_HASH_SIZE; i++) {
        g_text_cache_hash_table[i].cache_index = -1;
        g_text_cache_hash_table[i].next_index = -1;
    }
    for (int i = 0; i < TEXT_TEXTURE_CACHE_SIZE; i++) {
        g_text_texture_cache[i].valid = false;
        g_text_texture_cache[i].cache_index = i;
    }

    // Initialize font cache hash table
    for (int i = 0; i < 64; i++) {  // FONT_CACHE_HASH_SIZE = 64
        g_font_cache_hash_table[i] = -1;
    }

    // Register font metrics callbacks for IR layout system
    desktop_register_font_metrics();

    // Register text measurement callback for two-pass layout system
    ir_layout_set_text_measure_callback(desktop_text_measure_callback);

    renderer->initialized = true;
    printf("[sdl3] Renderer initialized successfully\n");
    return true;
}

static void sdl3_shutdown(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    SDL3RendererData* data = sdl3_get_data(renderer);
    if (!data) return;

    // Destroy kryon renderer wrapper
    if (data->kryon_renderer) {
        kryon_sdl3_renderer_destroy(data->kryon_renderer);
        data->kryon_renderer = NULL;
    }

    // Clean up font cache
    extern CachedFont g_font_cache[64];
    extern int g_font_cache_count;
    for (int i = 0; i < g_font_cache_count; i++) {
        if (g_font_cache[i].font) {
            TTF_CloseFont(g_font_cache[i].font);
            g_font_cache[i].font = NULL;
        }
    }
    g_font_cache_count = 0;

    // Clean up text texture cache
    for (int i = 0; i < TEXT_TEXTURE_CACHE_SIZE; i++) {
        if (g_text_texture_cache[i].valid && g_text_texture_cache[i].texture) {
            SDL_DestroyTexture(g_text_texture_cache[i].texture);
            g_text_texture_cache[i].texture = NULL;
            g_text_texture_cache[i].valid = false;
        }
    }

    // Clean up SDL resources
    if (data->cursor_hand) {
        SDL_DestroyCursor(data->cursor_hand);
        data->cursor_hand = NULL;
    }
    if (data->cursor_default) {
        SDL_DestroyCursor(data->cursor_default);
        data->cursor_default = NULL;
    }
    if (data->white_texture) {
        SDL_DestroyTexture(data->white_texture);
        data->white_texture = NULL;
    }
    if (data->default_font) {
        TTF_CloseFont(data->default_font);
        data->default_font = NULL;
    }
    if (data->renderer) {
        SDL_DestroyRenderer(data->renderer);
        data->renderer = NULL;
    }
    if (data->window) {
        SDL_DestroyWindow(data->window);
        data->window = NULL;
    }

    TTF_Quit();
    SDL_Quit();

    free(data);
    renderer->ops->backend_data = NULL;

    printf("[sdl3] Renderer shutdown complete\n");
}

// ============================================================================
// Event Handling
// ============================================================================

static void sdl3_poll_events(DesktopIRRenderer* renderer) {
    if (!renderer) return;
    handle_sdl3_events(renderer);
}

// ============================================================================
// Frame Rendering
// ============================================================================

static bool sdl3_begin_frame(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    SDL3RendererData* data = sdl3_get_data(renderer);
    if (!data || !data->renderer) return false;

    // Reset blend mode flag
    data->blend_mode_set = false;

    // Clear screen with background color
    IRComponent* root = renderer->last_root;
    if (root && root->style) {
        // Use ir_color_to_sdl from desktop_effects.c
        extern SDL_Color ir_color_to_sdl(IRColor color);
        SDL_Color bg = ir_color_to_sdl(root->style->background);
        SDL_SetRenderDrawColor(data->renderer, bg.r, bg.g, bg.b, bg.a);
    } else {
        SDL_SetRenderDrawColor(data->renderer, 240, 240, 240, 255);
    }
    SDL_RenderClear(data->renderer);

    return true;
}

static bool sdl3_render_component(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    SDL3RendererData* data = sdl3_get_data(renderer);
    if (!data || !data->renderer) return false;

    // Get window size
    int window_width = renderer->config.window_width;
    int window_height = renderer->config.window_height;
    SDL_GetWindowSize(data->window, &window_width, &window_height);

    // Compute layout using two-pass system before rendering
    ir_layout_compute_tree(root, (float)window_width, (float)window_height);

    LayoutRect root_rect = {
        .x = 0, .y = 0,
        .width = (float)window_width,
        .height = (float)window_height
    };

    // Generate rendering commands from IR component tree
    kryon_cmd_buf_t cmd_buf;
    kryon_cmd_buf_init(&cmd_buf);

    // Create backend context for plugin renderers
    IRPluginBackendContext backend_ctx = {
        .renderer = data->renderer,
        .font = data->default_font,
        .user_data = NULL
    };

    if (!ir_component_to_commands(root, &cmd_buf, &root_rect, 1.0f, &backend_ctx)) {
        fprintf(stderr, "[sdl3] Failed to generate rendering commands\n");
        return false;
    }

    // Debug: Show command buffer stats
    fprintf(stderr, "[sdl3][CMD_BUF] Used %u bytes, head=%u, tail=%u, overflow=%d\n",
            cmd_buf.count, cmd_buf.head, cmd_buf.tail, cmd_buf.overflow);

    // Debug: Check buffer contents before execute
    fprintf(stderr, "[SDL3_PRE_EXEC] Buffer at pos 0: %02x %02x %02x %02x\n",
            cmd_buf.buffer[0], cmd_buf.buffer[1], cmd_buf.buffer[2], cmd_buf.buffer[3]);
    fprintf(stderr, "[SDL3_PRE_EXEC] Buffer at pos 1632: %02x %02x %02x %02x\n",
            cmd_buf.buffer[1632], cmd_buf.buffer[1633], cmd_buf.buffer[1634], cmd_buf.buffer[1635]);
    fflush(stderr);

    // Execute commands using kryon renderer backend
    if (data->kryon_renderer) {
        data->kryon_renderer->ops->execute_commands(data->kryon_renderer, &cmd_buf);
    } else {
        fprintf(stderr, "[sdl3] Warning: No kryon_renderer available for command execution\n");
        return false;
    }

    // Debug: Check buffer contents after execute
    fprintf(stderr, "[SDL3_POST_EXEC] Buffer at pos 0: %02x %02x %02x %02x\n",
            cmd_buf.buffer[0], cmd_buf.buffer[1], cmd_buf.buffer[2], cmd_buf.buffer[3]);
    fflush(stderr);

    return true;
}

static void sdl3_end_frame(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    SDL3RendererData* data = sdl3_get_data(renderer);
    if (!data || !data->renderer) return;

    // Handle screenshot capture BEFORE present
    if (renderer->screenshot_path[0] != '\0' && !renderer->screenshot_taken) {
        renderer->frames_since_start++;
        if (renderer->frames_since_start >= renderer->screenshot_after_frames) {
            kryon_sdl3_save_screenshot(data->kryon_renderer, renderer->screenshot_path);
            renderer->screenshot_taken = true;

            // Exit if headless mode is enabled
            if (renderer->headless_mode) {
                printf("[sdl3] Headless mode: Screenshot taken, exiting\n");
                renderer->running = false;
            }
        }
    }

    // Present frame
    SDL_RenderPresent(data->renderer);
}

// ============================================================================
// Operations Table
// ============================================================================

static DesktopRendererOps g_sdl3_ops = {
    .initialize = sdl3_initialize,
    .shutdown = sdl3_shutdown,
    .poll_events = sdl3_poll_events,
    .begin_frame = sdl3_begin_frame,
    .render_component = sdl3_render_component,
    .end_frame = sdl3_end_frame,
    .font_ops = NULL,           // Will be set by sdl3_fonts.c
    .effects_ops = NULL,        // Will be set by sdl3_effects.c
    .text_measure = NULL,       // Will be set by sdl3_layout.c
    .backend_data = NULL        // Set during initialization
};

DesktopRendererOps* desktop_sdl3_get_ops(void) {
    return &g_sdl3_ops;
}

// ============================================================================
// Backend Registration
// ============================================================================

/**
 * Register SDL3 backend (must be called explicitly before use)
 */
void sdl3_backend_register(void) {
    desktop_register_backend(DESKTOP_BACKEND_SDL3, &g_sdl3_ops);
    printf("[sdl3] Backend registered\n");
}
