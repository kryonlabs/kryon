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
#include "desktop_platform.h"
#include "desktop_test_events.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_animation.h"

// External reference to global hover state (defined in desktop_globals.c)
extern IRComponent* g_hovered_component;

// Backend renderer headers for explicit registration
#ifdef ENABLE_SDL3
#include "renderers/sdl3/sdl3_renderer.h"
#endif
#ifdef ENABLE_RAYLIB
#include "renderers/raylib/raylib_renderer.h"
#endif
#include "../../ir/ir_hot_reload.h"
#include "../../ir/ir_style_vars.h"
#include "../../ir/ir_executor.h"

#ifdef ENABLE_SDL3
#include "ir_to_commands.h"
#include "../../renderers/sdl3/sdl3.h"
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

    // Register backends explicitly (static library constructors don't auto-run)
    static bool backends_registered = false;
    if (!backends_registered) {
        printf("[desktop_platform] Registering backends...\n");
#ifdef ENABLE_SDL3
        sdl3_backend_register();
#endif
#ifdef ENABLE_RAYLIB
        raylib_backend_register();
#endif
        backends_registered = true;
    }

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

    // Get ops table for requested backend
    renderer->ops = desktop_get_backend_ops(renderer->config.backend_type);
    if (!renderer->ops) {
        fprintf(stderr, "Error: Requested renderer backend not available\n");
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

    // Initialize via ops table
    if (!renderer->ops->initialize(renderer)) {
        fprintf(stderr, "Error: Backend initialization failed\n");
        return false;
    }

    // Set global renderer pointer
    g_desktop_renderer = renderer;

    printf("Desktop renderer initialized successfully\n");
    return true;
}

void desktop_ir_renderer_destroy(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    if (g_desktop_renderer == renderer) {
        g_desktop_renderer = NULL;
    }

    // Destroy animation and hot reload contexts
    if (renderer->animation_ctx) {
        ir_animation_context_destroy(renderer->animation_ctx);
        renderer->animation_ctx = NULL;
    }

    if (renderer->hot_reload_ctx) {
        ir_hot_reload_destroy(renderer->hot_reload_ctx);
        renderer->hot_reload_ctx = NULL;
    }

    // Shutdown via ops table
    if (renderer->ops && renderer->ops->shutdown) {
        renderer->ops->shutdown(renderer);
    }

    free(renderer);
}

/* ============================================================================
 * SHUTDOWN MANAGEMENT - Graceful shutdown with callbacks
 * ============================================================================ */

/**
 * Execute shutdown callbacks in priority order (highest first)
 * Returns false if any callback vetoes the shutdown (only for USER reason)
 */
static bool execute_shutdown_callbacks(DesktopIRRenderer* renderer) {
    if (!renderer || renderer->shutdown_callback_count == 0) return true;

    // Sort callbacks by priority (simple bubble sort - small array)
    for (int i = 0; i < renderer->shutdown_callback_count - 1; i++) {
        for (int j = 0; j < renderer->shutdown_callback_count - i - 1; j++) {
            if (renderer->shutdown_callbacks[j].priority < renderer->shutdown_callbacks[j + 1].priority) {
                // Swap - use individual field copies
                KryonShutdownCallback temp_cb = renderer->shutdown_callbacks[j].callback;
                void* temp_data = renderer->shutdown_callbacks[j].user_data;
                int temp_prio = renderer->shutdown_callbacks[j].priority;

                renderer->shutdown_callbacks[j].callback = renderer->shutdown_callbacks[j + 1].callback;
                renderer->shutdown_callbacks[j].user_data = renderer->shutdown_callbacks[j + 1].user_data;
                renderer->shutdown_callbacks[j].priority = renderer->shutdown_callbacks[j + 1].priority;

                renderer->shutdown_callbacks[j + 1].callback = temp_cb;
                renderer->shutdown_callbacks[j + 1].user_data = temp_data;
                renderer->shutdown_callbacks[j + 1].priority = temp_prio;
            }
        }
    }

    // Execute callbacks in priority order
    for (int i = 0; i < renderer->shutdown_callback_count; i++) {
        KryonShutdownCallback cb = renderer->shutdown_callbacks[i].callback;
        void* user_data = renderer->shutdown_callbacks[i].user_data;

        if (cb) {
            bool proceed = cb(renderer->shutdown_reason, user_data);

            // Only USER-initiated shutdowns can be vetoed
            if (!proceed && renderer->shutdown_reason == KRYON_SHUTDOWN_REASON_USER) {
                renderer->shutdown_state = KRYON_SHUTDOWN_RUNNING;
                renderer->shutdown_reason = KRYON_SHUTDOWN_REASON_NONE;
                return false;
            }
        }
    }

    return true;
}

