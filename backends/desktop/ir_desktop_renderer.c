/*
 * Kryon Desktop Renderer - Main API Implementation
 *
 * This file contains the public API for desktop rendering with SDL3.
 * It ties together the modular components:
 * - desktop_fonts.c for font management
 * - desktop_effects.c for visual effects
 * - desktop_layout.c for layout calculation
 * - desktop_rendering.c for component rendering
 * - desktop_input.c for event handling
 * - desktop_markdown.c for markdown rendering
 *
 * The renderer manages the window, animation system, hot reload, and main loop.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "desktop_internal.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_animation.h"
#include "../../ir/ir_hot_reload.h"
#include "../../ir/ir_style_vars.h"
#include "../../plugins/tilemap/tilemap_plugin.h"

// External plugins (weak linking - optional)
// These are installed separately and may not be available
extern bool kryon_markdown_plugin_init(void) __attribute__((weak));
extern void kryon_markdown_plugin_shutdown(void) __attribute__((weak));
extern bool kryon_canvas_plugin_init(void) __attribute__((weak));
extern void kryon_canvas_plugin_shutdown(void) __attribute__((weak));

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif

/* ============================================================================
 * FONT REGISTRATION API - Global resource registration
 * ============================================================================ */

void desktop_ir_register_font(const char* name, const char* path) {
    if (!name || !path) return;
    desktop_ir_register_font_internal(name, path);
}

void desktop_ir_set_default_font(const char* name) {
    if (!name) return;

    for (int i = 0; i < g_font_registry_count; i++) {
        if (strcmp(g_font_registry[i].name, name) == 0) {
            strncpy(g_default_font_name, g_font_registry[i].name, sizeof(g_default_font_name) - 1);
            strncpy(g_default_font_path, g_font_registry[i].path, sizeof(g_default_font_path) - 1);
            g_default_font_name[sizeof(g_default_font_name) - 1] = '\0';
            g_default_font_path[sizeof(g_default_font_path) - 1] = '\0';
            return;
        }
    }
}

/* ============================================================================
 * LIFECYCLE - Create, initialize, and destroy renderer
 * ============================================================================ */

DesktopIRRenderer* desktop_ir_renderer_create(const DesktopRendererConfig* config) {
    if (!config) return NULL;

    DesktopIRRenderer* renderer = malloc(sizeof(DesktopIRRenderer));
    if (!renderer) return NULL;

    memset(renderer, 0, sizeof(DesktopIRRenderer));
    renderer->config = *config;
    renderer->last_frame_time = 0;

    /* Initialize animation context */
    renderer->animation_ctx = ir_animation_context_create();
    if (!renderer->animation_ctx) {
        free(renderer);
        return NULL;
    }

    /* Initialize hot reload if enabled */
    const char* hot_reload_env = getenv("KRYON_HOT_RELOAD");
    if (hot_reload_env && strcmp(hot_reload_env, "1") == 0) {
        renderer->hot_reload_enabled = true;
        renderer->hot_reload_ctx = ir_hot_reload_create();
        if (renderer->hot_reload_ctx) {
            const char* source_file = getenv("KRYON_HOT_RELOAD_FILE");
            if (source_file) {
                ir_hot_reload_watch_file(renderer->hot_reload_ctx, source_file);
                printf("Hot reload enabled for: %s\n", source_file);
            }
        } else {
            renderer->hot_reload_enabled = false;
        }
    }

    printf("Creating desktop IR renderer with backend type: %d\n", config->backend_type);
    return renderer;
}

