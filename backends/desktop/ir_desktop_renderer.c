#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_serialization.h"
#include "ir_desktop_renderer.h"
#include "../../core/include/kryon_canvas.h"

// Platform-specific includes (conditional compilation)
#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif

// Nim bridge declarations - functions exported from Nim for C to call
extern void nimProcessReactiveUpdates();

// ============================================================================
// TEXT MEASUREMENT CONSTANTS
// ============================================================================
// These constants define heuristics for estimating text dimensions without
// actual font rendering. Used when calculating layout before rendering phase.

// Average character width as a ratio of font size (for proportional fonts)
// Example: 16px font → ~9.6px average character width
#define TEXT_CHAR_WIDTH_RATIO 0.6f

// Line height as a ratio of font size (includes typical line spacing)
// Example: 16px font → ~19.2px line height
#define TEXT_LINE_HEIGHT_RATIO 1.2f

// Desktop IR Renderer Context
struct DesktopIRRenderer {
    DesktopRendererConfig config;
    bool initialized;
    bool running;

    // SDL3-specific fields (only available when compiled with SDL3)
#ifdef ENABLE_SDL3
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* default_font;
    SDL_Cursor* cursor_default;
    SDL_Cursor* cursor_hand;
    SDL_Cursor* current_cursor;
    SDL_Texture* white_texture;  // 1x1 white texture for vertex coloring
#endif

    // Event handling
    DesktopEventCallback event_callback;
    void* event_user_data;

    // Performance tracking
    uint64_t frame_count;
    double last_frame_time;
    double fps;

    // Component rendering cache
    IRComponent* last_root;
    bool needs_relayout;
};

// Color conversion utilities (only when SDL3 is available)
static float ir_dimension_to_pixels(IRDimension dimension, float container_size) {
    switch (dimension.type) {
        case IR_DIMENSION_PX:
            return dimension.value;
        case IR_DIMENSION_PERCENT:
            return (dimension.value / 100.0f) * container_size;
        case IR_DIMENSION_AUTO:
            return 0.0f; // Let layout engine determine
        case IR_DIMENSION_FLEX:
            return dimension.value; // Treat flex as pixels for simplicity
        default:
            return 0.0f;
    }
}

#ifdef ENABLE_SDL3
static SDL_Color ir_color_to_sdl(IRColor color) {
    return (SDL_Color){
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a
    };
}
#endif

// Layout calculation helpers
typedef struct LayoutRect {
    float x, y, width, height;
} LayoutRect;

// Forward declaration
static float get_child_dimension(IRComponent* child, LayoutRect parent_rect, bool is_height);

#ifdef ENABLE_SDL3
// ============================================================================
// TEXT MEASUREMENT AND LAYOUT FLOW
// ============================================================================
//
// TEXT components require special handling in the layout engine because their
// dimensions cannot be determined from style properties alone - they depend on
// the actual text content and font properties.
//
// MEASUREMENT FLOW:
// -----------------
// 1. measure_text_dimensions() calculates natural text size using heuristics:
//    - Width: character_count × font_size × TEXT_CHAR_WIDTH_RATIO (0.6)
//    - Height: font_size × TEXT_LINE_HEIGHT_RATIO (1.2)
//    - Applies max_width constraint from parent container to prevent overflow
//
// 2. get_child_size() calls measure_text_dimensions() during layout phase
//    - Passes parent_rect.width as max_width constraint
//    - Returns measured dimensions for layout calculation
//
// 3. get_child_dimension() also calls measure_text_dimensions() for axis-specific sizing
//    - Used by flexbox layout algorithm
//    - Passes parent_rect.width as max_width constraint
//
// CRITICAL RULES FOR TEXT LAYOUT:
// --------------------------------
// ✓ TEXT should NEVER be stretched by STRETCH alignment (see lines ~1137, ~1174)
// ✓ TEXT should NEVER overflow parent container width (constrained by max_width)
// ✓ TEXT positioning should respect layout engine decisions (no hardcoded centering)
// ✓ TEXT should start at container's top-left (respecting padding)
//
// DEBUGGING:
// ----------
// Set KRYON_TRACE_LAYOUT=1 to see TEXT measurement and constraint application:
//   📏 TEXT width clamped: 480.0 -> 370.0 (container constraint)
//
// Helper function to estimate text dimensions using heuristics
static void measure_text_dimensions(IRComponent* component, float max_width, float* out_width, float* out_height) {
    *out_width = 0.0f;
    *out_height = 0.0f;

    if (!component || !component->text_content) {
        return;
    }

    // Get font size from style or use default
    float font_size = 16.0f;
    if (component->style && component->style->font.size > 0) {
        font_size = component->style->font.size;
    }

    // Estimate dimensions based on text length and font size using heuristic ratios
    size_t text_len = strlen(component->text_content);
    *out_height = font_size * TEXT_LINE_HEIGHT_RATIO;
    *out_width = (float)text_len * font_size * TEXT_CHAR_WIDTH_RATIO;

    // Constrain TEXT to parent container width to prevent overflow
    if (max_width > 0.0f && *out_width > max_width) {
        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    📏 TEXT width clamped: %.1f -> %.1f (container constraint)\n", *out_width, max_width);
        }
        *out_width = max_width;
    }
}
#endif

static LayoutRect calculate_component_layout(IRComponent* component, LayoutRect parent_rect) {
    LayoutRect rect = {0};

    if (!component) return rect;

    // Default to parent bounds for container
    rect.x = parent_rect.x;
    rect.y = parent_rect.y;

    // For TEXT components with AUTO dimensions, start with 0 instead of parent dimensions
    // This allows get_child_size() to detect and measure them properly
    if (component->type == IR_COMPONENT_TEXT) {
        rect.width = 0;
        rect.height = 0;
    } else {
        rect.width = parent_rect.width;
        rect.height = parent_rect.height;
    }

    // Apply component-specific dimensions
    if (component->style) {
        if (component->style->width.type != IR_DIMENSION_AUTO) {
            rect.width = ir_dimension_to_pixels(component->style->width, parent_rect.width);
        }
        if (component->style->height.type != IR_DIMENSION_AUTO) {
            rect.height = ir_dimension_to_pixels(component->style->height, parent_rect.height);
        }

        // For TEXT components, if explicit dimensions resolve to 0, treat as AUTO
        // This allows text measurement to happen in get_child_size()
        if (component->type == IR_COMPONENT_TEXT) {
            if (rect.width <= 0) rect.width = 0;
            if (rect.height <= 0) rect.height = 0;
        }

        // Apply margins
        if (component->style->margin.top > 0) rect.y += component->style->margin.top;
        if (component->style->margin.left > 0) rect.x += component->style->margin.left;
    }

    return rect;
}

