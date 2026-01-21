/**
 * @file ir_json_serialize.c
 * @brief JSON serialization functions for IR component trees
 *
 * This module contains all functions for converting IR component trees
 * to JSON format (KIR - Kryon Intermediate Representation).
 */

#define _GNU_SOURCE
#include "ir_json_serialize.h"
#include "ir_json_context.h"
#include "../include/ir_core.h"
#include "../include/ir_builder.h"
#include "../include/ir_logic.h"
#include "../style/ir_stylesheet.h"
#include "../logic/ir_foreach.h"
#include "../utils/ir_c_metadata.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Android logging
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "KryonIR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) fprintf(stderr, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

// External global context from ir_core.c
extern IRContext* g_ir_context;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Serialize dimension to JSON string (e.g., "100px", "50%", "auto")
 */
char* ir_json_serialize_dimension_to_string(IRDimension dim) {
    char buffer[64];

    switch (dim.type) {
        case IR_DIMENSION_AUTO:
            snprintf(buffer, sizeof(buffer), "auto");
            break;
        case IR_DIMENSION_PX:
            snprintf(buffer, sizeof(buffer), "%.1fpx", dim.value);
            break;
        case IR_DIMENSION_PERCENT:
            snprintf(buffer, sizeof(buffer), "%.1f%%", dim.value);
            break;
        case IR_DIMENSION_EM:
            snprintf(buffer, sizeof(buffer), "%.2fem", dim.value);
            break;
        case IR_DIMENSION_REM:
            snprintf(buffer, sizeof(buffer), "%.2frem", dim.value);
            break;
        case IR_DIMENSION_VW:
            snprintf(buffer, sizeof(buffer), "%.1fvw", dim.value);
            break;
        case IR_DIMENSION_VH:
            snprintf(buffer, sizeof(buffer), "%.1fvh", dim.value);
            break;
        case IR_DIMENSION_FLEX:
            snprintf(buffer, sizeof(buffer), "%.1ffr", dim.value);
            break;
        default:
            snprintf(buffer, sizeof(buffer), "%.1fpx", dim.value);
    }

    return strdup(buffer);
}

/**
 * Format rgba values to hex string in provided buffer
 */
static void json_rgba_to_hex(char* buffer, size_t size, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (a == 255) {
        snprintf(buffer, size, "#%02x%02x%02x", r, g, b);
    } else {
        snprintf(buffer, size, "#%02x%02x%02x%02x", r, g, b, a);
    }
}

/**
 * Serialize color to JSON string (e.g., "#rrggbb" or "#rrggbbaa")
 */
char* ir_json_serialize_color_to_string(IRColor color) {
    char buffer[64];

    if (color.type == IR_COLOR_SOLID) {
        json_rgba_to_hex(buffer, sizeof(buffer), color.data.r, color.data.g, color.data.b, color.data.a);
        return strdup(buffer);
    } else if (color.type == IR_COLOR_TRANSPARENT) {
        return strdup("transparent");
    } else if (color.type == IR_COLOR_VAR_REF) {
        // Prefer var_name (e.g., "var(--border)") over numeric var_id
        if (color.var_name) {
            return strdup(color.var_name);
        }
        snprintf(buffer, sizeof(buffer), "var(%u)", color.data.var_id);
        return strdup(buffer);
    }

    return strdup("#000000");
}

/**
 * Serialize gradient to JSON object
 * Format: { "type": "linear", "angle": 45, "stops": [...] }
 */
cJSON* ir_json_serialize_gradient(IRGradient* gradient) {
    if (!gradient) return NULL;

    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;

    // Type
    const char* type_str = "linear";
    if (gradient->type == IR_GRADIENT_RADIAL) type_str = "radial";
    else if (gradient->type == IR_GRADIENT_CONIC) type_str = "conic";
    cJSON_AddStringToObject(obj, "type", type_str);

    // Angle (for linear)
    if (gradient->type == IR_GRADIENT_LINEAR) {
        cJSON_AddNumberToObject(obj, "angle", gradient->angle);
    }

    // Center (for radial/conic)
    if (gradient->type == IR_GRADIENT_RADIAL || gradient->type == IR_GRADIENT_CONIC) {
        cJSON_AddNumberToObject(obj, "centerX", gradient->center_x);
        cJSON_AddNumberToObject(obj, "centerY", gradient->center_y);
    }

    // Color stops
    cJSON* stops = cJSON_CreateArray();
    if (stops) {
        for (int i = 0; i < gradient->stop_count; i++) {
            cJSON* stop = cJSON_CreateObject();
            if (stop) {
                cJSON_AddNumberToObject(stop, "position", gradient->stops[i].position);
                char color_str[16];
                json_rgba_to_hex(color_str, sizeof(color_str),
                    gradient->stops[i].r, gradient->stops[i].g,
                    gradient->stops[i].b, gradient->stops[i].a);
                cJSON_AddStringToObject(stop, "color", color_str);
                cJSON_AddItemToArray(stops, stop);
            }
        }
        cJSON_AddItemToObject(obj, "stops", stops);
    }

    return obj;
}

/**
 * Serialize spacing (margin/padding) to JSON
 * Returns number if uniform, array if non-uniform
 */
static cJSON* json_spacing_to_json(IRSpacing spacing) {
    // Check if all values are equal (uniform spacing)
    if (spacing.top == spacing.right && spacing.right == spacing.bottom && spacing.bottom == spacing.left) {
        return cJSON_CreateNumber(spacing.top);
    }

    // Check for vertical/horizontal shorthand [vertical, horizontal]
    if (spacing.top == spacing.bottom && spacing.left == spacing.right) {
        cJSON* array = cJSON_CreateArray();
        if (!array) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) in json_spacing_to_json (shorthand)\n");
            return NULL;
        }
        cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.top));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.left));
        return array;
    }

    // Full array [top, right, bottom, left]
    cJSON* array = cJSON_CreateArray();
    if (!array) {
        fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) in json_spacing_to_json (full)\n");
        return NULL;
    }
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.top));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.right));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.bottom));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.left));
    return array;
}

// ============================================================================
// Unified Enum-to-String Helpers (to eliminate duplication)
// ============================================================================

/**
 * Convert IR_TEXT_ALIGN_* enum to string
 */
static const char* json_text_align_to_string(IRTextAlign align) {
    switch (align) {
        case IR_TEXT_ALIGN_CENTER: return "center";
        case IR_TEXT_ALIGN_RIGHT: return "right";
        case IR_TEXT_ALIGN_JUSTIFY: return "justify";
        case IR_TEXT_ALIGN_LEFT:
        default: return "left";
    }
}

/**
 * Convert IR_ALIGNMENT_* enum to CSS justify-content string
 */
static const char* json_justify_content_to_string(IRAlignment align) {
    switch (align) {
        case IR_ALIGNMENT_CENTER: return "center";
        case IR_ALIGNMENT_END: return "flex-end";
        case IR_ALIGNMENT_SPACE_BETWEEN: return "space-between";
        case IR_ALIGNMENT_SPACE_AROUND: return "space-around";
        case IR_ALIGNMENT_SPACE_EVENLY: return "space-evenly";
        case IR_ALIGNMENT_START:
        default: return "flex-start";
    }
}

/**
 * Convert IR_ALIGNMENT_* enum to CSS align-items string
 */
static const char* json_align_items_to_string(IRAlignment align) {
    switch (align) {
        case IR_ALIGNMENT_CENTER: return "center";
        case IR_ALIGNMENT_END: return "flex-end";
        case IR_ALIGNMENT_STRETCH: return "stretch";
        case IR_ALIGNMENT_START:
        default: return "flex-start";
    }
}

/**
 * Convert IR_LAYOUT_MODE_* enum to CSS display string
 */
static const char* json_display_mode_to_string(IRLayoutMode mode) {
    switch (mode) {
        case IR_LAYOUT_MODE_FLEX: return "flex";
        case IR_LAYOUT_MODE_GRID: return "grid";
        case IR_LAYOUT_MODE_BLOCK:
        default: return "block";
    }
}

/**
 * Convert IR_GRID_TRACK_* enum to string for JSON serialization
 */
static const char* json_grid_track_type_to_string(IRGridTrackType type) {
    switch (type) {
        case IR_GRID_TRACK_PX: return "px";
        case IR_GRID_TRACK_PERCENT: return "percent";
        case IR_GRID_TRACK_FR: return "fr";
        case IR_GRID_TRACK_AUTO: return "auto";
        case IR_GRID_TRACK_MIN_CONTENT: return "min-content";
        case IR_GRID_TRACK_MAX_CONTENT: return "max-content";
        default: return "auto";
    }
}

/**
 * Convert IR_BACKGROUND_CLIP_* enum to string
 */
static const char* json_background_clip_to_string(IRBackgroundClip clip) {
    switch (clip) {
        case IR_BACKGROUND_CLIP_TEXT: return "text";
        case IR_BACKGROUND_CLIP_CONTENT_BOX: return "content-box";
        case IR_BACKGROUND_CLIP_PADDING_BOX: return "padding-box";
        case IR_BACKGROUND_CLIP_BORDER_BOX:
        default: return "border-box";
    }
}

// Forward declaration for property binding helper
static bool has_property_binding(IRComponent* component, const char* property_name);

// ============================================================================
// Style Serialization
// ============================================================================

static void json_serialize_style(cJSON* obj, IRStyle* style, IRComponent* component) {
    if (!style) return;

    // Only serialize non-default values for cleaner output

    // Dimensions
    if (style->width.type != IR_DIMENSION_AUTO) {
        char* widthStr = ir_json_serialize_dimension_to_string(style->width);
        cJSON_AddStringToObject(obj, "width", widthStr);
        free(widthStr);
    }

    if (style->height.type != IR_DIMENSION_AUTO) {
        char* heightStr = ir_json_serialize_dimension_to_string(style->height);
        cJSON_AddStringToObject(obj, "height", heightStr);
        free(heightStr);
    }

    // Visibility
    if (!style->visible) {
        cJSON_AddBoolToObject(obj, "visible", false);
    }

    // Opacity
    if (fabs(style->opacity - 1.0f) > 0.001f) {
        cJSON_AddNumberToObject(obj, "opacity", style->opacity);
    }

    // Z-index
    if (style->z_index != 0) {
        cJSON_AddNumberToObject(obj, "zIndex", style->z_index);
    }

    // Background color/gradient - always serialize if it has a binding
    if (style->background.type == IR_COLOR_GRADIENT && style->background.data.gradient) {
        // Serialize gradient as object
        cJSON* gradient_obj = ir_json_serialize_gradient(style->background.data.gradient);
        if (gradient_obj) {
            cJSON_AddItemToObject(obj, "backgroundGradient", gradient_obj);
        }
    } else if (style->background.type != IR_COLOR_TRANSPARENT || has_property_binding(component, "background")) {
        char* colorStr = ir_json_serialize_color_to_string(style->background);
        cJSON_AddStringToObject(obj, "background", colorStr);
        free(colorStr);
    }

    // Border (width, color, radius - any can be set independently)
    bool has_border = style->border.width > 0 || style->border.radius > 0 ||
                      style->border.width_top > 0 || style->border.width_right > 0 ||
                      style->border.width_bottom > 0 || style->border.width_left > 0;
    if (has_border) {
        cJSON* border = cJSON_CreateObject();
        if (!border) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for border in json_serialize_style\n");
            return;
        }

        if (style->border.width > 0) {
            cJSON_AddNumberToObject(border, "width", style->border.width);
        }

        // Per-side widths
        if (style->border.width_top > 0) {
            cJSON_AddNumberToObject(border, "widthTop", style->border.width_top);
        }
        if (style->border.width_right > 0) {
            cJSON_AddNumberToObject(border, "widthRight", style->border.width_right);
        }
        if (style->border.width_bottom > 0) {
            cJSON_AddNumberToObject(border, "widthBottom", style->border.width_bottom);
        }
        if (style->border.width_left > 0) {
            cJSON_AddNumberToObject(border, "widthLeft", style->border.width_left);
        }

        // Border color (if any width is set)
        if (style->border.width > 0 || style->border.width_top > 0 ||
            style->border.width_right > 0 || style->border.width_bottom > 0 ||
            style->border.width_left > 0) {
            char* colorStr = ir_json_serialize_color_to_string(style->border.color);
            cJSON_AddStringToObject(border, "color", colorStr);
            free(colorStr);
        }

        if (style->border.radius > 0) {
            cJSON_AddNumberToObject(border, "radius", style->border.radius);
        }

        cJSON_AddItemToObject(obj, "border", border);
    }

    // Absolute positioning
    if (style->position_mode != IR_POSITION_RELATIVE) {
        const char* posStr = "relative";
        if (style->position_mode == IR_POSITION_ABSOLUTE) posStr = "absolute";
        else if (style->position_mode == IR_POSITION_FIXED) posStr = "fixed";
        cJSON_AddStringToObject(obj, "position", posStr);
    }
    if (style->absolute_x != 0.0f || style->absolute_y != 0.0f) {
        cJSON_AddNumberToObject(obj, "left", style->absolute_x);
        cJSON_AddNumberToObject(obj, "top", style->absolute_y);
    }

    // Typography - always serialize fontSize if it has a binding
    if (style->font.size > 0 || has_property_binding(component, "fontSize")) {
        cJSON_AddNumberToObject(obj, "fontSize", style->font.size);
    }

    if (style->font.family && style->font.family[0] != '\0') {
        cJSON_AddStringToObject(obj, "fontFamily", style->font.family);
    }

    if (style->font.weight != 400 && style->font.weight > 0) {
        cJSON_AddNumberToObject(obj, "fontWeight", style->font.weight);
    }

    if (style->font.bold) {
        cJSON_AddBoolToObject(obj, "fontBold", true);
    }

    if (style->font.italic) {
        cJSON_AddBoolToObject(obj, "fontItalic", true);
    }

    // Line height
    if (style->font.line_height > 0.0f) {
        cJSON_AddNumberToObject(obj, "lineHeight", style->font.line_height);
    }

    // Text color - always serialize if it has a binding
    if (style->font.color.type != IR_COLOR_TRANSPARENT || has_property_binding(component, "color")) {
        char* textColorStr = ir_json_serialize_color_to_string(style->font.color);
        cJSON_AddStringToObject(obj, "color", textColorStr);
        free(textColorStr);
    }

    // Text alignment
    if (style->font.align != IR_TEXT_ALIGN_LEFT) {
        cJSON_AddStringToObject(obj, "textAlign", json_text_align_to_string(style->font.align));
    }

    // Letter and word spacing
    if (style->font.letter_spacing != 0.0f) {
        cJSON_AddNumberToObject(obj, "letterSpacing", style->font.letter_spacing);
    }

    if (style->font.word_spacing != 0.0f) {
        cJSON_AddNumberToObject(obj, "wordSpacing", style->font.word_spacing);
    }

    // Text decoration
    if (style->font.decoration != IR_TEXT_DECORATION_NONE) {
        const char* decorationStr = NULL;
        if (style->font.decoration == IR_TEXT_DECORATION_UNDERLINE) {
            decorationStr = "underline";
        } else if (style->font.decoration == IR_TEXT_DECORATION_OVERLINE) {
            decorationStr = "overline";
        } else if (style->font.decoration == IR_TEXT_DECORATION_LINE_THROUGH) {
            decorationStr = "line-through";
        } else if (style->font.decoration == (IR_TEXT_DECORATION_UNDERLINE | IR_TEXT_DECORATION_LINE_THROUGH)) {
            decorationStr = "underline line-through";
        }
        // Add more combinations as needed

        if (decorationStr) {
            cJSON_AddStringToObject(obj, "textDecoration", decorationStr);
        }
    }

    // Spacing
    bool hasPadding = style->padding.top > 0 || style->padding.right > 0 ||
                     style->padding.bottom > 0 || style->padding.left > 0;
    if (hasPadding) {
        cJSON* padding = json_spacing_to_json(style->padding);
        cJSON_AddItemToObject(obj, "padding", padding);
    }

    bool hasMargin = style->margin.top != 0 || style->margin.right != 0 ||
                    style->margin.bottom != 0 || style->margin.left != 0;
    if (hasMargin) {
        cJSON* margin = json_spacing_to_json(style->margin);
        cJSON_AddItemToObject(obj, "margin", margin);
    }

    // Transform
    if (fabs(style->transform.translate_x) > 0.001f || fabs(style->transform.translate_y) > 0.001f ||
        fabs(style->transform.scale_x - 1.0f) > 0.001f || fabs(style->transform.scale_y - 1.0f) > 0.001f ||
        fabs(style->transform.rotate) > 0.001f) {
        cJSON* transform = cJSON_CreateObject();
        if (!transform) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for transform in json_serialize_style\n");
            return;
        }

        if (fabs(style->transform.translate_x) > 0.001f || fabs(style->transform.translate_y) > 0.001f) {
            cJSON* translate = cJSON_CreateArray();
            if (!translate) {
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for translate in json_serialize_style\n");
                cJSON_Delete(transform);
                return;
            }
            cJSON_AddItemToArray(translate, cJSON_CreateNumber(style->transform.translate_x));
            cJSON_AddItemToArray(translate, cJSON_CreateNumber(style->transform.translate_y));
            cJSON_AddItemToObject(transform, "translate", translate);
        }

        if (fabs(style->transform.scale_x - 1.0f) > 0.001f || fabs(style->transform.scale_y - 1.0f) > 0.001f) {
            cJSON* scale = cJSON_CreateArray();
            if (!scale) {
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for scale in json_serialize_style\n");
                cJSON_Delete(transform);
                return;
            }
            cJSON_AddItemToArray(scale, cJSON_CreateNumber(style->transform.scale_x));
            cJSON_AddItemToArray(scale, cJSON_CreateNumber(style->transform.scale_y));
            cJSON_AddItemToObject(transform, "scale", scale);
        }

        if (fabs(style->transform.rotate) > 0.001f) {
            cJSON_AddNumberToObject(transform, "rotate", style->transform.rotate);
        }

        cJSON_AddItemToObject(obj, "transform", transform);
    }

    // Background image (gradient string) for gradient text effects
    if (style->background_image && style->background_image[0] != '\0') {
        cJSON_AddStringToObject(obj, "backgroundImage", style->background_image);
    }

    // Background clip (for gradient text effects)
    if (style->background_clip != IR_BACKGROUND_CLIP_BORDER_BOX) {
        cJSON_AddStringToObject(obj, "backgroundClip", json_background_clip_to_string(style->background_clip));
    }

    // Text fill color (for gradient text effects)
    // Only serialize if explicitly set: type is TRANSPARENT, or type is SOLID with non-zero alpha
    // Skip if all zeros (default uninitialized state)
    bool has_text_fill = false;
    if (style->text_fill_color.type == IR_COLOR_TRANSPARENT) {
        has_text_fill = true;
    } else if (style->text_fill_color.type == IR_COLOR_SOLID &&
               style->text_fill_color.data.a > 0) {
        has_text_fill = true;
    }
    if (has_text_fill) {
        char* fillColorStr = ir_json_serialize_color_to_string(style->text_fill_color);
        cJSON_AddStringToObject(obj, "textFillColor", fillColorStr);
        free(fillColorStr);
    }
}

