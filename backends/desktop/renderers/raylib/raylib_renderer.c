/**
 * Raylib Renderer Backend
 *
 * Complete raylib renderer implementation with ops table support.
 * Merges initialization and rendering logic from desktop_init_raylib.c
 * and desktop_rendering_raylib.c.
 */

#ifdef ENABLE_RAYLIB

#include "../../desktop_internal.h"
#include "../../desktop_platform.h"
#include "../../../../ir/ir_serialization.h"
#include "raylib_renderer.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// COLOR CONVERSION
// ============================================================================

/**
 * Convert IRColor to raylib Color
 */
static Color ir_color_to_raylib(IRColor c) {
    return (Color){
        .r = c.data.r,
        .g = c.data.g,
        .b = c.data.b,
        .a = c.data.a
    };
}

// ============================================================================
// RAYLIB LIFECYCLE OPERATIONS
// ============================================================================

/**
 * Initialize raylib backend
 */
static bool raylib_initialize(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    printf("[raylib] Initializing raylib renderer\n");

    // Allocate backend data
    RaylibRendererData* data = calloc(1, sizeof(RaylibRendererData));
    if (!data) {
        fprintf(stderr, "[raylib] Failed to allocate renderer data\n");
        return false;
    }

    // Configure raylib window flags
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

    // Initialize window
    InitWindow(
        renderer->config.window_width,
        renderer->config.window_height,
        renderer->config.window_title
    );

    if (!IsWindowReady()) {
        fprintf(stderr, "[raylib] Failed to create raylib window\n");
        free(data);
        return false;
    }

    // Set target FPS
    SetTargetFPS(renderer->config.target_fps);

    // Store window dimensions
    data->window_width = renderer->config.window_width;
    data->window_height = renderer->config.window_height;

    // Load default font (raylib provides built-in font)
    data->default_font = GetFontDefault();

    // Store backend data
    renderer->ops->backend_data = data;
    renderer->initialized = true;

    printf("[raylib] Initialized: %dx%d \"%s\"\n",
           data->window_width,
           data->window_height,
           renderer->config.window_title);

    return true;
}

/**
 * Shutdown raylib backend
 */
static void raylib_shutdown(DesktopIRRenderer* renderer) {
    if (!renderer || !renderer->initialized) return;

    printf("[raylib] Shutting down\n");

    RaylibRendererData* data = (RaylibRendererData*)renderer->ops->backend_data;
    if (data) {
        free(data);
        renderer->ops->backend_data = NULL;
    }

    CloseWindow();
    renderer->initialized = false;
}

// ============================================================================
// EVENT HANDLING
// ============================================================================

/**
 * Poll raylib events
 */
static void raylib_poll_events(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    // Handle input events (mouse, keyboard, window)
    raylib_handle_input_events(renderer, renderer->last_root);
}

// ============================================================================
// COMPONENT RENDERING
// ============================================================================

/**
 * Render a single component with raylib
 */