// Helper to get child size (both width and height) using the existing layout calculation
// This reuses calculate_component_layout but only extracts width/height, ignoring position
static LayoutRect get_child_size(IRComponent* child, LayoutRect parent_rect) {
    if (!child) {
        LayoutRect zero = {0, 0, 0, 0};
        return zero;
    }

    // Use the existing calculate_component_layout which handles all component types correctly
    LayoutRect layout = calculate_component_layout(child, parent_rect);

    // For AUTO-sized Row/Column, measure children to get actual size
    if ((child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN) && child->child_count > 0) {
        bool is_auto_width = !child->style || child->style->width.type == IR_DIMENSION_AUTO;
        bool is_auto_height = !child->style || child->style->height.type == IR_DIMENSION_AUTO;

        if (is_auto_width || is_auto_height) {
            float total_width = 0, total_height = 0;
            float gap = 20.0f;
            if (child->layout && child->layout->flex.gap > 0) {
                gap = (float)child->layout->flex.gap;
            }

            for (uint32_t i = 0; i < child->child_count; i++) {
                float child_w = get_child_dimension(child->children[i], parent_rect, false);
                float child_h = get_child_dimension(child->children[i], parent_rect, true);

                if (child->type == IR_COMPONENT_COLUMN) {
                    total_height += child_h;
                    if (i < child->child_count - 1) total_height += gap;
                    if (child_w > total_width) total_width = child_w;
                } else { // ROW
                    total_width += child_w;
                    if (i < child->child_count - 1) total_width += gap;
                    if (child_h > total_height) total_height = child_h;
                }
            }

            if (is_auto_width) layout.width = total_width;
            if (is_auto_height) layout.height = total_height;
        }
    }

    // Measure TEXT components using text dimensions
    if (child->type == IR_COMPONENT_TEXT) {
        // TEXT should auto-size if:
        // 1. No style at all
        // 2. Dimension type is AUTO
        // In both cases, calculate_component_layout will have returned 0 dimensions
        // So we just check if the calculated dimensions are 0
        if (layout.width <= 0.0f || layout.height <= 0.0f) {
#ifdef ENABLE_SDL3
            float text_width = 0.0f, text_height = 0.0f;
            measure_text_dimensions(child, parent_rect.width, &text_width, &text_height);

            if (layout.width <= 0.0f && text_width > 0.0f) layout.width = text_width;
            if (layout.height <= 0.0f && text_height > 0.0f) layout.height = text_height;
#endif
        }
    }

    // Return a rect with just width/height, position set to 0
    // (position will be set later in the main layout loop)
    LayoutRect result = {0, 0, layout.width, layout.height};
    return result;
}

// Helper to get child size without recursive layout
// IMPORTANT: This function must NOT recurse into child layout to prevent infinite loops
static float get_child_dimension(IRComponent* child, LayoutRect parent_rect, bool is_height) {
    if (!child) return 0.0f;

    // Check if component has explicit size in style
    if (child->style) {
        IRDimension style_dim = is_height ? child->style->height : child->style->width;
        if (style_dim.type != IR_DIMENSION_AUTO) {
            // Has explicit size, use it
            float result = ir_dimension_to_pixels(style_dim, is_height ? parent_rect.height : parent_rect.width);

            // WORKAROUND: Treat explicit 0 dimensions as AUTO for Row/Column/Text
            // This fixes DSL bug where these components get explicit height=0/width=0 instead of AUTO
            if (result == 0.0f && (child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN || child->type == IR_COMPONENT_TEXT)) {
                if (getenv("KRYON_TRACE_LAYOUT")) {
                    const char* type_name = "OTHER";
                    if (child->type == IR_COMPONENT_ROW) type_name = "ROW";
                    else if (child->type == IR_COMPONENT_COLUMN) type_name = "COLUMN";
                    else if (child->type == IR_COMPONENT_TEXT) type_name = "TEXT";
                    printf("    🔍 %s has EXPLICIT %s: %.1f --> treating as AUTO\n",
                           type_name, is_height ? "height" : "width", result);
                }
                // Fall through to AUTO sizing logic below
            } else {
                if (getenv("KRYON_TRACE_LAYOUT")) {
                    const char* type_name = "OTHER";
                    if (child->type == IR_COMPONENT_ROW) type_name = "ROW";
                    else if (child->type == IR_COMPONENT_COLUMN) type_name = "COLUMN";
                    else if (child->type == IR_COMPONENT_BUTTON) type_name = "BUTTON";
                    else if (child->type == IR_COMPONENT_TEXT) type_name = "TEXT";
                    printf("    🔍 %s has EXPLICIT %s: %.1f\n", type_name, is_height ? "height" : "width", result);
                }
                return result;
            }
        }
    }

    // Component has AUTO size - handle based on type
    if (child->type == IR_COMPONENT_TEXT || child->type == IR_COMPONENT_CHECKBOX) {
        // Text/checkbox auto-size to content
#ifdef ENABLE_SDL3
        float text_width = 0.0f, text_height = 0.0f;
        measure_text_dimensions(child, parent_rect.width, &text_width, &text_height);

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    📝 TEXT measurement result: width=%.1f, height=%.1f\n", text_width, text_height);
        }

        if (is_height && text_height > 0.0f) {
            return text_height;
        } else if (!is_height && text_width > 0.0f) {
            return text_width;
        }
#endif
        // Fallback if measurement failed or not available
        if (is_height) {
            float font_size = 16.0f;
            if (child->style && child->style->font.size > 0) {
                font_size = child->style->font.size;
            }
            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("    📝 TEXT fallback height: %.1f (font_size=%.1f)\n", font_size * 1.2f, font_size);
            }
            return font_size * 1.2f;
        }
        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    📝 TEXT fallback width: 0.0\n");
        }
        return 0.0f;  // Text width is auto (fallback)
    }

    // For Container/Row/Column with AUTO size, we need to measure children
    // But we CANNOT recurse into full layout - just sum up their explicit sizes
    if (getenv("KRYON_TRACE_LAYOUT")) {
        const char* type_name = "OTHER";
        if (child->type == IR_COMPONENT_ROW) type_name = "ROW";
        else if (child->type == IR_COMPONENT_COLUMN) type_name = "COLUMN";
        else if (child->type == IR_COMPONENT_CONTAINER) type_name = "CONTAINER";
        printf("    🔍 Checking %s for recursive measurement: type_match=%d, child_count=%d\n",
               type_name,
               (child->type == IR_COMPONENT_CONTAINER || child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN),
               child->child_count);
    }

    if ((child->type == IR_COMPONENT_CONTAINER ||
         child->type == IR_COMPONENT_ROW ||
         child->type == IR_COMPONENT_COLUMN) &&
        child->child_count > 0) {

        float total = 0.0f;
        float gap = 20.0f;
        if (child->layout && child->layout->flex.gap > 0) {
            gap = (float)child->layout->flex.gap;
        }

        // Determine if we're measuring the main axis
        bool is_main_axis = (is_height && child->type == IR_COMPONENT_COLUMN) ||
                           (!is_height && child->type == IR_COMPONENT_ROW);

        if (getenv("KRYON_TRACE_LAYOUT")) {
            const char* child_type_name = child->type == IR_COMPONENT_ROW ? "ROW" :
                                         child->type == IR_COMPONENT_COLUMN ? "COLUMN" : "CONTAINER";
            printf("    📐 Measuring %s's %s (main_axis=%d, %d children)\n",
                   child_type_name, is_height ? "HEIGHT" : "WIDTH", is_main_axis, child->child_count);
        }

        for (uint32_t i = 0; i < child->child_count; i++) {
            IRComponent* grandchild = child->children[i];

            // Get grandchild's explicit dimension only (no recursion)
            float child_dim = 0.0f;
            if (grandchild->style) {
                IRDimension gd = is_height ? grandchild->style->height : grandchild->style->width;
                if (gd.type != IR_DIMENSION_AUTO) {
                    // For grandchildren, we need to use ir_dimension_to_pixels to handle all dimension types
                    child_dim = ir_dimension_to_pixels(gd, is_height ? parent_rect.height : parent_rect.width);
                }
            }

            if (getenv("KRYON_TRACE_LAYOUT")) {
                const char* gc_type_name = "OTHER";
                if (grandchild) {
                    switch (grandchild->type) {
                        case IR_COMPONENT_BUTTON: gc_type_name = "BUTTON"; break;
                        case IR_COMPONENT_TEXT: gc_type_name = "TEXT"; break;
                        case IR_COMPONENT_ROW: gc_type_name = "ROW"; break;
                        case IR_COMPONENT_COLUMN: gc_type_name = "COLUMN"; break;
                        default: break;
                    }
                }
                printf("      ├─ grandchild[%d] %s: %s=%.1f\n", i, gc_type_name, is_height ? "height" : "width", child_dim);
            }

            if (is_main_axis) {
                // Sum up children along main axis
                total += child_dim;
                if (i < child->child_count - 1) {
                    total += gap;
                }
            } else {
                // Take max along cross axis
                if (child_dim > total) {
                    total = child_dim;
                }
            }
        }

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("      └─ Total %s: %.1f\n", is_height ? "HEIGHT" : "WIDTH", total);
        }

        return total;
    }

    // Default: return 0 for unknown AUTO-sized components
    return 0.0f;
}