void ir_json_serialize_style(cJSON* obj, IRStyle* style, IRComponent* component) {
    json_serialize_style(obj, style, component);
}

// ============================================================================
// Layout Serialization
// ============================================================================

static void json_serialize_layout(cJSON* obj, IRLayout* layout, IRComponent* component) {
    if (!layout) return;

    // Display mode (only if explicitly set)
    if (layout->display_explicit) {
        cJSON_AddStringToObject(obj, "display", json_display_mode_to_string(layout->mode));
    }

    // Min/Max dimensions
    if (layout->min_width.type != IR_DIMENSION_AUTO) {
        char* minWidthStr = ir_json_serialize_dimension_to_string(layout->min_width);
        cJSON_AddStringToObject(obj, "minWidth", minWidthStr);
        free(minWidthStr);
    }

    if (layout->min_height.type != IR_DIMENSION_AUTO) {
        char* minHeightStr = ir_json_serialize_dimension_to_string(layout->min_height);
        cJSON_AddStringToObject(obj, "minHeight", minHeightStr);
        free(minHeightStr);
    }

    if (layout->max_width.type != IR_DIMENSION_AUTO) {
        char* maxWidthStr = ir_json_serialize_dimension_to_string(layout->max_width);
        cJSON_AddStringToObject(obj, "maxWidth", maxWidthStr);
        free(maxWidthStr);
    }

    if (layout->max_height.type != IR_DIMENSION_AUTO) {
        char* maxHeightStr = ir_json_serialize_dimension_to_string(layout->max_height);
        cJSON_AddStringToObject(obj, "maxHeight", maxHeightStr);
        free(maxHeightStr);
    }

    // Flexbox properties
    if (layout->flex.direction != 0) {  // 0 = column
        cJSON_AddStringToObject(obj, "flexDirection", layout->flex.direction == 1 ? "row" : "column");
    }

    // BiDi direction property (CSS direction)
    if (layout->flex.base_direction != IR_DIRECTION_LTR) {
        const char* dirStr = "ltr";
        switch (layout->flex.base_direction) {
            case IR_DIRECTION_RTL: dirStr = "rtl"; break;
            case IR_DIRECTION_AUTO: dirStr = "auto"; break;
            case IR_DIRECTION_INHERIT: dirStr = "inherit"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "direction", dirStr);
    }

    // BiDi unicode-bidi property
    if (layout->flex.unicode_bidi != IR_UNICODE_BIDI_NORMAL) {
        const char* bidiStr = "normal";
        switch (layout->flex.unicode_bidi) {
            case IR_UNICODE_BIDI_EMBED: bidiStr = "embed"; break;
            case IR_UNICODE_BIDI_ISOLATE: bidiStr = "isolate"; break;
            case IR_UNICODE_BIDI_PLAINTEXT: bidiStr = "plaintext"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "unicodeBidi", bidiStr);
    }

    // Always serialize justifyContent if it has a binding, even if default value
    if (layout->flex.justify_content != IR_ALIGNMENT_START || has_property_binding(component, "justifyContent")) {
        cJSON_AddStringToObject(obj, "justifyContent", json_justify_content_to_string(layout->flex.justify_content));
    }

    // Always serialize alignItems if it has a binding, even if default value
    if (layout->flex.cross_axis != IR_ALIGNMENT_START || has_property_binding(component, "alignItems")) {
        cJSON_AddStringToObject(obj, "alignItems", json_align_items_to_string(layout->flex.cross_axis));
    }

    // Always serialize gap if it has a binding, even if default value
    if (layout->flex.gap > 0 || has_property_binding(component, "gap")) {
        cJSON_AddNumberToObject(obj, "gap", layout->flex.gap);
    }

    if (layout->flex.grow > 0) {
        cJSON_AddNumberToObject(obj, "flexGrow", layout->flex.grow);
    }

    if (layout->flex.shrink != 1) {
        cJSON_AddNumberToObject(obj, "flexShrink", layout->flex.shrink);
    }

    if (layout->flex.wrap) {
        cJSON_AddBoolToObject(obj, "flexWrap", true);
    }

    // Aspect ratio
    if (layout->aspect_ratio > 0) {
        cJSON_AddNumberToObject(obj, "aspectRatio", layout->aspect_ratio);
    }

    // Grid properties (only if grid mode)
    if (layout->mode == IR_LAYOUT_MODE_GRID) {
        IRGrid* grid = &layout->grid;

        // Grid row/column gaps
        if (grid->row_gap > 0) {
            cJSON_AddNumberToObject(obj, "rowGap", grid->row_gap);
        }
        if (grid->column_gap > 0) {
            cJSON_AddNumberToObject(obj, "columnGap", grid->column_gap);
        }

        // Grid column repeat
        if (grid->use_column_repeat && grid->column_repeat.mode != IR_GRID_REPEAT_NONE) {
            cJSON* repeat_obj = cJSON_CreateObject();
            if (!repeat_obj) {
                fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for grid column repeat\n");
                return;
            }

            const char* mode_str = "none";
            switch (grid->column_repeat.mode) {
                case IR_GRID_REPEAT_AUTO_FIT: mode_str = "auto-fit"; break;
                case IR_GRID_REPEAT_AUTO_FILL: mode_str = "auto-fill"; break;
                case IR_GRID_REPEAT_COUNT: mode_str = "count"; break;
                default: break;
            }
            cJSON_AddStringToObject(repeat_obj, "mode", mode_str);

            if (grid->column_repeat.mode == IR_GRID_REPEAT_COUNT) {
                cJSON_AddNumberToObject(repeat_obj, "count", grid->column_repeat.count);
            }

            // Minmax or track
            if (grid->column_repeat.has_minmax) {
                cJSON* minmax_obj = cJSON_CreateObject();
                if (minmax_obj) {
                    cJSON_AddStringToObject(minmax_obj, "minType",
                        json_grid_track_type_to_string(grid->column_repeat.minmax.min_type));
                    cJSON_AddNumberToObject(minmax_obj, "minValue", grid->column_repeat.minmax.min_value);
                    cJSON_AddStringToObject(minmax_obj, "maxType",
                        json_grid_track_type_to_string(grid->column_repeat.minmax.max_type));
                    cJSON_AddNumberToObject(minmax_obj, "maxValue", grid->column_repeat.minmax.max_value);
                    cJSON_AddItemToObject(repeat_obj, "minmax", minmax_obj);
                }
            } else {
                cJSON_AddStringToObject(repeat_obj, "trackType",
                    json_grid_track_type_to_string(grid->column_repeat.track.type));
                cJSON_AddNumberToObject(repeat_obj, "trackValue", grid->column_repeat.track.value);
            }

            cJSON_AddItemToObject(obj, "gridColumnRepeat", repeat_obj);
        } else if (grid->column_count > 0) {
            // Explicit columns
            cJSON* cols = cJSON_CreateArray();
            if (cols) {
                for (uint8_t i = 0; i < grid->column_count; i++) {
                    cJSON* col = cJSON_CreateObject();
                    if (col) {
                        cJSON_AddStringToObject(col, "type",
                            json_grid_track_type_to_string(grid->columns[i].type));
                        cJSON_AddNumberToObject(col, "value", grid->columns[i].value);
                        cJSON_AddItemToArray(cols, col);
                    }
                }
                cJSON_AddItemToObject(obj, "gridColumns", cols);
            }
        }

        // Grid row repeat (similar to columns)
        if (grid->use_row_repeat && grid->row_repeat.mode != IR_GRID_REPEAT_NONE) {
            cJSON* repeat_obj = cJSON_CreateObject();
            if (repeat_obj) {
                const char* mode_str = "none";
                switch (grid->row_repeat.mode) {
                    case IR_GRID_REPEAT_AUTO_FIT: mode_str = "auto-fit"; break;
                    case IR_GRID_REPEAT_AUTO_FILL: mode_str = "auto-fill"; break;
                    case IR_GRID_REPEAT_COUNT: mode_str = "count"; break;
                    default: break;
                }
                cJSON_AddStringToObject(repeat_obj, "mode", mode_str);

                if (grid->row_repeat.mode == IR_GRID_REPEAT_COUNT) {
                    cJSON_AddNumberToObject(repeat_obj, "count", grid->row_repeat.count);
                }

                if (grid->row_repeat.has_minmax) {
                    cJSON* minmax_obj = cJSON_CreateObject();
                    if (minmax_obj) {
                        cJSON_AddStringToObject(minmax_obj, "minType",
                            json_grid_track_type_to_string(grid->row_repeat.minmax.min_type));
                        cJSON_AddNumberToObject(minmax_obj, "minValue", grid->row_repeat.minmax.min_value);
                        cJSON_AddStringToObject(minmax_obj, "maxType",
                            json_grid_track_type_to_string(grid->row_repeat.minmax.max_type));
                        cJSON_AddNumberToObject(minmax_obj, "maxValue", grid->row_repeat.minmax.max_value);
                        cJSON_AddItemToObject(repeat_obj, "minmax", minmax_obj);
                    }
                } else {
                    cJSON_AddStringToObject(repeat_obj, "trackType",
                        json_grid_track_type_to_string(grid->row_repeat.track.type));
                    cJSON_AddNumberToObject(repeat_obj, "trackValue", grid->row_repeat.track.value);
                }

                cJSON_AddItemToObject(obj, "gridRowRepeat", repeat_obj);
            }
        } else if (grid->row_count > 0) {
            // Explicit rows
            cJSON* rows = cJSON_CreateArray();
            if (rows) {
                for (uint8_t i = 0; i < grid->row_count; i++) {
                    cJSON* row = cJSON_CreateObject();
                    if (row) {
                        cJSON_AddStringToObject(row, "type",
                            json_grid_track_type_to_string(grid->rows[i].type));
                        cJSON_AddNumberToObject(row, "value", grid->rows[i].value);
                        cJSON_AddItemToArray(rows, row);
                    }
                }
                cJSON_AddItemToObject(obj, "gridRows", rows);
            }
        }

        // Grid alignment
        if (grid->justify_items != IR_ALIGNMENT_START) {
            cJSON_AddStringToObject(obj, "justifyItems", json_align_items_to_string(grid->justify_items));
        }
        if (grid->align_items != IR_ALIGNMENT_START) {
            cJSON_AddStringToObject(obj, "gridAlignItems", json_align_items_to_string(grid->align_items));
        }
    }
}

