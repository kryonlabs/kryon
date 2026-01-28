/**
 * @file raylib_renderer.c
 * @brief Raylib Renderer Implementation
 * 
 * Raylib-based renderer following kryon-rust architecture patterns
 */

#include "renderer_interface.h"
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// =============================================================================
// RAYLIB SPECIFIC TYPES
// =============================================================================

typedef struct {
    int width;
    int height;
    const char* title;
    bool fullscreen;
    int target_fps;
} RaylibSurface;

typedef struct {
    Rectangle clip_stack[32];
    int clip_depth;
    Font current_font;
    bool has_custom_font;
} RaylibRenderContext;

typedef struct {
    int width;
    int height;
    Font default_font;
    bool initialized;
    bool window_should_close;
    
    // NEW: Event system integration
    KryonEventCallback event_callback;
    void* callback_data;
    
    // Mouse tracking for delta calculation
    Vector2 last_mouse_pos;
    bool mouse_tracking_initialized;
    
    // Cursor state tracking to prevent jittery behavior
    KryonCursorType current_cursor;
    bool cursor_initialized;
} RaylibRendererImpl;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonRenderResult raylib_initialize(void* surface);
static KryonRenderResult raylib_begin_frame(KryonRenderContext** context, KryonColor clear_color);
static KryonRenderResult raylib_end_frame(KryonRenderContext* context);
static KryonRenderResult raylib_execute_commands(KryonRenderContext* context,
                                                const KryonRenderCommand* commands,
                                                size_t command_count);
static KryonRenderResult raylib_resize(KryonVec2 new_size);
static KryonVec2 raylib_viewport_size(void);
static void raylib_destroy(void);

// Helper functions
static Color kryon_color_to_raylib(KryonColor color);
static void draw_rounded_rect_raylib(KryonRect rect, float radius, Color color);
static void draw_text_with_font_raylib(const char* text, KryonVec2 pos, 
                                      Font font, float font_size, Color color);
static int compare_z_index(const void* a, const void* b);

// =============================================================================
// GLOBAL STATE
// =============================================================================

static RaylibRendererImpl g_raylib_impl = {0};

// Forward declarations for input handling
static void raylib_process_input(RaylibRendererImpl* data);
static bool raylib_point_in_element(KryonVec2 point, KryonRect element_bounds);
static bool raylib_handle_event(const KryonEvent* event);
static void* raylib_get_native_window(void);
static float raylib_measure_text_width(const char* text, float font_size);
static KryonRenderResult raylib_set_cursor(KryonCursorType cursor_type);
static KryonRenderResult raylib_update_window_size(int width, int height);
static KryonRenderResult raylib_update_window_title(const char* title);

static KryonRendererVTable g_raylib_vtable = {
    .initialize = raylib_initialize,
    .begin_frame = raylib_begin_frame,
    .end_frame = raylib_end_frame,
    .execute_commands = raylib_execute_commands,
    .resize = raylib_resize,
    .viewport_size = raylib_viewport_size,
    .destroy = raylib_destroy,
    .point_in_element = raylib_point_in_element,
    .handle_event = raylib_handle_event,
    .get_native_window = raylib_get_native_window,
    .measure_text_width = raylib_measure_text_width,
    .set_cursor = raylib_set_cursor,
    .update_window_size = raylib_update_window_size,
    .update_window_title = raylib_update_window_title
};

// =============================================================================
// PUBLIC API
// =============================================================================

KryonRenderer* kryon_raylib_renderer_create(const KryonRendererConfig* config) {
    if (!config) {
        return NULL;
    }
    
    KryonRenderer* renderer = malloc(sizeof(KryonRenderer));
    if (!renderer) {
        return NULL;
    }
    
    // Set event callback in global impl
    g_raylib_impl.event_callback = config->event_callback;
    g_raylib_impl.callback_data = config->callback_data;
    g_raylib_impl.mouse_tracking_initialized = false;
    
    renderer->vtable = &g_raylib_vtable;
    renderer->impl_data = &g_raylib_impl;
    renderer->name = strdup("Raylib Renderer");
    renderer->backend = strdup("raylib");
    
    // Initialize with platform context from config
    if (raylib_initialize(config->platform_context) != KRYON_RENDER_SUCCESS) {
        free(renderer->name);
        free(renderer->backend);
        free(renderer);
        return NULL;
    }
    
    return renderer;
}

// Factory function for renderer system
KryonRenderer* kryon_renderer_create_raylib(void) {
    // Create default config for backward compatibility
    KryonRendererConfig config = {
        .event_callback = NULL,
        .callback_data = NULL,
        .platform_context = NULL
    };
    return kryon_raylib_renderer_create(&config);
}

// =============================================================================
// RENDERER IMPLEMENTATION
// =============================================================================

