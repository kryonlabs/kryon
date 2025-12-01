// JSON v2 Serialization - Complete IR Property Coverage
// This file implements full JSON serialization using cJSON

#define _GNU_SOURCE
#include "ir_serialization.h"
#include "ir_builder.h"
#include "third_party/cJSON/cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// JSON Helper Functions
// ============================================================================

/**
 * Serialize dimension to JSON string (e.g., "100px", "50%", "auto")
 */
static char* json_dimension_to_string(IRDimension dim) {
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
 * Serialize color to JSON string (e.g., "#rrggbb" or "#rrggbbaa")
 */
static char* json_color_to_string(IRColor color) {
    char buffer[16];

    if (color.type == IR_COLOR_SOLID) {
        if (color.data.a == 255) {
            // Fully opaque - use short form
            snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
                    color.data.r, color.data.g, color.data.b);
        } else {
            // Include alpha channel
            snprintf(buffer, sizeof(buffer), "#%02x%02x%02x%02x",
                    color.data.r, color.data.g, color.data.b, color.data.a);
        }
        return strdup(buffer);
    } else if (color.type == IR_COLOR_TRANSPARENT) {
        return strdup("transparent");
    } else if (color.type == IR_COLOR_VAR_REF) {
        snprintf(buffer, sizeof(buffer), "var(%u)", color.data.var_id);
        return strdup(buffer);
    }

    return strdup("#000000");
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
        cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.top));
        cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.left));
        return array;
    }

    // Full array [top, right, bottom, left]
    cJSON* array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.top));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.right));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.bottom));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(spacing.left));
    return array;
}

// ============================================================================
// Style Serialization
// ============================================================================