void ir_json_serialize_layout(cJSON* obj, IRLayout* layout, IRComponent* component) {
    json_serialize_layout(obj, layout, component);
}

// ============================================================================
// Component Serialization (Recursive)
// ============================================================================
// Forward declaration for property binding serialization
static cJSON* json_serialize_property_binding(IRPropertyBinding* binding);

// Helper to check if component has a binding for a given property
// This is used to ensure properties with bindings are always serialized, even if they have default values
static bool has_property_binding(IRComponent* component, const char* property_name) {
    if (!component || !property_name || component->property_binding_count == 0) {
        return false;
    }

    for (uint32_t i = 0; i < component->property_binding_count; i++) {
        IRPropertyBinding* binding = component->property_bindings[i];
        if (binding && binding->property_name && strcmp(binding->property_name, property_name) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Serialize component - with option to serialize as reference or full tree
 * @param component Component to serialize
 * @param as_template If true, serialize full tree (for templates). If false, output reference when component_ref is set.
 */
static cJSON* json_serialize_component_impl(IRComponent* component, bool as_template) {
    if (!component) return NULL;

    cJSON* obj = cJSON_CreateObject();
    if (!obj) {
        fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) in json_serialize_component_impl\n");
        return NULL;
    }

    // If this is a component instance (has component_ref) and we're not serializing as template,
    // output just a reference instead of the full tree
    if (!as_template && component->component_ref && component->component_ref[0] != '\0') {
        // Use "type" to match what the deserializer expects for component expansion
        cJSON_AddStringToObject(obj, "type", component->component_ref);
        cJSON_AddNumberToObject(obj, "id", component->id);

        // Flatten props to top-level fields (deserializer expects this format)
        if (component->component_props && component->component_props[0] != '\0') {
            cJSON* propsJson = cJSON_Parse(component->component_props);
            if (propsJson) {
                // Copy each prop field to the top-level object
                cJSON* prop = propsJson->child;
                while (prop) {
                    cJSON* nextProp = prop->next;
                    cJSON_DetachItemViaPointer(propsJson, prop);
                    cJSON_AddItemToObject(obj, prop->string, prop);
                    prop = nextProp;
                }
                cJSON_Delete(propsJson);  // Delete the empty container
            }
        }
        return obj;
    }

    // If this is a module reference (has module_ref) and we're not serializing as template,
    // output a module reference instead of the full tree
    if (!as_template && component->module_ref && component->module_ref[0] != '\0') {
        // Create type string with $module: prefix
        char* type_str = NULL;
        if (component->export_name && component->export_name[0] != '\0') {
            // Include export name: $module:components/tabs#buildTabsAndPanels
            asprintf(&type_str, "$module:%s#%s", component->module_ref, component->export_name);
        } else {
            // Just module reference: $module:components/tabs
            asprintf(&type_str, "$module:%s", component->module_ref);
        }

        if (type_str) {
            cJSON_AddStringToObject(obj, "type", type_str);
            free(type_str);
        }
        // Preserve actual component type for fallback when module resolution fails
        // This fixes the bug where buttons in tabs render as containers
        const char* actual_type_str = ir_component_type_to_string(component->type);
        if (actual_type_str) {
            cJSON_AddStringToObject(obj, "actual_type", actual_type_str);
        }
        cJSON_AddNumberToObject(obj, "id", component->id);

        // Flatten props to top-level fields (same as component_ref)
        if (component->component_props && component->component_props[0] != '\0') {
            cJSON* propsJson = cJSON_Parse(component->component_props);
            if (propsJson) {
                cJSON* prop = propsJson->child;
                while (prop) {
                    cJSON* nextProp = prop->next;
                    cJSON_DetachItemViaPointer(propsJson, prop);
                    cJSON_AddItemToObject(obj, prop->string, prop);
                    prop = nextProp;
                }
                cJSON_Delete(propsJson);
            }
        }

        // Include text content (for tab titles, labels, etc.)
        if (component->text_content && component->text_content[0] != '\0') {
            cJSON_AddStringToObject(obj, "text", component->text_content);
        }

        // Include background color if non-default (for tab colors, etc.)
        if (component->style) {
            IRStyle* style = component->style;
            if (style->background.type == IR_COLOR_SOLID) {
                char* bgColorStr = ir_json_serialize_color_to_string(style->background);
                if (bgColorStr) {
                    cJSON_AddStringToObject(obj, "background", bgColorStr);
                    free(bgColorStr);
                }
            }
            // Include text color
            if (style->font.color.type != IR_COLOR_TRANSPARENT) {
                char* textColorStr = ir_json_serialize_color_to_string(style->font.color);
                if (textColorStr) {
                    cJSON_AddStringToObject(obj, "color", textColorStr);
                    free(textColorStr);
                }
            }
        }

        return obj;
    }

    // Basic properties
    cJSON_AddNumberToObject(obj, "id", component->id);

    const char* typeStr = ir_component_type_to_string(component->type);
    cJSON_AddStringToObject(obj, "type", typeStr);

    // Semantic HTML tag (for roundtrip preservation)
    if (component->tag && component->tag[0] != '\0') {
        cJSON_AddStringToObject(obj, "tag", component->tag);
    }

    // CSS class name (for styling, separate from semantic tag)
    if (component->css_class && component->css_class[0] != '\0') {
        cJSON_AddStringToObject(obj, "css_class", component->css_class);
    }

    // CSS selector type (for accurate roundtrip: element vs class selector)
    if (component->selector_type != IR_SELECTOR_NONE) {
        const char* selectorStr = "class";
        switch (component->selector_type) {
            case IR_SELECTOR_ELEMENT: selectorStr = "element"; break;
            case IR_SELECTOR_CLASS: selectorStr = "class"; break;
            case IR_SELECTOR_ID: selectorStr = "id"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "selector_type", selectorStr);
    }

    // Text content - for templates with text_expression, use the expression as the text value
    // This ensures placeholders like {{value}} are preserved in component definitions
    if (as_template && component->text_expression && component->text_expression[0] != '\0') {
        // Template mode: use text_expression for both "text" and "text_expression"
        cJSON_AddStringToObject(obj, "text", component->text_expression);
        cJSON_AddStringToObject(obj, "text_expression", component->text_expression);
    } else {
        // Normal mode: use actual text_content
        if (component->text_content && component->text_content[0] != '\0') {
            cJSON_AddStringToObject(obj, "text", component->text_content);
        }
        // Reactive text expression (for declarative .kir files)
        if (component->text_expression && component->text_expression[0] != '\0') {
            cJSON_AddStringToObject(obj, "text_expression", component->text_expression);
        }
    }

    // Scope (for scoped variable lookups)
    if (component->scope && component->scope[0] != '\0') {
        cJSON_AddStringToObject(obj, "scope", component->scope);
    }

    // Component reference (for custom components) - only in template mode
    if (as_template && component->component_ref && component->component_ref[0] != '\0') {
        cJSON_AddStringToObject(obj, "component_ref", component->component_ref);
    }

    // Serialize style properties
    if (component->style) {
        json_serialize_style(obj, component->style, component);
    }

    // Serialize layout properties
    if (component->layout) {
        json_serialize_layout(obj, component->layout, component);
    }

    // Serialize checkbox state (stored in custom_data)
    if (component->type == IR_COMPONENT_CHECKBOX && component->custom_data) {
        bool is_checked = strcmp(component->custom_data, "checked") == 0;
        cJSON_AddBoolToObject(obj, "checked", is_checked);
    }

    // Serialize Image src and alt
    if (component->type == IR_COMPONENT_IMAGE) {
        if (component->custom_data) {
            cJSON_AddStringToObject(obj, "src", component->custom_data);
        }
        if (component->text_content) {
            cJSON_AddStringToObject(obj, "alt", component->text_content);
        }
    }

    // Serialize dropdown state (stored in custom_data as IRDropdownState*)
    if (component->type == IR_COMPONENT_DROPDOWN && component->custom_data) {
        IRDropdownState* state = (IRDropdownState*)component->custom_data;
        cJSON* dropdownState = cJSON_CreateObject();
        if (!dropdownState) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for dropdown state\n");
            return obj;
        }

        // Placeholder
        if (state->placeholder) {
            cJSON_AddStringToObject(dropdownState, "placeholder", state->placeholder);
        }

        // Options array
        if (state->options && state->option_count > 0) {
            cJSON* optionsArr = cJSON_CreateArray();
            if (!optionsArr) {
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for dropdown options\n");
                cJSON_Delete(dropdownState);
                return obj;
            }
            for (uint32_t i = 0; i < state->option_count; i++) {
                if (state->options[i]) {
                    cJSON_AddItemToArray(optionsArr, cJSON_CreateString(state->options[i]));
                }
            }
            cJSON_AddItemToObject(dropdownState, "options", optionsArr);
        }

        // Selected index
        cJSON_AddNumberToObject(dropdownState, "selectedIndex", state->selected_index);

        // Is open state (usually false when serializing)
        cJSON_AddBoolToObject(dropdownState, "isOpen", state->is_open);

        cJSON_AddItemToObject(obj, "dropdown_state", dropdownState);
    }

    // Serialize modal state (stored in custom_data as string: "open|title" or "closed|title")
    if (component->type == IR_COMPONENT_MODAL && component->custom_data) {
        const char* state_str = component->custom_data;
        bool is_open = (strncmp(state_str, "open", 4) == 0);
        const char* title = NULL;
        const char* pipe = strchr(state_str, '|');
        if (pipe && pipe[1] != '\0') {
            title = pipe + 1;
        }

        cJSON* modalState = cJSON_CreateObject();
        if (modalState) {
            cJSON_AddBoolToObject(modalState, "isOpen", is_open);
            if (title) {
                cJSON_AddStringToObject(modalState, "title", title);
            }
            cJSON_AddItemToObject(obj, "modal_state", modalState);
        }
    }

    // Serialize table state (stored in custom_data as IRTableState*)
    if (component->type == IR_COMPONENT_TABLE && component->custom_data) {
        IRTableState* state = (IRTableState*)component->custom_data;
        cJSON* tableConfig = cJSON_CreateObject();
        if (!tableConfig) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for table config\n");
            return obj;
        }

        // Columns
        if (state->columns && state->column_count > 0) {
            cJSON* columnsArr = cJSON_CreateArray();
            if (!columnsArr) {
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for table columns\n");
                cJSON_Delete(tableConfig);
                return obj;
            }
            for (uint32_t i = 0; i < state->column_count; i++) {
                cJSON* colObj = cJSON_CreateObject();
                if (!colObj) {
                    fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for column object\n");
                    cJSON_Delete(columnsArr);
                    cJSON_Delete(tableConfig);
                    return obj;
                }
                IRTableColumnDef* col = &state->columns[i];

                // Width dimension
                if (col->width.type == IR_DIMENSION_PX) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.0fpx", col->width.value);
                    cJSON_AddStringToObject(colObj, "width", buf);
                } else if (col->width.type == IR_DIMENSION_PERCENT) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.0f%%", col->width.value);
                    cJSON_AddStringToObject(colObj, "width", buf);
                } else {
                    cJSON_AddStringToObject(colObj, "width", "auto");
                }

                // Min/max widths
                if (col->min_width.type == IR_DIMENSION_PX && col->min_width.value > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.0fpx", col->min_width.value);
                    cJSON_AddStringToObject(colObj, "minWidth", buf);
                }
                if (col->max_width.type == IR_DIMENSION_PX && col->max_width.value > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.0fpx", col->max_width.value);
                    cJSON_AddStringToObject(colObj, "maxWidth", buf);
                }

                // Alignment
                if (col->alignment != IR_ALIGNMENT_START) {
                    const char* alignStr = "start";
                    switch (col->alignment) {
                        case IR_ALIGNMENT_CENTER: alignStr = "center"; break;
                        case IR_ALIGNMENT_END: alignStr = "end"; break;
                        default: break;
                    }
                    cJSON_AddStringToObject(colObj, "alignment", alignStr);
                }

                cJSON_AddBoolToObject(colObj, "autoSize", col->auto_size);
                cJSON_AddItemToArray(columnsArr, colObj);
            }
            cJSON_AddItemToObject(tableConfig, "columns", columnsArr);
        }

        // Table styling
        IRTableStyle* style = &state->style;

        // Border color
        char* colorStr = ir_json_serialize_color_to_string(style->border_color);
        cJSON_AddStringToObject(tableConfig, "borderColor", colorStr);
        free(colorStr);

        // Header background
        colorStr = ir_json_serialize_color_to_string(style->header_background);
        cJSON_AddStringToObject(tableConfig, "headerBackground", colorStr);
        free(colorStr);

        // Row backgrounds (for striping)
        colorStr = ir_json_serialize_color_to_string(style->even_row_background);
        cJSON_AddStringToObject(tableConfig, "evenRowBackground", colorStr);
        free(colorStr);
        colorStr = ir_json_serialize_color_to_string(style->odd_row_background);
        cJSON_AddStringToObject(tableConfig, "oddRowBackground", colorStr);
        free(colorStr);

        cJSON_AddNumberToObject(tableConfig, "borderWidth", style->border_width);
        cJSON_AddNumberToObject(tableConfig, "cellPadding", style->cell_padding);
        cJSON_AddBoolToObject(tableConfig, "showBorders", style->show_borders);
        cJSON_AddBoolToObject(tableConfig, "striped", style->striped_rows);
        cJSON_AddBoolToObject(tableConfig, "headerSticky", style->header_sticky);
        cJSON_AddBoolToObject(tableConfig, "collapseBorders", style->collapse_borders);

        cJSON_AddItemToObject(obj, "table_config", tableConfig);
    }

    // Serialize table cell data (for TableCell and TableHeaderCell)
    if ((component->type == IR_COMPONENT_TABLE_CELL ||
         component->type == IR_COMPONENT_TABLE_HEADER_CELL) && component->custom_data) {
        IRTableCellData* cellData = (IRTableCellData*)component->custom_data;
        cJSON* cellObj = cJSON_CreateObject();
        if (!cellObj) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for table cell data\n");
            return obj;
        }

        cJSON_AddNumberToObject(cellObj, "colspan", cellData->colspan);
        cJSON_AddNumberToObject(cellObj, "rowspan", cellData->rowspan);

        // Alignment
        const char* alignStr = "start";
        switch (cellData->alignment) {
            case IR_ALIGNMENT_CENTER: alignStr = "center"; break;
            case IR_ALIGNMENT_END: alignStr = "end"; break;
            default: break;
        }
        cJSON_AddStringToObject(cellObj, "alignment", alignStr);

        // Vertical alignment
        const char* vAlignStr = "top";
        switch (cellData->vertical_alignment) {
            case IR_ALIGNMENT_CENTER: vAlignStr = "middle"; break;
            case IR_ALIGNMENT_END: vAlignStr = "bottom"; break;
            default: break;
        }
        cJSON_AddStringToObject(cellObj, "verticalAlignment", vAlignStr);

        cJSON_AddItemToObject(obj, "cell_data", cellObj);
    }

    // Serialize markdown component data
    if (component->type == IR_COMPONENT_HEADING && component->custom_data) {
        IRHeadingData* data = (IRHeadingData*)component->custom_data;
        cJSON_AddNumberToObject(obj, "level", data->level);
        if (data->text) {
            cJSON_AddStringToObject(obj, "text", data->text);
        }
        if (data->id) {
            cJSON_AddStringToObject(obj, "id_attr", data->id);
        }
    }

    if (component->type == IR_COMPONENT_CODE_BLOCK && component->custom_data) {
        IRCodeBlockData* data = (IRCodeBlockData*)component->custom_data;
        if (data->language) {
            cJSON_AddStringToObject(obj, "language", data->language);
        }
        if (data->code) {
            cJSON_AddStringToObject(obj, "code", data->code);
        }
        cJSON_AddBoolToObject(obj, "showLineNumbers", data->show_line_numbers);
        // Syntax highlighting is handled by plugins, not serialized in IR
    }

    if (component->type == IR_COMPONENT_LIST && component->custom_data) {
        IRListData* data = (IRListData*)component->custom_data;
        cJSON_AddStringToObject(obj, "listType", data->type == IR_LIST_ORDERED ? "ordered" : "unordered");
        if (data->type == IR_LIST_ORDERED && data->start > 1) {
            cJSON_AddNumberToObject(obj, "start", data->start);
        }
        cJSON_AddBoolToObject(obj, "tight", data->tight);
    }

    if (component->type == IR_COMPONENT_LIST_ITEM && component->custom_data) {
        IRListItemData* data = (IRListItemData*)component->custom_data;
        if (data->marker) {
            cJSON_AddStringToObject(obj, "marker", data->marker);
        }
        if (data->number > 0) {
            cJSON_AddNumberToObject(obj, "number", data->number);
        }
    }

    if (component->type == IR_COMPONENT_LINK && component->custom_data) {
        IRLinkData* data = (IRLinkData*)component->custom_data;
        if (data->url) {
            cJSON_AddStringToObject(obj, "url", data->url);
        }
        if (data->title) {
            cJSON_AddStringToObject(obj, "title", data->title);
        }
        if (data->target) {
            cJSON_AddStringToObject(obj, "target", data->target);
        }
        if (data->rel) {
            cJSON_AddStringToObject(obj, "rel", data->rel);
        }
    }

    // Serialize events (IR v2.1: with bytecode support)
    if (component->events) {
        cJSON* events = cJSON_CreateArray();
        if (!events) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for events array\n");
            return obj;
        }
        IREvent* event = component->events;
        while (event) {
            cJSON* eventObj = cJSON_CreateObject();
            if (!eventObj) {
                fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for event object\n");
                cJSON_Delete(events);
                return obj;
            }

            // Event type
            const char* eventType = "unknown";

            // Core events - fast path with hardcoded switch
            if (event->type < IR_EVENT_PLUGIN_START) {
                switch (event->type) {
                    case IR_EVENT_CLICK: eventType = "click"; break;
                    case IR_EVENT_HOVER: eventType = "hover"; break;
                    case IR_EVENT_FOCUS: eventType = "focus"; break;
                    case IR_EVENT_BLUR: eventType = "blur"; break;
                    case IR_EVENT_TEXT_CHANGE: eventType = "text_change"; break;
                    case IR_EVENT_KEY: eventType = "key"; break;
                    case IR_EVENT_SCROLL: eventType = "scroll"; break;
                    case IR_EVENT_TIMER: eventType = "timer"; break;
                    case IR_EVENT_CUSTOM: eventType = "custom"; break;
                    default: eventType = "unknown"; break;
                }
            }
            // Plugin events - use cached name or lookup in registry
            else if (event->type >= IR_EVENT_PLUGIN_START && event->type <= IR_EVENT_PLUGIN_END) {
                // Use cached name from IREvent.event_name
                if (event->event_name) {
                    eventType = event->event_name;
                } else {
                    eventType = "plugin_event";
                }
            }

            cJSON_AddStringToObject(eventObj, "type", eventType);

            // Legacy logic ID (for C callbacks)
            if (event->logic_id && event->logic_id[0] != '\0') {
                cJSON_AddStringToObject(eventObj, "logic_id", event->logic_id);
            }

            // Handler data
            if (event->handler_data && event->handler_data[0] != '\0') {
                cJSON_AddStringToObject(eventObj, "handler_data", event->handler_data);
            }

            // Bytecode function ID (IR v2.1)
            if (event->bytecode_function_id != 0) {
                cJSON_AddNumberToObject(eventObj, "bytecode_function_id", event->bytecode_function_id);
            }

            // Handler source (IR v2.2: Lua source preservation)
            if (event->handler_source) {
                cJSON* handlerSourceObj = cJSON_CreateObject();
                if (handlerSourceObj) {
                    if (event->handler_source->language) {
                        cJSON_AddStringToObject(handlerSourceObj, "language", event->handler_source->language);
                    }
                    if (event->handler_source->code) {
                        cJSON_AddStringToObject(handlerSourceObj, "code", event->handler_source->code);
                    }
                    if (event->handler_source->file) {
                        cJSON_AddStringToObject(handlerSourceObj, "file", event->handler_source->file);
                    }
                    cJSON_AddNumberToObject(handlerSourceObj, "line", event->handler_source->line);

                    // Closure metadata (IR v2.3: Target-agnostic KIR)
                    if (event->handler_source->uses_closures) {
                        cJSON_AddBoolToObject(handlerSourceObj, "uses_closures", true);

                        if (event->handler_source->closure_vars && event->handler_source->closure_var_count > 0) {
                            cJSON* closureArray = cJSON_CreateArray();
                            for (int i = 0; i < event->handler_source->closure_var_count; i++) {
                                if (event->handler_source->closure_vars[i]) {
                                    cJSON_AddItemToArray(closureArray, cJSON_CreateString(event->handler_source->closure_vars[i]));
                                }
                            }
                            cJSON_AddItemToObject(handlerSourceObj, "closure_vars", closureArray);
                        }
                    } else {
                        cJSON_AddBoolToObject(handlerSourceObj, "uses_closures", false);
                    }

                    cJSON_AddItemToObject(eventObj, "handler_source", handlerSourceObj);
                }
            }

            cJSON_AddItemToArray(events, eventObj);
            event = event->next;
        }
        cJSON_AddItemToObject(obj, "events", events);
    }

    // Serialize conditional visibility
    if (component->visible_condition && component->visible_condition[0] != '\0') {
        cJSON_AddStringToObject(obj, "visible_condition", component->visible_condition);
        cJSON_AddBoolToObject(obj, "visible_when_true", component->visible_when_true);
    }

    // Serialize ForEach (dynamic list rendering) properties
    // First check direct fields (for components created by deserialization)
    if (component->each_source && component->each_source[0] != '\0') {
        cJSON_AddStringToObject(obj, "each_source", component->each_source);
    }
    if (component->each_item_name && component->each_item_name[0] != '\0') {
        cJSON_AddStringToObject(obj, "each_item_name", component->each_item_name);
    }
    if (component->each_index_name && component->each_index_name[0] != '\0') {
        cJSON_AddStringToObject(obj, "each_index_name", component->each_index_name);
    }

    // For ForEach components created by the DSL, parse metadata from custom_data
    if (component->type == IR_COMPONENT_FOR_EACH && !component->each_source && component->custom_data && component->custom_data[0] == '{') {
        cJSON* metadata = cJSON_Parse(component->custom_data);
        if (metadata) {
            cJSON* forEach = cJSON_GetObjectItem(metadata, "forEach");
            if (forEach && cJSON_IsBool(forEach) && cJSON_IsTrue(forEach)) {
                cJSON* each_source = cJSON_GetObjectItem(metadata, "each_source");
                cJSON* each_item_name = cJSON_GetObjectItem(metadata, "each_item_name");
                cJSON* each_index_name = cJSON_GetObjectItem(metadata, "each_index_name");

                if (each_source && cJSON_IsString(each_source)) {
                    cJSON_AddStringToObject(obj, "each_source", each_source->valuestring);
                }
                if (each_item_name && cJSON_IsString(each_item_name)) {
                    cJSON_AddStringToObject(obj, "each_item_name", each_item_name->valuestring);
                }
                if (each_index_name && cJSON_IsString(each_index_name)) {
                    cJSON_AddStringToObject(obj, "each_index_name", each_index_name->valuestring);
                }
            }
            cJSON_Delete(metadata);
        }
    }

    // Serialize foreach_def (new structured ForEach definition)
    if (component->type == IR_COMPONENT_FOR_EACH && component->foreach_def) {
        cJSON* foreach_def_json = ir_foreach_def_to_json(component->foreach_def);
        if (foreach_def_json) {
            cJSON_AddItemToObject(obj, "foreach_def", foreach_def_json);
        }
    }

    // Serialize property bindings (for round-trip codegen)
    if (component->property_binding_count > 0 && component->property_bindings) {
        cJSON* bindings = cJSON_CreateObject();
        if (bindings) {
            for (uint32_t i = 0; i < component->property_binding_count; i++) {
                IRPropertyBinding* binding = component->property_bindings[i];
                if (binding && binding->property_name) {
                    cJSON* binding_obj = json_serialize_property_binding(binding);
                    if (binding_obj) {
                        cJSON_AddItemToObject(bindings, binding->property_name, binding_obj);
                    }
                }
            }
            cJSON_AddItemToObject(obj, "property_bindings", bindings);
        }
    }

    // Serialize source metadata (for round-trip codegen)
    if (component->source_metadata.generated_by) {
        cJSON* src_meta = cJSON_CreateObject();
        if (src_meta) {
            cJSON_AddStringToObject(src_meta, "generated_by", component->source_metadata.generated_by);
            if (component->source_metadata.iteration_index > 0) {
                cJSON_AddNumberToObject(src_meta, "iteration_index", component->source_metadata.iteration_index);
            }
            if (component->source_metadata.is_template) {
                cJSON_AddBoolToObject(src_meta, "is_template", component->source_metadata.is_template);
            }
            cJSON_AddItemToObject(obj, "source_metadata", src_meta);
        }
    }

    // Serialize generic custom_data (for elementId, data-* attributes, etc.)
    // Only serialize if it's a JSON string (starts with '{') and not already handled by component-specific code
    if (component->custom_data && component->custom_data[0] == '{') {
        // Check if this component type uses custom_data for something else
        bool is_special_type = (
            component->type == IR_COMPONENT_CHECKBOX ||
            component->type == IR_COMPONENT_IMAGE ||
            component->type == IR_COMPONENT_DROPDOWN ||
            component->type == IR_COMPONENT_MODAL ||
            component->type == IR_COMPONENT_TABLE ||
            component->type == IR_COMPONENT_TABLE_CELL ||
            component->type == IR_COMPONENT_TABLE_HEADER_CELL ||
            component->type == IR_COMPONENT_HEADING ||
            component->type == IR_COMPONENT_CODE_BLOCK ||
            component->type == IR_COMPONENT_LIST ||
            component->type == IR_COMPONENT_LIST_ITEM ||
            component->type == IR_COMPONENT_LINK ||
            component->type == IR_COMPONENT_TAB_CONTENT
        );
        if (!is_special_type && component->custom_data) {
            // Try to parse custom_data as JSON for cleaner output
            // If it's valid JSON, add it as an object; otherwise add as string
            cJSON* custom_json = cJSON_Parse(component->custom_data);
            if (custom_json) {
                cJSON_AddItemToObject(obj, "custom_data", custom_json);
            } else {
                // Not valid JSON, add as escaped string
                cJSON_AddStringToObject(obj, "custom_data", component->custom_data);
            }
        }
    }

    // Children
    // SPECIAL CASE: TabContent should serialize ALL panels, not just the currently visible one
    // During runtime, ir_tabgroup_select() keeps only one panel in tab_content->children for performance,
    // but when serializing to KIR we need to preserve all panels for round-tripping
    if (component->type == IR_COMPONENT_TAB_CONTENT && component->custom_data) {
        TabGroupState* state = (TabGroupState*)component->custom_data;

        cJSON* children = cJSON_CreateArray();
        if (!children) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for children in json_serialize_component_impl\n");
            cJSON_Delete(obj);
            return NULL;
        }

        // Serialize ALL panels from the state, not just the visible one
        for (uint32_t i = 0; i < state->panel_count; i++) {
            if (state->panels[i]) {
                cJSON* child = json_serialize_component_impl(state->panels[i], as_template);
                if (child) {
                    cJSON_AddItemToArray(children, child);
                }
            }
        }
        cJSON_AddItemToObject(obj, "children", children);
    }
    // NORMAL CASE: All other components serialize their children normally
    else if (component->child_count > 0) {
        cJSON* children = cJSON_CreateArray();
        if (!children) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for children in json_serialize_component_impl\n");
            cJSON_Delete(obj);
            return NULL;
        }
        for (uint32_t i = 0; i < component->child_count; i++) {
            cJSON* child = json_serialize_component_impl(component->children[i], as_template);
            if (child) {
                cJSON_AddItemToArray(children, child);
            }
        }
        cJSON_AddItemToObject(obj, "children", children);
    }

    return obj;
}