/**
 * Request graceful shutdown
 * Transitions: RUNNING -> REQUESTED -> (callbacks) -> IN_PROGRESS
 */
bool kryon_request_shutdown_internal(DesktopIRRenderer* renderer, KryonShutdownReason reason) {
    if (!renderer) return false;

    // Already shutting down?
    if (renderer->shutdown_state != KRYON_SHUTDOWN_RUNNING) {
        return true;
    }

    renderer->shutdown_state = KRYON_SHUTDOWN_REQUESTED;
    renderer->shutdown_reason = reason;

    // Execute shutdown callbacks
    if (!execute_shutdown_callbacks(renderer)) {
        // Shutdown was vetoed
        return false;
    }

    // Advance to IN_PROGRESS state
    renderer->shutdown_state = KRYON_SHUTDOWN_IN_PROGRESS;
    renderer->running = false;

    return true;
}

/**
 * Register a shutdown callback
 * Callbacks are called in priority order (highest first) when shutdown is requested
 */
bool kryon_register_shutdown_callback_internal(DesktopIRRenderer* renderer,
                                                KryonShutdownCallback callback,
                                                void* user_data,
                                                int priority) {
    if (!renderer || !callback) return false;

    if (renderer->shutdown_callback_count >= KRYON_MAX_SHUTDOWN_CALLBACKS) {
        fprintf(stderr, "[kryon] Max shutdown callbacks reached (%d)\n",
                KRYON_MAX_SHUTDOWN_CALLBACKS);
        return false;
    }

    int idx = renderer->shutdown_callback_count++;
    renderer->shutdown_callbacks[idx].callback = callback;
    renderer->shutdown_callbacks[idx].user_data = user_data;
    renderer->shutdown_callbacks[idx].priority = priority;

    printf("[kryon] Registered shutdown callback (priority=%d, total=%d)\n",
           priority, renderer->shutdown_callback_count);
    return true;
}

/**
 * Set cleanup callback (called after main loop exits, before window closes)
 */
void kryon_set_cleanup_callback_internal(DesktopIRRenderer* renderer,
                                          KryonCleanupCallback callback,
                                          void* user_data) {
    if (!renderer) return;

    renderer->cleanup_callback = callback;
    renderer->cleanup_user_data = user_data;
    printf("[kryon] Cleanup callback set\n");
}

/**
 * Get current shutdown state
 */
KryonShutdownState kryon_get_shutdown_state_internal(DesktopIRRenderer* renderer) {
    if (!renderer) return KRYON_SHUTDOWN_COMPLETE;
    return renderer->shutdown_state;
}

/**
 * Check if renderer is shutting down
 */
bool kryon_is_shutting_down_internal(DesktopIRRenderer* renderer) {
    if (!renderer) return true;
    return renderer->shutdown_state != KRYON_SHUTDOWN_RUNNING;
}

/**
 * Complete shutdown process - call cleanup and mark complete
 * Called by main loop after exiting
 */
static void complete_shutdown(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    // Execute cleanup callback if set
    if (renderer->cleanup_callback) {
        printf("[kryon] Executing cleanup callback\n");
        renderer->cleanup_callback(renderer->cleanup_user_data);
    }

    renderer->shutdown_state = KRYON_SHUTDOWN_COMPLETE;
    printf("[kryon] Shutdown complete\n");
}

/* ============================================================================
 * CANVAS CALLBACKS - Invoke onDraw/onUpdate before rendering
 * ============================================================================ */

