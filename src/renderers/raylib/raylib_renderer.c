/**
 * @file raylib_renderer.c
 * @brief Raylib Renderer Implementation
 * 
 * Raylib-based renderer following kryon-rust architecture patterns
 */

#include "internal/renderer_interface.h"
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

// =============================================================================
// GLOBAL STATE
// =============================================================================

static RaylibRendererImpl g_raylib_impl = {0};

// Forward declarations for input handling
static KryonRenderResult raylib_get_input_state(KryonInputState* input_state);
static bool raylib_point_in_widget(KryonVec2 point, KryonRect widget_bounds);
static bool raylib_handle_event(const KryonEvent* event);
static void* raylib_get_native_window(void);
static float raylib_measure_text_width(const char* text, float font_size);

static KryonRendererVTable g_raylib_vtable = {
    .initialize = raylib_initialize,
    .begin_frame = raylib_begin_frame,
    .end_frame = raylib_end_frame,
    .execute_commands = raylib_execute_commands,
    .resize = raylib_resize,
    .viewport_size = raylib_viewport_size,
    .destroy = raylib_destroy,
    .get_input_state = raylib_get_input_state,
    .point_in_element = raylib_point_in_widget,
    .handle_event = raylib_handle_event,
    .get_native_window = raylib_get_native_window,
    .measure_text_width = raylib_measure_text_width
};

// =============================================================================
// PUBLIC API
// =============================================================================

KryonRenderer* kryon_raylib_renderer_create(void* surface) {
    KryonRenderer* renderer = malloc(sizeof(KryonRenderer));
    if (!renderer) {
        return NULL;
    }
    
    renderer->vtable = &g_raylib_vtable;
    renderer->impl_data = &g_raylib_impl;
    renderer->name = strdup("Raylib Renderer");
    renderer->backend = strdup("raylib");
    
    // Initialize with provided surface
    if (raylib_initialize(surface) != KRYON_RENDER_SUCCESS) {
        free(renderer->name);
        free(renderer->backend);
        free(renderer);
        return NULL;
    }
    
    return renderer;
}