// Wrapper for main tree serialization (outputs references for component instances)
cJSON* ir_json_serialize_component_recursive(IRComponent* component) {
    return json_serialize_component_impl(component, false);
}

// Wrapper for template serialization (full tree)
cJSON* ir_json_serialize_component_as_template(IRComponent* component) {
    return json_serialize_component_impl(component, true);
}

// ============================================================================
// Reactive Manifest Serialization (v2.1 POC)
// ============================================================================

/**
 * Serialize property binding to JSON
 */
static cJSON* json_serialize_property_binding(IRPropertyBinding* binding) {
    if (!binding) return NULL;

    cJSON* obj = cJSON_CreateObject();
    if (binding->source_expr) {
        cJSON_AddStringToObject(obj, "source_expr", binding->source_expr);
    }
    if (binding->resolved_value) {
        cJSON_AddStringToObject(obj, "resolved_value", binding->resolved_value);
    }
    if (binding->binding_type) {
        cJSON_AddStringToObject(obj, "binding_type", binding->binding_type);
    }
    return obj;
}

/**
 * Serialize component definitions to JSON
 */
cJSON* ir_json_serialize_component_definitions(IRReactiveManifest* manifest) {
    fprintf(stderr, "[json_serialize_component_definitions] Called\n");
    fflush(stderr);
    if (!manifest || manifest->component_def_count == 0) {
        fprintf(stderr, "[json_serialize_component_definitions] Returning NULL: manifest=%p, count=%u\n",
                (void*)manifest, manifest ? manifest->component_def_count : 0);
        fflush(stderr);
        return NULL;
    }

    fprintf(stderr, "[json_serialize_component_definitions] Creating array for %u defs\n",
            manifest->component_def_count);
    fflush(stderr);

    cJSON* arr = cJSON_CreateArray();
    if (!arr) {
        fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for component definitions\n");
        return NULL;
    }

    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        IRComponentDefinition* def = &manifest->component_defs[i];
        fprintf(stderr, "[json_serialize_component_definitions] Def %u: name='%s'\n",
                i, def->name ? def->name : "(null)");
        fflush(stderr);
        cJSON* defObj = cJSON_CreateObject();
        if (!defObj) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for component definition\n");
            cJSON_Delete(arr);
            return NULL;
        }

        if (def->name) {
            cJSON_AddStringToObject(defObj, "name", def->name);
        }

        // Serialize props
        if (def->prop_count > 0 && def->props) {
            cJSON* propsArr = cJSON_CreateArray();
            if (!propsArr) {
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for component props\n");
                cJSON_Delete(defObj);
                return NULL;
            }
            for (uint32_t j = 0; j < def->prop_count; j++) {
                IRComponentProp* prop = &def->props[j];
                cJSON* propObj = cJSON_CreateObject();
                if (!propObj) {
                    fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for prop object\n");
                    cJSON_Delete(propsArr);
                    cJSON_Delete(defObj);
                    return NULL;
                }
                if (prop->name) cJSON_AddStringToObject(propObj, "name", prop->name);
                if (prop->type) cJSON_AddStringToObject(propObj, "type", prop->type);
                if (prop->default_value) cJSON_AddStringToObject(propObj, "default", prop->default_value);
                cJSON_AddItemToArray(propsArr, propObj);
            }
            cJSON_AddItemToObject(defObj, "props", propsArr);
        }

        // Serialize state variables
        if (def->state_var_count > 0 && def->state_vars) {
            cJSON* stateArr = cJSON_CreateArray();
            if (!stateArr) {
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for state variables\n");
                cJSON_Delete(defObj);
                return NULL;
            }
            for (uint32_t j = 0; j < def->state_var_count; j++) {
                IRComponentStateVar* sv = &def->state_vars[j];
                cJSON* svObj = cJSON_CreateObject();
                if (!svObj) {
                    fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for state var object\n");
                    cJSON_Delete(stateArr);
                    cJSON_Delete(defObj);
                    return NULL;
                }
                if (sv->name) cJSON_AddStringToObject(svObj, "name", sv->name);
                if (sv->type) cJSON_AddStringToObject(svObj, "type", sv->type);
                if (sv->initial_expr) cJSON_AddStringToObject(svObj, "initial", sv->initial_expr);
                cJSON_AddItemToArray(stateArr, svObj);
            }
            cJSON_AddItemToObject(defObj, "state", stateArr);
        }

        // Serialize template if present (use template mode for full tree)
        if (def->template_root) {
            cJSON* templateJson = ir_json_serialize_component_as_template(def->template_root);
            if (templateJson) {
                cJSON_AddItemToObject(defObj, "template", templateJson);
            }
        }

        cJSON_AddItemToArray(arr, defObj);
    }

    return arr;
}