// Platform-specific rendering implementations
#ifdef ENABLE_SDL3
static bool render_component_sdl3(DesktopIRRenderer* renderer, IRComponent* component, LayoutRect rect) {
    if (!renderer || !component || !renderer->renderer) return false;

    // Cache rendered bounds for hit testing
    ir_set_rendered_bounds(component, rect.x, rect.y, rect.width, rect.height);

    SDL_FRect sdl_rect = {
        .x = rect.x,
        .y = rect.y,
        .w = rect.width,
        .h = rect.height
    };

    // Render based on component type
    switch (component->type) {
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            // Only render background if component has a style with non-transparent background
            if (component->style) {
                SDL_Color bg_color = ir_color_to_sdl(component->style->background);
                // Only render if alpha > 0 (not fully transparent)
                if (bg_color.a > 0) {
                    SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
                    SDL_RenderFillRect(renderer->renderer, &sdl_rect);
                }
            }
            break;

        case IR_COMPONENT_CENTER:
            // Center is a layout-only component, completely transparent - don't render background
            break;

        case IR_COMPONENT_BUTTON:
            // Set button background color
            if (component->style) {
                SDL_Color bg_color = ir_color_to_sdl(component->style->background);
                SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            } else {
                SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 200, 255); // Default blue
            }
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);

            // Draw button border
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderRect(renderer->renderer, &sdl_rect);

            // Render button text if present
            if (component->text_content && renderer->default_font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){0, 0, 0, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font,
                                                              component->text_content,
                                                              strlen(component->text_content),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        // Center text in button (round to integer pixels for crisp rendering)
                        SDL_FRect text_rect = {
                            .x = roundf(rect.x + (rect.width - surface->w) / 2),
                            .y = roundf(rect.y + (rect.height - surface->h) / 2),
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;

        case IR_COMPONENT_TEXT:
            if (component->text_content && renderer->default_font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){0, 0, 0, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font,
                                                              component->text_content,
                                                              strlen(component->text_content),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        // Render text at the position determined by the layout engine (round to integer pixels)
                        float text_x = roundf(rect.x);
                        SDL_FRect text_rect = {
                            .x = text_x,
                            .y = roundf(rect.y),  // Round Y to integer pixel
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;

        case IR_COMPONENT_INPUT:
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderRect(renderer->renderer, &sdl_rect);
            break;

        case IR_COMPONENT_CHECKBOX: {
            // Checkbox is rendered as a small box on the left + label text
            float checkbox_size = fminf(rect.height * 0.6f, 20.0f);
            float checkbox_y = rect.y + (rect.height - checkbox_size) / 2;

            SDL_FRect checkbox_rect = {
                .x = rect.x + 5,
                .y = checkbox_y,
                .w = checkbox_size,
                .h = checkbox_size
            };

            // Draw checkbox background
            SDL_Color bg_color = component->style ?
                ir_color_to_sdl(component->style->background) :
                (SDL_Color){255, 255, 255, 255};
            SDL_SetRenderDrawColor(renderer->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            SDL_RenderFillRect(renderer->renderer, &checkbox_rect);

            // Draw checkbox border
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderRect(renderer->renderer, &checkbox_rect);

            // Check if checkbox is checked (stored in custom_data)
            bool is_checked = component->custom_data &&
                             strcmp(component->custom_data, "checked") == 0;

            // Draw checkmark if checked
            if (is_checked) {
                SDL_SetRenderDrawColor(renderer->renderer, 50, 200, 50, 255);
                // Draw a simple checkmark using lines
                float cx = checkbox_rect.x + checkbox_size / 2;
                float cy = checkbox_rect.y + checkbox_size / 2;
                float size = checkbox_size * 0.3f;

                SDL_RenderLine(renderer->renderer,
                              cx - size, cy,
                              cx - size/3, cy + size);
                SDL_RenderLine(renderer->renderer,
                              cx - size/3, cy + size,
                              cx + size, cy - size);
            }

            // Render label text next to checkbox
            if (component->text_content && renderer->default_font) {
                SDL_Color text_color = component->style ?
                    ir_color_to_sdl(component->style->font.color) :
                    (SDL_Color){0, 0, 0, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(renderer->default_font,
                                                              component->text_content,
                                                              strlen(component->text_content),
                                                              text_color);
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
                    if (texture) {
                        // Round text position to integer pixels for crisp rendering
                        SDL_FRect text_rect = {
                            .x = roundf(checkbox_rect.x + checkbox_size + 10),
                            .y = roundf(rect.y + (rect.height - surface->h) / 2.0f),
                            .w = (float)surface->w,
                            .h = (float)surface->h
                        };
                        SDL_RenderTexture(renderer->renderer, texture, NULL, &text_rect);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
            }
            break;
        }

        case IR_COMPONENT_CANVAS: {
            // Initialize canvas for this component's bounds
            extern kryon_cmd_buf_t* kryon_canvas_get_command_buffer(void);
            kryon_cmd_buf_t* existing_buf = kryon_canvas_get_command_buffer();

            if (existing_buf == NULL) {
                // First time - initialize canvas
                kryon_canvas_init(rect.width, rect.height);
            } else {
                // Already initialized - just clear command buffer for this frame
                extern void kryon_cmd_buf_clear(kryon_cmd_buf_t* buf);
                kryon_cmd_buf_clear(existing_buf);
            }

            kryon_canvas_set_offset(rect.x, rect.y);

            // Call the Nim callback - it will populate the canvas's command buffer
            extern void nimCanvasBridge(uint32_t componentId);
            nimCanvasBridge(component->id);

            // Get the canvas command buffer and execute commands using SDL3
            extern kryon_cmd_buf_t* kryon_canvas_get_command_buffer(void);
            extern kryon_cmd_iterator_t kryon_cmd_iter_create(kryon_cmd_buf_t* buf);
            extern bool kryon_cmd_iter_has_next(kryon_cmd_iterator_t* iter);
            extern bool kryon_cmd_iter_next(kryon_cmd_iterator_t* iter, kryon_command_t* cmd);

            kryon_cmd_buf_t* canvas_buf = kryon_canvas_get_command_buffer();
            if (canvas_buf) {
                kryon_cmd_iterator_t iter = kryon_cmd_iter_create(canvas_buf);
                kryon_command_t cmd;
                while (kryon_cmd_iter_has_next(&iter)) {
                    if (!kryon_cmd_iter_next(&iter, &cmd)) break;

                    switch (cmd.type) {
                        case KRYON_CMD_DRAW_POLYGON: {
                            const kryon_fp_t* vertices = cmd.data.draw_polygon.vertices;
                            uint16_t vertex_count = cmd.data.draw_polygon.vertex_count;
                            uint32_t color = cmd.data.draw_polygon.color;
                            bool filled = cmd.data.draw_polygon.filled;

                            if (filled && vertex_count >= 3) {
                                // Draw filled polygon using triangle fan
                                uint8_t r = (color >> 24) & 0xFF;
                                uint8_t g = (color >> 16) & 0xFF;
                                uint8_t b = (color >> 8) & 0xFF;
                                uint8_t a = color & 0xFF;

                                fprintf(stderr, "[SDL] POLYGON color=0x%08x unpacked to r=%d g=%d b=%d a=%d\n", color, r, g, b, a);

                                SDL_Vertex* sdl_vertices = malloc(vertex_count * sizeof(SDL_Vertex));
                                for (uint16_t i = 0; i < vertex_count; i++) {
                                    sdl_vertices[i].position.x = vertices[i * 2];
                                    sdl_vertices[i].position.y = vertices[i * 2 + 1];
                                    // SDL3 uses SDL_FColor with float values 0.0-1.0, not bytes 0-255
                                    sdl_vertices[i].color.r = r / 255.0f;
                                    sdl_vertices[i].color.g = g / 255.0f;
                                    sdl_vertices[i].color.b = b / 255.0f;
                                    sdl_vertices[i].color.a = a / 255.0f;
                                    sdl_vertices[i].tex_coord.x = 0.0f;
                                    sdl_vertices[i].tex_coord.y = 0.0f;
                                }

                                // Create triangle fan indices
                                int* indices = malloc((vertex_count - 2) * 3 * sizeof(int));
                                for (uint16_t i = 0; i < vertex_count - 2; i++) {
                                    indices[i * 3] = 0;
                                    indices[i * 3 + 1] = i + 1;
                                    indices[i * 3 + 2] = i + 2;
                                }

                                // Reset render draw color to white to prevent color modulation
                                SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
                                SDL_RenderGeometry(renderer->renderer, renderer->white_texture, sdl_vertices, vertex_count, indices, (vertex_count - 2) * 3);
                                free(sdl_vertices);
                                free(indices);
                            }
                            break;
                        }
                        case KRYON_CMD_DRAW_RECT: {
                            uint8_t r = (cmd.data.draw_rect.color >> 24) & 0xFF;
                            uint8_t g = (cmd.data.draw_rect.color >> 16) & 0xFF;
                            uint8_t b = (cmd.data.draw_rect.color >> 8) & 0xFF;
                            uint8_t a = cmd.data.draw_rect.color & 0xFF;
                            SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a);
                            SDL_FRect rect = {cmd.data.draw_rect.x, cmd.data.draw_rect.y, cmd.data.draw_rect.w, cmd.data.draw_rect.h};
                            SDL_RenderFillRect(renderer->renderer, &rect);
                            break;
                        }
                        case KRYON_CMD_DRAW_LINE: {
                            uint8_t r = (cmd.data.draw_line.color >> 24) & 0xFF;
                            uint8_t g = (cmd.data.draw_line.color >> 16) & 0xFF;
                            uint8_t b = (cmd.data.draw_line.color >> 8) & 0xFF;
                            uint8_t a = cmd.data.draw_line.color & 0xFF;
                            SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a);
                            SDL_RenderLine(renderer->renderer, cmd.data.draw_line.x1, cmd.data.draw_line.y1, cmd.data.draw_line.x2, cmd.data.draw_line.y2);
                            break;
                        }
                        case KRYON_CMD_DRAW_TEXT: {
                            // Extract color components
                            uint8_t r = (cmd.data.draw_text.color >> 24) & 0xFF;
                            uint8_t g = (cmd.data.draw_text.color >> 16) & 0xFF;
                            uint8_t b = (cmd.data.draw_text.color >> 8) & 0xFF;
                            uint8_t a = cmd.data.draw_text.color & 0xFF;

                            // Render text using SDL_ttf
                            if (!renderer->default_font) {
                                break;
                            }

                            SDL_Color text_color = {r, g, b, a};
                            size_t text_len = strlen(cmd.data.draw_text.text_storage);

                            SDL_Surface* text_surface = TTF_RenderText_Blended(renderer->default_font, cmd.data.draw_text.text_storage, text_len, text_color);
                            if (!text_surface) {
                                break;
                            }

                            SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer->renderer, text_surface);
                            if (!text_texture) {
                                SDL_DestroySurface(text_surface);
                                break;
                            }

                            SDL_FRect dest_rect = {
                                (float)cmd.data.draw_text.x,
                                (float)cmd.data.draw_text.y,
                                (float)text_surface->w,
                                (float)text_surface->h
                            };

                            SDL_RenderTexture(renderer->renderer, text_texture, NULL, &dest_rect);

                            SDL_DestroyTexture(text_texture);
                            SDL_DestroySurface(text_surface);
                            break;
                        }
                        case KRYON_CMD_DRAW_ARC: {
                            // Extract arc parameters
                            int16_t cx = cmd.data.draw_arc.cx;
                            int16_t cy = cmd.data.draw_arc.cy;
                            uint16_t radius = cmd.data.draw_arc.radius;
                            int16_t start_angle = cmd.data.draw_arc.start_angle;
                            int16_t end_angle = cmd.data.draw_arc.end_angle;
                            uint32_t color = cmd.data.draw_arc.color;

                            // Extract color components
                            uint8_t r = (color >> 24) & 0xFF;
                            uint8_t g = (color >> 16) & 0xFF;
                            uint8_t b = (color >> 8) & 0xFF;
                            uint8_t a = color & 0xFF;

                            // Tessellate arc into line segments
                            const int segments = 32;  // More segments for smoother arcs
                            SDL_FPoint* points = malloc((segments + 1) * sizeof(SDL_FPoint));
                            if (!points) break;

                            // Calculate arc span
                            float angle_span = end_angle - start_angle;

                            // Generate arc vertices
                            for (int i = 0; i <= segments; i++) {
                                float t = (float)i / segments;
                                float angle_deg = start_angle + angle_span * t;
                                float angle_rad = angle_deg * M_PI / 180.0f;

                                points[i].x = cx + radius * cosf(angle_rad);
                                points[i].y = cy + radius * sinf(angle_rad);
                            }

                            // Draw arc as connected line segments
                            SDL_SetRenderDrawColor(renderer->renderer, r, g, b, a);
                            SDL_RenderLines(renderer->renderer, points, segments + 1);

                            free(points);
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            break;
        }

        default:
            SDL_RenderFillRect(renderer->renderer, &sdl_rect);
            break;
    }

    // Render children
    LayoutRect child_rect = rect;

    // For AUTO-sized Row/Column components, use their measured content dimensions
    // for child alignment calculations instead of 0 or parent's full height
    if ((component->type == IR_COMPONENT_COLUMN || component->type == IR_COMPONENT_ROW)) {
        // Get measured content dimensions
        float content_width = get_child_dimension(component, rect, false);
        float content_height = get_child_dimension(component, rect, true);

        // If rect dimensions are 0 (AUTO-sized), use measured content dimensions
        if (rect.width == 0.0f && content_width > 0.0f) {
            child_rect.width = content_width;
        }
        if (rect.height == 0.0f && content_height > 0.0f) {
            child_rect.height = content_height;
        }
    }

    if (component->style) {
        child_rect.x += component->style->padding.left;
        child_rect.y += component->style->padding.top;
        child_rect.width -= (component->style->padding.left + component->style->padding.right);
        child_rect.height -= (component->style->padding.top + component->style->padding.bottom);
    }

    // Two-pass layout for COLUMN: measure total height, then position based on justify_content
    float start_y = child_rect.y;
    float start_x = child_rect.x;

    #ifdef KRYON_TRACE_LAYOUT
    if (component->type == IR_COMPONENT_COLUMN) {
        printf("🔎 COLUMN check: child_count=%d, child_rect.height=%.1f\n", component->child_count, child_rect.height);
    }
    #endif

    if (component->type == IR_COMPONENT_COLUMN && component->child_count > 0) {
        // Pass 1: Measure total content height (just get sizes, don't do full layout)
        float total_height = 0.0f;
        float gap = 20.0f;
        if (component->layout && component->layout->flex.gap > 0) {
            gap = (float)component->layout->flex.gap;
        }

        for (uint32_t i = 0; i < component->child_count; i++) {
            // Use get_child_dimension to correctly measure all component types
            float child_height = get_child_dimension(component->children[i], child_rect, true);

            if (getenv("KRYON_TRACE_LAYOUT")) {
                const char* child_type_name = "OTHER";
                if (component->children[i]) {
                    switch (component->children[i]->type) {
                        case IR_COMPONENT_ROW: child_type_name = "ROW"; break;
                        case IR_COMPONENT_COLUMN: child_type_name = "COLUMN"; break;
                        case IR_COMPONENT_BUTTON: child_type_name = "BUTTON"; break;
                        case IR_COMPONENT_TEXT: child_type_name = "TEXT"; break;
                        default: break;
                    }
                }
                printf("  🔍 Child %d (%s): measured height=%.1f\n", i, child_type_name, child_height);
            }

            total_height += child_height;
            if (i < component->child_count - 1) {  // Don't add gap after last child
                total_height += gap;
            }
        }

        #ifdef KRYON_TRACE_LAYOUT
        printf("  📏 Total height: %.1f, child_rect.height: %.1f\n", total_height, child_rect.height);
        #endif

        // Calculate starting Y based on justify_content (main axis alignment for column)
        if (component->layout) {
            #ifdef KRYON_TRACE_LAYOUT
            printf("  🎯 Alignment mode: %d\n", component->layout->flex.justify_content);
            #endif

            switch (component->layout->flex.justify_content) {
                case IR_ALIGNMENT_CENTER:
                    // Center if container has explicit non-zero height
                    if (child_rect.height > 0) {
                        float offset = (child_rect.height - total_height) / 2.0f;
                        start_y = child_rect.y + (offset > 0 ? offset : 0);  // Don't go negative
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  ↔️  CENTER: offset=%.1f, start_y=%.1f\n", offset, start_y);
                        #endif
                    } else {
                        start_y = child_rect.y;
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  ↔️  CENTER: child_rect.height=0, using START\n");
                        #endif
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Align to end if container has explicit non-zero height
                    if (child_rect.height > 0) {
                        float offset = child_rect.height - total_height;
                        start_y = child_rect.y + (offset > 0 ? offset : 0);  // Don't go negative
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  ⬇️  END: offset=%.1f, start_y=%.1f\n", offset, start_y);
                        #endif
                    } else {
                        start_y = child_rect.y;
                        #ifdef KRYON_TRACE_LAYOUT
                        printf("  ⬇️  END: child_rect.height=0, using START\n");
                        #endif
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    start_y = child_rect.y;
                    #ifdef KRYON_TRACE_LAYOUT
                    printf("  ⬆️  START: start_y=%.1f\n", start_y);
                    #endif
                    break;
                // Space distribution modes handled differently in Pass 2
                case IR_ALIGNMENT_SPACE_BETWEEN:
                case IR_ALIGNMENT_SPACE_AROUND:
                case IR_ALIGNMENT_SPACE_EVENLY:
                    start_y = child_rect.y;  // Will be calculated per-item in Pass 2
                    break;
            }
        }
    }

    // Two-pass layout for ROW: measure total width, then position based on justify_content
    if (component->type == IR_COMPONENT_ROW && component->child_count > 0) {
        // Pass 1: Measure total content width (just get sizes, don't do full layout)
        float total_width = 0.0f;
        float gap = 20.0f;
        if (component->layout && component->layout->flex.gap > 0) {
            gap = (float)component->layout->flex.gap;
        }

        for (uint32_t i = 0; i < component->child_count; i++) {
            // Use get_child_dimension to correctly measure all component types
            float child_width = get_child_dimension(component->children[i], child_rect, false);

            total_width += child_width;
            if (i < component->child_count - 1) {  // Don't add gap after last child
                total_width += gap;
            }
        }

        // Calculate starting X based on justify_content (main axis alignment for row)
        if (component->layout) {
            switch (component->layout->flex.justify_content) {
                case IR_ALIGNMENT_CENTER:
                    // Center if container has explicit non-zero width
                    if (child_rect.width > 0) {
                        float offset = (child_rect.width - total_width) / 2.0f;
                        start_x = child_rect.x + (offset > 0 ? offset : 0);  // Don't go negative
                    } else {
                        start_x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Align to end if container has explicit non-zero width
                    if (child_rect.width > 0) {
                        float offset = child_rect.width - total_width;
                        start_x = child_rect.x + (offset > 0 ? offset : 0);  // Don't go negative
                    } else {
                        start_x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    start_x = child_rect.x;
                    break;
                // Space distribution modes handled differently in Pass 2
                case IR_ALIGNMENT_SPACE_BETWEEN:
                case IR_ALIGNMENT_SPACE_AROUND:
                case IR_ALIGNMENT_SPACE_EVENLY:
                    start_x = child_rect.x;  // Will be calculated per-item in Pass 2
                    break;
            }
        }
    }

    // Calculate spacing for space distribution modes
    float item_gap = 20.0f;  // Default gap
    if (component->layout && component->layout->flex.gap > 0) {
        item_gap = (float)component->layout->flex.gap;
    }

    // For space distribution, calculate custom gap
    float available_space = 0.0f;
    float distributed_gap = item_gap;

    // Store original child_rect for measurements
    LayoutRect measure_rect = child_rect;

    // COLUMN space distribution
    if (component->type == IR_COMPONENT_COLUMN && component->layout && component->child_count > 0) {
        switch (component->layout->flex.justify_content) {
            case IR_ALIGNMENT_SPACE_BETWEEN: {
                // Equal space between items, no space at edges
                float total_height = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_height = get_child_dimension(component->children[i], measure_rect, true);
                    total_height += child_height;
                }
                available_space = child_rect.height - total_height;
                if (component->child_count > 1 && available_space > 0) {
                    distributed_gap = available_space / (component->child_count - 1);
                } else {
                    distributed_gap = 0;  // No space to distribute, pack items tightly
                }
                start_y = child_rect.y;  // Start at top
                break;
            }
            case IR_ALIGNMENT_SPACE_AROUND: {
                // Equal space around each item (half space at edges)
                float total_height = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_height = get_child_dimension(component->children[i], measure_rect, true);
                    total_height += child_height;
                }
                available_space = child_rect.height - total_height;
                if (available_space > 0) {
                    distributed_gap = available_space / component->child_count;
                    start_y = child_rect.y + distributed_gap / 2.0f;  // Start with half gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_y = child_rect.y;
                }
                break;
            }
            case IR_ALIGNMENT_SPACE_EVENLY: {
                // Equal space everywhere including edges
                float total_height = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_height = get_child_dimension(component->children[i], measure_rect, true);
                    total_height += child_height;
                }
                available_space = child_rect.height - total_height;
                if (available_space > 0) {
                    distributed_gap = available_space / (component->child_count + 1);
                    start_y = child_rect.y + distributed_gap;  // Start with full gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_y = child_rect.y;
                }
                break;
            }
            default:
                break;
        }
    }

    // ROW space distribution
    if (component->type == IR_COMPONENT_ROW && component->layout && component->child_count > 0) {
        switch (component->layout->flex.justify_content) {
            case IR_ALIGNMENT_SPACE_BETWEEN: {
                // Equal space between items, no space at edges
                float total_width = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_width = get_child_dimension(component->children[i], measure_rect, false);
                    total_width += child_width;
                }
                available_space = child_rect.width - total_width;
                if (component->child_count > 1 && available_space > 0) {
                    distributed_gap = available_space / (component->child_count - 1);
                } else {
                    distributed_gap = 0;  // No space to distribute, pack items tightly
                }
                start_x = child_rect.x;  // Start at left
                break;
            }
            case IR_ALIGNMENT_SPACE_AROUND: {
                // Equal space around each item (half space at edges)
                float total_width = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_width = get_child_dimension(component->children[i], measure_rect, false);
                    total_width += child_width;
                }
                available_space = child_rect.width - total_width;
                if (available_space > 0) {
                    distributed_gap = available_space / component->child_count;
                    start_x = child_rect.x + distributed_gap / 2.0f;  // Start with half gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_x = child_rect.x;
                }
                break;
            }
            case IR_ALIGNMENT_SPACE_EVENLY: {
                // Equal space everywhere including edges
                float total_width = 0.0f;
                for (uint32_t i = 0; i < component->child_count; i++) {
                    float child_width = get_child_dimension(component->children[i], measure_rect, false);
                    total_width += child_width;
                }
                available_space = child_rect.width - total_width;
                if (available_space > 0) {
                    distributed_gap = available_space / (component->child_count + 1);
                    start_x = child_rect.x + distributed_gap;  // Start with full gap
                } else {
                    distributed_gap = 0;  // No space to distribute
                    start_x = child_rect.x;
                }
                break;
            }
            default:
                break;
        }
    }

    // Track current position separately from child_rect to avoid modifying container bounds
    // Initialize from start_x/start_y which already account for alignment
    float current_x = start_x;
    float current_y = start_y;

    // Layout tracing (only when enabled)
    if (getenv("KRYON_TRACE_LAYOUT")) {
        const char* type_name = component->type == IR_COMPONENT_COLUMN ? "COLUMN" :
                               component->type == IR_COMPONENT_ROW ? "ROW" : "OTHER";
        if (component->layout) {
            printf("📦 Layout %s: start=(%.1f,%.1f) child_rect=(%.1f,%.1f %.1fx%.1f) gap=%.1f main_axis=%d cross_axis=%d\n",
                   type_name, start_x, start_y, child_rect.x, child_rect.y,
                   child_rect.width, child_rect.height, distributed_gap,
                   component->layout->flex.main_axis, component->layout->flex.cross_axis);
        } else {
            printf("📦 Layout %s: start=(%.1f,%.1f) child_rect=(%.1f,%.1f %.1fx%.1f) gap=%.1f\n",
                   type_name, start_x, start_y, child_rect.x, child_rect.y,
                   child_rect.width, child_rect.height, distributed_gap);
        }
    }

    for (uint32_t i = 0; i < component->child_count; i++) {
        IRComponent* child = component->children[i];
        // Get child size WITHOUT recursive layout (just from style)
        LayoutRect child_layout = get_child_size(child, child_rect);

        // Now position the child based on current_x/current_y (main axis)
        // and alignment settings (cross axis)

        // Set main axis position
        child_layout.x = current_x;
        child_layout.y = current_y;

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("  📍 Child %d BEFORE align: pos=(%.1f,%.1f) size=(%.1fx%.1f)\n",
                   i, child_layout.x, child_layout.y, child_layout.width, child_layout.height);
        }

        // Special handling for Center component - center the child
        if (component->type == IR_COMPONENT_CENTER) {
            child_layout.x = child_rect.x + (child_rect.width - child_layout.width) / 2;
            child_layout.y = child_rect.y + (child_rect.height - child_layout.height) / 2;
        }

        // Apply horizontal alignment for COLUMN layout based on cross_axis (align_items)
        if (component->type == IR_COMPONENT_COLUMN && component->layout) {
            switch (component->layout->flex.cross_axis) {
                case IR_ALIGNMENT_CENTER:
                    // Only center if child fits in container, otherwise align to start
                    if (child_layout.width <= child_rect.width) {
                        child_layout.x = child_rect.x + (child_rect.width - child_layout.width) / 2;
                    } else {
                        child_layout.x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Only align to end if child fits, otherwise align to start
                    if (child_layout.width <= child_rect.width) {
                        child_layout.x = child_rect.x + child_rect.width - child_layout.width;
                    } else {
                        child_layout.x = child_rect.x;
                    }
                    break;
                case IR_ALIGNMENT_STRETCH:
                    child_layout.x = child_rect.x;
                    // Only stretch if child doesn't have explicit width AND container has non-zero width
                    // Never stretch TEXT components - they should use their measured width
                    // Check if child has explicit width (not AUTO)
                    bool has_explicit_width = child && child->style &&
                                            child->style->width.type != IR_DIMENSION_AUTO;
                    if (!has_explicit_width && child_rect.width > 0 && child->type != IR_COMPONENT_TEXT) {
                        child_layout.width = child_rect.width;
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    child_layout.x = child_rect.x;
                    break;
            }
        }

        // Apply vertical alignment for ROW layout based on cross_axis (align_items)
        if (component->type == IR_COMPONENT_ROW && component->layout) {
            switch (component->layout->flex.cross_axis) {
                case IR_ALIGNMENT_CENTER:
                    // Only center if child fits in container, otherwise align to start
                    if (child_layout.height <= child_rect.height) {
                        child_layout.y = child_rect.y + (child_rect.height - child_layout.height) / 2;
                    } else {
                        child_layout.y = child_rect.y;
                    }
                    break;
                case IR_ALIGNMENT_END:
                    // Only align to end if child fits, otherwise align to start
                    if (child_layout.height <= child_rect.height) {
                        child_layout.y = child_rect.y + child_rect.height - child_layout.height;
                    } else {
                        child_layout.y = child_rect.y;
                    }
                    break;
                case IR_ALIGNMENT_STRETCH:
                    child_layout.y = child_rect.y;
                    // Only stretch if child doesn't have explicit height AND container has non-zero height
                    // Never stretch TEXT components - they should use their measured height
                    // Check if child has explicit height (not AUTO)
                    bool has_explicit_height = child && child->style &&
                                             child->style->height.type != IR_DIMENSION_AUTO;
                    if (!has_explicit_height && child_rect.height > 0 && child->type != IR_COMPONENT_TEXT) {
                        child_layout.height = child_rect.height;
                    }
                    break;
                case IR_ALIGNMENT_START:
                default:
                    child_layout.y = child_rect.y;
                    break;
            }
        }

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("  ✅ Child %d AFTER align: pos=(%.1f,%.1f) size=(%.1fx%.1f)\n",
                   i, child_layout.x, child_layout.y, child_layout.width, child_layout.height);
        }

        // For AUTO-sized Row/Column children, pass the available space from parent's child_rect
        // instead of the measured child size, so they can properly calculate alignment
        LayoutRect rect_for_child = child_layout;

        if (child && (child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN)) {
            // Detect AUTO-sized Row/Column by checking if measured size is 0
            // This is more reliable than checking style, since style defaults may vary
            bool is_auto_width = child_layout.width == 0.0f;
            bool is_auto_height = child_layout.height == 0.0f;

            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("  🔧 AUTO-sized %s detected: w=%.1f (auto=%d), h=%.1f (auto=%d), available=(%.1fx%.1f)\n",
                       child->type == IR_COMPONENT_ROW ? "ROW" : "COLUMN",
                       child_layout.width, is_auto_width,
                       child_layout.height, is_auto_height,
                       child_rect.width, child_rect.height);
            }

            if (is_auto_width || is_auto_height) {
                // Preserve position from child_layout, but use available space from child_rect
                rect_for_child.x = child_layout.x;
                rect_for_child.y = child_layout.y;
                if (is_auto_width) {
                    rect_for_child.width = child_rect.width;
                }
                if (is_auto_height) {
                    // For auto-height children in a Column with CENTER/END alignment,
                    // use their measured content height instead of full available space
                    // to prevent cross-axis alignment from pushing content off-screen
                    bool parent_is_centering = component->type == IR_COMPONENT_COLUMN &&
                                              component->layout &&
                                              (component->layout->flex.justify_content == IR_ALIGNMENT_CENTER ||
                                               component->layout->flex.justify_content == IR_ALIGNMENT_END);
                    if (parent_is_centering) {
                        // Measure the actual content height of the child
                        float content_height = get_child_dimension(child, child_rect, true);
                        if (content_height > 0) {
                            rect_for_child.height = content_height;
                        } else {
                            // Fallback to measured height if available
                            rect_for_child.height = child_layout.height > 0 ? child_layout.height : child_rect.height;
                        }
                    } else {
                        // Use full available height for stretch/start alignment
                        rect_for_child.height = child_rect.height;
                    }
                }

                if (getenv("KRYON_TRACE_LAYOUT")) {
                    printf("  🔧 FIXED: Passing rect_for_child=(%.1f,%.1f %.1fx%.1f) instead of child_layout\n",
                           rect_for_child.x, rect_for_child.y, rect_for_child.width, rect_for_child.height);
                }
            }
        }

        render_component_sdl3(renderer, child, rect_for_child);

        // Vertical stacking for column layout - advance current_y
        if (component->type == IR_COMPONENT_COLUMN) {
            // Use rect_for_child height for auto-sized children (which may have been measured)
            // instead of child_layout height (which may be 0 for auto-sized components)
            float advance_height = rect_for_child.height > 0 ? rect_for_child.height : child_layout.height;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                float old_y = current_y;
                current_y += advance_height + distributed_gap;
                printf("  ⬇️  Advance Y: %.1f -> %.1f (height=%.1f gap=%.1f)\n",
                       old_y, current_y, advance_height, distributed_gap);
            } else {
                current_y += advance_height + distributed_gap;
            }
        }

        // Horizontal stacking for row layout - advance current_x
        if (component->type == IR_COMPONENT_ROW) {
            // Use rect_for_child width for auto-sized children (which may have been measured)
            // instead of child_layout width (which may be 0 for auto-sized components)
            float advance_width = rect_for_child.width > 0 ? rect_for_child.width : child_layout.width;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                float old_x = current_x;
                current_x += advance_width + distributed_gap;
                printf("  ➡️  Advance X: %.1f -> %.1f (width=%.1f gap=%.1f)\n",
                       old_x, current_x, advance_width, distributed_gap);
            } else {
                current_x += advance_width + distributed_gap;
            }
        }
    }
    if (getenv("KRYON_TRACE_LAYOUT")) {
        const char* type_name = component->type == IR_COMPONENT_COLUMN ? "COLUMN" :
                               component->type == IR_COMPONENT_ROW ? "ROW" : "OTHER";
        printf("📦 Layout %s complete\n\n", type_name);
    }

    return true;
}
#endif

// Event handling
#ifdef ENABLE_SDL3
static void handle_sdl3_events(DesktopIRRenderer* renderer) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        DesktopEvent desktop_event = {0};

#ifdef ENABLE_SDL3
        desktop_event.timestamp = SDL_GetTicks();
#else
        desktop_event.timestamp = 0;
#endif

        switch (event.type) {
            case SDL_EVENT_QUIT:
                desktop_event.type = DESKTOP_EVENT_QUIT;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                renderer->running = false;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                desktop_event.type = DESKTOP_EVENT_MOUSE_CLICK;
                desktop_event.data.mouse.x = event.button.x;
                desktop_event.data.mouse.y = event.button.y;
                desktop_event.data.mouse.button = event.button.button;

                // Dispatch click event to IR component at mouse position
                if (renderer->last_root) {
                    IRComponent* clicked = ir_find_component_at_point(
                        renderer->last_root,
                        (float)event.button.x,
                        (float)event.button.y
                    );

                    if (clicked) {
                        // Find and trigger IR_EVENT_CLICK handler
                        IREvent* ir_event = ir_find_event(clicked, IR_EVENT_CLICK);
                        if (ir_event && ir_event->logic_id) {
                            printf("🖱️  Click on component ID %u (logic: %s)\n",
                                   clicked->id, ir_event->logic_id);

                            // Check if this is a Nim button handler
                            if (strncmp(ir_event->logic_id, "nim_button_", 11) == 0) {
                                // Declare external Nim callback
                                extern void nimButtonBridge(uint32_t componentId);
                                // Invoke Nim handler
                                nimButtonBridge(clicked->id);
                            }
                            // Check if this is a Nim checkbox handler
                            else if (strncmp(ir_event->logic_id, "nim_checkbox_", 13) == 0) {
                                // Toggle checkbox state BEFORE calling user handler
                                if (clicked->type == IR_COMPONENT_CHECKBOX) {
                                    ir_toggle_checkbox_state(clicked);
                                }
                                // Declare external Nim callback
                                extern void nimCheckboxBridge(uint32_t componentId);
                                // Invoke Nim handler
                                nimCheckboxBridge(clicked->id);
                            }
                        }
                    }
                }

                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                desktop_event.type = DESKTOP_EVENT_MOUSE_MOVE;
                desktop_event.data.mouse.x = event.motion.x;
                desktop_event.data.mouse.y = event.motion.y;

                // Update cursor based on what component is under the mouse
                if (renderer->last_root) {
                    IRComponent* hovered = ir_find_component_at_point(
                        renderer->last_root,
                        (float)event.motion.x,
                        (float)event.motion.y
                    );

                    // Set cursor to hand for clickable components
                    SDL_Cursor* desired_cursor;
                    if (hovered && (hovered->type == IR_COMPONENT_BUTTON ||
                                    hovered->type == IR_COMPONENT_INPUT ||
                                    hovered->type == IR_COMPONENT_CHECKBOX)) {
                        desired_cursor = renderer->cursor_hand;
                    } else {
                        desired_cursor = renderer->cursor_default;
                    }

                    // Only update if cursor changed
                    if (desired_cursor != renderer->current_cursor) {
                        SDL_SetCursor(desired_cursor);
                        renderer->current_cursor = desired_cursor;
                    }
                }

                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                // ESC key closes window by default
                if (event.key.key == SDLK_ESCAPE) {
                    renderer->running = false;
                    break;
                }

                desktop_event.type = DESKTOP_EVENT_KEY_PRESS;
                desktop_event.data.keyboard.key_code = event.key.key;
                desktop_event.data.keyboard.shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                desktop_event.data.keyboard.ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                desktop_event.data.keyboard.alt = (event.key.mod & SDL_KMOD_ALT) != 0;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                desktop_event.type = DESKTOP_EVENT_WINDOW_RESIZE;
                desktop_event.data.resize.width = event.window.data1;
                desktop_event.data.resize.height = event.window.data2;
                renderer->needs_relayout = true;
                if (renderer->event_callback) {
                    renderer->event_callback(&desktop_event, renderer->event_user_data);
                }
                break;

            default:
                break;
        }
    }
}
#endif