// Factory function for renderer system
KryonRenderer* kryon_renderer_create_raylib(void) {
    return kryon_raylib_renderer_create(NULL);
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
    printf("🔍 DEBUG: Setting target FPS to %d\n", surf->target_fps > 0 ? surf->target_fps : 60);
    // DISABLED: SetTargetFPS(surf->target_fps > 0 ? surf->target_fps : 60);
    // ^ SetTargetFPS causes segfault in this environment - skip it for now
    printf("🔍 DEBUG: SetTargetFPS skipped (would cause segfault)\n");
    
    // Load default font (Raylib has a built-in default font)
    printf("🔍 DEBUG: Getting default font\n");
    g_raylib_impl.default_font = GetFontDefault();
    printf("🔍 DEBUG: GetFontDefault loaded successfully\n");
    
    printf("🔍 DEBUG: Setting impl variables\n");
    g_raylib_impl.width = surf->width;
    g_raylib_impl.height = surf->height;
    g_raylib_impl.initialized = true;
    g_raylib_impl.window_should_close = false;
    printf("🔍 DEBUG: Impl variables set\n");
    
    printf("Raylib Renderer initialized: %dx%d @ %d FPS\n", 
           surf->width, surf->height, surf->target_fps > 0 ? surf->target_fps : 60);
    
    printf("🔍 DEBUG: About to return KRYON_RENDER_SUCCESS\n");
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult raylib_begin_frame(KryonRenderContext** context, KryonColor clear_color) {
    if (!g_raylib_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
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
    
    for (size_t i = 0; i < command_count; i++) {
        const KryonRenderCommand* cmd = &commands[i];
        
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
                        printf("🎯 TEXT CENTER: size=%.1fx%.1f, orig_pos=%.1f,%.1f, final_pos=%.1f,%.1f\n",
                               text_size.x, text_size.y, data->position.x, data->position.y,
                               final_pos.x, final_pos.y);
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
                        DrawRectangleRoundedLines(
                            (Rectangle){data->position.x, data->position.y, data->size.x, data->size.y},
                            data->border_radius / (data->size.x < data->size.y ? data->size.x : data->size.y),
                            16,
                            data->border_width,
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
                // Widget state updates would be handled by the widget management system
                // This is a placeholder for state synchronization
                break;
            }
            
            default:
                // Unsupported command, skip
                break;
        }
    }
    
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

static KryonKey raylib_key_to_kryon(int raylib_key) {
    switch (raylib_key) {
        case KEY_SPACE: return KRYON_KEY_SPACE;
        case KEY_ENTER: return KRYON_KEY_ENTER;
        case KEY_TAB: return KRYON_KEY_TAB;
        case KEY_BACKSPACE: return KRYON_KEY_BACKSPACE;
        case KEY_DELETE: return KRYON_KEY_DELETE;
        case KEY_RIGHT: return KRYON_KEY_RIGHT;
        case KEY_LEFT: return KRYON_KEY_LEFT;
        case KEY_DOWN: return KRYON_KEY_DOWN;
        case KEY_UP: return KRYON_KEY_UP;
        case KEY_ESCAPE: return KRYON_KEY_ESCAPE;
        case KEY_A: return KRYON_KEY_A;
        case KEY_C: return KRYON_KEY_C;
        case KEY_V: return KRYON_KEY_V;
        case KEY_X: return KRYON_KEY_X;
        case KEY_Z: return KRYON_KEY_Z;
        default: return KRYON_KEY_UNKNOWN;
    }
}

static KryonRenderResult raylib_get_input_state(KryonInputState* input_state) {
    if (!input_state) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    // Initialize input state
    memset(input_state, 0, sizeof(KryonInputState));
    
    // Mouse state
    Vector2 mouse_pos = GetMousePosition();
    Vector2 mouse_delta = GetMouseDelta();
    float mouse_wheel = GetMouseWheelMove();
    
    input_state->mouse.position = (KryonVec2){mouse_pos.x, mouse_pos.y};
    input_state->mouse.delta = (KryonVec2){mouse_delta.x, mouse_delta.y};
    
    // Update global mouse position for hover detection (simplified approach)
    extern KryonVec2 g_mouse_position;
    g_mouse_position = (KryonVec2){mouse_pos.x, mouse_pos.y};
    
    // Update cursor based on global state
    extern bool g_cursor_should_be_pointer;
    static bool cursor_is_pointer = false;
    if (g_cursor_should_be_pointer != cursor_is_pointer) {
        SetMouseCursor(g_cursor_should_be_pointer ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
        cursor_is_pointer = g_cursor_should_be_pointer;
        // Cursor state changed
    }
    
    // Mouse position captured
    input_state->mouse.wheel = mouse_wheel;
    input_state->mouse.left_pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    input_state->mouse.right_pressed = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
    input_state->mouse.middle_pressed = IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON);
    input_state->mouse.left_released = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    input_state->mouse.right_released = IsMouseButtonReleased(MOUSE_RIGHT_BUTTON);
    input_state->mouse.middle_released = IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON);
    
    // Keyboard state - check common keys
    int raylib_keys[] = {
        KEY_SPACE, KEY_ENTER, KEY_TAB, KEY_BACKSPACE, KEY_DELETE,
        KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_ESCAPE,
        KEY_A, KEY_C, KEY_V, KEY_X, KEY_Z
    };
    
    for (size_t i = 0; i < sizeof(raylib_keys) / sizeof(raylib_keys[0]); i++) {
        int raylib_key = raylib_keys[i];
        KryonKey kryon_key = raylib_key_to_kryon(raylib_key);
        
        if (kryon_key != KRYON_KEY_UNKNOWN && kryon_key < 512) {
            input_state->keyboard.keys_pressed[kryon_key] = IsKeyPressed(raylib_key);
            input_state->keyboard.keys_released[kryon_key] = IsKeyReleased(raylib_key);
            input_state->keyboard.keys_down[kryon_key] = IsKeyDown(raylib_key);
        }
    }
    
    // Text input
    int key = GetCharPressed();
    if (key > 0) {
        char text_char[2] = {(char)key, '\0'};
        strncpy(input_state->keyboard.text_input, text_char, sizeof(input_state->keyboard.text_input) - 1);
        input_state->keyboard.text_input_length = 1;
    } else {
        input_state->keyboard.text_input[0] = '\0';
        input_state->keyboard.text_input_length = 0;
    }
    
    return KRYON_RENDER_SUCCESS;
}

static bool raylib_point_in_widget(KryonVec2 point, KryonRect widget_bounds) {
    return point.x >= widget_bounds.x && 
           point.x <= widget_bounds.x + widget_bounds.width &&
           point.y >= widget_bounds.y && 
           point.y <= widget_bounds.y + widget_bounds.height;
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