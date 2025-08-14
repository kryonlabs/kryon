/**
 * @file sdl2_renderer.c
 * @brief SDL2 Renderer Implementation
 * 
 * SDL2-based renderer following kryon-rust architecture patterns
 */

#include "renderer_interface.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// =============================================================================
// SDL2 SPECIFIC TYPES
// =============================================================================

typedef struct {
    int width;
    int height;
    const char* title;
    bool fullscreen;
} SDL2Surface;

typedef struct {
    SDL_Renderer* renderer;
    SDL_Rect clip_stack[32];
    int clip_depth;
} SDL2RenderContext;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* default_font;
    int width;
    int height;
    bool initialized;
    bool quit_requested;
} SDL2RendererImpl;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonRenderResult sdl2_initialize(void* surface);
static KryonRenderResult sdl2_begin_frame(KryonRenderContext** context, KryonColor clear_color);
static KryonRenderResult sdl2_end_frame(KryonRenderContext* context);
static KryonRenderResult sdl2_execute_commands(KryonRenderContext* context,
                                              const KryonRenderCommand* commands,
                                              size_t command_count);
static KryonRenderResult sdl2_resize(KryonVec2 new_size);
static KryonVec2 sdl2_viewport_size(void);
static void sdl2_destroy(void);

// Helper functions
static SDL_Color kryon_color_to_sdl(KryonColor color);
static void draw_rounded_rect(SDL_Renderer* renderer, KryonRect rect, float radius, SDL_Color color);
static void draw_text_with_font(SDL_Renderer* renderer, const char* text, 
                               KryonVec2 pos, TTF_Font* font, SDL_Color color);

// =============================================================================
// GLOBAL STATE (for simplicity - could be improved with context passing)
// =============================================================================

static SDL2RendererImpl g_sdl2_impl = {0};

// Forward declarations for input handling
static KryonRenderResult sdl2_get_input_state(KryonInputState* input_state);
static bool sdl2_point_in_element(KryonVec2 point, KryonRect element_bounds);
static bool sdl2_handle_event(const KryonEvent* event);
static void* sdl2_get_native_window(void);

static KryonRendererVTable g_sdl2_vtable = {
    .initialize = sdl2_initialize,
    .begin_frame = sdl2_begin_frame,
    .end_frame = sdl2_end_frame,
    .execute_commands = sdl2_execute_commands,
    .resize = sdl2_resize,
    .viewport_size = sdl2_viewport_size,
    .destroy = sdl2_destroy,
    .get_input_state = sdl2_get_input_state,
    .point_in_element = sdl2_point_in_element,
    .handle_event = sdl2_handle_event,
    .get_native_window = sdl2_get_native_window
};

// =============================================================================
// PUBLIC API
// =============================================================================

KryonRenderer* kryon_sdl2_renderer_create(void* surface) {
    KryonRenderer* renderer = malloc(sizeof(KryonRenderer));
    if (!renderer) {
        return NULL;
    }
    
    renderer->vtable = &g_sdl2_vtable;
    renderer->impl_data = &g_sdl2_impl;
    renderer->name = strdup("SDL2 Renderer");
    renderer->backend = strdup("sdl2");
    
    // Initialize with provided surface
    if (sdl2_initialize(surface) != KRYON_RENDER_SUCCESS) {
        free(renderer->name);
        free(renderer->backend);
        free(renderer);
        return NULL;
    }
    
    return renderer;
}

// =============================================================================
// RENDERER IMPLEMENTATION
// =============================================================================

