/*
 * Desktop Layout Calculation Module
 *
 * This module handles layout computation for the desktop renderer, including:
 * - Converting IR dimensions to pixel values
 * - Measuring text dimensions using heuristics and font metrics
 * - Computing component layout rectangles
 * - Determining child component sizes based on flexbox rules
 *
 * The layout system operates on LayoutRect structures and respects flexbox properties,
 * absolute positioning, and responsive sizing (pixels, percentages, auto).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "desktop_internal.h"

// ============================================================================
// LAYOUT CALCULATION FUNCTIONS
// ============================================================================

/**
 * Convert an IR dimension to pixel value
 *
 * Handles different dimension types:
 * - IR_DIMENSION_PX: Returns the value directly
 * - IR_DIMENSION_PERCENT: Calculates percentage of container size
 * - IR_DIMENSION_AUTO: Returns 0 (layout engine determines size)
 * - IR_DIMENSION_FLEX: Treats flex value as pixels (simplified)
 *
 * @param dimension The IR dimension to convert
 * @param container_size The container size for percentage calculations
 * @return The computed pixel value
 */
float ir_dimension_to_pixels(IRDimension dimension, float container_size) {
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

/**
 * Measure text dimensions using actual font rendering or heuristics
 *
 * First attempts to use SDL_TTF for accurate text measurement if a renderer
 * and font are available. Falls back to heuristic calculations (character width
 * and line height ratios) when actual rendering isn't available.
 *
 * Constrains width to parent container to prevent overflow.
 *
 * @param component The text component to measure
 * @param max_width Maximum width constraint (0 for no limit)
 * @param out_width Pointer to store calculated width
 * @param out_height Pointer to store calculated height
 */
/**
 * Wrap text into lines based on max_width constraint
 * Returns the number of lines needed to fit the text
 */
int wrap_text_into_lines(const char* text, float max_width, TTF_Font* font,
                         float font_size, char*** out_lines) {
    if (!text || !*text) return 0;

    // Trace wrapping during RENDERING (not measurement)
    if (getenv("KRYON_TRACE_TEXT_WRAP") && out_lines) {
        fprintf(stderr, "[TEXT_WRAP_RENDER] text=\"%.50s%s\" max_width=%.1f\n",
                text, strlen(text) > 50 ? "..." : "", max_width);
    }

    // If no max_width constraint, single line
    if (max_width <= 0.0f) {
        if (out_lines) {
            *out_lines = malloc(sizeof(char*));
            (*out_lines)[0] = strdup(text);
        }
        return 1;
    }

    // Strategy 1: Use SDL_TTF for accurate measurement if available
    if (font) {
        // Split text into words
        char* text_copy = strdup(text);
        char** words = NULL;
        int word_count = 0;

        // Tokenize by spaces
        char* saveptr = NULL;
        char* token = strtok_r(text_copy, " ", &saveptr);
        while (token) {
            words = realloc(words, sizeof(char*) * (word_count + 1));
            words[word_count++] = strdup(token);
            token = strtok_r(NULL, " ", &saveptr);
        }

        if (word_count == 0) {
            free(text_copy);
            return 0;
        }

        // Build lines by adding words until exceeding max_width
        char** lines = NULL;
        int line_count = 0;
        char current_line[4096] = "";

        for (int i = 0; i < word_count; i++) {
            char test_line[4096];
            if (current_line[0]) {
                snprintf(test_line, sizeof(test_line), "%s %s", current_line, words[i]);
            } else {
                snprintf(test_line, sizeof(test_line), "%s", words[i]);
            }

            // Measure test line
            int line_w = 0, line_h = 0;
            TTF_GetStringSize(font, test_line, 0, &line_w, &line_h);

            // Check if line exceeds max_width
            if (line_w > max_width && current_line[0]) {
                // Line would be too long, finalize current line
                lines = realloc(lines, sizeof(char*) * (line_count + 1));
                lines[line_count++] = strdup(current_line);
                snprintf(current_line, sizeof(current_line), "%s", words[i]);
            } else {
                // Line still fits, keep building
                strcpy(current_line, test_line);
            }
        }

        // Add last line
        if (current_line[0]) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            lines[line_count++] = strdup(current_line);
        }

        // Cleanup
        for (int i = 0; i < word_count; i++) free(words[i]);
        free(words);
        free(text_copy);

        if (out_lines) {
            *out_lines = lines;
        } else {
            // Caller doesn't want lines, just free them
            for (int i = 0; i < line_count; i++) free(lines[i]);
            free(lines);
        }

        return line_count;
    }

    // Strategy 2: Heuristic fallback when no font available
    size_t text_len = strlen(text);
    float char_width = font_size * TEXT_CHAR_WIDTH_RATIO;
    int chars_per_line = (int)(max_width / char_width);
    if (chars_per_line < 1) chars_per_line = 1;

    int line_count = (text_len + chars_per_line - 1) / chars_per_line;
    return line_count > 0 ? line_count : 1;
}