// ============================================================================
// Stylesheet Serialization
// ============================================================================

static cJSON* json_serialize_style_properties(const IRStyleProperties* props) {
    if (!props || props->set_flags == 0) return NULL;

    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;

    // Only serialize properties that are explicitly set
    if (props->set_flags & IR_PROP_DISPLAY) {
        const char* display_str = "block";
        switch (props->display) {
            case IR_LAYOUT_MODE_FLEX: display_str = "flex"; break;
            case IR_LAYOUT_MODE_INLINE_FLEX: display_str = "inline-flex"; break;
            case IR_LAYOUT_MODE_GRID: display_str = "grid"; break;
            case IR_LAYOUT_MODE_INLINE_GRID: display_str = "inline-grid"; break;
            case IR_LAYOUT_MODE_BLOCK: display_str = "block"; break;
            case IR_LAYOUT_MODE_INLINE: display_str = "inline"; break;
            case IR_LAYOUT_MODE_INLINE_BLOCK: display_str = "inline-block"; break;
            case IR_LAYOUT_MODE_NONE: display_str = "none"; break;
        }
        cJSON_AddStringToObject(obj, "display", display_str);
    }

    if (props->set_flags & IR_PROP_FLEX_DIRECTION) {
        cJSON_AddStringToObject(obj, "flexDirection",
            props->flex_direction ? "row" : "column");
    }

    if (props->set_flags & IR_PROP_FLEX_WRAP) {
        cJSON_AddBoolToObject(obj, "flexWrap", props->flex_wrap);
    }

    if (props->set_flags & IR_PROP_JUSTIFY_CONTENT) {
        cJSON_AddStringToObject(obj, "justifyContent", json_justify_content_to_string(props->justify_content));
    }

    if (props->set_flags & IR_PROP_ALIGN_ITEMS) {
        cJSON_AddStringToObject(obj, "alignItems", json_align_items_to_string(props->align_items));
    }

    if (props->set_flags & IR_PROP_GAP) {
        cJSON_AddNumberToObject(obj, "gap", props->gap);
    }

    // Colors
    if (props->set_flags & IR_PROP_COLOR) {
        char* colorStr = ir_json_serialize_color_to_string(props->color);
        cJSON_AddStringToObject(obj, "color", colorStr);
        free(colorStr);
    }

    if (props->set_flags & IR_PROP_BACKGROUND) {
        char* colorStr = ir_json_serialize_color_to_string(props->background);
        cJSON_AddStringToObject(obj, "background", colorStr);
        free(colorStr);
    }

    // Font properties
    if (props->set_flags & IR_PROP_FONT_SIZE) {
        // Dimension needs special handling
        char buf[64];
        if (props->font_size.type == IR_DIMENSION_PX) {
            snprintf(buf, sizeof(buf), "%.0fpx", props->font_size.value);
        } else if (props->font_size.type == IR_DIMENSION_EM) {
            snprintf(buf, sizeof(buf), "%.2fem", props->font_size.value);
        } else if (props->font_size.type == IR_DIMENSION_REM) {
            snprintf(buf, sizeof(buf), "%.2frem", props->font_size.value);
        } else {
            snprintf(buf, sizeof(buf), "%.0f", props->font_size.value);
        }
        cJSON_AddStringToObject(obj, "fontSize", buf);
    }

    if (props->set_flags & IR_PROP_FONT_WEIGHT) {
        cJSON_AddNumberToObject(obj, "fontWeight", props->font_weight);
    }

    if (props->set_flags & IR_PROP_LINE_HEIGHT) {
        cJSON_AddNumberToObject(obj, "lineHeight", props->line_height);
    }

    if (props->set_flags & IR_PROP_LETTER_SPACING) {
        cJSON_AddNumberToObject(obj, "letterSpacing", props->letter_spacing);
    }

    if (props->set_flags & IR_PROP_FONT_FAMILY) {
        cJSON_AddStringToObject(obj, "fontFamily", props->font_family);
    }

    if (props->set_flags & IR_PROP_TEXT_ALIGN) {
        const char* align_str = "left";
        switch (props->text_align) {
            case IR_TEXT_ALIGN_CENTER: align_str = "center"; break;
            case IR_TEXT_ALIGN_RIGHT: align_str = "right"; break;
            case IR_TEXT_ALIGN_JUSTIFY: align_str = "justify"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "textAlign", align_str);
    }

    if (props->set_flags & IR_PROP_OPACITY) {
        cJSON_AddNumberToObject(obj, "opacity", props->opacity);
    }

    if (props->set_flags & IR_PROP_BORDER) {
        cJSON_AddNumberToObject(obj, "borderWidth", props->border_width);
    }

    if (props->set_flags & IR_PROP_BORDER_RADIUS) {
        cJSON_AddNumberToObject(obj, "borderRadius", props->border_radius);
    }

    if (props->set_flags & IR_PROP_BORDER_COLOR) {
        char* colorStr = ir_json_serialize_color_to_string(props->border_color);
        cJSON_AddStringToObject(obj, "borderColor", colorStr);
        free(colorStr);
    }

    // Border sides
    if (props->set_flags & IR_PROP_BORDER_TOP) {
        cJSON_AddNumberToObject(obj, "borderTopWidth", props->border_width_top);
    }
    if (props->set_flags & IR_PROP_BORDER_RIGHT) {
        cJSON_AddNumberToObject(obj, "borderRightWidth", props->border_width_right);
    }
    if (props->set_flags & IR_PROP_BORDER_BOTTOM) {
        cJSON_AddNumberToObject(obj, "borderBottomWidth", props->border_width_bottom);
    }
    if (props->set_flags & IR_PROP_BORDER_LEFT) {
        cJSON_AddNumberToObject(obj, "borderLeftWidth", props->border_width_left);
    }

    // Padding
    if (props->set_flags & IR_PROP_PADDING) {
        cJSON* padding = cJSON_CreateArray();
        cJSON_AddItemToArray(padding, cJSON_CreateNumber(props->padding.top));
        cJSON_AddItemToArray(padding, cJSON_CreateNumber(props->padding.right));
        cJSON_AddItemToArray(padding, cJSON_CreateNumber(props->padding.bottom));
        cJSON_AddItemToArray(padding, cJSON_CreateNumber(props->padding.left));
        cJSON_AddItemToObject(obj, "padding", padding);
    }

    // Margin
    if (props->set_flags & IR_PROP_MARGIN) {
        cJSON* margin = cJSON_CreateArray();
        cJSON_AddItemToArray(margin, cJSON_CreateNumber(props->margin.top));
        cJSON_AddItemToArray(margin, cJSON_CreateNumber(props->margin.right));
        cJSON_AddItemToArray(margin, cJSON_CreateNumber(props->margin.bottom));
        cJSON_AddItemToArray(margin, cJSON_CreateNumber(props->margin.left));
        cJSON_AddItemToObject(obj, "margin", margin);
    }

    // Dimensions
    if (props->set_flags & IR_PROP_WIDTH) {
        char buf[64];
        if (props->width.type == IR_DIMENSION_PX) {
            snprintf(buf, sizeof(buf), "%.0fpx", props->width.value);
        } else if (props->width.type == IR_DIMENSION_PERCENT) {
            snprintf(buf, sizeof(buf), "%.0f%%", props->width.value);
        } else {
            snprintf(buf, sizeof(buf), "auto");
        }
        cJSON_AddStringToObject(obj, "width", buf);
    }

    if (props->set_flags & IR_PROP_HEIGHT) {
        char buf[64];
        if (props->height.type == IR_DIMENSION_PX) {
            snprintf(buf, sizeof(buf), "%.0fpx", props->height.value);
        } else if (props->height.type == IR_DIMENSION_PERCENT) {
            snprintf(buf, sizeof(buf), "%.0f%%", props->height.value);
        } else {
            snprintf(buf, sizeof(buf), "auto");
        }
        cJSON_AddStringToObject(obj, "height", buf);
    }

    if (props->set_flags & IR_PROP_MIN_WIDTH) {
        char buf[64];
        if (props->min_width.type == IR_DIMENSION_PX) {
            snprintf(buf, sizeof(buf), "%.0fpx", props->min_width.value);
        } else if (props->min_width.type == IR_DIMENSION_PERCENT) {
            snprintf(buf, sizeof(buf), "%.0f%%", props->min_width.value);
        } else {
            snprintf(buf, sizeof(buf), "auto");
        }
        cJSON_AddStringToObject(obj, "minWidth", buf);
    }

    if (props->set_flags & IR_PROP_MAX_WIDTH) {
        char buf[64];
        if (props->max_width.type == IR_DIMENSION_PX) {
            snprintf(buf, sizeof(buf), "%.0fpx", props->max_width.value);
        } else if (props->max_width.type == IR_DIMENSION_PERCENT) {
            snprintf(buf, sizeof(buf), "%.0f%%", props->max_width.value);
        } else {
            snprintf(buf, sizeof(buf), "auto");
        }
        cJSON_AddStringToObject(obj, "maxWidth", buf);
    }

    // Background image
    if (props->set_flags & IR_PROP_BACKGROUND_IMAGE) {
        cJSON_AddStringToObject(obj, "backgroundImage", props->background_image);
    }

    // Background clip
    if (props->set_flags & IR_PROP_BACKGROUND_CLIP) {
        const char* clip_str = "border-box";
        switch (props->background_clip) {
            case IR_BACKGROUND_CLIP_TEXT: clip_str = "text"; break;
            case IR_BACKGROUND_CLIP_CONTENT_BOX: clip_str = "content-box"; break;
            case IR_BACKGROUND_CLIP_PADDING_BOX: clip_str = "padding-box"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "backgroundClip", clip_str);
    }

    // Text fill color
    if (props->set_flags & IR_PROP_TEXT_FILL_COLOR) {
        char* colorStr = ir_json_serialize_color_to_string(props->text_fill_color);
        cJSON_AddStringToObject(obj, "textFillColor", colorStr);
        free(colorStr);
    }

    // Grid template columns (raw CSS string for roundtrip)
    if (props->set_flags & IR_PROP_GRID_TEMPLATE_COLUMNS) {
        cJSON_AddStringToObject(obj, "gridTemplateColumns", props->grid_template_columns);
    }

    // Grid template rows (raw CSS string for roundtrip)
    if (props->set_flags & IR_PROP_GRID_TEMPLATE_ROWS) {
        cJSON_AddStringToObject(obj, "gridTemplateRows", props->grid_template_rows);
    }

    // Transform
    if (props->set_flags & IR_PROP_TRANSFORM) {
        cJSON* transform = cJSON_CreateObject();
        cJSON_AddNumberToObject(transform, "translateX", props->transform.translate_x);
        cJSON_AddNumberToObject(transform, "translateY", props->transform.translate_y);
        cJSON_AddNumberToObject(transform, "scaleX", props->transform.scale_x);
        cJSON_AddNumberToObject(transform, "scaleY", props->transform.scale_y);
        cJSON_AddNumberToObject(transform, "rotate", props->transform.rotate);
        cJSON_AddItemToObject(obj, "transform", transform);
    }

    // Text decoration
    if (props->set_flags & IR_PROP_TEXT_DECORATION) {
        cJSON_AddNumberToObject(obj, "textDecoration", props->text_decoration);
    }

    // Box sizing
    if (props->set_flags & IR_PROP_BOX_SIZING) {
        cJSON_AddNumberToObject(obj, "boxSizing", props->box_sizing);
    }

    // Object fit
    if (props->set_flags & IR_PROP_OBJECT_FIT) {
        const char* fit_str = "fill";
        switch (props->object_fit) {
            case IR_OBJECT_FIT_CONTAIN: fit_str = "contain"; break;
            case IR_OBJECT_FIT_COVER: fit_str = "cover"; break;
            case IR_OBJECT_FIT_NONE: fit_str = "none"; break;
            case IR_OBJECT_FIT_SCALE_DOWN: fit_str = "scale-down"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "objectFit", fit_str);
    }

    return obj;
}