static void invoke_canvas_callbacks_recursive(DesktopIRRenderer* renderer, IRComponent* component) {
    if (!component) return;

    // Invoke onDraw callback for Canvas components
    if (component->type == IR_COMPONENT_CANVAS) {
        // Find canvas_draw event (plugin-registered event type)
        IREvent* draw_event = NULL;
        for (IREvent* evt = component->events; evt != NULL; evt = evt->next) {
            // Check if this is a canvas_draw event
            // Match both plugin events (type >= 100) and unregistered events treated as custom (type == 8)
            if (evt->event_name && strcmp(evt->event_name, "canvas_draw") == 0 &&
                ((evt->type >= IR_EVENT_PLUGIN_START && evt->type <= IR_EVENT_PLUGIN_END) ||
                 evt->type == IR_EVENT_CUSTOM)) {
                draw_event = evt;
                break;
            }
        }

        if (draw_event && draw_event->logic_id) {
            // Extract handler ID from logic_id (e.g., "lua_event_1" → 1)
            if (strncmp(draw_event->logic_id, "lua_event_", 10) == 0) {
                uint32_t handler_id = 0;
                if (sscanf(draw_event->logic_id + 10, "%u", &handler_id) == 1) {
                    // Dispatch through standard event callback
                    if (renderer->lua_event_callback) {
                        renderer->lua_event_callback(handler_id, draw_event->type, NULL);
                    }
                }
            }
        }
    }

    // Recurse into children
    for (int i = 0; i < component->child_count; i++) {
        invoke_canvas_callbacks_recursive(renderer, component->children[i]);
    }
}

static void invoke_canvas_update_callbacks_recursive(DesktopIRRenderer* renderer, IRComponent* component, double delta_time) {
    if (!component) return;

    // Invoke onUpdate callback for Canvas components
    if (component->type == IR_COMPONENT_CANVAS) {
        // Find canvas_update event (plugin-registered event type)
        IREvent* update_event = NULL;
        for (IREvent* evt = component->events; evt != NULL; evt = evt->next) {
            // Check if this is a canvas_update event
            // Match both plugin events (type >= 100) and unregistered events treated as custom (type == 8)
            if (evt->event_name && strcmp(evt->event_name, "canvas_update") == 0 &&
                ((evt->type >= IR_EVENT_PLUGIN_START && evt->type <= IR_EVENT_PLUGIN_END) ||
                 evt->type == IR_EVENT_CUSTOM)) {
                update_event = evt;
                break;
            }
        }

        if (update_event && update_event->logic_id) {
            // Extract handler ID from logic_id (e.g., "lua_event_1" → 1)
            if (strncmp(update_event->logic_id, "lua_event_", 10) == 0) {
                uint32_t handler_id = 0;
                if (sscanf(update_event->logic_id + 10, "%u", &handler_id) == 1) {
                    // Dispatch through standard event callback
                    if (renderer->lua_event_callback) {
                        renderer->lua_event_callback(handler_id, update_event->type, NULL);
                    }
                }
            }
        }
    }

    // Recurse into children
    for (int i = 0; i < component->child_count; i++) {
        invoke_canvas_update_callbacks_recursive(renderer, component->children[i], delta_time);
    }
}

/* ============================================================================
 * RENDERING - Frame rendering and main loop
 * ============================================================================ */

static int g_debug_frame_count = 0;

bool desktop_ir_renderer_render_frame(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root || !renderer->initialized) return false;
    if (!renderer->ops) return false;

    g_frame_counter++;
    renderer->needs_relayout = false;

    // Calculate delta time for animations (platform-agnostic timing)
    // Note: This uses a simple frame counter for now; backends can provide better timing
    static double last_time = 0;
    double current_time = (double)g_frame_counter / 60.0; // Assume 60 FPS for now
    float delta_time = (float)(current_time - last_time);
    last_time = current_time;

    // Update animation contexts
    if (renderer->animation_ctx) {
        ir_animation_update(renderer->animation_ctx, delta_time);
    }
    ir_animation_tree_update(root, (float)current_time);

    // Compute layout using two-pass system before rendering
    ir_layout_compute_tree(root, (float)renderer->config.window_width,
                           (float)renderer->config.window_height);

    // Invoke canvas onUpdate callbacks with delta time
    invoke_canvas_update_callbacks_recursive(renderer, root, delta_time);

    // Invoke canvas onDraw callbacks before rendering
    invoke_canvas_callbacks_recursive(renderer, root);

    // Render via ops table
    if (!renderer->ops->begin_frame(renderer)) {
        return false;
    }

    if (!renderer->ops->render_component(renderer, root)) {
        return false;
    }

    // Render dropdown menu overlays (second pass after main tree)
    // Collect all open dropdowns and render their menus on top
    extern void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count);
    extern void render_dropdown_menu_sdl3(DesktopIRRenderer* renderer, IRComponent* component);

    #define MAX_DROPDOWN_MENUS 10
    IRComponent* open_dropdowns[MAX_DROPDOWN_MENUS];
    int dropdown_count = 0;
    collect_open_dropdowns(root, open_dropdowns, &dropdown_count, MAX_DROPDOWN_MENUS);

    for (int i = 0; i < dropdown_count; i++) {
        render_dropdown_menu_sdl3(renderer, open_dropdowns[i]);
    }

    renderer->ops->end_frame(renderer);

    // Update frame stats
    renderer->frame_count++;
    renderer->last_frame_time = current_time;

    return true;
}