static void json_serialize_style(cJSON* obj, IRStyle* style) {
    if (!style) return;

    // Only serialize non-default values for cleaner output

    // Dimensions
    if (style->width.type != IR_DIMENSION_AUTO) {
        char* widthStr = json_dimension_to_string(style->width);
        cJSON_AddStringToObject(obj, "width", widthStr);
        free(widthStr);
    }

    if (style->height.type != IR_DIMENSION_AUTO) {
        char* heightStr = json_dimension_to_string(style->height);
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

    // Background color
    if (style->background.type != IR_COLOR_TRANSPARENT) {
        char* colorStr = json_color_to_string(style->background);
        cJSON_AddStringToObject(obj, "background", colorStr);
        free(colorStr);
    }

    // Border
    if (style->border.width > 0) {
        cJSON* border = cJSON_CreateObject();
        cJSON_AddNumberToObject(border, "width", style->border.width);

        char* colorStr = json_color_to_string(style->border.color);
        cJSON_AddStringToObject(border, "color", colorStr);
        free(colorStr);

        if (style->border.radius > 0) {
            cJSON_AddNumberToObject(border, "radius", style->border.radius);
        }

        cJSON_AddItemToObject(obj, "border", border);
    }

    // Typography
    if (style->font.size > 0) {
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

    if (style->font.color.type != IR_COLOR_TRANSPARENT) {
        char* textColorStr = json_color_to_string(style->font.color);
        cJSON_AddStringToObject(obj, "color", textColorStr);
        free(textColorStr);
    }

    // Text alignment
    if (style->font.align != IR_TEXT_ALIGN_LEFT) {
        const char* alignStr = "left";
        switch (style->font.align) {
            case IR_TEXT_ALIGN_CENTER: alignStr = "center"; break;
            case IR_TEXT_ALIGN_RIGHT: alignStr = "right"; break;
            case IR_TEXT_ALIGN_JUSTIFY: alignStr = "justify"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "textAlign", alignStr);
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

        if (fabs(style->transform.translate_x) > 0.001f || fabs(style->transform.translate_y) > 0.001f) {
            cJSON* translate = cJSON_CreateArray();
            cJSON_AddItemToArray(translate, cJSON_CreateNumber(style->transform.translate_x));
            cJSON_AddItemToArray(translate, cJSON_CreateNumber(style->transform.translate_y));
            cJSON_AddItemToObject(transform, "translate", translate);
        }

        if (fabs(style->transform.scale_x - 1.0f) > 0.001f || fabs(style->transform.scale_y - 1.0f) > 0.001f) {
            cJSON* scale = cJSON_CreateArray();
            cJSON_AddItemToArray(scale, cJSON_CreateNumber(style->transform.scale_x));
            cJSON_AddItemToArray(scale, cJSON_CreateNumber(style->transform.scale_y));
            cJSON_AddItemToObject(transform, "scale", scale);
        }

        if (fabs(style->transform.rotate) > 0.001f) {
            cJSON_AddNumberToObject(transform, "rotate", style->transform.rotate);
        }

        cJSON_AddItemToObject(obj, "transform", transform);
    }
}

// ============================================================================
// Layout Serialization
// ============================================================================

static void json_serialize_layout(cJSON* obj, IRLayout* layout) {
    if (!layout) return;

    // Min/Max dimensions
    if (layout->min_width.type != IR_DIMENSION_AUTO) {
        char* minWidthStr = json_dimension_to_string(layout->min_width);
        cJSON_AddStringToObject(obj, "minWidth", minWidthStr);
        free(minWidthStr);
    }

    if (layout->min_height.type != IR_DIMENSION_AUTO) {
        char* minHeightStr = json_dimension_to_string(layout->min_height);
        cJSON_AddStringToObject(obj, "minHeight", minHeightStr);
        free(minHeightStr);
    }

    if (layout->max_width.type != IR_DIMENSION_AUTO) {
        char* maxWidthStr = json_dimension_to_string(layout->max_width);
        cJSON_AddStringToObject(obj, "maxWidth", maxWidthStr);
        free(maxWidthStr);
    }

    if (layout->max_height.type != IR_DIMENSION_AUTO) {
        char* maxHeightStr = json_dimension_to_string(layout->max_height);
        cJSON_AddStringToObject(obj, "maxHeight", maxHeightStr);
        free(maxHeightStr);
    }

    // Flexbox properties
    if (layout->flex.direction != 0) {  // 0 = column
        cJSON_AddStringToObject(obj, "flexDirection", layout->flex.direction == 1 ? "row" : "column");
    }

    if (layout->flex.justify_content != IR_ALIGNMENT_START) {
        const char* justifyStr = "flex-start";
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_CENTER: justifyStr = "center"; break;
            case IR_ALIGNMENT_END: justifyStr = "flex-end"; break;
            case IR_ALIGNMENT_SPACE_BETWEEN: justifyStr = "space-between"; break;
            case IR_ALIGNMENT_SPACE_AROUND: justifyStr = "space-around"; break;
            case IR_ALIGNMENT_SPACE_EVENLY: justifyStr = "space-evenly"; break;
            default: break;
        }
        cJSON_AddStringToObject(obj, "justifyContent", justifyStr);
    }

    if (layout->flex.cross_axis != IR_ALIGNMENT_START) {
        const char* alignStr = "flex-start";
        switch (layout->flex.cross_axis) {
            case IR_ALIGNMENT_CENTER: alignStr = "center"; break;
            case IR_ALIGNMENT_END: alignStr = "flex-end"; break;
            case IR_ALIGNMENT_STRETCH: alignStr = "stretch"; break;
            default: break;
        }
        cJSON_AddItemToObject(obj, "alignItems", cJSON_CreateString(alignStr));
    }

    if (layout->flex.gap > 0) {
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
}

// ============================================================================
// Component Serialization (Recursive)
// ============================================================================

static cJSON* json_serialize_component_recursive(IRComponent* component) {
    if (!component) return NULL;

    cJSON* obj = cJSON_CreateObject();

    // Basic properties
    cJSON_AddNumberToObject(obj, "id", component->id);

    const char* typeStr = ir_component_type_to_string(component->type);
    cJSON_AddStringToObject(obj, "type", typeStr);

    // Text content
    if (component->text_content && component->text_content[0] != '\0') {
        cJSON_AddStringToObject(obj, "text", component->text_content);
    }

    // Serialize style properties
    if (component->style) {
        json_serialize_style(obj, component->style);
    }

    // Serialize layout properties
    if (component->layout) {
        json_serialize_layout(obj, component->layout);
    }

    // Children
    if (component->child_count > 0) {
        cJSON* children = cJSON_CreateArray();
        for (uint32_t i = 0; i < component->child_count; i++) {
            cJSON* child = json_serialize_component_recursive(component->children[i]);
            if (child) {
                cJSON_AddItemToArray(children, child);
            }
        }
        cJSON_AddItemToObject(obj, "children", children);
    }

    return obj;
}

// ============================================================================
// Main JSON v2 Serialization Function
// ============================================================================

/**
 * Serialize IR component tree to JSON v2 format (complete property coverage)
 * @param root Root component to serialize
 * @return JSON string (caller must free), or NULL on error
 */
char* ir_serialize_json_v2(IRComponent* root) {
    if (!root) return NULL;

    cJSON* json = json_serialize_component_recursive(root);
    if (!json) return NULL;

    char* jsonStr = cJSON_Print(json);  // Pretty-printed JSON
    cJSON_Delete(json);

    return jsonStr;
}

/**
 * Write IR component tree to JSON v2 file
 * @param root Root component to serialize
 * @param filename Output file path
 * @return true on success, false on error
 */
bool ir_write_json_v2_file(IRComponent* root, const char* filename) {
    char* json = ir_serialize_json_v2(root);
    if (!json) return false;

    FILE* file = fopen(filename, "w");
    if (!file) {
        free(json);
        return false;
    }

    bool success = (fprintf(file, "%s", json) >= 0);
    fclose(file);
    free(json);

    return success;
}
