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
#include "desktop_test_events.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_animation.h"
#include "../../ir/ir_hot_reload.h"
#include "../../ir/ir_style_vars.h"

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif

/* ============================================================================
 * FONT REGISTRATION API - Global resource registration
 * ============================================================================ */

void desktop_ir_register_font(const char* name, const char* path) {
    if (!name || !path) return;
#ifdef ENABLE_SDL3
    desktop_ir_register_font_internal(name, path);
#else
    // Font registration not implemented for other backends yet
    (void)name;
    (void)path;
#endif
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

    /* Initialize screenshot capture from environment variables */
    const char* screenshot_path = getenv("KRYON_SCREENSHOT");
    if (screenshot_path) {
        strncpy(renderer->screenshot_path, screenshot_path, sizeof(renderer->screenshot_path) - 1);
        renderer->screenshot_path[sizeof(renderer->screenshot_path) - 1] = '\0';

        const char* after_frames_str = getenv("KRYON_SCREENSHOT_AFTER_FRAMES");
        renderer->screenshot_after_frames = after_frames_str ? atoi(after_frames_str) : 5;
        renderer->frames_since_start = 0;
        renderer->screenshot_taken = false;

        printf("Screenshot capture enabled: %s (after %d frames)\n",
               renderer->screenshot_path, renderer->screenshot_after_frames);
    }

    /* Check for headless mode */
    const char* headless_env = getenv("KRYON_HEADLESS");
    if (headless_env && strcmp(headless_env, "1") == 0) {
        renderer->headless_mode = true;
        printf("Headless mode enabled\n");
    }

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
    if (renderer->config.backend_type == DESKTOP_BACKEND_SDL3) {
        /* Initialize SDL3 and TTF */
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
        return false;
    }

    printf("[renderer] SDL3 initialized successfully\n");

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
        return false;
    }

    printf("[renderer] Window created: %dx%d\n", renderer->config.window_width, renderer->config.window_height);

    /* Create renderer */
    renderer->renderer = SDL_CreateRenderer(renderer->window, NULL);
    if (!renderer->renderer) {
        fprintf(stderr, "\n╔════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║         Renderer Creation Failed                         ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════╝\n\n");
        fprintf(stderr, "Error: %s\n\n", SDL_GetError());
        fprintf(stderr, "Possible causes:\n");
        fprintf(stderr, "  • Graphics driver issues\n");
        fprintf(stderr, "  • Incompatible OpenGL version\n");
        fprintf(stderr, "  • Try software rendering: export SDL_RENDER_DRIVER=software\n\n");
        SDL_DestroyWindow(renderer->window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    printf("[renderer] SDL renderer created successfully\n");

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

    /* Register font metrics callbacks for IR layout system */
    desktop_register_font_metrics();

    /* Register text measurement callback for two-pass layout system */
    ir_layout_set_text_measure_callback(desktop_text_measure_callback);

        printf("Desktop renderer initialized successfully\n");
        return true;
    }
#endif

#ifdef ENABLE_RAYLIB
    if (renderer->config.backend_type == DESKTOP_BACKEND_RAYLIB) {
        return initialize_raylib_backend(renderer);
    }
#endif

    fprintf(stderr, "Error: Requested renderer backend not compiled in\n");
    fprintf(stderr, "Backend type: %d\n", renderer->config.backend_type);
    fprintf(stderr, "Available backends:\n");
#ifdef ENABLE_SDL3
    fprintf(stderr, "  - SDL3 (DESKTOP_BACKEND_SDL3 = 0)\n");
#endif
#ifdef ENABLE_RAYLIB
    fprintf(stderr, "  - Raylib (DESKTOP_BACKEND_RAYLIB = 1)\n");
#endif
    return false;
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

#ifdef ENABLE_RAYLIB
    if (renderer->config.backend_type == DESKTOP_BACKEND_RAYLIB) {
        shutdown_raylib_backend(renderer);
    }
#endif

    free(renderer);
}

/* ============================================================================
 * RENDERING - Frame rendering and main loop
 * ============================================================================ */

static int g_debug_frame_count = 0;

bool desktop_ir_renderer_render_frame(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root || !renderer->initialized) return false;

