/**
 * Raylib Backend Rendering
 * Component rendering implementation for raylib
 */

#ifdef ENABLE_RAYLIB

#include "desktop_internal.h"
#include "../../ir/ir_serialization.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

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

    // Check layout is computed
    if (!component->layout_state || !component->layout_state->layout_valid) {
        fprintf(stderr, "Warning: Layout not computed for component %u type %d\n",
                component->id, component->type);
        return false;
    }

    // Read pre-computed layout
    IRComputedLayout* layout = &component->layout_state->computed;

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
// FRAME RENDERING
// ============================================================================

/**
 * Render a complete frame with raylib
 */
bool render_frame_raylib(DesktopIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    BeginDrawing();

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
        .width = (float)renderer->window_width,
        .height = (float)renderer->window_height
    };

    // Render component tree
    render_component_raylib(renderer, root, root_rect, 1.0f);

    EndDrawing();
    return true;
}

#endif // ENABLE_RAYLIB