static KryonRenderResult sdl2_initialize(void* surface) {
    SDL2Surface* surf = (SDL2Surface*)surface;
    if (!surf) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        SDL_Quit();
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    // Initialize SDL_image
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        fprintf(stderr, "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        // Not fatal, continue without image support
    }
    
    // Create window
    Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (surf->fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    
    g_sdl2_impl.window = SDL_CreateWindow(
        surf->title ? surf->title : "Kryon Application",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        surf->width,
        surf->height,
        window_flags
    );
    
    if (!g_sdl2_impl.window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    // Create renderer
    g_sdl2_impl.renderer = SDL_CreateRenderer(
        g_sdl2_impl.window, 
        -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!g_sdl2_impl.renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_sdl2_impl.window);
        TTF_Quit();
        SDL_Quit();
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    // Load default font
    g_sdl2_impl.default_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    if (!g_sdl2_impl.default_font) {
        // Try alternative font paths
        g_sdl2_impl.default_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 16);
        if (!g_sdl2_impl.default_font) {
            // Create a basic font as fallback
            fprintf(stderr, "Warning: Could not load font. Text rendering may not work.\n");
        }
    }
    
    g_sdl2_impl.width = surf->width;
    g_sdl2_impl.height = surf->height;
    g_sdl2_impl.initialized = true;
    
    printf("SDL2 Renderer initialized: %dx%d\n", surf->width, surf->height);
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult sdl2_begin_frame(KryonRenderContext** context, KryonColor clear_color) {
    if (!g_sdl2_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    SDL2RenderContext* ctx = malloc(sizeof(SDL2RenderContext));
    if (!ctx) {
        return KRYON_RENDER_ERROR_OUT_OF_MEMORY;
    }
    
    ctx->renderer = g_sdl2_impl.renderer;
    ctx->clip_depth = 0;
    
    // Clear the screen
    SDL_Color bg = kryon_color_to_sdl(clear_color);
    SDL_SetRenderDrawColor(g_sdl2_impl.renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderClear(g_sdl2_impl.renderer);
    
    *context = (KryonRenderContext*)ctx;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult sdl2_end_frame(KryonRenderContext* context) {
    if (!context) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    SDL_RenderPresent(g_sdl2_impl.renderer);
    free(context);
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult sdl2_execute_commands(KryonRenderContext* context,
                                              const KryonRenderCommand* commands,
                                              size_t command_count) {
    if (!context || !commands) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    SDL2RenderContext* ctx = (SDL2RenderContext*)context;
    
    for (size_t i = 0; i < command_count; i++) {
        const KryonRenderCommand* cmd = &commands[i];
        
        switch (cmd->type) {
            case KRYON_CMD_SET_CANVAS_SIZE: {
                // Canvas size is handled during initialization
                break;
            }
            
            case KRYON_CMD_DRAW_RECT: {
                const KryonDrawRectData* data = &cmd->data.draw_rect;
                
                SDL_Rect rect = {
                    .x = (int)data->position.x,
                    .y = (int)data->position.y,
                    .w = (int)data->size.x,
                    .h = (int)data->size.y
                };
                
                SDL_Color color = kryon_color_to_sdl(data->color);
                
                if (data->border_radius > 0) {
                    KryonRect kryon_rect = {
                        data->position.x, data->position.y,
                        data->size.x, data->size.y
                    };
                    draw_rounded_rect(ctx->renderer, kryon_rect, data->border_radius, color);
                } else {
                    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderFillRect(ctx->renderer, &rect);
                }
                
                // Draw border if specified
                if (data->border_width > 0) {
                    SDL_Color border_color = kryon_color_to_sdl(data->border_color);
                    SDL_SetRenderDrawColor(ctx->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
                    
                    for (int i = 0; i < (int)data->border_width; i++) {
                        SDL_Rect border_rect = {
                            rect.x - i, rect.y - i,
                            rect.w + 2*i, rect.h + 2*i
                        };
                        SDL_RenderDrawRect(ctx->renderer, &border_rect);
                    }
                }
                break;
            }
            
            case KRYON_CMD_DRAW_TEXT: {
                const KryonDrawTextData* data = &cmd->data.draw_text;
                
                if (g_sdl2_impl.default_font && data->text) {
                    SDL_Color color = kryon_color_to_sdl(data->color);
                    draw_text_with_font(ctx->renderer, data->text, data->position, 
                                       g_sdl2_impl.default_font, color);
                }
                break;
            }
            
            case KRYON_CMD_DRAW_IMAGE: {
                const KryonDrawImageData* data = &cmd->data.draw_image;
                
                if (data->source) {
                    SDL_Surface* surface = IMG_Load(data->source);
                    if (surface) {
                        SDL_Texture* texture = SDL_CreateTextureFromSurface(ctx->renderer, surface);
                        if (texture) {
                            SDL_Rect dst_rect = {
                                (int)data->position.x,
                                (int)data->position.y,
                                (int)data->size.x,
                                (int)data->size.y
                            };
                            
                            SDL_SetTextureAlphaMod(texture, (Uint8)(data->opacity * 255));
                            SDL_RenderCopy(ctx->renderer, texture, NULL, &dst_rect);
                            SDL_DestroyTexture(texture);
                        }
                        SDL_FreeSurface(surface);
                    }
                }
                break;
            }
            
            case KRYON_CMD_BEGIN_CONTAINER: {
                const KryonBeginContainerData* data = &cmd->data.begin_container;
                
                // Draw container background
                if (data->background_color.a > 0) {
                    SDL_Rect rect = {
                        (int)data->position.x,
                        (int)data->position.y,
                        (int)data->size.x,
                        (int)data->size.y
                    };
                    
                    SDL_Color bg = kryon_color_to_sdl(data->background_color);
                    SDL_SetRenderDrawColor(ctx->renderer, bg.r, bg.g, bg.b, bg.a);
                    SDL_RenderFillRect(ctx->renderer, &rect);
                }
                
                // Set up clipping if requested
                if (data->clip_children && ctx->clip_depth < 31) {
                    SDL_Rect clip_rect = {
                        (int)data->position.x,
                        (int)data->position.y,
                        (int)data->size.x,
                        (int)data->size.y
                    };
                    
                    ctx->clip_stack[ctx->clip_depth++] = clip_rect;
                    SDL_RenderSetClipRect(ctx->renderer, &clip_rect);
                }
                break;
            }
            
            case KRYON_CMD_END_CONTAINER: {
                // Restore previous clip rect
                if (ctx->clip_depth > 0) {
                    ctx->clip_depth--;
                    if (ctx->clip_depth > 0) {
                        SDL_RenderSetClipRect(ctx->renderer, &ctx->clip_stack[ctx->clip_depth - 1]);
                    } else {
                        SDL_RenderSetClipRect(ctx->renderer, NULL);
                    }
                }
                break;
            }
            
            case KRYON_CMD_DRAW_BUTTON: {
                const KryonDrawButtonData* data = &cmd->data.draw_button;
                
                // Determine colors based on state
                SDL_Color bg_color = kryon_color_to_sdl(data->background_color);
                SDL_Color text_color = kryon_color_to_sdl(data->text_color);
                SDL_Color border_color = kryon_color_to_sdl(data->border_color);
                
                switch (data->state) {
                    case KRYON_ELEMENT_STATE_HOVERED:
                        bg_color.r = (Uint8)(bg_color.r * 0.9f);
                        bg_color.g = (Uint8)(bg_color.g * 0.9f);
                        bg_color.b = (Uint8)(bg_color.b * 0.9f);
                        break;
                    case KRYON_ELEMENT_STATE_PRESSED:
                        bg_color.r = (Uint8)(bg_color.r * 0.8f);
                        bg_color.g = (Uint8)(bg_color.g * 0.8f);
                        bg_color.b = (Uint8)(bg_color.b * 0.8f);
                        break;
                    case KRYON_ELEMENT_STATE_DISABLED:
                        bg_color.a = 128;
                        text_color.a = 128;
                        break;
                    default:
                        break;
                }
                
                // Draw button background
                SDL_Rect button_rect = {
                    (int)data->position.x, (int)data->position.y,
                    (int)data->size.x, (int)data->size.y
                };
                
                SDL_SetRenderDrawColor(ctx->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
                SDL_RenderFillRect(ctx->renderer, &button_rect);
                
                // Draw border
                if (data->border_width > 0) {
                    SDL_SetRenderDrawColor(ctx->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
                    for (int i = 0; i < (int)data->border_width; i++) {
                        SDL_Rect border_rect = {
                            button_rect.x - i, button_rect.y - i,
                            button_rect.w + 2*i, button_rect.h + 2*i
                        };
                        SDL_RenderDrawRect(ctx->renderer, &border_rect);
                    }
                }
                
                // Draw button text
                if (data->text && g_sdl2_impl.default_font) {
                    draw_text_with_font(ctx->renderer, data->text,
                                       (KryonVec2){data->position.x + data->size.x/2 - 50, 
                                                  data->position.y + data->size.y/2 - 10},
                                       g_sdl2_impl.default_font, text_color);
                }
                break;
            }
            
            case KRYON_CMD_DRAW_DROPDOWN: {
                const KryonDrawDropdownData* data = &cmd->data.draw_dropdown;
                
                SDL_Color bg_color = kryon_color_to_sdl(data->background_color);
                SDL_Color text_color = kryon_color_to_sdl(data->text_color);
                SDL_Color border_color = kryon_color_to_sdl(data->border_color);
                
                // Draw main dropdown button
                SDL_Rect dropdown_rect = {
                    (int)data->position.x, (int)data->position.y,
                    (int)data->size.x, (int)data->size.y
                };
                
                SDL_SetRenderDrawColor(ctx->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
                SDL_RenderFillRect(ctx->renderer, &dropdown_rect);
                
                // Draw border
                if (data->border_width > 0) {
                    SDL_SetRenderDrawColor(ctx->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
                    SDL_RenderDrawRect(ctx->renderer, &dropdown_rect);
                }
                
                // Draw selected text
                if (data->selected_index >= 0 && data->selected_index < (int)data->item_count && 
                    data->items && data->items[data->selected_index].text && g_sdl2_impl.default_font) {
                    draw_text_with_font(ctx->renderer, data->items[data->selected_index].text,
                                       (KryonVec2){data->position.x + 10, data->position.y + 8},
                                       g_sdl2_impl.default_font, text_color);
                }
                
                // Draw arrow indicator
                int arrow_x = (int)(data->position.x + data->size.x - 20);
                int arrow_y = (int)(data->position.y + data->size.y / 2);
                
                SDL_SetRenderDrawColor(ctx->renderer, text_color.r, text_color.g, text_color.b, text_color.a);
                if (data->is_open) {
                    // Up arrow - simple triangle
                    SDL_Point points[] = {
                        {arrow_x, arrow_y + 3},
                        {arrow_x + 10, arrow_y + 3},
                        {arrow_x + 5, arrow_y - 3}
                    };
                    SDL_RenderDrawLines(ctx->renderer, points, 3);
                } else {
                    // Down arrow
                    SDL_Point points[] = {
                        {arrow_x, arrow_y - 3},
                        {arrow_x + 10, arrow_y - 3},
                        {arrow_x + 5, arrow_y + 3}
                    };
                    SDL_RenderDrawLines(ctx->renderer, points, 3);
                }
                
                // Draw dropdown items if open
                if (data->is_open && data->items) {
                    for (size_t j = 0; j < data->item_count; j++) {
                        int item_y = (int)(data->position.y + data->size.y + j * data->size.y);
                        
                        SDL_Rect item_rect = {
                            (int)data->position.x, item_y,
                            (int)data->size.x, (int)data->size.y
                        };
                        
                        // Item background
                        SDL_Color item_bg = bg_color;
                        if ((int)j == data->hovered_index) {
                            item_bg = kryon_color_to_sdl(data->hover_color);
                        } else if ((int)j == data->selected_index) {
                            item_bg = kryon_color_to_sdl(data->selected_color);
                        }
                        
                        SDL_SetRenderDrawColor(ctx->renderer, item_bg.r, item_bg.g, item_bg.b, item_bg.a);
                        SDL_RenderFillRect(ctx->renderer, &item_rect);
                        
                        // Item border
                        SDL_SetRenderDrawColor(ctx->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
                        SDL_RenderDrawRect(ctx->renderer, &item_rect);
                        
                        // Item text
                        if (data->items[j].text && g_sdl2_impl.default_font) {
                            draw_text_with_font(ctx->renderer, data->items[j].text,
                                               (KryonVec2){data->position.x + 10, item_y + 8},
                                               g_sdl2_impl.default_font, text_color);
                        }
                    }
                }
                break;
            }
            
            case KRYON_CMD_DRAW_INPUT: {
                const KryonDrawInputData* data = &cmd->data.draw_input;
                
                SDL_Color bg_color = kryon_color_to_sdl(data->background_color);
                SDL_Color text_color = kryon_color_to_sdl(data->text_color);
                SDL_Color border_color = kryon_color_to_sdl(data->border_color);
                
                // Adjust colors based on state
                if (data->state == KRYON_ELEMENT_STATE_FOCUSED) {
                    border_color = (SDL_Color){100, 150, 255, 255}; // Blue focus border
                } else if (data->state == KRYON_ELEMENT_STATE_DISABLED) {
                    bg_color.a = 128;
                    text_color.a = 128;
                }
                
                // Draw input background
                SDL_Rect input_rect = {
                    (int)data->position.x, (int)data->position.y,
                    (int)data->size.x, (int)data->size.y
                };
                
                SDL_SetRenderDrawColor(ctx->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
                SDL_RenderFillRect(ctx->renderer, &input_rect);
                
                // Draw border
                if (data->border_width > 0) {
                    SDL_SetRenderDrawColor(ctx->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
                    for (int i = 0; i < (int)data->border_width; i++) {
                        SDL_Rect border_rect = {
                            input_rect.x - i, input_rect.y - i,
                            input_rect.w + 2*i, input_rect.h + 2*i
                        };
                        SDL_RenderDrawRect(ctx->renderer, &border_rect);
                    }
                }
                
                // Draw text or placeholder
                const char* display_text = (data->text && strlen(data->text) > 0) ? data->text : data->placeholder;
                SDL_Color display_color = (data->text && strlen(data->text) > 0) ? text_color : (SDL_Color){128, 128, 128, 255};
                
                if (display_text && g_sdl2_impl.default_font) {
                    // Handle password input
                    if (data->is_password && data->text && strlen(data->text) > 0) {
                        char* masked_text = malloc(strlen(data->text) + 1);
                        for (size_t k = 0; k < strlen(data->text); k++) {
                            masked_text[k] = '*';
                        }
                        masked_text[strlen(data->text)] = '\0';
                        draw_text_with_font(ctx->renderer, masked_text,
                                           (KryonVec2){data->position.x + 8, data->position.y + 8},
                                           g_sdl2_impl.default_font, display_color);
                        free(masked_text);
                    } else {
                        draw_text_with_font(ctx->renderer, display_text,
                                           (KryonVec2){data->position.x + 8, data->position.y + 8},
                                           g_sdl2_impl.default_font, display_color);
                    }
                }
                
                // Draw cursor if focused (simple implementation)
                if (data->has_focus && data->text) {
                    // Simple blinking cursor
                    static Uint32 last_blink = 0;
                    Uint32 current_time = SDL_GetTicks();
                    if (current_time - last_blink > 500) {
                        last_blink = current_time;
                    }
                    
                    if ((current_time - last_blink) < 250) {
                        int cursor_x = (int)(data->position.x + 8 + strlen(data->text) * 8); // Approximate
                        SDL_Rect cursor_rect = {cursor_x, (int)(data->position.y + 5), 2, (int)(data->size.y - 10)};
                        SDL_SetRenderDrawColor(ctx->renderer, text_color.r, text_color.g, text_color.b, text_color.a);
                        SDL_RenderFillRect(ctx->renderer, &cursor_rect);
                    }
                }
                break;
            }
            
            case KRYON_CMD_DRAW_CHECKBOX: {
                const KryonDrawCheckboxData* data = &cmd->data.draw_checkbox;
                
                SDL_Color bg_color = kryon_color_to_sdl(data->background_color);
                SDL_Color border_color = kryon_color_to_sdl(data->border_color);
                SDL_Color check_color = kryon_color_to_sdl(data->check_color);
                
                // Calculate checkbox size (square)
                int checkbox_size = (int)(data->size.y * 0.8f);
                int checkbox_x = (int)data->position.x;
                int checkbox_y = (int)(data->position.y + (data->size.y - checkbox_size) / 2);
                
                // Draw checkbox background
                SDL_Rect checkbox_rect = {checkbox_x, checkbox_y, checkbox_size, checkbox_size};
                SDL_SetRenderDrawColor(ctx->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
                SDL_RenderFillRect(ctx->renderer, &checkbox_rect);
                
                // Draw checkbox border
                if (data->border_width > 0) {
                    SDL_SetRenderDrawColor(ctx->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
                    SDL_RenderDrawRect(ctx->renderer, &checkbox_rect);
                }
                
                // Draw checkmark if checked
                if (data->checked) {
                    int check_size = (int)(checkbox_size * 0.6f);
                    int check_x = checkbox_x + (checkbox_size - check_size) / 2;
                    int check_y = checkbox_y + (checkbox_size - check_size) / 2;
                    
                    SDL_Rect check_rect = {check_x, check_y, check_size, check_size};
                    SDL_SetRenderDrawColor(ctx->renderer, check_color.r, check_color.g, check_color.b, check_color.a);
                    SDL_RenderFillRect(ctx->renderer, &check_rect);
                }
                
                // Draw label
                if (data->label && g_sdl2_impl.default_font) {
                    draw_text_with_font(ctx->renderer, data->label,
                                       (KryonVec2){checkbox_x + checkbox_size + 8, data->position.y + 8},
                                       g_sdl2_impl.default_font, (SDL_Color){0, 0, 0, 255});
                }
                break;
            }
            
            case KRYON_CMD_UPDATE_WIDGET_STATE: {
                // Widget state updates would be handled by the element management system
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

static KryonRenderResult sdl2_resize(KryonVec2 new_size) {
    if (!g_sdl2_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    SDL_SetWindowSize(g_sdl2_impl.window, (int)new_size.x, (int)new_size.y);
    g_sdl2_impl.width = (int)new_size.x;
    g_sdl2_impl.height = (int)new_size.y;
    
    return KRYON_RENDER_SUCCESS;
}

static KryonVec2 sdl2_viewport_size(void) {
    return (KryonVec2){(float)g_sdl2_impl.width, (float)g_sdl2_impl.height};
}

static void sdl2_destroy(void) {
    if (g_sdl2_impl.default_font) {
        TTF_CloseFont(g_sdl2_impl.default_font);
        g_sdl2_impl.default_font = NULL;
    }
    
    if (g_sdl2_impl.renderer) {
        SDL_DestroyRenderer(g_sdl2_impl.renderer);
        g_sdl2_impl.renderer = NULL;
    }
    
    if (g_sdl2_impl.window) {
        SDL_DestroyWindow(g_sdl2_impl.window);
        g_sdl2_impl.window = NULL;
    }
    
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    
    g_sdl2_impl.initialized = false;
    printf("SDL2 Renderer destroyed\n");
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static SDL_Color kryon_color_to_sdl(KryonColor color) {
    return (SDL_Color){
        .r = (Uint8)(color.r * 255),
        .g = (Uint8)(color.g * 255),
        .b = (Uint8)(color.b * 255),
        .a = (Uint8)(color.a * 255)
    };
}

static void draw_rounded_rect(SDL_Renderer* renderer, KryonRect rect, float radius, SDL_Color color) {
    // Simplified rounded rectangle - just draw a regular rectangle for now
    // A full implementation would use trigonometry for proper rounded corners
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    SDL_Rect sdl_rect = {
        (int)rect.x, (int)rect.y,
        (int)rect.width, (int)rect.height
    };
    
    SDL_RenderFillRect(renderer, &sdl_rect);
    
    // TODO: Implement proper rounded corners using SDL_RenderDrawPoint
    // for each corner with appropriate radius calculation
}

static void draw_text_with_font(SDL_Renderer* renderer, const char* text, 
                               KryonVec2 pos, TTF_Font* font, SDL_Color color) {
    if (!text || !font) return;
    
    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text, color);
    if (!text_surface) return;
    
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    if (text_texture) {
        SDL_Rect dst_rect = {
            (int)pos.x, (int)pos.y,
            text_surface->w, text_surface->h
        };
        
        SDL_RenderCopy(renderer, text_texture, NULL, &dst_rect);
        SDL_DestroyTexture(text_texture);
    }
    
    SDL_FreeSurface(text_surface);
}

// =============================================================================
// INPUT HANDLING IMPLEMENTATION
// =============================================================================

static KryonKey sdl2_key_to_kryon(SDL_Keycode sdl_key) {
    switch (sdl_key) {
        case SDLK_SPACE: return KRYON_KEY_SPACE;
        case SDLK_RETURN: return KRYON_KEY_ENTER;
        case SDLK_TAB: return KRYON_KEY_TAB;
        case SDLK_BACKSPACE: return KRYON_KEY_BACKSPACE;
        case SDLK_DELETE: return KRYON_KEY_DELETE;
        case SDLK_RIGHT: return KRYON_KEY_RIGHT;
        case SDLK_LEFT: return KRYON_KEY_LEFT;
        case SDLK_DOWN: return KRYON_KEY_DOWN;
        case SDLK_UP: return KRYON_KEY_UP;
        case SDLK_ESCAPE: return KRYON_KEY_ESCAPE;
        case SDLK_a: return KRYON_KEY_A;
        case SDLK_c: return KRYON_KEY_C;
        case SDLK_v: return KRYON_KEY_V;
        case SDLK_x: return KRYON_KEY_X;
        case SDLK_z: return KRYON_KEY_Z;
        default: return KRYON_KEY_UNKNOWN;
    }
}

static KryonRenderResult sdl2_get_input_state(KryonInputState* input_state) {
    if (!input_state) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    // Initialize input state
    memset(input_state, 0, sizeof(KryonInputState));
    
    // Mouse state
    int mouse_x, mouse_y;
    Uint32 mouse_state = SDL_GetMouseState(&mouse_x, &mouse_y);
    
    input_state->mouse.position = (KryonVec2){(float)mouse_x, (float)mouse_y};
    
    // Note: SDL2 doesn't provide mouse delta directly in this context
    // Mouse button states would need to be tracked via events
    
    // Keyboard state
    const Uint8* keyboard_state = SDL_GetKeyboardState(NULL);
    
    // Map common keys
    SDL_Keycode sdl_keys[] = {
        SDLK_SPACE, SDLK_RETURN, SDLK_TAB, SDLK_BACKSPACE, SDLK_DELETE,
        SDLK_RIGHT, SDLK_LEFT, SDLK_DOWN, SDLK_UP, SDLK_ESCAPE,
        SDLK_a, SDLK_c, SDLK_v, SDLK_x, SDLK_z
    };
    
    for (size_t i = 0; i < sizeof(sdl_keys) / sizeof(sdl_keys[0]); i++) {
        SDL_Keycode sdl_key = sdl_keys[i];
        KryonKey kryon_key = sdl2_key_to_kryon(sdl_key);
        SDL_Scancode scancode = SDL_GetScancodeFromKey(sdl_key);
        
        if (kryon_key != KRYON_KEY_UNKNOWN && kryon_key < 512 && scancode != SDL_SCANCODE_UNKNOWN) {
            input_state->keyboard.keys_down[kryon_key] = keyboard_state[scancode] != 0;
            // Note: pressed/released states would need to be tracked via events
        }
    }
    
    return KRYON_RENDER_SUCCESS;
}

static bool sdl2_point_in_element(KryonVec2 point, KryonRect element_bounds) {
    return point.x >= element_bounds.x && 
           point.x <= element_bounds.x + element_bounds.width &&
           point.y >= element_bounds.y && 
           point.y <= element_bounds.y + element_bounds.height;
}

// =============================================================================
// EVENT HANDLING IMPLEMENTATION
// =============================================================================

static bool sdl2_handle_event(const KryonEvent* event) {
    if (!event || !g_sdl2_impl.initialized) return false;
    
    // Handle window events
    switch (event->type) {
        case KRYON_EVENT_WINDOW_RESIZE:
            // SDL2 handles window resize automatically
            g_sdl2_impl.width = event->data.windowResize.width;
            g_sdl2_impl.height = event->data.windowResize.height;
            return true;
            
        case KRYON_EVENT_WINDOW_CLOSE:
            g_sdl2_impl.quit_requested = true;
            return true;
            
        default:
            // Other events are handled by the application
            return false;
    }
}

static void* sdl2_get_native_window(void) {
    return g_sdl2_impl.window;
}