void measure_text_dimensions(IRComponent* component, float max_width, float* out_width, float* out_height) {
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

    float line_height = font_size * TEXT_LINE_HEIGHT_RATIO;

    // Try to use actual font measurement via SDL_TTF if a renderer is available
    DesktopIRRenderer* renderer = g_desktop_renderer;
    if (renderer) {
        // Get font at the correct size using the font resolver
        TTF_Font* font = desktop_ir_resolve_font(renderer, component, font_size);

        // Try actual text measurement with wrapping
        if (font && component->text_content[0] != '\0') {
            // Wrap text into lines
            char** lines = NULL;
            int line_count = wrap_text_into_lines(component->text_content, max_width, font, font_size, &lines);

            if (line_count > 0 && lines) {
                // Measure each line to find maximum width
                float max_line_width = 0.0f;
                for (int i = 0; i < line_count; i++) {
                    int line_w = 0, line_h = 0;
                    TTF_GetStringSize(font, lines[i], 0, &line_w, &line_h);
                    if (line_w > max_line_width) max_line_width = line_w;
                    free(lines[i]);
                }
                free(lines);

                *out_width = max_line_width;
                *out_height = line_count * line_height;

                if (getenv("KRYON_TRACE_TEXT_WRAP")) {
                    fprintf(stderr, "[TEXT_WRAP] Component %u: text=\"%.50s%s\" max_width=%.1f → %d lines, width=%.1f, height=%.1f\n",
                            component->id, component->text_content,
                            strlen(component->text_content) > 50 ? "..." : "",
                            max_width, line_count, *out_width, *out_height);
                }

                if (getenv("KRYON_TRACE_LAYOUT")) {
                    printf("    📏 TEXT measured via TTF (wrapped): %d lines, width=%.1f, height=%.1f\n",
                           line_count, *out_width, *out_height);
                }

                return;
            }
        }
    }

    // Fallback: Estimate dimensions with wrapping using heuristic
    int line_count = wrap_text_into_lines(component->text_content, max_width, NULL, font_size, NULL);

    size_t text_len = strlen(component->text_content);
    float char_width = font_size * TEXT_CHAR_WIDTH_RATIO;
    int chars_per_line = max_width > 0 ? (int)(max_width / char_width) : text_len;
    if (chars_per_line < 1) chars_per_line = 1;

    *out_height = line_count * line_height;
    *out_width = (chars_per_line < text_len) ? max_width : text_len * char_width;

    if (getenv("KRYON_TRACE_TEXT_WRAP")) {
        fprintf(stderr, "[TEXT_WRAP] Component %u (heuristic): text=\"%.50s%s\" max_width=%.1f → %d lines, width=%.1f, height=%.1f\n",
                component->id, component->text_content,
                strlen(component->text_content) > 50 ? "..." : "",
                max_width, line_count, *out_width, *out_height);
    }

    if (getenv("KRYON_TRACE_LAYOUT")) {
        printf("    📏 TEXT measured via heuristic (wrapped): %d lines, width=%.1f, height=%.1f (len=%zu, fontSize=%.1f)\n",
               line_count, *out_width, *out_height, text_len, font_size);
    }
}

#endif // ENABLE_SDL3

/**
 * Get intrinsic width of a component (no parent constraints)
 *
 * Calculates the natural width a component needs based on its content,
 * WITHOUT being constrained by parent dimensions. This is critical for
 * proper flexbox layout where siblings must share available space.
 *
 * Based on ir_get_component_intrinsic_width() from ir_layout.c but adapted
 * for the desktop renderer with SDL_TTF text measurement.
 *
 * @param component The component to measure
 * @return The intrinsic width in pixels
 */