// Public API implementation
DesktopIRRenderer* desktop_ir_renderer_create(const DesktopRendererConfig* config) {
    if (!config) return NULL;

    DesktopIRRenderer* renderer = malloc(sizeof(DesktopIRRenderer));
    if (!renderer) return NULL;

    memset(renderer, 0, sizeof(DesktopIRRenderer));
    renderer->config = *config;
    renderer->last_frame_time = 0; // Will be set when SDL3 initializes

    printf("🖥️ Creating desktop IR renderer with backend type: %d\n", config->backend_type);
    return renderer;
}

void desktop_ir_renderer_destroy(DesktopIRRenderer* renderer) {
    if (!renderer) return;

#ifdef ENABLE_SDL3
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

bool desktop_ir_renderer_initialize(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    printf("🚀 Initializing desktop renderer...\n");

#ifdef ENABLE_SDL3
    if (renderer->config.backend_type != DESKTOP_BACKEND_SDL3) {
        printf("❌ Only SDL3 backend is currently implemented\n");
        return false;
    }

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("❌ Failed to initialize SDL3: %s\n", SDL_GetError());
        return false;
    }

    // Initialize SDL_ttf
    if (!TTF_Init()) {
        printf("❌ Failed to initialize SDL_ttf: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // Create window
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
        printf("❌ Failed to create window: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Create renderer
    renderer->renderer = SDL_CreateRenderer(renderer->window, NULL);
    if (!renderer->renderer) {
        printf("❌ Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(renderer->window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Set vsync
    if (renderer->config.vsync_enabled) {
        SDL_SetRenderVSync(renderer->renderer, 1);
    }

    // Create 1x1 white texture for vertex coloring
    // This is necessary because SDL_RenderGeometry with NULL texture doesn't properly use vertex colors
    renderer->white_texture = SDL_CreateTexture(renderer->renderer, SDL_PIXELFORMAT_RGBA8888,
                                               SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (renderer->white_texture) {
        uint32_t white_pixel = 0xFFFFFFFF;  // White RGBA
        SDL_UpdateTexture(renderer->white_texture, NULL, &white_pixel, 4);
        SDL_SetTextureBlendMode(renderer->white_texture, SDL_BLENDMODE_BLEND);
    }

    // Load default font using fontconfig to find system fonts
    FILE* fc = popen("fc-match sans:style=Regular -f \"%{file}\"", "r");
    char fontconfig_path[512] = {0};
    if (fc) {
        if (fgets(fontconfig_path, sizeof(fontconfig_path), fc)) {
            // Remove newline
            size_t len = strlen(fontconfig_path);
            if (len > 0 && fontconfig_path[len-1] == '\n') {
                fontconfig_path[len-1] = '\0';
            }
            renderer->default_font = TTF_OpenFont(fontconfig_path, 16);
            if (renderer->default_font) {
                printf("✅ Loaded font via fontconfig: %s\n", fontconfig_path);
            }
        }
        pclose(fc);
    }

    // Fallback to hardcoded paths if fontconfig fails
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
                printf("✅ Loaded font: %s\n", font_paths[i]);
                break;
            }
        }
    }

    if (!renderer->default_font) {
        printf("⚠️ Could not load any system font, text rendering will be limited\n");
    }

    // Initialize cursors
    renderer->cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    renderer->cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    renderer->current_cursor = renderer->cursor_default;
    SDL_SetCursor(renderer->cursor_default);

    renderer->initialized = true;
    printf("✅ Desktop renderer initialized successfully\n");
    return true;

#else
    printf("❌ SDL3 support not enabled at compile time\n");
    return false;
#endif
}

bool desktop_ir_renderer_render_frame(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root || !renderer->initialized) return false;

#ifdef ENABLE_SDL3
    // Clear screen with root component's background color if available
    if (root->style) {
        SDL_Color bg = ir_color_to_sdl(root->style->background);
        SDL_SetRenderDrawColor(renderer->renderer, bg.r, bg.g, bg.b, bg.a);
    } else {
        SDL_SetRenderDrawColor(renderer->renderer, 240, 240, 240, 255);
    }
    SDL_RenderClear(renderer->renderer);

    // Get window size
    int window_width = renderer->config.window_width;
    int window_height = renderer->config.window_height;
#ifdef ENABLE_SDL3
    SDL_GetWindowSize(renderer->window, &window_width, &window_height);
#endif

    // Calculate root layout
    LayoutRect root_rect = {
        .x = 0, .y = 0,
        .width = (float)window_width,
        .height = (float)window_height
    };

    // Render component tree
    if (!render_component_sdl3(renderer, root, root_rect)) {
        printf("❌ Failed to render component\n");
        return false;
    }

    // Present frame
    SDL_RenderPresent(renderer->renderer);

    // Update performance stats
    renderer->frame_count++;
    double current_time = SDL_GetTicks() / 1000.0;
    double delta_time = current_time - renderer->last_frame_time;
    if (delta_time > 0.1) { // Update FPS every 100ms
        renderer->fps = renderer->frame_count / delta_time;
        renderer->frame_count = 0;
        renderer->last_frame_time = current_time;
    }

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

    printf("🎮 Starting desktop main loop...\n");
    renderer->running = true;
    renderer->last_root = root;

#ifdef ENABLE_SDL3
    while (renderer->running) {
        handle_sdl3_events(renderer);

        // Process reactive updates (text expressions, conditionals, etc.)
        // This ensures UI updates when reactive variables change
        nimProcessReactiveUpdates();

        if (!desktop_ir_renderer_render_frame(renderer, root)) {
            printf("❌ Frame rendering failed\n");
            break;
        }

        // Frame rate limiting
        if (renderer->config.target_fps > 0) {
#ifdef ENABLE_SDL3
            SDL_Delay(1000 / renderer->config.target_fps);
#else
            // Simple sleep fallback when SDL3 is not available
            struct timespec ts = {0, (1000000000L / renderer->config.target_fps)};
            nanosleep(&ts, NULL);
#endif
        }
    }
#endif

    printf("🛑 Desktop main loop ended\n");
    return true;
}

void desktop_ir_renderer_stop(DesktopIRRenderer* renderer) {
    if (renderer) {
        renderer->running = false;
    }
}

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
    // Image loading would be implemented here
    printf("📷 Image loading not yet implemented: %s\n", image_path);
    return false;
}

void desktop_ir_renderer_clear_resources(DesktopIRRenderer* renderer) {
    printf("🧹 Clearing desktop renderer resources\n");
    // Resource cleanup would be implemented here
}

bool desktop_ir_renderer_validate_ir(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    printf("🔍 Validating IR for desktop rendering...\n");
    return ir_validate_component(root);
}

void desktop_ir_renderer_print_tree_info(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    printf("🌳 Desktop IR Component Tree Information:\n");
    ir_print_component_info(root, 0);
}

void desktop_ir_renderer_print_performance_stats(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    printf("📊 Desktop Renderer Performance Stats:\n");
    printf("   FPS: %.1f\n", renderer->fps);
    printf("   Backend type: %d\n", renderer->config.backend_type);
    printf("   Window size: %dx%d\n", renderer->config.window_width, renderer->config.window_height);
    printf("   Running: %s\n", renderer->running ? "Yes" : "No");
}

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