static cJSON* json_serialize_selector_chain(const IRSelectorChain* chain) {
    if (!chain) return NULL;

    cJSON* obj = cJSON_CreateObject();

    // Store original selector string for easy round-trip
    if (chain->original_selector) {
        cJSON_AddStringToObject(obj, "selector", chain->original_selector);
    }

    cJSON_AddNumberToObject(obj, "specificity", chain->specificity);

    // Serialize selector parts for detailed inspection
    cJSON* parts = cJSON_CreateArray();
    for (uint32_t i = 0; i < chain->part_count; i++) {
        cJSON* part = cJSON_CreateObject();
        cJSON_AddStringToObject(part, "type",
            ir_selector_part_type_to_string(chain->parts[i].type));
        if (chain->parts[i].name) {
            cJSON_AddStringToObject(part, "name", chain->parts[i].name);
        }

        // Add combinator (for parts after the first)
        if (i > 0) {
            cJSON_AddStringToObject(part, "combinator",
                ir_combinator_to_string(chain->combinators[i - 1]));
        }

        cJSON_AddItemToArray(parts, part);
    }
    cJSON_AddItemToObject(obj, "parts", parts);

    return obj;
}

cJSON* ir_json_serialize_stylesheet(IRStylesheet* stylesheet) {
    if (!stylesheet) return NULL;

    cJSON* obj = cJSON_CreateObject();

    // Serialize CSS variables
    if (stylesheet->variable_count > 0) {
        cJSON* variables = cJSON_CreateObject();
        for (uint32_t i = 0; i < stylesheet->variable_count; i++) {
            cJSON_AddStringToObject(variables,
                stylesheet->variables[i].name,
                stylesheet->variables[i].value);
        }
        cJSON_AddItemToObject(obj, "variables", variables);
    }

    // Serialize rules
    if (stylesheet->rule_count > 0) {
        cJSON* rules = cJSON_CreateArray();
        for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
            IRStyleRule* rule = &stylesheet->rules[i];
            cJSON* ruleObj = cJSON_CreateObject();

            // Add selector string directly for readability
            if (rule->selector && rule->selector->original_selector) {
                cJSON_AddStringToObject(ruleObj, "selector",
                    rule->selector->original_selector);
            }

            // Add parsed selector details
            cJSON* selectorChain = json_serialize_selector_chain(rule->selector);
            if (selectorChain) {
                cJSON_AddItemToObject(ruleObj, "selectorChain", selectorChain);
            }

            // Add properties
            cJSON* props = json_serialize_style_properties(&rule->properties);
            if (props) {
                cJSON_AddItemToObject(ruleObj, "properties", props);
            }

            cJSON_AddItemToArray(rules, ruleObj);
        }
        cJSON_AddItemToObject(obj, "rules", rules);
    }

    // Serialize media queries
    if (stylesheet->media_query_count > 0) {
        cJSON* media_queries = cJSON_CreateArray();
        for (uint32_t i = 0; i < stylesheet->media_query_count; i++) {
            IRMediaQuery* mq = &stylesheet->media_queries[i];
            cJSON* mqObj = cJSON_CreateObject();
            if (mq->condition) {
                cJSON_AddStringToObject(mqObj, "condition", mq->condition);
            }
            if (mq->css_content) {
                cJSON_AddStringToObject(mqObj, "cssContent", mq->css_content);
            }
            cJSON_AddItemToArray(media_queries, mqObj);
        }
        cJSON_AddItemToObject(obj, "mediaQueries", media_queries);
    }

    return obj;
}

cJSON* ir_json_serialize_reactive_manifest(IRReactiveManifest* manifest) {
    if (!manifest || (manifest->variable_count == 0 && manifest->component_def_count == 0 &&
                      manifest->conditional_count == 0 && manifest->for_loop_count == 0)) return NULL;

    cJSON* obj = cJSON_CreateObject();
    if (!obj) {
        fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for reactive manifest\n");
        return NULL;
    }

    // Serialize variables
    if (manifest->variable_count > 0) {
        cJSON* vars = cJSON_CreateArray();
        if (!vars) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for reactive variables\n");
            cJSON_Delete(obj);
            return NULL;
        }
        for (uint32_t i = 0; i < manifest->variable_count; i++) {
            IRReactiveVarDescriptor* var = &manifest->variables[i];
            cJSON* varObj = cJSON_CreateObject();
            if (!varObj) {
                fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for reactive var object\n");
                cJSON_Delete(vars);
                cJSON_Delete(obj);
                return NULL;
            }

            cJSON_AddNumberToObject(varObj, "id", var->id);
            if (var->name) cJSON_AddStringToObject(varObj, "name", var->name);

            // Type as string
            if (var->type_string) {
                cJSON_AddStringToObject(varObj, "type", var->type_string);
            } else {
                // Fallback to numeric type
                const char* typeStr = "unknown";
                switch (var->type) {
                    case IR_REACTIVE_TYPE_INT: typeStr = "int"; break;
                    case IR_REACTIVE_TYPE_FLOAT: typeStr = "float"; break;
                    case IR_REACTIVE_TYPE_STRING: typeStr = "string"; break;
                    case IR_REACTIVE_TYPE_BOOL: typeStr = "bool"; break;
                    case IR_REACTIVE_TYPE_CUSTOM: typeStr = "custom"; break;
                }
                cJSON_AddStringToObject(varObj, "type", typeStr);
            }

            // Initial value as JSON string
            if (var->initial_value_json) {
                cJSON_AddStringToObject(varObj, "initial_value", var->initial_value_json);
            }

            // Scope
            if (var->scope) {
                cJSON_AddStringToObject(varObj, "scope", var->scope);
            }

            cJSON_AddItemToArray(vars, varObj);
        }
        cJSON_AddItemToObject(obj, "variables", vars);
    }

    // Serialize bindings
    if (manifest->binding_count > 0) {
        cJSON* bindings = cJSON_CreateArray();
        if (!bindings) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for bindings\n");
            cJSON_Delete(obj);
            return NULL;
        }
        for (uint32_t i = 0; i < manifest->binding_count; i++) {
            IRReactiveBinding* binding = &manifest->bindings[i];
            cJSON* bindingObj = cJSON_CreateObject();
            if (!bindingObj) {
                fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for binding object\n");
                cJSON_Delete(bindings);
                cJSON_Delete(obj);
                return NULL;
            }

            cJSON_AddNumberToObject(bindingObj, "component_id", binding->component_id);
            cJSON_AddNumberToObject(bindingObj, "var_id", binding->reactive_var_id);

            if (binding->expression) {
                cJSON_AddStringToObject(bindingObj, "expression", binding->expression);
            }

            cJSON_AddItemToArray(bindings, bindingObj);
        }
        cJSON_AddItemToObject(obj, "bindings", bindings);
    }

    // Serialize conditionals
    if (manifest->conditional_count > 0) {
        cJSON* conditionals = cJSON_CreateArray();
        if (!conditionals) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for conditionals\n");
            cJSON_Delete(obj);
            return NULL;
        }
        for (uint32_t i = 0; i < manifest->conditional_count; i++) {
            IRReactiveConditional* cond = &manifest->conditionals[i];
            cJSON* condObj = cJSON_CreateObject();
            if (!condObj) {
                fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for conditional object\n");
                cJSON_Delete(conditionals);
                cJSON_Delete(obj);
                return NULL;
            }

            cJSON_AddNumberToObject(condObj, "component_id", cond->component_id);

            if (cond->condition && cond->condition[0] != '\0') {
                // Construct {"var": "varName"} object from variable name
                cJSON* conditionObj = cJSON_CreateObject();
                cJSON_AddStringToObject(conditionObj, "var", cond->condition);
                cJSON_AddItemToObject(condObj, "condition", conditionObj);
            }

            if (cond->dependent_var_count > 0 && cond->dependent_var_ids) {
                cJSON* deps = cJSON_CreateArray();
                for (uint32_t j = 0; j < cond->dependent_var_count; j++) {
                    cJSON_AddItemToArray(deps, cJSON_CreateNumber(cond->dependent_var_ids[j]));
                }
                cJSON_AddItemToObject(condObj, "dependent_vars", deps);
            }

            // Serialize branch children IDs for round-trip support
            if (cond->then_children_count > 0 && cond->then_children_ids) {
                cJSON* thenIds = cJSON_CreateArray();
                if (!thenIds) {
                    fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for then_children_ids\n");
                    cJSON_Delete(condObj);
                    cJSON_Delete(conditionals);
                    cJSON_Delete(obj);
                    return NULL;
                }
                for (uint32_t j = 0; j < cond->then_children_count; j++) {
                    cJSON_AddItemToArray(thenIds, cJSON_CreateNumber(cond->then_children_ids[j]));
                }
                cJSON_AddItemToObject(condObj, "then_children_ids", thenIds);
            }

            if (cond->else_children_count > 0 && cond->else_children_ids) {
                cJSON* elseIds = cJSON_CreateArray();
                if (!elseIds) {
                    fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for else_children_ids\n");
                    cJSON_Delete(condObj);
                    cJSON_Delete(conditionals);
                    cJSON_Delete(obj);
                    return NULL;
                }
                for (uint32_t j = 0; j < cond->else_children_count; j++) {
                    cJSON_AddItemToArray(elseIds, cJSON_CreateNumber(cond->else_children_ids[j]));
                }
                cJSON_AddItemToObject(condObj, "else_children_ids", elseIds);
            }

            cJSON_AddItemToArray(conditionals, condObj);
        }
        cJSON_AddItemToObject(obj, "conditionals", conditionals);
    }

    // Serialize for-loops
    if (manifest->for_loop_count > 0) {
        cJSON* loops = cJSON_CreateArray();
        if (!loops) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for for-loops\n");
            cJSON_Delete(obj);
            return NULL;
        }
        for (uint32_t i = 0; i < manifest->for_loop_count; i++) {
            IRReactiveForLoop* loop = &manifest->for_loops[i];
            cJSON* loopObj = cJSON_CreateObject();
            if (!loopObj) {
                fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for for-loop object\n");
                cJSON_Delete(loops);
                cJSON_Delete(obj);
                return NULL;
            }

            cJSON_AddNumberToObject(loopObj, "parent_component_id", loop->parent_component_id);
            cJSON_AddNumberToObject(loopObj, "collection_var_id", loop->collection_var_id);

            if (loop->collection_expr) {
                cJSON_AddStringToObject(loopObj, "collection_expr", loop->collection_expr);
            }

            cJSON_AddItemToArray(loops, loopObj);
        }
        cJSON_AddItemToObject(obj, "for_loops", loops);
    }

    // Serialize component definitions
    if (manifest->component_def_count > 0) {
        cJSON* component_defs = ir_json_serialize_component_definitions(manifest);
        if (component_defs) {
            cJSON_AddItemToObject(obj, "component_definitions", component_defs);
        }
    }

    return obj;
}

// ============================================================================
// Main JSON Serialization Function
// ============================================================================

/**
 * Serialize source metadata to JSON
 */
cJSON* ir_json_serialize_metadata(IRSourceMetadata* metadata) {
    if (!metadata) return NULL;

    cJSON* meta = cJSON_CreateObject();
    if (!meta) return NULL;

    if (metadata->source_language) {
        cJSON_AddStringToObject(meta, "source_language", metadata->source_language);
    }
    if (metadata->compiler_version) {
        cJSON_AddStringToObject(meta, "compiler_version", metadata->compiler_version);
    }
    if (metadata->timestamp) {
        cJSON_AddStringToObject(meta, "timestamp", metadata->timestamp);
    }
    if (metadata->source_file) {
        cJSON_AddStringToObject(meta, "source_file", metadata->source_file);
    }

    return meta;
}