float get_component_intrinsic_width(IRComponent* component) {
    if (!component) return 0.0f;
    if (!component->style) return 100.0f;

    IRStyle* style = component->style;

    // Use explicit width if set
    if (style->width.type == IR_DIMENSION_PX) {
        return style->width.value;
    }

    // Calculate based on component type and content
    switch (component->type) {
        case IR_COMPONENT_TEXT:
            if (component->text_content) {
#ifdef ENABLE_SDL3
                float width, height;
                // Measure text without width constraint (0 = unlimited)
                measure_text_dimensions(component, 0, &width, &height);
                return width;
#else
                // Fallback estimate
                float font_size = style->font.size > 0 ? style->font.size : 16.0f;
                return strlen(component->text_content) * font_size * 0.5f;
#endif
            }
            return 50.0f;

        case IR_COMPONENT_BUTTON:
        case IR_COMPONENT_TAB:
        case IR_COMPONENT_LINK:
            if (component->text_content) {
#ifdef ENABLE_SDL3
                float text_width, text_height;
                measure_text_dimensions(component, 0, &text_width, &text_height);
                float padding = style->padding.left + style->padding.right;
                return text_width + padding + 20.0f; // Extra space for button chrome
#else
                float font_size = style->font.size > 0 ? style->font.size : 14.0f;
                float text_width = strlen(component->text_content) * font_size * 0.5f;
                float padding = style->padding.left + style->padding.right;
                return text_width + padding + 20.0f;
#endif
            }
            return 80.0f;

        case IR_COMPONENT_INPUT:
            return 200.0f; // Default input width

        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
        case IR_COMPONENT_TAB_BAR:
        case IR_COMPONENT_CENTER:
            // Container width depends on children and layout direction
            if (component->child_count > 0 && component->layout) {
                float total_width = 0.0f;
                float max_width = 0.0f;
                float gap = component->layout->flex.gap;

                bool is_row = (component->layout->flex.direction == 1) ||
                             (component->type == IR_COMPONENT_ROW) ||
                             (component->type == IR_COMPONENT_TAB_BAR);

                for (uint32_t i = 0; i < component->child_count; i++) {
                    IRComponent* child = component->children[i];
                    if (!child->style || !child->style->visible) continue;

                    // Recursively measure child intrinsic width
                    float child_width = get_component_intrinsic_width(child);
                    child_width += child->style->margin.left + child->style->margin.right;

                    if (is_row) {
                        total_width += child_width;
                        if (i > 0) total_width += gap;
                    } else {
                        if (child_width > max_width) max_width = child_width;
                    }
                }

                float result = is_row ? total_width : max_width;
                result += style->padding.left + style->padding.right;
                return result;
            }
            return 100.0f;

        default:
            return 100.0f;
    }
}

/**
 * Calculate layout rectangle for a component
 *
 * Determines the position and dimensions of a component based on:
 * - Parent container bounds
 * - Component style properties (width, height, margin, position mode)
 * - Aspect ratio constraints
 * - Component type-specific defaults (TEXT/BUTTON use 0 width/height)
 *
 * Handles absolute and relative positioning:
 * - Absolute: Uses style->absolute_x/Y
 * - Relative: Inherits parent position, applies margins
 *
 * @param component The component to layout
 * @param parent_rect The parent container's layout rectangle
 * @return The computed layout rectangle for the component
 */