static KryonRenderResult raylib_initialize(void* surface) {
    RaylibSurface* surf = (RaylibSurface*)surface;
    if (!surf) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    // Set configuration flags before InitWindow
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    if (surf->fullscreen) {
        SetConfigFlags(FLAG_FULLSCREEN_MODE);
    }
    
    // Initialize Raylib window
    InitWindow(surf->width, surf->height, surf->title ? surf->title : "Kryon Application");
    
    if (!IsWindowReady()) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    // Set target FPS
    // DISABLED: SetTargetFPS(surf->target_fps > 0 ? surf->target_fps : 60);
    // ^ SetTargetFPS causes segfault in this environment - skip it for now
    
    // Load default font (Raylib has a built-in default font)
    g_raylib_impl.default_font = GetFontDefault();
    
    g_raylib_impl.width = surf->width;
    g_raylib_impl.height = surf->height;
    g_raylib_impl.initialized = true;
    g_raylib_impl.window_should_close = false;
    
    // Initialize cursor state
    g_raylib_impl.current_cursor = KRYON_CURSOR_DEFAULT;
    g_raylib_impl.cursor_initialized = false;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult raylib_begin_frame(KryonRenderContext** context, KryonColor clear_color) {
    if (!g_raylib_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    // Process input events FIRST
    raylib_process_input(&g_raylib_impl);
    
    // Check if window should close
    if (WindowShouldClose()) {
        g_raylib_impl.window_should_close = true;
        return KRYON_RENDER_ERROR_SURFACE_LOST;
    }
    
    RaylibRenderContext* ctx = malloc(sizeof(RaylibRenderContext));
    if (!ctx) {
        return KRYON_RENDER_ERROR_OUT_OF_MEMORY;
    }
    
    ctx->clip_depth = 0;
    ctx->current_font = g_raylib_impl.default_font;
    ctx->has_custom_font = false;
    
    // Begin drawing
    BeginDrawing();
    
    // Clear screen with specified color
    Color bg = kryon_color_to_raylib(clear_color);
    ClearBackground(bg);
    
    *context = (KryonRenderContext*)ctx;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult raylib_end_frame(KryonRenderContext* context) {
    if (!context) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    // End drawing and present
    EndDrawing();
    
    // Check if window should close
    if (WindowShouldClose()) {
        printf("🔲 Window close requested\n");
        free(context);
        return KRYON_RENDER_ERROR_WINDOW_CLOSED;  // Signal window closed
    }
    
    free(context);
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult raylib_execute_commands(KryonRenderContext* context,
                                                const KryonRenderCommand* commands,
                                                size_t command_count) {
    if (!context || !commands) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    RaylibRenderContext* ctx = (RaylibRenderContext*)context;
    
    // Create a copy of commands array for z-index sorting
    KryonRenderCommand* sorted_commands = malloc(command_count * sizeof(KryonRenderCommand));
    if (!sorted_commands) {
        return KRYON_RENDER_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy commands to sortable array
    for (size_t i = 0; i < command_count; i++) {
        sorted_commands[i] = commands[i];
    }
    
    // Sort by z-index (ascending order - lowest z-index renders first)
    qsort(sorted_commands, command_count, sizeof(KryonRenderCommand), compare_z_index);
    
    for (size_t i = 0; i < command_count; i++) {
        const KryonRenderCommand* cmd = &sorted_commands[i];
        
        switch (cmd->type) {
            case KRYON_CMD_SET_CANVAS_SIZE: {
                // Canvas size is handled during initialization
                break;
            }
            
            case KRYON_CMD_DRAW_RECT: {
                const KryonDrawRectData* data = &cmd->data.draw_rect;
                
                Color color = kryon_color_to_raylib(data->color);
                
                if (data->border_radius > 0) {
                    KryonRect rect = {
                        data->position.x, data->position.y,
                        data->size.x, data->size.y
                    };
                    draw_rounded_rect_raylib(rect, data->border_radius, color);
                } else {
                    DrawRectangle(
                        (int)data->position.x, (int)data->position.y,
                        (int)data->size.x, (int)data->size.y,
                        color
                    );
                }
                
                // Draw border if specified
                if (data->border_width > 0) {
                    Color border_color = kryon_color_to_raylib(data->border_color);
                    DrawRectangleLinesEx(
                        (Rectangle){
                            data->position.x, data->position.y,
                            data->size.x, data->size.y
                        },
                        data->border_width,
                        border_color
                    );
                }
                break;
            }
            
            case KRYON_CMD_DRAW_TEXT: {
                const KryonDrawTextData* data = &cmd->data.draw_text;
                
                if (data->text) {
                    Color color = kryon_color_to_raylib(data->color);
                    
                    // Calculate position based on text alignment
                    Vector2 text_size = MeasureTextEx(ctx->current_font, data->text, 
                                                     data->font_size, 1.0f);
                    Vector2 final_pos = {data->position.x, data->position.y};
                    
                    // Adjust position based on alignment (text_align: 0=left, 1=center, 2=right)
                    if (data->text_align == 1) { // center
                        final_pos.x -= text_size.x / 2.0f;
                        final_pos.y -= text_size.y / 2.0f;
                    } else if (data->text_align == 2) { // right
                        final_pos.x -= text_size.x;
                    }
                    // text_align == 0 (left) uses original position
                    
                    DrawTextEx(ctx->current_font, data->text, final_pos, 
                              data->font_size, 1.0f, color);
                }
                break;
            }
            
            case KRYON_CMD_DRAW_IMAGE: {
                const KryonDrawImageData* data = &cmd->data.draw_image;
                
                if (data->source) {
                    // Load texture (Raylib caches textures automatically)
                    Texture2D texture = LoadTexture(data->source);
                    if (texture.id > 0) {
                        Rectangle src_rect = {0, 0, (float)texture.width, (float)texture.height};
                        Rectangle dst_rect = {
                            data->position.x, data->position.y,
                            data->size.x, data->size.y
                        };
                        
                        Color tint = WHITE;
                        tint.a = (unsigned char)(data->opacity * 255);
                        
                        DrawTexturePro(texture, src_rect, dst_rect, (Vector2){0, 0}, 0.0f, tint);
                        
                        // Note: In a real implementation, you'd want to manage texture lifecycle
                        UnloadTexture(texture);
                    }
                }
                break;
            }
            
            case KRYON_CMD_DRAW_BUTTON: {
                const KryonDrawButtonData* data = &cmd->data.draw_button;
                
                // Determine colors based on state
                Color bg_color = kryon_color_to_raylib(data->background_color);
                Color text_color = kryon_color_to_raylib(data->text_color);
                Color border_color = kryon_color_to_raylib(data->border_color);
                
                switch (data->state) {
                    case KRYON_ELEMENT_STATE_HOVERED:
                        bg_color.r = (unsigned char)(bg_color.r * 0.9f);
                        bg_color.g = (unsigned char)(bg_color.g * 0.9f);
                        bg_color.b = (unsigned char)(bg_color.b * 0.9f);
                        break;
                    case KRYON_ELEMENT_STATE_PRESSED:
                        bg_color.r = (unsigned char)(bg_color.r * 0.8f);
                        bg_color.g = (unsigned char)(bg_color.g * 0.8f);
                        bg_color.b = (unsigned char)(bg_color.b * 0.8f);
                        break;
                    case KRYON_ELEMENT_STATE_DISABLED:
                        bg_color.a = 128;
                        text_color.a = 128;
                        break;
                    default:
                        break;
                }
                
                // Draw button background
                if (data->border_radius > 0) {
                    DrawRectangleRounded(
                        (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                        data->border_radius / (data->size.x < data->size.y ? data->size.x : data->size.y),
                        16,
                        bg_color
                    );
                } else {
                    DrawRectangle(
                        (int)data->position.x, (int)data->position.y,
                        (int)data->size.x, (int)data->size.y,
                        bg_color
                    );
                }
                
                // Draw border
                if (data->border_width > 0) {
                    if (data->border_radius > 0) {
                        // Draw rounded border (raylib 5.5 doesn't support border width for rounded rectangles)
                        // Draw the outline without width
                        DrawRectangleRoundedLines(
                            (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                            data->border_radius / (data->size.x < data->size.y ? data->size.x : data->size.y),
                            16,
                            border_color
                        );
                    } else {
                        DrawRectangleLinesEx(
                            (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                            data->border_width,
                            border_color
                        );
                    }
                }
                
                // Draw button text
                if (data->text) {
                    // Use dynamic font size from the button data (will be set by element renderer)
                    // For now, use a reasonable default that can be overridden
                    float button_font_size = 20.0f; // TODO: Add font_size field to KryonDrawButtonData
                    Vector2 text_size = MeasureTextEx(ctx->current_font, data->text, button_font_size, 1.0f);
                    Vector2 text_pos = {
                        data->position.x + (data->size.x - text_size.x) / 2,
                        data->position.y + (data->size.y - text_size.y) / 2
                    };
                    DrawTextEx(ctx->current_font, data->text, text_pos, button_font_size, 1.0f, text_color);
                }
                break;
            }
            
            case KRYON_CMD_DRAW_DROPDOWN: {
                const KryonDrawDropdownData* data = &cmd->data.draw_dropdown;
                
                // Draw main dropdown button
                Color bg_color = kryon_color_to_raylib(data->background_color);
                Color text_color = kryon_color_to_raylib(data->text_color);
                Color border_color = kryon_color_to_raylib(data->border_color);
                
                // Main button background
                if (data->border_radius > 0) {
                    DrawRectangleRounded(
                        (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                        data->border_radius / (data->size.x < data->size.y ? data->size.x : data->size.y),
                        16,
                        bg_color
                    );
                } else {
                    DrawRectangle(
                        (int)data->position.x, (int)data->position.y,
                        (int)data->size.x, (int)data->size.y,
                        bg_color
                    );
                }
                
                // Main button border
                if (data->border_width > 0) {
                    DrawRectangleLinesEx(
                        (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                        data->border_width,
                        border_color
                    );
                }
                
                // Selected text
                if (data->selected_index >= 0 && data->selected_index < (int)data->item_count && data->items) {
                    const char* selected_text = data->items[data->selected_index].text;
                    if (selected_text) {
                        Vector2 text_pos = {
                            data->position.x + 10,
                            data->position.y + (data->size.y - 20) / 2
                        };
                        DrawTextEx(ctx->current_font, selected_text, text_pos, 20, 1.0f, text_color);
                    }
                }
                
                // Draw arrow
                float arrow_x = data->position.x + data->size.x - 20;
                float arrow_y = data->position.y + data->size.y / 2;
                
                if (data->is_open) {
                    // Up arrow
                    DrawTriangle(
                        (Vector2){arrow_x + 5, arrow_y - 3},
                        (Vector2){arrow_x + 10, arrow_y + 3},
                        (Vector2){arrow_x, arrow_y + 3},
                        text_color
                    );
                } else {
                    // Down arrow
                    DrawTriangle(
                        (Vector2){arrow_x, arrow_y - 3},
                        (Vector2){arrow_x + 10, arrow_y - 3},
                        (Vector2){arrow_x + 5, arrow_y + 3},
                        text_color
                    );
                }
               
                // Draw dropdown items if open
                if (data->is_open && data->items) {
                    for (size_t j = 0; j < data->item_count; j++) {
                        float item_y = data->position.y + data->size.y + j * data->size.y;
                        
                        // Item background
                        Color item_bg = bg_color;
                        if ((int)j == data->hovered_index) {
                            item_bg = kryon_color_to_raylib(data->hover_color);
                        } else if ((int)j == data->selected_index) {
                            item_bg = kryon_color_to_raylib(data->selected_color);
                        }
                        
                        DrawRectangle(
                            (int)data->position.x, (int)item_y,
                            (int)data->size.x, (int)data->size.y,
                            item_bg
                        );
                        
                        // Item border
                        DrawRectangleLinesEx(
                            (Rectangle){data->position.x, item_y, data->size.x, data->size.y},
                            1.0f,
                            border_color
                        );
                        
                        // Item text
                        if (data->items[j].text) {
                            Vector2 item_text_pos = {
                                data->position.x + 10,
                                item_y + (data->size.y - 20) / 2
                            };
                            DrawTextEx(ctx->current_font, data->items[j].text, item_text_pos, 20, 1.0f, text_color);
                        }
                    }
                }
                break;
            }
            
            case KRYON_CMD_DRAW_INPUT: {
                const KryonDrawInputData* data = &cmd->data.draw_input;
                
                Color bg_color = kryon_color_to_raylib(data->background_color);
                Color text_color = kryon_color_to_raylib(data->text_color);
                Color border_color = kryon_color_to_raylib(data->border_color);
                
                // Adjust colors based on state
                if (data->state == KRYON_ELEMENT_STATE_FOCUSED) {
                    border_color = (Color){100, 150, 255, 255}; // Blue focus border
                } else if (data->state == KRYON_ELEMENT_STATE_DISABLED) {
                    bg_color.a = 128;
                    text_color.a = 128;
                }
                
                // Draw input background
                if (data->border_radius > 0) {
                    DrawRectangleRounded(
                        (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                        data->border_radius / (data->size.x < data->size.y ? data->size.x : data->size.y),
                        16,
                        bg_color
                    );
                } else {
                    DrawRectangle(
                        (int)data->position.x, (int)data->position.y,
                        (int)data->size.x, (int)data->size.y,
                        bg_color
                    );
                }
                
                // Draw border
                if (data->border_width > 0) {
                    DrawRectangleLinesEx(
                        (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                        data->border_width,
                        border_color
                    );
                }
                
                // Draw text or placeholder
                const char* display_text = (data->text && strlen(data->text) > 0) ? data->text : data->placeholder;
                Color display_color = (data->text && strlen(data->text) > 0) ? text_color : (Color){128, 128, 128, 255};
                
                if (display_text) {
                    Vector2 text_pos = {
                        data->position.x + 8,
                        data->position.y + (data->size.y - 20) / 2
                    };
                    
                    // Handle password input
                    if (data->is_password && data->text && strlen(data->text) > 0) {
                        char* masked_text = malloc(strlen(data->text) + 1);
                        for (size_t k = 0; k < strlen(data->text); k++) {
                            masked_text[k] = '*';
                        }
                        masked_text[strlen(data->text)] = '\0';
                        DrawTextEx(ctx->current_font, masked_text, text_pos, data->font_size, 1.0f, display_color);
                        free(masked_text);
                    } else {
                        DrawTextEx(ctx->current_font, display_text, text_pos, data->font_size, 1.0f, display_color);
                    }
                }
                
                // Draw cursor if focused
                if (data->has_focus && data->text) {
                    // Simple blinking cursor implementation
                    static float cursor_timer = 0;
                    cursor_timer += GetFrameTime();
                    if (fmodf(cursor_timer, 1.0f) < 0.5f) {
                        Vector2 cursor_text_size = MeasureTextEx(ctx->current_font, 
                            data->text ? data->text : "", data->font_size, 1.0f);
                        float cursor_x = data->position.x + 8 + cursor_text_size.x;
                        float cursor_y = data->position.y + 5;
                        DrawRectangle((int)cursor_x, (int)cursor_y, 2, (int)(data->size.y - 10), text_color);
                    }
                }
                break;
            }
            
            case KRYON_CMD_DRAW_CHECKBOX: {
                const KryonDrawCheckboxData* data = &cmd->data.draw_checkbox;
                
                Color bg_color = kryon_color_to_raylib(data->background_color);
                Color border_color = kryon_color_to_raylib(data->border_color);
                Color check_color = kryon_color_to_raylib(data->check_color);
                
                // Calculate checkbox size (square)
                float checkbox_size = data->size.y * 0.8f;
                float checkbox_x = data->position.x;
                float checkbox_y = data->position.y + (data->size.y - checkbox_size) / 2;
                
                // Draw checkbox background
                DrawRectangle((int)checkbox_x, (int)checkbox_y, (int)checkbox_size, (int)checkbox_size, bg_color);
                
                // Draw checkbox border
                if (data->border_width > 0) {
                    DrawRectangleLinesEx(
                        (Rectangle){checkbox_x, checkbox_y, checkbox_size, checkbox_size},
                        data->border_width,
                        border_color
                    );
                }
                
                // Draw checkmark if checked
                if (data->checked) {
                    float check_size = checkbox_size * 0.6f;
                    float check_x = checkbox_x + (checkbox_size - check_size) / 2;
                    float check_y = checkbox_y + (checkbox_size - check_size) / 2;
                    
                    // Simple checkmark (could be improved with actual checkmark shape)
                    DrawRectangle((int)check_x, (int)check_y, (int)check_size, (int)check_size, check_color);
                }
                
                // Draw label
                if (data->label) {
                    Vector2 label_pos = {
                        checkbox_x + checkbox_size + 8,
                        data->position.y + (data->size.y - 20) / 2
                    };
                    DrawTextEx(ctx->current_font, data->label, label_pos, 20, 1.0f, BLACK);
                }
                break;
            }
            
            case KRYON_CMD_BEGIN_CONTAINER: {
                const KryonBeginContainerData* data = &cmd->data.begin_container;
                
                // Draw container background
                if (data->background_color.a > 0) {
                    Color bg = kryon_color_to_raylib(data->background_color);
                    DrawRectangle(
                        (int)data->position.x, (int)data->position.y,
                        (int)data->size.x, (int)data->size.y,
                        bg
                    );
                }
                
                // Set up clipping if requested
                if (data->clip_children && ctx->clip_depth < 31) {
                    Rectangle clip_rect = {
                        data->position.x, data->position.y,
                        data->size.x, data->size.y
                    };
                    
                    ctx->clip_stack[ctx->clip_depth++] = clip_rect;
                    BeginScissorMode(
                        (int)clip_rect.x, (int)clip_rect.y,
                        (int)clip_rect.width, (int)clip_rect.height
                    );
                }
                break;
            }
            
            case KRYON_CMD_END_CONTAINER: {
                // Restore previous clip rect
                if (ctx->clip_depth > 0) {
                    EndScissorMode();
                    ctx->clip_depth--;
                    
                    if (ctx->clip_depth > 0) {
                        Rectangle clip_rect = ctx->clip_stack[ctx->clip_depth - 1];
                        BeginScissorMode(
                            (int)clip_rect.x, (int)clip_rect.y,
                            (int)clip_rect.width, (int)clip_rect.height
                        );
                    }
                }
                break;
            }
            
            case KRYON_CMD_UPDATE_ELEMENT_STATE: {
                // Widget state updates would be handled by the element management system
                // This is a placeholder for state synchronization
                break;
            }
            
            case KRYON_CMD_PUSH_CLIP: {
                const KryonDrawRectData* data = &cmd->data.draw_rect;
                
                // Push current scissor state and set new one
                Rectangle clip_rect = {
                    data->position.x,
                    data->position.y,
                    data->size.x,
                    data->size.y
                };
                
                // Store current clip in stack
                if (ctx->clip_depth < 31) {
                    ctx->clip_stack[ctx->clip_depth] = clip_rect;
                    ctx->clip_depth++;
                    
                    // Begin scissor mode for this clip area
                    BeginScissorMode(
                        (int)clip_rect.x,
                        (int)clip_rect.y,
                        (int)clip_rect.width,
                        (int)clip_rect.height
                    );
                }
                break;
            }
            
            case KRYON_CMD_POP_CLIP: {
                // Restore previous scissor state
                if (ctx->clip_depth > 0) {
                    EndScissorMode();
                    ctx->clip_depth--;
                    
                    // If there's a previous clip level, restore it
                    if (ctx->clip_depth > 0) {
                        Rectangle prev_clip = ctx->clip_stack[ctx->clip_depth - 1];
                        BeginScissorMode(
                            (int)prev_clip.x,
                            (int)prev_clip.y,
                            (int)prev_clip.width,
                            (int)prev_clip.height
                        );
                    }
                }
                break;
            }
            
            default:
                // Unsupported command, skip
                break;
        }
    }
    
    // Cleanup sorted commands array
    free(sorted_commands);
    
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult raylib_resize(KryonVec2 new_size) {
    if (!g_raylib_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    SetWindowSize((int)new_size.x, (int)new_size.y);
    g_raylib_impl.width = (int)new_size.x;
    g_raylib_impl.height = (int)new_size.y;
    
    return KRYON_RENDER_SUCCESS;
}

static KryonVec2 raylib_viewport_size(void) {
    return (KryonVec2){(float)g_raylib_impl.width, (float)g_raylib_impl.height};
}

static void raylib_destroy(void) {
    if (g_raylib_impl.initialized) {
        CloseWindow();
        g_raylib_impl.initialized = false;
        printf("Raylib Renderer destroyed\n");
    }
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static Color kryon_color_to_raylib(KryonColor color) {
    return (Color){
        .r = (unsigned char)(color.r * 255),
        .g = (unsigned char)(color.g * 255),
        .b = (unsigned char)(color.b * 255),
        .a = (unsigned char)(color.a * 255)
    };
}

static void draw_rounded_rect_raylib(KryonRect rect, float radius, Color color) {
    // Raylib has built-in rounded rectangle support
    DrawRectangleRounded(
        (Rectangle){rect.x, rect.y, rect.width, rect.height},
        radius / (rect.width < rect.height ? rect.width : rect.height),
        16, // segments
        color
    );
}

static void draw_text_with_font_raylib(const char* text, KryonVec2 pos, 
                                      Font font, float font_size, Color color) {
    if (!text) return;
    
    DrawTextEx(font, text, (Vector2){pos.x, pos.y}, font_size, 1.0f, color);
}

// =============================================================================
// INPUT HANDLING IMPLEMENTATION
// =============================================================================


static inline KryonKey raylib_key_to_kryon_ascii(int key) {
    if (key >= KEY_A && key <= KEY_Z) return KRYON_KEY_A + (key - KEY_A);
    if (key >= KEY_ZERO && key <= KEY_NINE) return KRYON_KEY_0 + (key - KEY_ZERO);
    if (key == KEY_SPACE) return KRYON_KEY_SPACE;
    if (key == KEY_APOSTROPHE) return KRYON_KEY_APOSTROPHE;
    if (key == KEY_COMMA) return KRYON_KEY_COMMA;
    if (key == KEY_MINUS) return KRYON_KEY_MINUS;
    if (key == KEY_PERIOD) return KRYON_KEY_PERIOD;
    if (key == KEY_SLASH) return KRYON_KEY_SLASH;
    if (key == KEY_SEMICOLON) return KRYON_KEY_SEMICOLON;
    if (key == KEY_EQUAL) return KRYON_KEY_EQUAL;
    if (key == KEY_LEFT_BRACKET) return KRYON_KEY_LEFT_BRACKET;
    if (key == KEY_BACKSLASH) return KRYON_KEY_BACKSLASH;
    if (key == KEY_RIGHT_BRACKET) return KRYON_KEY_RIGHT_BRACKET;
    if (key == KEY_GRAVE) return KRYON_KEY_GRAVE;
    return KRYON_KEY_UNKNOWN;
}


static const KryonKey raylib_to_kryon_map[] = {
    DEFINE_KEY_MAPPING(KEY, NULL, UNKNOWN),
    DEFINE_KEY_MAPPING(KEY, ESCAPE, ESCAPE),
    DEFINE_KEY_MAPPING(KEY, ENTER, ENTER),
    DEFINE_KEY_MAPPING(KEY, TAB, TAB),
    DEFINE_KEY_MAPPING(KEY, BACKSPACE, BACKSPACE),
    DEFINE_KEY_MAPPING(KEY, INSERT, INSERT),
    DEFINE_KEY_MAPPING(KEY, DELETE, DELETE),
    DEFINE_KEY_MAPPING(KEY, RIGHT, RIGHT),
    DEFINE_KEY_MAPPING(KEY, LEFT, LEFT),
    DEFINE_KEY_MAPPING(KEY, DOWN, DOWN),
    DEFINE_KEY_MAPPING(KEY, UP, UP),
    DEFINE_KEY_MAPPING(KEY, PAGE_UP, PAGE_UP),
    DEFINE_KEY_MAPPING(KEY, PAGE_DOWN, PAGE_DOWN),
    DEFINE_KEY_MAPPING(KEY, HOME, HOME),
    DEFINE_KEY_MAPPING(KEY, END, END),
    DEFINE_KEY_MAPPING(KEY, CAPS_LOCK, CAPS_LOCK),
    DEFINE_KEY_MAPPING(KEY, SCROLL_LOCK, SCROLL_LOCK),
    DEFINE_KEY_MAPPING(KEY, NUM_LOCK, NUM_LOCK),
    DEFINE_KEY_MAPPING(KEY, PRINT_SCREEN, PRINT_SCREEN),
    DEFINE_KEY_MAPPING(KEY, PAUSE, PAUSE),
    DEFINE_KEY_MAPPING(KEY, F1, F1),
    DEFINE_KEY_MAPPING(KEY, F2, F2),
    DEFINE_KEY_MAPPING(KEY, F3, F3),
    DEFINE_KEY_MAPPING(KEY, F4, F4),
    DEFINE_KEY_MAPPING(KEY, F5, F5),
    DEFINE_KEY_MAPPING(KEY, F6, F6),
    DEFINE_KEY_MAPPING(KEY, F7, F7),
    DEFINE_KEY_MAPPING(KEY, F8, F8),
    DEFINE_KEY_MAPPING(KEY, F9, F9),
    DEFINE_KEY_MAPPING(KEY, F10, F10),
    DEFINE_KEY_MAPPING(KEY, F11, F11),
    DEFINE_KEY_MAPPING(KEY, F12, F12),
    DEFINE_KEY_MAPPING(KEY, LEFT_SHIFT, LEFT_SHIFT),
    DEFINE_KEY_MAPPING(KEY, LEFT_CONTROL, LEFT_CONTROL),
    DEFINE_KEY_MAPPING(KEY, LEFT_ALT, LEFT_ALT),
    DEFINE_KEY_MAPPING(KEY, RIGHT_SHIFT, RIGHT_SHIFT),
    DEFINE_KEY_MAPPING(KEY, RIGHT_CONTROL, RIGHT_CONTROL),
    DEFINE_KEY_MAPPING(KEY, RIGHT_ALT, RIGHT_ALT)
};


static inline KryonKey raylib_key_to_kryon(int raylib_key) {
    KryonKey key = raylib_key_to_kryon_ascii(raylib_key);
    if (key != KRYON_KEY_UNKNOWN) return key;
    return (raylib_key >= 0 && raylib_key < sizeof(raylib_to_kryon_map) / sizeof(raylib_to_kryon_map[0]))
           ? raylib_to_kryon_map[raylib_key]
           : KRYON_KEY_UNKNOWN;
}

static void raylib_process_input(RaylibRendererImpl* data) {
    if (!data->event_callback) {
        return; // No callback set
    }
    
    // Mouse button events
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 pos = GetMousePosition();
        printf("🖱️ LEFT MOUSE BUTTON PRESSED at (%.1f, %.1f)\n", pos.x, pos.y);
        KryonEvent event = kryon_event_create_mouse_button(0, pos.x, pos.y, true);
        data->event_callback(&event, data->callback_data);
    }
    
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        Vector2 pos = GetMousePosition();
        printf("🖱️ LEFT MOUSE BUTTON RELEASED at (%.1f, %.1f)\n", pos.x, pos.y);
        KryonEvent event = kryon_event_create_mouse_button(0, pos.x, pos.y, false);
        data->event_callback(&event, data->callback_data);
    }
    
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        Vector2 pos = GetMousePosition();
        KryonEvent event = kryon_event_create_mouse_button(1, pos.x, pos.y, true);
        data->event_callback(&event, data->callback_data);
    }
    
    if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
        Vector2 pos = GetMousePosition();
        KryonEvent event = kryon_event_create_mouse_button(1, pos.x, pos.y, false);
        data->event_callback(&event, data->callback_data);
    }
    
    // Mouse movement events
    Vector2 current_pos = GetMousePosition();
    if (!data->mouse_tracking_initialized) {
        data->last_mouse_pos = current_pos;
        data->mouse_tracking_initialized = true;
    } else if (current_pos.x != data->last_mouse_pos.x || current_pos.y != data->last_mouse_pos.y) {
        float deltaX = current_pos.x - data->last_mouse_pos.x;
        float deltaY = current_pos.y - data->last_mouse_pos.y;
        
        KryonEvent event = kryon_event_create_mouse_move(current_pos.x, current_pos.y, deltaX, deltaY);
        data->event_callback(&event, data->callback_data);
        
        data->last_mouse_pos = current_pos;
    }
    
    // Mouse scroll events
    float scroll = GetMouseWheelMove();
    if (scroll != 0.0f) {
        Vector2 pos = GetMousePosition();
        KryonEvent event = kryon_event_create_mouse_scroll(0.0f, scroll, pos.x, pos.y);
        data->event_callback(&event, data->callback_data);
    }
    
    // Keyboard events - key presses
    int key = GetKeyPressed();
    while (key != 0) {
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        bool meta = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        
        KryonEvent event = kryon_event_create_key(key, true, ctrl, shift, alt, meta);
        data->event_callback(&event, data->callback_data);
        
        key = GetKeyPressed(); // Get next key
    }
    
    // Text input events
    int codepoint = GetCharPressed();
    while (codepoint != 0) {
        // Convert codepoint to UTF-8
        char text[5] = {0};
        if (codepoint < 0x80) {
            text[0] = (char)codepoint;
        } else if (codepoint < 0x800) {
            text[0] = (char)(0xC0 | (codepoint >> 6));
            text[1] = (char)(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            text[0] = (char)(0xE0 | (codepoint >> 12));
            text[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            text[2] = (char)(0x80 | (codepoint & 0x3F));
        } else {
            text[0] = (char)(0xF0 | (codepoint >> 18));
            text[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
            text[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            text[3] = (char)(0x80 | (codepoint & 0x3F));
        }
        
        KryonEvent event = kryon_event_create_text_input(text);
        data->event_callback(&event, data->callback_data);
        
        codepoint = GetCharPressed(); // Get next character
    }
    
    // Window events
    if (IsWindowResized()) {
        KryonEvent event = kryon_event_create_window_resize(GetScreenWidth(), GetScreenHeight());
        data->event_callback(&event, data->callback_data);
    }
}

static bool raylib_point_in_element(KryonVec2 point, KryonRect element_bounds) {
    return point.x >= element_bounds.x && 
           point.x <= element_bounds.x + element_bounds.width &&
           point.y >= element_bounds.y && 
           point.y <= element_bounds.y + element_bounds.height;
}

// =============================================================================
// EVENT HANDLING IMPLEMENTATION
// =============================================================================

static bool raylib_handle_event(const KryonEvent* event) {
    if (!event || !g_raylib_impl.initialized) return false;
    
    // Handle window events
    switch (event->type) {
        case KRYON_EVENT_WINDOW_RESIZE:
            // Raylib handles window resize automatically
            g_raylib_impl.width = event->data.windowResize.width;
            g_raylib_impl.height = event->data.windowResize.height;
            return true;
            
        case KRYON_EVENT_WINDOW_CLOSE:
            g_raylib_impl.window_should_close = true;
            return true;
            
        default:
            // Other events are handled by the application
            return false;
    }
}

static void* raylib_get_native_window(void) {
    // Raylib doesn't expose the native window handle easily
    return NULL;
}

// Measure text width using raylib's font system
static float raylib_measure_text_width(const char* text, float font_size) {
    if (!text || strlen(text) == 0) {
        return 0.0f;
    }
    
    // Use the default font (same as what's used in rendering)
    Font current_font = g_raylib_impl.initialized ? g_raylib_impl.default_font : GetFontDefault();
    
    // Use the actual font_size parameter for accurate measurement
    Vector2 text_size = MeasureTextEx(current_font, text, font_size, 1.0f);
    return text_size.x;
}


// Set cursor implementation
static KryonRenderResult raylib_set_cursor(KryonCursorType cursor_type) {
    if (!g_raylib_impl.initialized) {
        return KRYON_RENDER_ERROR_INVALID_PARAM;
    }
    
    // Only change cursor if it's different from current state (prevents jittery behavior)
    if (g_raylib_impl.cursor_initialized && g_raylib_impl.current_cursor == cursor_type) {
        return KRYON_RENDER_SUCCESS;
    }
    
    // Map Kryon cursor types to Raylib cursor types
    MouseCursor raylib_cursor;
    switch (cursor_type) {
        case KRYON_CURSOR_DEFAULT:
            raylib_cursor = MOUSE_CURSOR_DEFAULT;
            break;
        case KRYON_CURSOR_POINTER:
            raylib_cursor = MOUSE_CURSOR_POINTING_HAND;
            break;
        case KRYON_CURSOR_TEXT:
            raylib_cursor = MOUSE_CURSOR_IBEAM;
            break;
        case KRYON_CURSOR_CROSSHAIR:
            raylib_cursor = MOUSE_CURSOR_CROSSHAIR;
            break;
        case KRYON_CURSOR_RESIZE_H:
            raylib_cursor = MOUSE_CURSOR_RESIZE_EW;
            break;
        case KRYON_CURSOR_RESIZE_V:
            raylib_cursor = MOUSE_CURSOR_RESIZE_NS;
            break;
        case KRYON_CURSOR_RESIZE_NESW:
            raylib_cursor = MOUSE_CURSOR_RESIZE_NESW;
            break;
        case KRYON_CURSOR_RESIZE_NWSE:
            raylib_cursor = MOUSE_CURSOR_RESIZE_NWSE;
            break;
        case KRYON_CURSOR_RESIZE_ALL:
            raylib_cursor = MOUSE_CURSOR_RESIZE_ALL;
            break;
        case KRYON_CURSOR_NOT_ALLOWED:
            raylib_cursor = MOUSE_CURSOR_NOT_ALLOWED;
            break;
        default:
            raylib_cursor = MOUSE_CURSOR_DEFAULT;
            break;
    }
    
    SetMouseCursor(raylib_cursor);
    
    // Update state tracking
    g_raylib_impl.current_cursor = cursor_type;
    g_raylib_impl.cursor_initialized = true;
    
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult raylib_update_window_size(int width, int height) {
    if (!g_raylib_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    printf("🪟 Raylib: Updating window size to %dx%d\n", width, height);
    
    // Update raylib window size
    SetWindowSize(width, height);
    
    // Update our internal tracking
    g_raylib_impl.width = width;
    g_raylib_impl.height = height;
    
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult raylib_update_window_title(const char* title) {
    if (!g_raylib_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    if (!title) {
        return KRYON_RENDER_ERROR_INVALID_PARAM;
    }
    
    printf("🪟 Raylib: Updating window title to '%s'\n", title);
    
    // Update raylib window title
    SetWindowTitle(title);
    
    return KRYON_RENDER_SUCCESS;
}

// Comparison function for z-index sorting
static int compare_z_index(const void* a, const void* b) {
    const KryonRenderCommand* cmd_a = (const KryonRenderCommand*)a;
    const KryonRenderCommand* cmd_b = (const KryonRenderCommand*)b;
    
    if (cmd_a->z_index < cmd_b->z_index) return -1;
    if (cmd_a->z_index > cmd_b->z_index) return 1;
    return 0;
}