bool render_component_raylib(DesktopIRRenderer* renderer, IRComponent* component,
                              LayoutRect rect, float inherited_opacity) {
    if (!renderer || !component) return false;

    // Skip if not visible
    if (component->style && !component->style->visible) {
        return true;
    }

    // Debug: Show layout for all components
    if (component->layout_state && component->layout_state->layout_valid) {
        printf("[raylib_render] id=%u type=%d visible=%d layout=(%.0f,%.0f,%.0fx%.0f)\n",
               component->id, component->type,
               component->style ? component->style->visible : -1,
               component->layout_state->computed.x,
               component->layout_state->computed.y,
               component->layout_state->computed.width,
               component->layout_state->computed.height);
    }

    // Check layout is computed
    if (!component->layout_state || !component->layout_state->layout_valid) {
        fprintf(stderr, "[raylib] Warning: Layout not computed for component %u type %d\n",
                component->id, component->type);
        return false;
    }

    // Read pre-computed layout
    IRComputedLayout* layout = &component->layout_state->computed;

    // Set rendered bounds for hit testing
    component->rendered_bounds.x = layout->x;
    component->rendered_bounds.y = layout->y;
    component->rendered_bounds.width = layout->width;
    component->rendered_bounds.height = layout->height;
    component->rendered_bounds.valid = true;

    // Calculate opacity
    float opacity = inherited_opacity;
    if (component->style) {
        opacity *= component->style->opacity;
    }

    // Render based on component type
    switch (component->type) {
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            // Render background if present
            if (component->style) {
                Color bg = ir_color_to_raylib(component->style->background);
                if (bg.a > 0) {
                    bg.a = (uint8_t)(bg.a * opacity);
                    DrawRectangle(
                        (int)layout->x, (int)layout->y,
                        (int)layout->width, (int)layout->height,
                        bg
                    );
                }

                // Render border if present
                if (component->style->border.width > 0) {
                    Color border = ir_color_to_raylib(component->style->border.color);
                    border.a = (uint8_t)(border.a * opacity);
                    DrawRectangleLines(
                        (int)layout->x, (int)layout->y,
                        (int)layout->width, (int)layout->height,
                        border
                    );
                }
            }
            break;

        case IR_COMPONENT_CENTER:
            // Center is layout-only, no rendering
            break;

        case IR_COMPONENT_BUTTON:
            // Render button background
            if (component->style) {
                Color bg = ir_color_to_raylib(component->style->background);
                bg.a = (uint8_t)(bg.a * opacity);
                DrawRectangle(
                    (int)layout->x, (int)layout->y,
                    (int)layout->width, (int)layout->height,
                    bg
                );
            }

            // Render button border
            DrawRectangleLines(
                (int)layout->x, (int)layout->y,
                (int)layout->width, (int)layout->height,
                (Color){100, 100, 100, 255}
            );

            // Render button text (centered)
            if (component->text_content) {
                Color text_color = component->style ?
                    ir_color_to_raylib(component->style->font.color) :
                    (Color){255, 255, 255, 255};
                text_color.a = (uint8_t)(text_color.a * opacity);

                int font_size = component->style && component->style->font.size > 0 ?
                    (int)component->style->font.size : 20;

                // Measure text for centering
                int text_width = MeasureText(component->text_content, font_size);
                int center_x = (int)(layout->x + (layout->width - text_width) / 2);
                int center_y = (int)(layout->y + (layout->height - font_size) / 2);

                DrawText(
                    component->text_content,
                    center_x, center_y,
                    font_size,
                    text_color
                );
            }
            break;

        case IR_COMPONENT_TEXT:
            // Render text
            if (component->text_content) {
                Color text_color = component->style ?
                    ir_color_to_raylib(component->style->font.color) :
                    (Color){51, 51, 51, 255};

                // Handle transparent color (default to dark gray)
                if (text_color.a == 0) {
                    text_color = (Color){51, 51, 51, 255};
                }
                text_color.a = (uint8_t)(text_color.a * opacity);

                int font_size = component->style && component->style->font.size > 0 ?
                    (int)component->style->font.size : 20;

                DrawText(
                    component->text_content,
                    (int)layout->x, (int)layout->y,
                    font_size,
                    text_color
                );
            }
            break;

        case IR_COMPONENT_INPUT:
            // Render input background
            DrawRectangle(
                (int)layout->x, (int)layout->y,
                (int)layout->width, (int)layout->height,
                (Color){255, 255, 255, 255}
            );

            // Render border
            DrawRectangleLines(
                (int)layout->x, (int)layout->y,
                (int)layout->width, (int)layout->height,
                (Color){180, 180, 180, 255}
            );

            // Render text content
            if (component->text_content && component->text_content[0] != '\0') {
                DrawText(
                    component->text_content,
                    (int)(layout->x + 8), (int)(layout->y + 8),
                    16,
                    (Color){40, 40, 40, 255}
                );
            }
            break;

        default:
            // Unhandled component type - render nothing
            break;
    }

    // Render children recursively
    float child_opacity = opacity;
    for (uint32_t i = 0; i < component->child_count; i++) {
        IRComponent* child = component->children[i];
        if (child && (!child->style || child->style->visible)) {
            LayoutRect dummy_rect = {0};
            render_component_raylib(renderer, child, dummy_rect, child_opacity);
        }
    }

    return true;
}

// ============================================================================
// FRAME RENDERING OPERATIONS
// ============================================================================

/**
 * Begin frame rendering
 */
static bool raylib_begin_frame(DesktopIRRenderer* renderer) {
    if (!renderer) return false;

    BeginDrawing();
    return true;
}

/**
 * Render component tree
 */
static bool raylib_render_component(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    RaylibRendererData* data = (RaylibRendererData*)renderer->ops->backend_data;
    if (!data) return false;

    // Find background color from component tree
    Color bg_color = {0, 0, 0, 255}; // Default black

    // Try root component first
    if (root && root->style && root->style->background.data.a > 0) {
        bg_color = ir_color_to_raylib(root->style->background);
    }
    // If root has no background, try first child (common pattern)
    else if (root && root->child_count > 0 && root->children[0]) {
        IRComponent* first_child = root->children[0];
        if (first_child->style && first_child->style->background.data.a > 0) {
            bg_color = ir_color_to_raylib(first_child->style->background);
        }
    }

    ClearBackground(bg_color);

    // Root layout rectangle
    LayoutRect root_rect = {
        .x = 0,
        .y = 0,
        .width = (float)data->window_width,
        .height = (float)data->window_height
    };

    // Render component tree
    return render_component_raylib(renderer, root, root_rect, 1.0f);
}

/**
 * End frame rendering
 */
static void raylib_end_frame(DesktopIRRenderer* renderer) {
    if (!renderer) return;

    EndDrawing();
}

// ============================================================================
// BACKEND OPERATIONS TABLE
// ============================================================================

static DesktopRendererOps g_raylib_ops = {
    .initialize = raylib_initialize,
    .shutdown = raylib_shutdown,
    .poll_events = raylib_poll_events,
    .begin_frame = raylib_begin_frame,
    .render_component = raylib_render_component,
    .end_frame = raylib_end_frame,
    .font_ops = NULL,      // TODO: Implement in raylib_fonts.c
    .effects_ops = NULL,   // TODO: Implement if needed
    .text_measure = NULL,  // TODO: Implement in raylib_fonts.c
    .backend_data = NULL   // Set during initialization
};

DesktopRendererOps* desktop_raylib_get_ops(void) {
    return &g_raylib_ops;
}

// ============================================================================
// Backend Registration
// ============================================================================

/**
 * Register Raylib backend (must be called explicitly before use)
 */
void raylib_backend_register(void) {
    desktop_register_backend(DESKTOP_BACKEND_RAYLIB, &g_raylib_ops);
    printf("[raylib] Backend registered\n");
}

#endif // ENABLE_RAYLIB