LayoutRect calculate_component_layout(IRComponent* component, LayoutRect parent_rect) {
    LayoutRect rect = {0};

    if (!component) return rect;

    // Check for absolute positioning
    bool is_absolute = component->style && component->style->position_mode == IR_POSITION_ABSOLUTE;

    if (is_absolute) {
        // Absolute positioning: use explicit coordinates relative to root/window
        rect.x = component->style->absolute_x;
        rect.y = component->style->absolute_y;

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    🎯 ABSOLUTE positioning: component at (%.1f, %.1f)\n", rect.x, rect.y);
        }
    } else {
        // Default to parent bounds for container (relative positioning)
        rect.x = parent_rect.x;
        rect.y = parent_rect.y;
    }

    // For TEXT, BUTTON, TAB, and LINK components with AUTO dimensions, start with 0 instead of parent dimensions
    // This allows get_child_size() to detect and measure them properly
    // CRITICAL: Buttons in a ROW must NOT default to parent width, or flex_grow won't work correctly!
    if (component->type == IR_COMPONENT_TEXT || component->type == IR_COMPONENT_BUTTON ||
        component->type == IR_COMPONENT_TAB || component->type == IR_COMPONENT_LINK) {
        rect.width = 0;
        rect.height = 0;
    } else if (component->type == IR_COMPONENT_CENTER) {
        // CENTER always uses parent dimensions (to provide space for centering its child)
        rect.width = parent_rect.width;
        rect.height = parent_rect.height;
    } else {
        // Check if component has explicit dimensions or should auto-size
        bool has_explicit_width = component->style && component->style->width.type != IR_DIMENSION_AUTO;
        bool has_explicit_height = component->style && component->style->height.type != IR_DIMENSION_AUTO;

        // Default to parent dimensions, BUT use 0 for auto-sized dimensions
        // This allows auto-sized Row/Column to measure their content properly
        rect.width = has_explicit_width ? parent_rect.width : 0;
        rect.height = has_explicit_height ? parent_rect.height : 0;
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

        // Apply aspect ratio constraint (width/height ratio)
        if (component->layout && component->layout->aspect_ratio > 0) {
            bool has_explicit_width = component->style && component->style->width.type != IR_DIMENSION_AUTO;
            bool has_explicit_height = component->style && component->style->height.type != IR_DIMENSION_AUTO;

            if (has_explicit_width && !has_explicit_height) {
                // Width is set, calculate height from aspect ratio
                rect.height = rect.width / component->layout->aspect_ratio;
            } else if (has_explicit_height && !has_explicit_width) {
                // Height is set, calculate width from aspect ratio
                rect.width = rect.height * component->layout->aspect_ratio;
            }
        }

        // Apply margins (only for relative positioning)
        if (!is_absolute) {
            if (component->style->margin.top > 0) rect.y += component->style->margin.top;
            if (component->style->margin.left > 0) rect.x += component->style->margin.left;
        }
    }

    return rect;
}

/**
 * Calculate the layout size of a child component
 *
 * Computes both width and height dimensions for a component by:
 * - Using calculate_component_layout for basic sizing
 * - Measuring children for AUTO-sized containers
 * - Text measurement for TEXT components
 * - Button-specific sizing (text + padding + minWidth constraints)
 *
 * For absolute positioning, preserves x/y coordinates.
 * For relative positioning, returns position (0,0) to be set in layout loop.
 *
 * @param child The child component to size
 * @param parent_rect The parent container's layout rectangle
 * @return The computed layout rectangle (position may be 0 for relative components)
 */
