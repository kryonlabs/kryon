/*
 * Kryon Android Backend - IR to Renderer Bridge
 *
 * This file bridges the Kryon IR system with the Android renderer.
 * It loads KIR files and renders IR components using AndroidRenderer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "android_internal.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================

RegisteredFont g_font_registry[32];
int g_font_registry_count = 0;
char g_default_font_name[128] = "default";
char g_default_font_path[512] = "";

// ============================================================================
// FONT REGISTRATION
// ============================================================================

void android_ir_register_font_internal(const char* name, const char* path) {
    if (!name || !path || g_font_registry_count >= 32) return;

    RegisteredFont* font = &g_font_registry[g_font_registry_count++];
    strncpy(font->name, name, sizeof(font->name) - 1);
    strncpy(font->path, path, sizeof(font->path) - 1);
}

void android_ir_register_font(const char* name, const char* path) {
    if (!name || !path) return;
    android_ir_register_font_internal(name, path);
}

void android_ir_set_default_font(const char* name) {
    if (!name) return;

    for (int i = 0; i < g_font_registry_count; i++) {
        if (strcmp(g_font_registry[i].name, name) == 0) {
            strncpy(g_default_font_name, g_font_registry[i].name,
                    sizeof(g_default_font_name) - 1);
            strncpy(g_default_font_path, g_font_registry[i].path,
                    sizeof(g_default_font_path) - 1);
            return;
        }
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

AndroidIRRenderer* android_ir_renderer_create(const AndroidIRRendererConfig* config) {
    if (!config) return NULL;

    AndroidIRRenderer* ir_renderer = malloc(sizeof(AndroidIRRenderer));
    if (!ir_renderer) return NULL;

    memset(ir_renderer, 0, sizeof(AndroidIRRenderer));
    ir_renderer->config = *config;
    ir_renderer->window_width = config->window_width;
    ir_renderer->window_height = config->window_height;

    return ir_renderer;
}

bool android_ir_renderer_initialize(AndroidIRRenderer* ir_renderer,
                                    void* platform_context) {
    if (!ir_renderer || !platform_context) return false;

    ir_renderer->platform_context = platform_context;

    // Create AndroidRenderer
    AndroidRendererConfig renderer_config = {
        .window_width = ir_renderer->window_width,
        .window_height = ir_renderer->window_height,
        .vsync_enabled = true,
        .target_fps = 60,
        .debug_mode = false,
        .default_font_path = NULL,
        .default_font_size = 16,
        .enable_texture_cache = true,
        .texture_cache_size_mb = 64,
        .enable_glyph_cache = true,
        .glyph_cache_size_mb = 16
    };

    ir_renderer->renderer = android_renderer_create(&renderer_config);
    if (!ir_renderer->renderer) {
        return false;
    }

    // Initialize renderer (will be called from Java when surface is ready)
    // android_renderer_initialize() will be called separately

    // Register default fonts
    if (g_font_registry_count > 0 && g_default_font_path[0] != '\0') {
        android_renderer_register_font(ir_renderer->renderer,
                                       g_default_font_name,
                                       g_default_font_path);
        android_renderer_set_default_font(ir_renderer->renderer,
                                          g_default_font_name, 16);
    }

    // Initialize animation system if enabled
    if (ir_renderer->config.enable_animations) {
        ir_renderer->animation_ctx = ir_animation_context_create();
    }

    // Initialize hot reload if enabled
    // TODO: Hot reload not yet implemented for Android
    // if (ir_renderer->config.enable_hot_reload &&
    //     ir_renderer->config.hot_reload_watch_path) {
    //     ir_renderer->hot_reload_ctx = ir_hot_reload_create();
    //     ir_renderer->hot_reload_enabled = true;
    // }

    // Set renderer reference for text measurement
    android_layout_set_renderer(ir_renderer);

    // Register text measurement callback with IR layout system
    android_register_text_measurement();

    ir_renderer->initialized = true;
    ir_renderer->running = true;

    return true;
}

bool android_ir_renderer_load_kir(AndroidIRRenderer* ir_renderer,
                                  const char* kir_path) {
    if (!ir_renderer || !kir_path) return false;

    // Load KIR file (JSON format)
    FILE* f = fopen(kir_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open KIR file: %s\n", kir_path);
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read file into string buffer
    char* json_data = malloc(size + 1);
    if (!json_data) {
        fclose(f);
        return false;
    }
    fread(json_data, 1, size, f);
    json_data[size] = '\0';
    fclose(f);

    // Deserialize JSON
    IRComponent* root = ir_deserialize_json(json_data);
    free(json_data);

    if (!root) {
        fprintf(stderr, "Failed to deserialize KIR file: %s\n", kir_path);
        return false;
    }

    android_ir_renderer_set_root(ir_renderer, root);
    return true;
}

void android_ir_renderer_set_root(AndroidIRRenderer* ir_renderer,
                                  IRComponent* root) {
    if (!ir_renderer || !root) return;

    __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
        "=== SET ROOT ===");
    __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
        "Root: ptr=%p, type=%d, child_count=%d, children=%p",
        root, root->type, root->child_count, root->children);

    // Log children if present
    if (root->children && root->child_count > 0) {
        for (uint32_t i = 0; i < root->child_count; i++) {
            IRComponent* child = root->children[i];
            if (child) {
                uint32_t bg_color = child->style ?
                    (child->style->background.data.a << 24) | (child->style->background.data.r << 16) |
                    (child->style->background.data.g << 8) | child->style->background.data.b : 0;
                __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
                    "  Child[%d]: ptr=%p, type=%d, bg=0x%08x",
                    i, child, child->type, bg_color);
            } else {
                __android_log_print(ANDROID_LOG_WARN, "KryonRenderer",
                    "  Child[%d]: NULL!", i);
            }
        }
    } else {
        __android_log_print(ANDROID_LOG_WARN, "KryonRenderer",
            "Root has no children! (children=%p, child_count=%d)",
            root->children, root->child_count);
    }
    __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
        "=== END SET ROOT ===");

    ir_renderer->last_root = root;
    ir_renderer->needs_relayout = true;
}

void android_ir_renderer_update(AndroidIRRenderer* ir_renderer,
                                float delta_time) {
    if (!ir_renderer) return;

    // Update animations
    if (ir_renderer->animation_ctx) {
        ir_animation_update(ir_renderer->animation_ctx, delta_time);
    }

    // Check for hot reload
    // TODO: Hot reload not yet implemented for Android
    // if (ir_renderer->hot_reload_enabled && ir_renderer->hot_reload_ctx) {
    //     IRReloadResult result = ir_hot_reload_poll(ir_renderer->hot_reload_ctx, NULL, NULL);
    //     if (result.changed && result.new_root) {
    //         ir_renderer->last_root = result.new_root;
    //         ir_renderer->needs_relayout = true;
    //     }
    // }

    // Update FPS
    ir_renderer->frame_count++;
    double current_time = (double)clock() / CLOCKS_PER_SEC;
    double elapsed = current_time - ir_renderer->last_frame_time;
    if (elapsed > 0.0) {
        ir_renderer->fps = 1.0 / elapsed;
    }
    ir_renderer->last_frame_time = current_time;
}

void android_ir_renderer_render(AndroidIRRenderer* ir_renderer) {
    static int render_frame_count = 0;
    render_frame_count++;

    if (render_frame_count % 60 == 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "KryonRenderer", "Render frame %d", render_frame_count);
    }

    if (!ir_renderer || !ir_renderer->renderer) {
        __android_log_print(ANDROID_LOG_WARN, "KryonRenderer", "render: ir_renderer or renderer is NULL");
        return;
    }

    // Begin frame
    android_renderer_begin_frame(ir_renderer->renderer);

    // Render root component if available
    if (ir_renderer->last_root) {
        static void* last_root_ptr = NULL;
        static int last_child_count = -1;

        if (last_root_ptr != ir_renderer->last_root || last_child_count != ir_renderer->last_root->child_count) {
            __android_log_print(ANDROID_LOG_WARN, "KryonRenderer", "Root changed! ptr=%p->%p, children=%d->%d",
                              last_root_ptr, ir_renderer->last_root, last_child_count, ir_renderer->last_root->child_count);
            last_root_ptr = ir_renderer->last_root;
            last_child_count = ir_renderer->last_root->child_count;
        }

        // Compute layout using IR system if dirty
        if (ir_renderer->needs_relayout) {
            __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
                "=== LAYOUT COMPUTATION START ===");
            __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
                "Root: type=%d, child_count=%d, children=%p",
                ir_renderer->last_root->type,
                ir_renderer->last_root->child_count,
                ir_renderer->last_root->children);

            // Log each child before layout
            if (ir_renderer->last_root->children) {
                for (uint32_t i = 0; i < ir_renderer->last_root->child_count; i++) {
                    IRComponent* child = ir_renderer->last_root->children[i];
                    __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
                        "  Child[%d] BEFORE: ptr=%p, type=%d, has_layout_state=%d",
                        i, child, child ? child->type : -1,
                        child && child->layout_state ? 1 : 0);
                }
            }

            ir_layout_compute_tree(ir_renderer->last_root,
                                  (float)ir_renderer->window_width,
                                  (float)ir_renderer->window_height);

            __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
                "Layout complete (child_count=%d after layout)",
                ir_renderer->last_root->child_count);

            // Log each child after layout
            if (ir_renderer->last_root->children) {
                for (uint32_t i = 0; i < ir_renderer->last_root->child_count; i++) {
                    IRComponent* child = ir_renderer->last_root->children[i];
                    __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
                        "  Child[%d] AFTER: type=%d, layout_valid=%d, pos=(%.1f,%.1f), size=%.1fx%.1f",
                        i, child ? child->type : -1,
                        child && child->layout_state && child->layout_state->layout_valid ? 1 : 0,
                        child && child->layout_state ? child->layout_state->computed.x : -1,
                        child && child->layout_state ? child->layout_state->computed.y : -1,
                        child && child->layout_state ? child->layout_state->computed.width : -1,
                        child && child->layout_state ? child->layout_state->computed.height : -1);
                }
            }
            __android_log_print(ANDROID_LOG_INFO, "KryonRenderer",
                "=== LAYOUT COMPUTATION END ===");

            ir_renderer->needs_relayout = false;
        }

        // Render component tree - use wrapper to avoid pointer corruption
        __android_log_print(ANDROID_LOG_DEBUG, "KryonRenderer", "Rendering root ptr=%p, child_count=%d",
                          ir_renderer->last_root, ir_renderer->last_root->child_count);

        // WORKAROUND: Render inline to avoid ABI mismatch
        if (ir_renderer->last_root && ir_renderer->renderer) {
            // Manually traverse and render the tree
            extern void render_component_tree_inline(AndroidIRRenderer* ir_renderer);
            render_component_tree_inline(ir_renderer);
        }
    } else {
        __android_log_print(ANDROID_LOG_WARN, "KryonRenderer", "render: last_root is NULL!");
    }

    // End frame
    android_renderer_end_frame(ir_renderer->renderer);
}

void android_ir_renderer_handle_event(AndroidIRRenderer* ir_renderer,
                                      void* event) {
    if (!ir_renderer || !event) return;

    // TODO: Convert platform events to IR events
    // For now, just pass through
}

void android_ir_renderer_set_event_callback(AndroidIRRenderer* ir_renderer,
                                           AndroidEventCallback callback,
                                           void* user_data) {
    if (!ir_renderer) return;

    ir_renderer->event_callback = callback;
    ir_renderer->event_user_data = user_data;
}

void android_ir_renderer_destroy(AndroidIRRenderer* ir_renderer) {
    if (!ir_renderer) return;

    if (ir_renderer->animation_ctx) {
        ir_animation_context_destroy(ir_renderer->animation_ctx);
    }

    // TODO: Hot reload not yet implemented for Android
    // if (ir_renderer->hot_reload_ctx) {
    //     ir_hot_reload_destroy(ir_renderer->hot_reload_ctx);
    // }

    if (ir_renderer->renderer) {
        android_renderer_destroy(ir_renderer->renderer);
    }

    free(ir_renderer);
}

// ============================================================================
// PERFORMANCE
// ============================================================================

float android_ir_renderer_get_fps(AndroidIRRenderer* ir_renderer) {
    return ir_renderer ? (float)ir_renderer->fps : 0.0f;
}

uint64_t android_ir_renderer_get_frame_count(AndroidIRRenderer* ir_renderer) {
    return ir_renderer ? ir_renderer->frame_count : 0;
}