bool desktop_ir_renderer_run_main_loop(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    if (!renderer->initialized && !desktop_ir_renderer_initialize(renderer)) {
        return false;
    }

    if (!renderer->ops) {
        fprintf(stderr, "Error: No backend ops table available\n");
        return false;
    }

    printf("Starting desktop main loop\n");
    renderer->running = true;
    renderer->last_root = root;
    renderer->shutdown_state = KRYON_SHUTDOWN_RUNNING;
    renderer->shutdown_reason = KRYON_SHUTDOWN_REASON_NONE;

    int frame_count = 0;
    while (renderer->running && renderer->shutdown_state == KRYON_SHUTDOWN_RUNNING) {
        frame_count++;
        if (frame_count % 60 == 0) {
            printf("[MAIN_LOOP] Frame %d, running=%d, shutdown_state=%d\n",
                   frame_count, renderer->running, renderer->shutdown_state);
        }

        // Poll events via ops table
        if (renderer->ops->poll_events) {
            renderer->ops->poll_events(renderer);
        }

        // Check if poll_events triggered shutdown (window close button)
        if (!renderer->running && renderer->shutdown_state == KRYON_SHUTDOWN_RUNNING) {
            // Window was closed - initiate graceful shutdown
            kryon_request_shutdown_internal(renderer, KRYON_SHUTDOWN_REASON_WINDOW);
            break;
        }

        // Exit if shutdown in progress
        if (renderer->shutdown_state != KRYON_SHUTDOWN_RUNNING) {
            break;
        }

        // Process reactive updates (platform-agnostic)
        if (nimProcessReactiveUpdates) {
            nimProcessReactiveUpdates();
        }

        // Check for style variable changes
        if (ir_style_vars_is_dirty()) {
            ir_style_vars_clear_dirty();
        }

        // Hot reload polling
        if (renderer->hot_reload_enabled && renderer->hot_reload_ctx) {
            ir_file_watcher_poll(ir_hot_reload_get_watcher(renderer->hot_reload_ctx), 0);
        }

        // Render frame
        if (!desktop_ir_renderer_render_frame(renderer, renderer->last_root)) {
            printf("Frame rendering failed\n");
            kryon_request_shutdown_internal(renderer, KRYON_SHUTDOWN_REASON_ERROR);
            break;
        }

        // Simple frame rate limiting (could be backend-specific in future)
        // Note: Backends may implement their own FPS control in end_frame
        #ifdef _WIN32
        Sleep(1);
        #else
        usleep(1000);
        #endif
    }

    printf("Desktop main loop ended after %d frames (running=%d, shutdown_state=%d)\n",
           frame_count, renderer->running, renderer->shutdown_state);

    // Complete shutdown - execute cleanup callback before window closes
    complete_shutdown(renderer);

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
                                                void (*callback)(uint32_t, int, const char*)) {
    if (renderer) {
        renderer->lua_event_callback = callback;
    }
}

void desktop_ir_renderer_set_lua_canvas_draw_callback(DesktopIRRenderer* renderer,
                                                      void (*callback)(uint32_t)) {
    if (renderer) {
        renderer->lua_canvas_draw_callback = callback;
    }
}

void desktop_ir_renderer_set_lua_canvas_update_callback(DesktopIRRenderer* renderer,
                                                        void (*callback)(uint32_t, double)) {
    if (renderer) {
        renderer->lua_canvas_update_callback = callback;
    }
}

void desktop_ir_renderer_update_root(DesktopIRRenderer* renderer, IRComponent* new_root) {
    if (renderer && new_root) {
        renderer->last_root = new_root;
        renderer->needs_relayout = true;

        // Clear hover state when tree changes to prevent stale references
        g_hovered_component = NULL;
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
    config.backend_type = DESKTOP_BACKEND_SDL3;  // Default backend
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