bool desktop_ir_renderer_initialize(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    g_debug_renderer = (getenv("KRYON_DEBUG_RENDERER") != NULL);

#ifdef ENABLE_SDL3
    if (renderer->config.backend_type != DESKTOP_BACKEND_SDL3) {
        printf("Only SDL3 backend is currently implemented\n");
        return false;
    }

    /* Initialize SDL3 and TTF */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Failed to initialize SDL3: %s\n", SDL_GetError());
        return false;
    }

    if (!TTF_Init()) {
        printf("Failed to initialize SDL_ttf: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    /* Flush stale events */
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    /* Create window */
    Uint32 window_flags = SDL_WINDOW_OPENGL;
    if (renderer->config.resizable) window_flags |= SDL_WINDOW_RESIZABLE;
    if (renderer->config.fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;

    renderer->window = SDL_CreateWindow(
        renderer->config.window_title,
        renderer->config.window_width,
        renderer->config.window_height,
        window_flags
    );

    if (!renderer->window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    /* Create renderer */
    renderer->renderer = SDL_CreateRenderer(renderer->window, NULL);
    if (!renderer->renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(renderer->window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    /* Configure rendering */
    if (renderer->config.vsync_enabled) {
        SDL_SetRenderVSync(renderer->renderer, 1);
    }

    /* Create 1x1 white texture for vertex coloring */
    renderer->white_texture = SDL_CreateTexture(renderer->renderer, SDL_PIXELFORMAT_RGBA8888,
                                               SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (renderer->white_texture) {
        uint32_t white_pixel = 0xFFFFFFFF;
        SDL_UpdateTexture(renderer->white_texture, NULL, &white_pixel, 4);
        SDL_SetTextureBlendMode(renderer->white_texture, SDL_BLENDMODE_BLEND);
    }

    /* Load default font */
    if (g_default_font_path[0] != '\0') {
        renderer->default_font = TTF_OpenFont(g_default_font_path, 16);
        if (renderer->default_font) {
            printf("Loaded default font (registered): %s\n", g_default_font_path);
        }
    }

    /* Try fontconfig if no default font yet */
    if (!renderer->default_font) {
        FILE* fc = popen("fc-match sans:style=Regular -f \"%{file}\"", "r");
        if (fc) {
            char fontconfig_path[512] = {0};
            if (fgets(fontconfig_path, sizeof(fontconfig_path), fc)) {
                size_t len = strlen(fontconfig_path);
                if (len > 0 && fontconfig_path[len-1] == '\n') {
                    fontconfig_path[len-1] = '\0';
                }
                renderer->default_font = TTF_OpenFont(fontconfig_path, 16);
                if (renderer->default_font) {
                    printf("Loaded font via fontconfig: %s\n", fontconfig_path);
                    strncpy(g_default_font_path, fontconfig_path, sizeof(g_default_font_path) - 1);
                    g_default_font_path[sizeof(g_default_font_path) - 1] = '\0';
                }
            }
            pclose(fc);
        }
    }

    /* Fallback font paths */
    if (!renderer->default_font) {
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
            renderer->default_font = TTF_OpenFont(font_paths[i], 16);
            if (renderer->default_font) {
                printf("Loaded font: %s\n", font_paths[i]);
                strncpy(g_default_font_path, font_paths[i], sizeof(g_default_font_path) - 1);
                g_default_font_path[sizeof(g_default_font_path) - 1] = '\0';
                break;
            }
        }
    }

    if (!renderer->default_font) {
        printf("Warning: Could not load any system font\n");
    }

    /* Initialize cursors */
    renderer->cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    renderer->cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    renderer->current_cursor = renderer->cursor_default;
    SDL_SetCursor(renderer->cursor_default);

    renderer->initialized = true;
    g_desktop_renderer = renderer;

    /* Initialize text cache hash table (Phase 1 optimization) */
    for (int i = 0; i < TEXT_CACHE_HASH_SIZE; i++) {
        g_text_cache_hash_table[i].cache_index = -1;
        g_text_cache_hash_table[i].next_index = -1;
    }
    for (int i = 0; i < TEXT_TEXTURE_CACHE_SIZE; i++) {
        g_text_texture_cache[i].valid = false;
        g_text_texture_cache[i].cache_index = i;
    }

    /* Initialize font cache hash table (Phase 1 optimization) */
    extern int g_font_cache_hash_table[];  // Defined in desktop_fonts.c
    for (int i = 0; i < 64; i++) {  // FONT_CACHE_HASH_SIZE = 64
        g_font_cache_hash_table[i] = -1;
    }

    /* Initialize tilemap plugin */
    if (!kryon_tilemap_plugin_init(renderer->renderer)) {
        printf("Warning: Tilemap plugin initialization failed\n");
    }

    /* Initialize external plugins (if linked) */
    if (kryon_markdown_plugin_init != NULL) {
        if (!kryon_markdown_plugin_init()) {
            printf("Warning: Markdown plugin initialization failed\n");
        }
    }

    if (kryon_canvas_plugin_init != NULL) {
        if (!kryon_canvas_plugin_init()) {
            printf("Warning: Canvas plugin initialization failed\n");
        }
    }

    printf("Desktop renderer initialized successfully\n");
    return true;

#else
    printf("SDL3 support not enabled at compile time\n");
    return false;
#endif
}

void desktop_ir_renderer_destroy(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    if (g_desktop_renderer == renderer) {
        g_desktop_renderer = NULL;
    }

#ifdef ENABLE_SDL3
    /* Clean up font cache */
    for (int i = 0; i < g_font_cache_count; i++) {
        if (g_font_cache[i].font) {
            TTF_CloseFont(g_font_cache[i].font);
            g_font_cache[i].font = NULL;
        }
    }
    g_font_cache_count = 0;

    /* Clean up text texture cache */
    for (int i = 0; i < TEXT_TEXTURE_CACHE_SIZE; i++) {
        if (g_text_texture_cache[i].valid && g_text_texture_cache[i].texture) {
            SDL_DestroyTexture(g_text_texture_cache[i].texture);
            g_text_texture_cache[i].texture = NULL;
            g_text_texture_cache[i].valid = false;
        }
    }

    /* Destroy animation and hot reload contexts */
    if (renderer->animation_ctx) {
        ir_animation_context_destroy(renderer->animation_ctx);
        renderer->animation_ctx = NULL;
    }

    if (renderer->hot_reload_ctx) {
        ir_hot_reload_destroy(renderer->hot_reload_ctx);
        renderer->hot_reload_ctx = NULL;
    }

    /* Shutdown canvas plugin */
    kryon_canvas_plugin_shutdown();

    /* Shutdown tilemap plugin */
    kryon_tilemap_plugin_shutdown();

    /* Shutdown external plugins (if linked) */
    if (kryon_markdown_plugin_shutdown != NULL) {
        kryon_markdown_plugin_shutdown();
    }

    /* Clean up SDL resources */
    if (renderer->cursor_hand) {
        SDL_DestroyCursor(renderer->cursor_hand);
    }
    if (renderer->cursor_default) {
        SDL_DestroyCursor(renderer->cursor_default);
    }
    if (renderer->white_texture) {
        SDL_DestroyTexture(renderer->white_texture);
    }
    if (renderer->default_font) {
        TTF_CloseFont(renderer->default_font);
    }
    if (renderer->renderer) {
        SDL_DestroyRenderer(renderer->renderer);
    }
    if (renderer->window) {
        SDL_DestroyWindow(renderer->window);
    }
#endif

    free(renderer);
}

/* ============================================================================
 * RENDERING - Frame rendering and main loop
 * ============================================================================ */

bool desktop_ir_renderer_render_frame(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root || !renderer->initialized) return false;

#ifdef ENABLE_SDL3
    renderer->blend_mode_set = false;
    g_frame_counter++;

    /* Calculate delta time for animations */
    double current_time = SDL_GetTicks() / 1000.0;
    float delta_time = (float)(current_time - renderer->last_frame_time);

    /* Update animation contexts */
    if (renderer->animation_ctx) {
        ir_animation_update(renderer->animation_ctx, delta_time);
    }
    ir_animation_tree_update(root, (float)current_time);

    /* Clear screen */
    if (root->style) {
        SDL_Color bg = ir_color_to_sdl(root->style->background);
        SDL_SetRenderDrawColor(renderer->renderer, bg.r, bg.g, bg.b, bg.a);
    } else {
        SDL_SetRenderDrawColor(renderer->renderer, 240, 240, 240, 255);
    }
    SDL_RenderClear(renderer->renderer);

    /* Get window size and render root component */
    int window_width = renderer->config.window_width;
    int window_height = renderer->config.window_height;
    SDL_GetWindowSize(renderer->window, &window_width, &window_height);

    LayoutRect root_rect = {
        .x = 0, .y = 0,
        .width = (float)window_width,
        .height = (float)window_height
    };

    render_component_sdl3(renderer, root, root_rect, 1.0f);

    /* Render dropdowns in second pass for correct z-index */
    #define MAX_OPEN_DROPDOWNS 10
    IRComponent* open_dropdowns[MAX_OPEN_DROPDOWNS];
    int dropdown_count = 0;
    collect_open_dropdowns(root, open_dropdowns, &dropdown_count, MAX_OPEN_DROPDOWNS);

    for (int i = 0; i < dropdown_count; i++) {
        render_dropdown_menu_sdl3(renderer, open_dropdowns[i]);
    }

    /* Present frame and update stats */
    SDL_RenderPresent(renderer->renderer);

    renderer->frame_count++;
    double fps_delta = current_time - renderer->last_frame_time;
    if (fps_delta > 0.1) {
        renderer->fps = renderer->frame_count / fps_delta;
        renderer->frame_count = 0;
    }

    renderer->last_frame_time = current_time;
    return true;

#else
    return false;
#endif
}

bool desktop_ir_renderer_run_main_loop(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    if (!renderer->initialized && !desktop_ir_renderer_initialize(renderer)) {
        return false;
    }

    printf("Starting desktop main loop\n");
    renderer->running = true;
    renderer->last_root = root;

#ifdef ENABLE_SDL3
    SDL_ResetKeyboard();
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    while (renderer->running) {
        handle_sdl3_events(renderer);

        /* Process reactive updates */
        nimProcessReactiveUpdates();

        /* Check for style variable changes */
        if (ir_style_vars_is_dirty()) {
            ir_style_vars_clear_dirty();
        }

        /* Hot reload polling */
        if (renderer->hot_reload_enabled && renderer->hot_reload_ctx) {
            ir_file_watcher_poll(ir_hot_reload_get_watcher(renderer->hot_reload_ctx), 0);
        }

        if (!desktop_ir_renderer_render_frame(renderer, root)) {
            printf("Frame rendering failed\n");
            break;
        }

        /* Frame rate limiting */
        if (renderer->config.target_fps > 0) {
            SDL_Delay(1000 / renderer->config.target_fps);
        }
    }
#endif

    printf("Desktop main loop ended\n");
    return true;
}

void desktop_ir_renderer_stop(DesktopIRRenderer* renderer) {
    if (renderer) {
        renderer->running = false;
    }
}


/* ============================================================================
 * EVENT HANDLING AND UTILITIES
 * ============================================================================ */

void desktop_ir_renderer_set_event_callback(DesktopIRRenderer* renderer,
                                          DesktopEventCallback callback,
                                          void* user_data) {
    if (renderer) {
        renderer->event_callback = callback;
        renderer->event_user_data = user_data;
    }
}

bool desktop_ir_renderer_load_font(DesktopIRRenderer* renderer, const char* font_path, float size) {
#ifdef ENABLE_SDL3
    if (!renderer || !font_path) return false;

    if (renderer->default_font) {
        TTF_CloseFont(renderer->default_font);
    }

    renderer->default_font = TTF_OpenFont(font_path, (int)size);
    return renderer->default_font != NULL;

#else
    return false;
#endif
}

bool desktop_ir_renderer_load_image(DesktopIRRenderer* renderer, const char* image_path) {
    printf("Image loading not yet implemented: %s\n", image_path);
    return false;
}

void desktop_ir_renderer_clear_resources(DesktopIRRenderer* renderer) {
    printf("Clearing desktop renderer resources\n");
}

/* ============================================================================
 * DEBUG AND VALIDATION
 * ============================================================================ */

bool desktop_ir_renderer_validate_ir(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    printf("Validating IR for desktop rendering\n");
    return ir_validate_component(root);
}

void desktop_ir_renderer_print_tree_info(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    printf("Desktop IR Component Tree Information:\n");
    ir_print_component_info(root, 0);
}

void desktop_ir_renderer_print_performance_stats(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    printf("Desktop Renderer Performance Stats:\n");
    printf("   FPS: %.1f\n", renderer->fps);
    printf("   Backend type: %d\n", renderer->config.backend_type);
    printf("   Window size: %dx%d\n", renderer->config.window_width, renderer->config.window_height);
    printf("   Running: %s\n", renderer->running ? "Yes" : "No");
}

/* ============================================================================
 * CONFIGURATION HELPERS
 * ============================================================================ */

DesktopRendererConfig desktop_renderer_config_default(void) {
    DesktopRendererConfig config = {0};
    config.backend_type = DESKTOP_BACKEND_SDL3;
    config.window_width = 800;
    config.window_height = 600;
    config.window_title = "Kryon Desktop Application";
    config.resizable = true;
    config.fullscreen = false;
    config.vsync_enabled = true;
    config.target_fps = 60;
    return config;
}

DesktopRendererConfig desktop_renderer_config_sdl3(int width, int height, const char* title) {
    DesktopRendererConfig config = desktop_renderer_config_default();
    config.window_width = width;
    config.window_height = height;
    config.window_title = title ? title : "Kryon Desktop Application";
    return config;
}

bool desktop_render_ir_component(IRComponent* root, const DesktopRendererConfig* config) {
    DesktopRendererConfig default_config = desktop_renderer_config_default();
    if (!config) config = &default_config;

    DesktopIRRenderer* renderer = desktop_ir_renderer_create(config);
    if (!renderer) return false;

    bool success = desktop_ir_renderer_run_main_loop(renderer, root);
    desktop_ir_renderer_destroy(renderer);
    return success;
}