/**
 * Serialize IRLogicBlock (from ir_logic.h) to JSON
 * This includes multi-language function definitions
 */
cJSON* ir_json_serialize_logic_block(IRLogicBlock* logic_block) {
    if (!logic_block) return NULL;

    cJSON* blockObj = cJSON_CreateObject();
    if (!blockObj) return NULL;

    // Serialize functions
    if (logic_block->function_count > 0 && logic_block->functions) {
        cJSON* functionsArray = cJSON_CreateArray();

        for (int i = 0; i < logic_block->function_count; i++) {
            IRLogicFunction* func = logic_block->functions[i];
            if (!func) continue;

            cJSON* funcObj = cJSON_CreateObject();

            if (func->name) {
                cJSON_AddStringToObject(funcObj, "name", func->name);
            }

            // Serialize universal logic statements
            if (func->has_universal && func->statement_count > 0 && func->statements) {
                cJSON* statementsArray = cJSON_CreateArray();

                for (int j = 0; j < func->statement_count; j++) {
                    if (func->statements[j]) {
                        cJSON* stmtJson = ir_stmt_to_json(func->statements[j]);
                        if (stmtJson) {
                            cJSON_AddItemToArray(statementsArray, stmtJson);
                        }
                    }
                }

                if (cJSON_GetArraySize(statementsArray) > 0) {
                    cJSON* universalObj = cJSON_CreateObject();
                    cJSON_AddItemToObject(universalObj, "statements", statementsArray);
                    cJSON_AddItemToObject(funcObj, "universal", universalObj);
                } else {
                    cJSON_Delete(statementsArray);
                }
            }

            // Serialize multi-language sources
            if (func->source_count > 0) {
                cJSON* sourcesArray = cJSON_CreateArray();

                for (int j = 0; j < func->source_count; j++) {
                    cJSON* sourceObj = cJSON_CreateObject();
                    cJSON_AddStringToObject(sourceObj, "language", func->sources[j].language);
                    cJSON_AddStringToObject(sourceObj, "source", func->sources[j].source);
                    cJSON_AddItemToArray(sourcesArray, sourceObj);
                }

                cJSON_AddItemToObject(funcObj, "sources", sourcesArray);
            }

            cJSON_AddItemToArray(functionsArray, funcObj);
        }

        cJSON_AddItemToObject(blockObj, "functions", functionsArray);
    }

    // Serialize event bindings
    if (logic_block->event_binding_count > 0 && logic_block->event_bindings) {
        cJSON* bindingsArray = cJSON_CreateArray();

        for (int i = 0; i < logic_block->event_binding_count; i++) {
            IREventBinding* binding = logic_block->event_bindings[i];
            if (!binding) continue;

            cJSON* bindingObj = cJSON_CreateObject();
            cJSON_AddNumberToObject(bindingObj, "component_id", binding->component_id);
            if (binding->event_type) {
                cJSON_AddStringToObject(bindingObj, "event_type", binding->event_type);
            }
            if (binding->handler_name) {
                cJSON_AddStringToObject(bindingObj, "handler_name", binding->handler_name);
            }
            cJSON_AddItemToArray(bindingsArray, bindingObj);
        }

        cJSON_AddItemToObject(blockObj, "event_bindings", bindingsArray);
    }

    return blockObj;
}

// ============================================================================
// Source Structures Serialization (for Kry -> KIR -> Kry round-trip)
// ============================================================================

static cJSON* json_serialize_var_decl(IRVarDecl* var) {
    if (!var) return NULL;

    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", var->id);
    cJSON_AddStringToObject(obj, "name", var->name);
    cJSON_AddStringToObject(obj, "var_type", var->var_type);

    if (var->value_type) {
        cJSON_AddStringToObject(obj, "value_type", var->value_type);
    }
    if (var->value_json) {
        cJSON_AddStringToObject(obj, "value_json", var->value_json);
    }
    if (var->scope) {
        cJSON_AddStringToObject(obj, "scope", var->scope);
    }

    return obj;
}

static cJSON* json_serialize_for_loop(IRForLoopData* loop) {
    if (!loop) return NULL;

    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", loop->id);

    if (loop->parent_id) {
        cJSON_AddStringToObject(obj, "parent_id", loop->parent_id);
    }
    cJSON_AddStringToObject(obj, "iterator_name", loop->iterator_name);
    cJSON_AddStringToObject(obj, "collection_ref", loop->collection_ref);

    if (loop->collection_expr) {
        cJSON_AddStringToObject(obj, "collection_expr", loop->collection_expr);
    }

    // Serialize template component if present
    if (loop->template_component) {
        cJSON* template_json = ir_json_serialize_component_recursive(loop->template_component);
        if (template_json) {
            cJSON_AddItemToObject(obj, "template_component", template_json);
        }
    }

    // Serialize expanded component IDs
    if (loop->expanded_count > 0 && loop->expanded_component_ids) {
        cJSON* ids_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < loop->expanded_count; i++) {
            cJSON_AddItemToArray(ids_array, cJSON_CreateNumber(loop->expanded_component_ids[i]));
        }
        cJSON_AddItemToObject(obj, "expanded_component_ids", ids_array);
    }

    return obj;
}

static cJSON* json_serialize_static_block(IRStaticBlockData* block) {
    if (!block) return NULL;

    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", block->id);
    cJSON_AddNumberToObject(obj, "parent_component_id", block->parent_component_id);

    // Serialize children IDs
    if (block->children_count > 0 && block->children_ids) {
        cJSON* ids_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < block->children_count; i++) {
            cJSON_AddItemToArray(ids_array, cJSON_CreateNumber(block->children_ids[i]));
        }
        cJSON_AddItemToObject(obj, "children_ids", ids_array);
    }

    // Serialize var declaration IDs
    if (block->var_decl_count > 0 && block->var_declaration_ids) {
        cJSON* var_ids = cJSON_CreateArray();
        for (uint32_t i = 0; i < block->var_decl_count; i++) {
            cJSON_AddItemToArray(var_ids, cJSON_CreateString(block->var_declaration_ids[i]));
        }
        cJSON_AddItemToObject(obj, "var_declaration_ids", var_ids);
    }

    // Serialize for loop IDs
    if (block->for_loop_count > 0 && block->for_loop_ids) {
        cJSON* loop_ids = cJSON_CreateArray();
        for (uint32_t i = 0; i < block->for_loop_count; i++) {
            cJSON_AddItemToArray(loop_ids, cJSON_CreateString(block->for_loop_ids[i]));
        }
        cJSON_AddItemToObject(obj, "for_loop_ids", loop_ids);
    }

    return obj;
}

cJSON* ir_json_serialize_source_structures(IRSourceStructures* ss) {
    if (!ss) return NULL;

    cJSON* obj = cJSON_CreateObject();

    // Serialize static blocks
    if (ss->static_block_count > 0) {
        cJSON* blocks_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < ss->static_block_count; i++) {
            cJSON* block_json = json_serialize_static_block(ss->static_blocks[i]);
            if (block_json) {
                cJSON_AddItemToArray(blocks_array, block_json);
            }
        }
        cJSON_AddItemToObject(obj, "static_blocks", blocks_array);
    }

    // Serialize variable declarations
    if (ss->var_decl_count > 0) {
        cJSON* vars_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < ss->var_decl_count; i++) {
            cJSON* var_json = json_serialize_var_decl(ss->var_decls[i]);
            if (var_json) {
                cJSON_AddItemToArray(vars_array, var_json);
            }
        }
        cJSON_AddItemToObject(obj, "const_declarations", vars_array);
    }

    // Serialize for loops (compile-time only)
    if (ss->for_loop_count > 0) {
        cJSON* loops_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < ss->for_loop_count; i++) {
            cJSON* loop_json = json_serialize_for_loop(ss->for_loops[i]);
            if (loop_json) {
                cJSON_AddItemToArray(loops_array, loop_json);
            }
        }
        cJSON_AddItemToObject(obj, "for_loop_templates", loops_array);
    }

    // Serialize import statements
    if (ss->import_count > 0) {
        cJSON* imports_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < ss->import_count; i++) {
            IRImport* import = ss->imports[i];
            if (import) {
                cJSON* import_json = cJSON_CreateObject();
                cJSON_AddStringToObject(import_json, "variable", import->variable);
                cJSON_AddStringToObject(import_json, "module", import->module);
                cJSON_AddItemToArray(imports_array, import_json);
            }
        }
        cJSON_AddItemToObject(obj, "requires", imports_array);
    }

    // Serialize module exports
    if (ss->export_count > 0) {
        cJSON* exports_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < ss->export_count; i++) {
            IRModuleExport* export = ss->exports[i];
            if (export) {
                cJSON* export_json = cJSON_CreateObject();
                cJSON_AddStringToObject(export_json, "name", export->name);
                cJSON_AddStringToObject(export_json, "type", export->type);
                if (export->value_json) {
                    cJSON_AddStringToObject(export_json, "value", export->value_json);
                }
                if (export->function_name) {
                    cJSON_AddStringToObject(export_json, "function", export->function_name);
                }
                // Serialize struct exports
                if (export->struct_def) {
                    cJSON* struct_json = cJSON_CreateObject();
                    cJSON_AddStringToObject(struct_json, "name", export->struct_def->name);

                    cJSON* fields_array = cJSON_CreateArray();
                    for (uint32_t j = 0; j < export->struct_def->field_count; j++) {
                        IRStructField* field = export->struct_def->fields[j];
                        if (field) {
                            cJSON* field_json = cJSON_CreateObject();
                            cJSON_AddStringToObject(field_json, "name", field->name);
                            cJSON_AddStringToObject(field_json, "type", field->type);
                            if (field->default_value && strlen(field->default_value) > 0) {
                                cJSON_AddStringToObject(field_json, "default", field->default_value);
                            }
                            cJSON_AddNumberToObject(field_json, "line", field->line);
                            cJSON_AddItemToArray(fields_array, field_json);
                        }
                    }
                    cJSON_AddItemToObject(struct_json, "fields", fields_array);

                    cJSON_AddItemToObject(export_json, "struct", struct_json);
                }
                cJSON_AddNumberToObject(export_json, "line", export->line);
                cJSON_AddItemToArray(exports_array, export_json);
            }
        }
        cJSON_AddItemToObject(obj, "exports", exports_array);
    }

    // Serialize struct types
    if (ss->struct_type_count > 0) {
        cJSON* structs_array = cJSON_CreateArray();
        for (uint32_t i = 0; i < ss->struct_type_count; i++) {
            IRStructType* struct_type = ss->struct_types[i];
            if (!struct_type) continue;

            cJSON* struct_json = cJSON_CreateObject();
            cJSON_AddStringToObject(struct_json, "name", struct_type->name);
            cJSON_AddNumberToObject(struct_json, "line", struct_type->line);

            // Serialize fields
            cJSON* fields_array = cJSON_CreateArray();
            for (uint32_t j = 0; j < struct_type->field_count; j++) {
                IRStructField* field = struct_type->fields[j];
                if (!field) continue;

                cJSON* field_json = cJSON_CreateObject();
                cJSON_AddStringToObject(field_json, "name", field->name);
                cJSON_AddStringToObject(field_json, "type", field->type);
                if (field->default_value && strlen(field->default_value) > 0) {
                    cJSON_AddStringToObject(field_json, "default", field->default_value);
                }
                cJSON_AddNumberToObject(field_json, "line", field->line);

                cJSON_AddItemToArray(fields_array, field_json);
            }
            cJSON_AddItemToObject(struct_json, "fields", fields_array);

            cJSON_AddItemToArray(structs_array, struct_json);
        }
        cJSON_AddItemToObject(obj, "struct_types", structs_array);
    }

    return obj;
}

/**
 * Serialize IR component tree with complete metadata, logic, and reactive manifest
 * @param root Root component to serialize
 * @param manifest Reactive manifest to include (can be NULL)
 * @param logic_block Logic block with functions and event bindings (can be NULL)
 * @param source_metadata Source file metadata (can be NULL)
 * @param source_structures Source preservation structures (can be NULL)
 * @return cJSON object (caller must delete), or NULL on error
 */