#ifdef ENABLE_SDL3
    if (renderer->config.backend_type == DESKTOP_BACKEND_SDL3) {
        renderer->blend_mode_set = false;
    g_frame_counter++;
    g_debug_frame_count++;

    // Clear needs_relayout flag after each frame (full redraw already happens every frame)
    renderer->needs_relayout = false;

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

    /* Compute layout using two-pass system before rendering */
    ir_layout_compute_tree(root, (float)window_width, (float)window_height);

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

    /* Render debug overlay if enabled */
    desktop_render_debug_overlay(renderer, root);

    /* Handle screenshot capture BEFORE present */
    if (renderer->screenshot_path[0] != '\0' && !renderer->screenshot_taken) {
        renderer->frames_since_start++;
        if (renderer->frames_since_start >= renderer->screenshot_after_frames) {
            desktop_save_screenshot(renderer, renderer->screenshot_path);
            renderer->screenshot_taken = true;

            /* Exit if headless mode is enabled */
            if (renderer->headless_mode) {
                printf("Headless mode: Screenshot taken, exiting\n");
                renderer->running = false;
            }
        }
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
    }
#endif

#ifdef ENABLE_RAYLIB
    if (renderer->config.backend_type == DESKTOP_BACKEND_RAYLIB) {
        /* Compute layout before rendering */
        ir_layout_compute_tree(root, (float)renderer->window_width, (float)renderer->window_height);

        return render_frame_raylib(renderer, root);
    }
#endif

    return false;
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
    // Initialize test event queue if test mode enabled
    TestEventQueue* test_queue = NULL;
    const char* test_mode = getenv("KRYON_TEST_MODE");
    const char* test_events_file = getenv("KRYON_TEST_EVENTS");
    if (test_mode && strcmp(test_mode, "1") == 0 && test_events_file) {
        test_queue = test_queue_init_from_file(test_events_file);
        if (!test_queue) {
            fprintf(stderr, "[renderer] Failed to load test events from %s\n", test_events_file);
        }
    }
    SDL_ResetKeyboard();
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    while (renderer->running) {
        // Process test events before SDL events
        if (test_queue) {
            test_queue_process(test_queue, renderer);

            // Stop if all test events completed and in headless mode
            if (!test_queue->enabled && renderer->headless_mode) {
                printf("[renderer] Test events completed in headless mode, exiting\n");
                renderer->running = false;
                break;
            }
        }

        handle_sdl3_events(renderer);

        /* Process reactive updates */
        if (nimProcessReactiveUpdates) {
            nimProcessReactiveUpdates();
        }

        /* Check for style variable changes */
        if (ir_style_vars_is_dirty()) {
            ir_style_vars_clear_dirty();
        }

        /* Hot reload polling */
        if (renderer->hot_reload_enabled && renderer->hot_reload_ctx) {
            ir_file_watcher_poll(ir_hot_reload_get_watcher(renderer->hot_reload_ctx), 0);
        }

        if (!desktop_ir_renderer_render_frame(renderer, renderer->last_root)) {
            printf("Frame rendering failed\n");
            break;
        }

        /* Frame rate limiting */
        if (renderer->config.target_fps > 0) {
            SDL_Delay(1000 / renderer->config.target_fps);
        } else {
            SDL_Delay(1);  // Minimum delay to allow event processing
        }
    }

    // Cleanup test event queue
    if (test_queue) {
        test_queue_free(test_queue);
    }
#endif

#ifdef ENABLE_RAYLIB
    // Raylib main loop
    while (renderer->running && !WindowShouldClose()) {
        /* Handle keyboard input */
        if (IsKeyPressed(KEY_ESCAPE)) {
            renderer->running = false;
            break;
        }

        /* Handle mouse input for hover/click */
        Vector2 mouse_pos = GetMousePosition();
        bool mouse_over_button = false;

        // Check if mouse is over any interactive component
        IRComponent* hovered = ir_find_component_at_point(renderer->last_root, mouse_pos.x, mouse_pos.y);
        if (hovered && (hovered->type == IR_COMPONENT_BUTTON || hovered->type == IR_COMPONENT_INPUT)) {
            mouse_over_button = true;
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }

        /* Process reactive updates */
        if (nimProcessReactiveUpdates) {
            nimProcessReactiveUpdates();
        }

        /* Check for style variable changes */
        if (ir_style_vars_is_dirty()) {
            ir_style_vars_clear_dirty();
        }

        /* Hot reload polling */
        if (renderer->hot_reload_enabled && renderer->hot_reload_ctx) {
            ir_file_watcher_poll(ir_hot_reload_get_watcher(renderer->hot_reload_ctx), 0);
        }

        if (!desktop_ir_renderer_render_frame(renderer, renderer->last_root)) {
            printf("Frame rendering failed\n");
            break;
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

void desktop_ir_renderer_set_lua_event_callback(DesktopIRRenderer* renderer,
                                                void (*callback)(uint32_t, int)) {
    if (renderer) {
        renderer->lua_event_callback = callback;
    }
}

void desktop_ir_renderer_update_root(DesktopIRRenderer* renderer, IRComponent* new_root) {
    if (renderer && new_root) {
        renderer->last_root = new_root;
        renderer->needs_relayout = true;
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

/**
 * Reload the root IR tree in the renderer.
 * Used for page navigation - swaps the current IR tree with a new one.
 * Preserves font and texture caches, but clears layout cache to force recalculation.
 */
void desktop_renderer_reload_tree(void* renderer_ptr, IRComponent* new_root) {
    if (!renderer_ptr || !new_root) return;

    DesktopIRRenderer* renderer = (DesktopIRRenderer*)renderer_ptr;

    // Update root component - layout will be recalculated on next frame
    renderer->last_root = new_root;

    if (getenv("KRYON_TRACE_LAYOUT")) {
        printf("📄 Page tree reloaded, layouts will be recalculated on next frame\n");
    }
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