LayoutRect get_child_size(IRComponent* child, LayoutRect parent_rect) {
    if (!child) {
        LayoutRect zero = {0, 0, 0, 0};
        return zero;
    }

    if (getenv("KRYON_TRACE_LAYOUT") && child->type == IR_COMPONENT_COLUMN && child->child_count > 0) {
        printf("  🔍 get_child_size(COLUMN #%u): parent_rect.width=%.1f, child_count=%u\n",
               child->id, parent_rect.width, child->child_count);
    }

    // Use the existing calculate_component_layout which handles all component types correctly
    LayoutRect layout = calculate_component_layout(child, parent_rect);

    // For AUTO-sized Row/Column/TabBar, measure children to get actual size
    // TabBar behaves like a Row (horizontal layout)
    if ((child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN ||
         child->type == IR_COMPONENT_TAB_BAR) && child->child_count > 0) {
        bool is_auto_width = !child->style || child->style->width.type == IR_DIMENSION_AUTO;
        bool is_auto_height = !child->style || child->style->height.type == IR_DIMENSION_AUTO;

        // Check if component needs main-axis space for alignment (justifyContent)
        bool needs_main_axis_space = child->layout &&
                                    child->layout->flex.justify_content != IR_ALIGNMENT_START;

        if (is_auto_width || is_auto_height) {
            float total_width = 0, total_height = 0;
            float gap = 0.0f;
            if (child->layout && child->layout->flex.gap > 0) {
                gap = (float)child->layout->flex.gap;
            }

            for (uint32_t i = 0; i < child->child_count; i++) {
                if (getenv("KRYON_TRACE_LAYOUT") && child->type == IR_COMPONENT_COLUMN &&
                    child->children[i] && child->children[i]->type == IR_COMPONENT_TEXT) {
                    printf("    🔍 COLUMN measuring TEXT child: parent_rect.width=%.1f\n", parent_rect.width);
                }

                float child_w = get_child_dimension(child->children[i], parent_rect, false);
                float child_h = get_child_dimension(child->children[i], parent_rect, true);

                if (child->type == IR_COMPONENT_COLUMN) {
                    total_height += child_h;
                    if (i < child->child_count - 1) total_height += gap;
                    if (child_w > total_width) total_width = child_w;
                } else {  // Row or TabBar - horizontal layout
                    total_width += child_w;
                    if (i < child->child_count - 1) total_width += gap;
                    if (child_h > total_height) total_height = child_h;
                }
            }

            // For COLUMN: Don't shrink height if justifyContent != START
            // For ROW: Don't shrink width if justifyContent != START
            bool shrink_width = !(child->type == IR_COMPONENT_ROW && needs_main_axis_space);
            bool shrink_height = !(child->type == IR_COMPONENT_COLUMN && needs_main_axis_space);

            if (is_auto_width) {
                if (shrink_width) {
                    layout.width = total_width;
                } else {
                    // Don't shrink - use parent's available width for alignment
                    layout.width = parent_rect.width;
                }
            }
            if (is_auto_height) {
                if (shrink_height) {
                    layout.height = total_height;
                } else {
                    // Don't shrink - use parent's available height for alignment
                    layout.height = parent_rect.height;
                }
            }
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

    // Measure BUTTON components using get_child_dimension (which includes text + padding + minWidth)
    if (child->type == IR_COMPONENT_BUTTON) {
        // BUTTON should auto-size if calculate_component_layout returned 0 dimensions
        if (layout.width <= 0.0f) {
            layout.width = get_child_dimension(child, parent_rect, false);
        }
        if (layout.height <= 0.0f) {
            layout.height = get_child_dimension(child, parent_rect, true);
        }
    }

    // Measure TAB components using get_child_dimension (which includes title text + padding)
    if (child->type == IR_COMPONENT_TAB) {
        // TAB should auto-size if calculate_component_layout returned 0 dimensions
        if (layout.width <= 0.0f) {
            layout.width = get_child_dimension(child, parent_rect, false);
        }
        if (layout.height <= 0.0f) {
            layout.height = get_child_dimension(child, parent_rect, true);
        }
    }

    // Measure LINK components using get_child_dimension (which measures text via measure_text_dimensions)
    if (child->type == IR_COMPONENT_LINK) {
        // ALWAYS measure link text dimensions for proper hit testing
        layout.width = get_child_dimension(child, parent_rect, false);
        layout.height = get_child_dimension(child, parent_rect, true);
    }

    // For absolutely positioned components, preserve the x/y coordinates from calculate_component_layout
    // For relative positioned components, return position 0 (will be set in main layout loop)
    bool is_absolute = child->style && child->style->position_mode == IR_POSITION_ABSOLUTE;

    LayoutRect result;
    if (is_absolute) {
        result = layout;  // Preserve all coordinates for absolute positioning
    } else {
        result.x = 0;
        result.y = 0;
        result.width = layout.width;
        result.height = layout.height;
    }

    return result;
}

/**
 * Get a single dimension (width or height) of a child component
 *
 * This function handles the core layout logic for sizing components:
 * - Checks for explicit size in style (px or %)
 * - Measures TEXT components using font metrics
 * - Sizes BUTTON components (text + padding + constraints)
 * - Recursively measures Container/Row/Column children
 * - Adds padding to dimension calculations
 *
 * IMPORTANT: This function must NOT fully recurse into child layout to prevent infinite loops.
 * It only measures individual dimensions (width OR height), not full layout rectangles.
 *
 * @param child The component to size
 * @param parent_rect The parent container's layout rectangle
 * @param is_height Whether to get height (true) or width (false)
 * @return The computed dimension in pixels
 */
float get_child_dimension(IRComponent* child, LayoutRect parent_rect, bool is_height) {
    if (!child) return 0.0f;

    // Check if component has explicit size in style
    if (child->style) {
        IRDimension style_dim = is_height ? child->style->height : child->style->width;
        if (style_dim.type != IR_DIMENSION_AUTO) {
            // Has explicit size, use it
            float result = ir_dimension_to_pixels(style_dim, is_height ? parent_rect.height : parent_rect.width);

            // WORKAROUND: Treat explicit 0 dimensions as AUTO for Row/Column/Text/Button
            // This fixes DSL bug where these components get explicit height=0/width=0 instead of AUTO
            if (result == 0.0f && (child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN ||
                                   child->type == IR_COMPONENT_TEXT || child->type == IR_COMPONENT_BUTTON)) {
                if (getenv("KRYON_TRACE_LAYOUT")) {
                    const char* type_name = "OTHER";
                    if (child->type == IR_COMPONENT_ROW) type_name = "ROW";
                    else if (child->type == IR_COMPONENT_COLUMN) type_name = "COLUMN";
                    else if (child->type == IR_COMPONENT_TEXT) type_name = "TEXT";
                    else if (child->type == IR_COMPONENT_BUTTON) type_name = "BUTTON";
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
        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    📝 TEXT Component %u: get_child_dimension called with parent_rect.width=%.1f\n",
                   child->id, parent_rect.width);
        }
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

    // Tab auto-size: measure title text + padding (like button)
    if (child->type == IR_COMPONENT_TAB && child->tab_data && child->tab_data->title) {
#ifdef ENABLE_SDL3
        float text_width = 0.0f, text_height = 0.0f;

        // Temporarily set text_content to title for measurement
        char* original_text = child->text_content;
        child->text_content = child->tab_data->title;
        measure_text_dimensions(child, parent_rect.width, &text_width, &text_height);
        child->text_content = original_text;

        // Add default padding for tabs
        float pad_x = 20.0f, pad_y = 10.0f;
        if (child->style) {
            if (child->style->padding.left > 0) pad_x = child->style->padding.left + child->style->padding.right;
            if (child->style->padding.top > 0) pad_y = child->style->padding.top + child->style->padding.bottom;
        }

        if (is_height && text_height > 0.0f) {
            return text_height + pad_y;
        } else if (!is_height && text_width > 0.0f) {
            return text_width + pad_x;
        }
#endif
        // Fallback for tabs
        return is_height ? 40.0f : 100.0f;
    }

    // Link auto-size: measure text content (like text, but inline)
    if (child->type == IR_COMPONENT_LINK) {
#ifdef ENABLE_SDL3
        float text_width = 0.0f, text_height = 0.0f;
        measure_text_dimensions(child, parent_rect.width, &text_width, &text_height);

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    🔗 LINK measurement result: width=%.1f, height=%.1f\n", text_width, text_height);
        }

        if (is_height && text_height > 0.0f) {
            return text_height;
        } else if (!is_height && text_width > 0.0f) {
            return text_width;
        }
#endif
        // Fallback if measurement failed
        if (is_height) {
            float font_size = 16.0f;
            if (child->style && child->style->font.size > 0) {
                font_size = child->style->font.size;
            }
            return font_size * 1.2f;
        }
        return 0.0f;  // Width fallback
    }

    // Button auto-size: measure text content + padding
    if (child->type == IR_COMPONENT_BUTTON) {
#ifdef ENABLE_SDL3
        float text_width = 0.0f, text_height = 0.0f;
        measure_text_dimensions(child, parent_rect.width, &text_width, &text_height);

        // Add padding to button dimensions
        float pad_left = 0.0f, pad_right = 0.0f, pad_top = 0.0f, pad_bottom = 0.0f;
        if (child->style) {
            pad_left = child->style->padding.left;
            pad_right = child->style->padding.right;
            pad_top = child->style->padding.top;
            pad_bottom = child->style->padding.bottom;
        }

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    🔘 BUTTON measurement: text=(%.1f,%.1f) pad=(%.1f,%.1f,%.1f,%.1f)\n",
                   text_width, text_height, pad_top, pad_right, pad_bottom, pad_left);
        }

        if (is_height && text_height > 0.0f) {
            float result = text_height + pad_top + pad_bottom;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("    ✅ BUTTON height from text: %.1f\n", result);
            }
            return result;
        } else if (!is_height && text_width > 0.0f) {
            float result = text_width + pad_left + pad_right;

            // If maxWidth is set, cap the natural width
            // This allows flex-shrink to work properly for tabs
            if (child->layout && child->layout->max_width.type == IR_DIMENSION_PX &&
                child->layout->max_width.value > 0 && result > child->layout->max_width.value) {
                result = child->layout->max_width.value;
            }

            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("    ✅ BUTTON width from text: %.1f (text=%.1f + pad=%.1f)\n", result, text_width, pad_left + pad_right);
            }
            return result;
        }
#endif
        // Fallback
        if (is_height) {
            float font_size = 16.0f;
            if (child->style && child->style->font.size > 0) {
                font_size = child->style->font.size;
            }
            float pad = child->style ? (child->style->padding.top + child->style->padding.bottom) : 0.0f;
            float result = font_size * 1.2f + pad;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("    ⚠️  BUTTON height FALLBACK: %.1f\n", result);
            }
            return result;
        } else {
            // Width fallback: use minWidth if set, otherwise estimate
            if (child->layout && child->layout->min_width.type == IR_DIMENSION_PX &&
                child->layout->min_width.value > 0) {
                if (getenv("KRYON_TRACE_LAYOUT")) {
                    printf("    ✅ BUTTON width from minWidth: %.1f\n", child->layout->min_width.value);
                }
                return child->layout->min_width.value;
            }

            // Otherwise estimate minimum readable button width
            float font_size = 16.0f;
            if (child->style && child->style->font.size > 0) {
                font_size = child->style->font.size;
            }
            float pad = child->style ? (child->style->padding.left + child->style->padding.right) : 0.0f;
            // Estimate ~4 average characters worth of width as minimum
            // Average character width ≈ font_size * 0.5 for typical fonts
            float result = (font_size * 0.5f * 4.0f) + pad;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("    ⚠️  BUTTON width FALLBACK (estimated): %.1f (font=%.1f, pad=%.1f)\n", result, font_size, pad);
            }
            return result;
        }
    }

    // Handle TABLE components - measure based on row count and content
    if (child->type == IR_COMPONENT_TABLE && child->child_count > 0) {
        IRTableState* state = (IRTableState*)child->custom_data;

        // Return cached height if layout has been computed
        // This ensures measurement matches layout, preventing thrashing
        if (state && state->layout_valid && state->cached_total_height > 0) {
            if (getenv("KRYON_TRACE_LAYOUT")) {
                printf("    📊 TABLE measurement: returning cached height=%.1f\n", state->cached_total_height);
            }
            return is_height ? state->cached_total_height : parent_rect.width;
        }

        // First frame: compute estimate by counting rows
        float total_height = 0.0f;
        float row_height = 32.0f;  // Default row height

        // Count rows and measure them
        for (uint32_t i = 0; i < child->child_count; i++) {
            IRComponent* section = child->children[i];
            if (section->type != IR_COMPONENT_TABLE_HEAD &&
                section->type != IR_COMPONENT_TABLE_BODY &&
                section->type != IR_COMPONENT_TABLE_FOOT) continue;

            for (uint32_t j = 0; j < section->child_count; j++) {
                IRComponent* row = section->children[j];
                if (row->type != IR_COMPONENT_TABLE_ROW) continue;
                total_height += row_height;
            }
        }

        // Add cell padding (state already declared above)
        if (state) {
            // Account for cell padding and borders
            float cell_padding = state->style.cell_padding > 0 ? state->style.cell_padding : 8.0f;
            total_height += cell_padding * 2.0f;  // Top/bottom padding for header
        }

        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    📊 TABLE measurement: height=%.1f\n", total_height);
        }

        return is_height ? (total_height > 0 ? total_height : 100.0f) : parent_rect.width;
    }

    // Flowchart AUTO sizing - use IR layer intrinsic sizing
    if (child->type == IR_COMPONENT_FLOWCHART) {
        fprintf(stderr, "    🎯 Desktop FLOWCHART case: calling IR intrinsic\n");
        float result = is_height ?
            ir_get_component_intrinsic_height(child) :
            ir_get_component_intrinsic_width(child);
        fprintf(stderr, "    🎯 Desktop FLOWCHART result: %.1f\n", result);
        if (getenv("KRYON_TRACE_LAYOUT")) {
            printf("    📐 FLOWCHART %s: %.1f\n", is_height ? "height" : "width", result);
        }
        return result;
    }

    // For Container/Row/Column with AUTO size, we need to measure children
    // But we CANNOT recurse into full layout - just sum up their explicit sizes
    if (getenv("KRYON_TRACE_LAYOUT")) {
        const char* type_name = "OTHER";
        if (child->type == IR_COMPONENT_ROW) type_name = "ROW";
        else if (child->type == IR_COMPONENT_COLUMN) type_name = "COLUMN";
        else if (child->type == IR_COMPONENT_CONTAINER) type_name = "CONTAINER";
        else if (child->type == IR_COMPONENT_TABLE) type_name = "TABLE";
        printf("    🔍 Checking %s for recursive measurement: type_match=%d, child_count=%d\n",
               type_name,
               (child->type == IR_COMPONENT_CONTAINER || child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_COLUMN),
               child->child_count);
    }

    if ((child->type == IR_COMPONENT_CONTAINER ||
         child->type == IR_COMPONENT_ROW ||
         child->type == IR_COMPONENT_COLUMN ||
         child->type == IR_COMPONENT_TAB_BAR) &&
        child->child_count > 0) {

        float total = 0.0f;
        float gap = 0.0f;
        if (child->layout && child->layout->flex.gap > 0) {
            gap = (float)child->layout->flex.gap;
        }

        // Determine if we're measuring the main axis
        // TabBar is treated as horizontal (Row-like)
        bool is_main_axis = (is_height && (child->type == IR_COMPONENT_COLUMN || child->type == IR_COMPONENT_CONTAINER)) ||
                           (!is_height && (child->type == IR_COMPONENT_ROW || child->type == IR_COMPONENT_TAB_BAR));

        if (getenv("KRYON_TRACE_LAYOUT")) {
            const char* child_type_name = child->type == IR_COMPONENT_ROW ? "ROW" :
                                         child->type == IR_COMPONENT_COLUMN ? "COLUMN" : "CONTAINER";
            printf("    📐 Measuring %s's %s (main_axis=%d, %d children)\n",
                   child_type_name, is_height ? "HEIGHT" : "WIDTH", is_main_axis, child->child_count);
        }

        for (uint32_t i = 0; i < child->child_count; i++) {
            IRComponent* grandchild = child->children[i];

            // Get grandchild dimension; prefer explicit, otherwise measure recursively
            float child_dim = 0.0f;
            if (grandchild && grandchild->style) {
                IRDimension gd = is_height ? grandchild->style->height : grandchild->style->width;
                if (gd.type != IR_DIMENSION_AUTO) {
                    child_dim = ir_dimension_to_pixels(gd, is_height ? parent_rect.height : parent_rect.width);
                }
            }
            if (child_dim <= 0.0f && grandchild) {
                child_dim = get_child_dimension(grandchild, parent_rect, is_height);
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

        // Add padding to the total dimension
        if (child->style) {
            if (is_height) {
                total += child->style->padding.top + child->style->padding.bottom;
            } else {
                total += child->style->padding.left + child->style->padding.right;
            }
        }

        if (getenv("KRYON_TRACE_LAYOUT")) {
            if (is_height && child->style) {
                printf("      └─ Content %s: %.1f + padding (%.1f+%.1f) = %.1f\n",
                       is_height ? "HEIGHT" : "WIDTH",
                       total - child->style->padding.top - child->style->padding.bottom,
                       child->style->padding.top, child->style->padding.bottom, total);
            } else if (child->style) {
                printf("      └─ Content %s: %.1f + padding (%.1f+%.1f) = %.1f\n",
                       is_height ? "HEIGHT" : "WIDTH",
                       total - child->style->padding.left - child->style->padding.right,
                       child->style->padding.left, child->style->padding.right, total);
            } else {
                printf("      └─ Total %s: %.1f (no padding)\n", is_height ? "HEIGHT" : "WIDTH", total);
            }
        }

        return total;
    }

    // Default: return 0 for unknown AUTO-sized components
    return 0.0f;
}