cJSON* ir_json_serialize_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    IRLogicBlock* logic_block,
    IRSourceMetadata* source_metadata,
    IRSourceStructures* source_structures
) {
    if (!root) return NULL;

    // Create wrapper object
    cJSON* wrapper = cJSON_CreateObject();

    // Add format identifier
    cJSON_AddStringToObject(wrapper, "format", "kir");

    // Add source metadata
    if (source_metadata) {
        cJSON* metaJson = ir_json_serialize_metadata(source_metadata);
        if (metaJson) {
            cJSON_AddItemToObject(wrapper, "metadata", metaJson);
        }
    }

    // Add window properties from global IR context
    IRContext* ctx = g_ir_context;
    if (ctx && ctx->metadata) {
        cJSON* appJson = cJSON_CreateObject();
        if (ctx->metadata->window_title) {
            cJSON_AddStringToObject(appJson, "windowTitle", ctx->metadata->window_title);
        }
        if (ctx->metadata->window_width > 0) {
            cJSON_AddNumberToObject(appJson, "windowWidth", ctx->metadata->window_width);
        }
        if (ctx->metadata->window_height > 0) {
            cJSON_AddNumberToObject(appJson, "windowHeight", ctx->metadata->window_height);
        }
        // Only add app object if it has any properties
        int app_size = cJSON_GetArraySize(appJson);
        if (app_size > 0) {
            cJSON_AddItemToObject(wrapper, "app", appJson);
        } else {
            cJSON_Delete(appJson);
        }
    }

    // Add component definitions FIRST (at the top)
    if (manifest && manifest->component_def_count > 0) {
        cJSON* componentDefsJson = ir_json_serialize_component_definitions(manifest);
        if (componentDefsJson) {
            cJSON_AddItemToObject(wrapper, "component_definitions", componentDefsJson);
        }
    }

    // Add reactive manifest if present (variables, bindings, conditionals, for-loops)
    if (manifest && (manifest->variable_count > 0 || manifest->binding_count > 0 ||
                     manifest->conditional_count > 0 || manifest->for_loop_count > 0)) {
        cJSON* manifestJson = ir_json_serialize_reactive_manifest(manifest);
        if (manifestJson) {
            cJSON_AddItemToObject(wrapper, "reactive_manifest", manifestJson);
        }
    }

    // Add global stylesheet if present (for CSS descendant selectors round-trip)
    if (ctx && ctx->stylesheet && (ctx->stylesheet->rule_count > 0 ||
                                    ctx->stylesheet->variable_count > 0 ||
                                    ctx->stylesheet->media_query_count > 0)) {
        cJSON* stylesheetJson = ir_json_serialize_stylesheet(ctx->stylesheet);
        if (stylesheetJson) {
            cJSON_AddItemToObject(wrapper, "stylesheet", stylesheetJson);
        }
    }

    // Add source structures if present (for Kry->KIR->Kry round-trip)
    if (source_structures && (source_structures->static_block_count > 0 ||
                              source_structures->var_decl_count > 0 ||
                              source_structures->for_loop_count > 0)) {
        cJSON* sourceStructsJson = ir_json_serialize_source_structures(source_structures);
        if (sourceStructsJson) {
            cJSON_AddItemToObject(wrapper, "source_structures", sourceStructsJson);
        }
    }

    // Add logic block (new!)
    if (logic_block) {
        cJSON* logicJson = ir_json_serialize_logic_block(logic_block);
        if (logicJson) {
            cJSON_AddItemToObject(wrapper, "logic_block", logicJson);
        }
    }

    // Add Lua modules (for Lua->KIR->Web round-trip)
    if (ctx && ctx->lua_modules && ctx->lua_modules->module_count > 0) {
        cJSON* luaModulesJson = ir_json_serialize_lua_modules(ctx->lua_modules);
        if (luaModulesJson) {
            cJSON_AddItemToObject(wrapper, "lua_modules", luaModulesJson);
        }
    }

    // Add component tree
    cJSON* componentJson = ir_json_serialize_component_recursive(root);
    if (!componentJson) {
        cJSON_Delete(wrapper);
        return NULL;
    }
    cJSON_AddItemToObject(wrapper, "root", componentJson);

    // Plugin requirements are now handled by the capability system
    // and do not need to be serialized in the KIR file

    // Add source code entries from manifest
    if (manifest && manifest->source_count > 0) {
        cJSON* sourcesArray = cJSON_CreateArray();
        for (uint32_t i = 0; i < manifest->source_count; i++) {
            cJSON* sourceObj = cJSON_CreateObject();
            cJSON_AddStringToObject(sourceObj, "lang", manifest->sources[i].lang);
            cJSON_AddStringToObject(sourceObj, "code", manifest->sources[i].code);
            cJSON_AddItemToArray(sourcesArray, sourceObj);
        }
        cJSON_AddItemToObject(wrapper, "sources", sourcesArray);
    }

    return wrapper;
}

// ============================================================================
// C Metadata Serialization (for C->KIR->C round-trip)
// ============================================================================

// External global C metadata instance (defined in bindings/c/kryon.c or weak symbol in ir_json.c)
extern CSourceMetadata g_c_metadata;

/**
 * Serialize C source metadata to JSON object
 * Used for C->KIR->C round-trip to preserve event handlers and other C code
 */
cJSON* ir_json_serialize_c_metadata(void) {
    // Check if there's any C metadata to serialize
    if (g_c_metadata.event_handler_count == 0 &&
        g_c_metadata.variable_count == 0 &&
        g_c_metadata.helper_function_count == 0 &&
        g_c_metadata.include_count == 0 &&
        g_c_metadata.preprocessor_directive_count == 0) {
        return NULL;  // No C metadata to serialize
    }

    cJSON* c_meta = cJSON_CreateObject();
    if (!c_meta) return NULL;

    // Serialize event_handlers array
    if (g_c_metadata.event_handler_count > 0 && g_c_metadata.event_handlers) {
        cJSON* handlers_array = cJSON_CreateArray();

        for (size_t i = 0; i < g_c_metadata.event_handler_count; i++) {
            CEventHandlerDecl* handler = &g_c_metadata.event_handlers[i];
            cJSON* handler_obj = cJSON_CreateObject();

            if (handler->logic_id) {
                cJSON_AddStringToObject(handler_obj, "logic_id", handler->logic_id);
            }
            if (handler->function_name) {
                cJSON_AddStringToObject(handler_obj, "function_name", handler->function_name);
            }
            if (handler->return_type) {
                cJSON_AddStringToObject(handler_obj, "return_type", handler->return_type);
            }
            if (handler->parameters) {
                cJSON_AddStringToObject(handler_obj, "parameters", handler->parameters);
            }
            if (handler->body) {
                cJSON_AddStringToObject(handler_obj, "body", handler->body);
            }
            cJSON_AddNumberToObject(handler_obj, "line_number", handler->line_number);

            cJSON_AddItemToArray(handlers_array, handler_obj);
        }

        cJSON_AddItemToObject(c_meta, "event_handlers", handlers_array);
    }

    // Serialize variables array
    if (g_c_metadata.variable_count > 0 && g_c_metadata.variables) {
        cJSON* variables_array = cJSON_CreateArray();

        for (size_t i = 0; i < g_c_metadata.variable_count; i++) {
            CVariableDecl* var = &g_c_metadata.variables[i];
            cJSON* var_obj = cJSON_CreateObject();

            if (var->name) {
                cJSON_AddStringToObject(var_obj, "name", var->name);
            }
            if (var->type) {
                cJSON_AddStringToObject(var_obj, "type", var->type);
            }
            if (var->storage) {
                cJSON_AddStringToObject(var_obj, "storage", var->storage);
            }
            if (var->initial_value) {
                cJSON_AddStringToObject(var_obj, "initial_value", var->initial_value);
            }
            cJSON_AddNumberToObject(var_obj, "component_id", var->component_id);
            cJSON_AddNumberToObject(var_obj, "line_number", var->line_number);

            cJSON_AddItemToArray(variables_array, var_obj);
        }

        cJSON_AddItemToObject(c_meta, "variables", variables_array);
    }

    // Serialize helper_functions array
    if (g_c_metadata.helper_function_count > 0 && g_c_metadata.helper_functions) {
        cJSON* helpers_array = cJSON_CreateArray();

        for (size_t i = 0; i < g_c_metadata.helper_function_count; i++) {
            CHelperFunction* helper = &g_c_metadata.helper_functions[i];
            cJSON* helper_obj = cJSON_CreateObject();

            if (helper->name) {
                cJSON_AddStringToObject(helper_obj, "name", helper->name);
            }
            if (helper->return_type) {
                cJSON_AddStringToObject(helper_obj, "return_type", helper->return_type);
            }
            if (helper->parameters) {
                cJSON_AddStringToObject(helper_obj, "parameters", helper->parameters);
            }
            if (helper->body) {
                cJSON_AddStringToObject(helper_obj, "body", helper->body);
            }
            cJSON_AddNumberToObject(helper_obj, "line_number", helper->line_number);

            cJSON_AddItemToArray(helpers_array, helper_obj);
        }

        cJSON_AddItemToObject(c_meta, "helper_functions", helpers_array);
    }

    // Serialize includes array
    if (g_c_metadata.include_count > 0 && g_c_metadata.includes) {
        cJSON* includes_array = cJSON_CreateArray();

        for (size_t i = 0; i < g_c_metadata.include_count; i++) {
            CInclude* inc = &g_c_metadata.includes[i];
            cJSON* inc_obj = cJSON_CreateObject();

            if (inc->include_string) {
                cJSON_AddStringToObject(inc_obj, "include_string", inc->include_string);
            }
            cJSON_AddBoolToObject(inc_obj, "is_system", inc->is_system);
            cJSON_AddNumberToObject(inc_obj, "line_number", inc->line_number);

            cJSON_AddItemToArray(includes_array, inc_obj);
        }

        cJSON_AddItemToObject(c_meta, "includes", includes_array);
    }

    // Serialize preprocessor_directives array
    if (g_c_metadata.preprocessor_directive_count > 0 && g_c_metadata.preprocessor_directives) {
        cJSON* directives_array = cJSON_CreateArray();

        for (size_t i = 0; i < g_c_metadata.preprocessor_directive_count; i++) {
            CPreprocessorDirective* dir = &g_c_metadata.preprocessor_directives[i];
            cJSON* dir_obj = cJSON_CreateObject();

            if (dir->directive_type) {
                cJSON_AddStringToObject(dir_obj, "directive_type", dir->directive_type);
            }
            if (dir->condition) {
                cJSON_AddStringToObject(dir_obj, "condition", dir->condition);
            }
            if (dir->value) {
                cJSON_AddStringToObject(dir_obj, "value", dir->value);
            }
            cJSON_AddNumberToObject(dir_obj, "line_number", dir->line_number);

            cJSON_AddItemToArray(directives_array, dir_obj);
        }

        cJSON_AddItemToObject(c_meta, "preprocessor_directives", directives_array);
    }

    return c_meta;
}

/**
 * Serialize component tree to JSON string
 * Returns: Newly allocated string that must be freed by caller, or NULL on error
 */
char* ir_json_serialize_component_tree(IRComponent* root) {
    if (!root) return NULL;

    // Create the complete JSON object
    cJSON* wrapper = ir_json_serialize_complete(root, NULL, NULL, NULL, NULL);
    if (!wrapper) return NULL;

    // Convert to string
    char* jsonString = cJSON_Print(wrapper);
    cJSON_Delete(wrapper);

    return jsonString;
}

// ============================================================================
// Lua Module Serialization (for Lua->KIR->Web round-trip)
// ============================================================================

/**
 * Serialize Lua module collection to JSON object
 * This captures the complete Lua source context for web codegen
 */
cJSON* ir_json_serialize_lua_modules(IRLuaModuleCollection* lua_modules) {
    if (!lua_modules || lua_modules->module_count == 0) return NULL;

    cJSON* modulesArray = cJSON_CreateArray();
    if (!modulesArray) return NULL;

    for (uint32_t i = 0; i < lua_modules->module_count; i++) {
        IRLuaModule* mod = &lua_modules->modules[i];
        cJSON* modObj = cJSON_CreateObject();
        if (!modObj) continue;

        // Basic module info
        if (mod->name) {
            cJSON_AddStringToObject(modObj, "name", mod->name);
        }
        if (mod->source_language) {
            cJSON_AddStringToObject(modObj, "language", mod->source_language);
        }
        if (mod->full_source) {
            cJSON_AddStringToObject(modObj, "full_source", mod->full_source);
        }

        // Serialize variables
        if (mod->variable_count > 0) {
            cJSON* varsArray = cJSON_CreateArray();
            for (uint32_t j = 0; j < mod->variable_count; j++) {
                IRLuaVariable* var = &mod->variables[j];
                cJSON* varObj = cJSON_CreateObject();
                if (var->name) {
                    cJSON_AddStringToObject(varObj, "name", var->name);
                }
                if (var->initial_value_source) {
                    cJSON_AddStringToObject(varObj, "initial_value", var->initial_value_source);
                }
                if (var->var_type) {
                    cJSON_AddStringToObject(varObj, "type", var->var_type);
                }
                cJSON_AddBoolToObject(varObj, "is_reactive", var->is_reactive);
                cJSON_AddItemToArray(varsArray, varObj);
            }
            cJSON_AddItemToObject(modObj, "variables", varsArray);
        }

        // Serialize functions
        if (mod->function_count > 0) {
            cJSON* funcsArray = cJSON_CreateArray();
            for (uint32_t j = 0; j < mod->function_count; j++) {
                IRLuaFunc* func = &mod->functions[j];
                cJSON* funcObj = cJSON_CreateObject();
                if (func->name) {
                    cJSON_AddStringToObject(funcObj, "name", func->name);
                }
                if (func->source) {
                    cJSON_AddStringToObject(funcObj, "source", func->source);
                }
                cJSON_AddBoolToObject(funcObj, "is_local", func->is_local);

                // Serialize parameters
                if (func->parameter_count > 0 && func->parameters) {
                    cJSON* paramsArray = cJSON_CreateArray();
                    for (uint32_t k = 0; k < func->parameter_count; k++) {
                        cJSON_AddItemToArray(paramsArray, cJSON_CreateString(func->parameters[k]));
                    }
                    cJSON_AddItemToObject(funcObj, "parameters", paramsArray);
                }

                // Serialize closure vars
                if (func->closure_var_count > 0 && func->closure_vars) {
                    cJSON* closureArray = cJSON_CreateArray();
                    for (uint32_t k = 0; k < func->closure_var_count; k++) {
                        cJSON_AddItemToArray(closureArray, cJSON_CreateString(func->closure_vars[k]));
                    }
                    cJSON_AddItemToObject(funcObj, "closure_vars", closureArray);
                }

                cJSON_AddItemToArray(funcsArray, funcObj);
            }
            cJSON_AddItemToObject(modObj, "functions", funcsArray);
        }

        // Serialize imports
        if (mod->import_count > 0) {
            cJSON* importsArray = cJSON_CreateArray();
            for (uint32_t j = 0; j < mod->import_count; j++) {
                cJSON_AddItemToArray(importsArray, cJSON_CreateString(mod->imports[j]));
            }
            cJSON_AddItemToObject(modObj, "imports", importsArray);
        }

        // Serialize ForEach providers
        if (mod->foreach_provider_count > 0) {
            cJSON* providersArray = cJSON_CreateArray();
            for (uint32_t j = 0; j < mod->foreach_provider_count; j++) {
                IRLuaForEachProvider* provider = &mod->foreach_providers[j];
                cJSON* providerObj = cJSON_CreateObject();
                if (provider->container_id) {
                    cJSON_AddStringToObject(providerObj, "container_id", provider->container_id);
                }
                if (provider->expression) {
                    cJSON_AddStringToObject(providerObj, "expression", provider->expression);
                }
                if (provider->source_function) {
                    cJSON_AddStringToObject(providerObj, "source_function", provider->source_function);
                }
                cJSON_AddItemToArray(providersArray, providerObj);
            }
            cJSON_AddItemToObject(modObj, "foreach_providers", providersArray);
        }

        cJSON_AddItemToArray(modulesArray, modObj);
    }

    return modulesArray;
}
