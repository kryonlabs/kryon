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

// ============================================================================
// Reactive Manifest JSON Serialization
// ============================================================================

/**
 * Convert IRReactiveVarType enum to JSON string
 */
static const char* json_reactive_type_to_string(IRReactiveVarType type) {
    switch (type) {
        case IR_REACTIVE_TYPE_INT: return "int";
        case IR_REACTIVE_TYPE_FLOAT: return "float";
        case IR_REACTIVE_TYPE_STRING: return "string";
        case IR_REACTIVE_TYPE_BOOL: return "bool";
        case IR_REACTIVE_TYPE_CUSTOM: return "custom";
        default: return "unknown";
    }
}

/**
 * Convert IRBindingType enum to JSON string
 */
static const char* json_binding_type_to_string(IRBindingType type) {
    switch (type) {
        case IR_BINDING_TEXT: return "text";
        case IR_BINDING_CONDITIONAL: return "conditional";
        case IR_BINDING_ATTRIBUTE: return "attribute";
        case IR_BINDING_FOR_LOOP: return "for_loop";
        case IR_BINDING_CUSTOM: return "custom";
        default: return "unknown";
    }
}

/**
 * Serialize reactive variable to cJSON object
 */
static cJSON* json_serialize_reactive_var(IRReactiveVarDescriptor* var) {
    if (!var) return NULL;

    cJSON* obj = cJSON_CreateObject();

    cJSON_AddNumberToObject(obj, "id", var->id);
    if (var->name) {
        cJSON_AddStringToObject(obj, "name", var->name);
    }
    cJSON_AddStringToObject(obj, "type", json_reactive_type_to_string(var->type));
    cJSON_AddNumberToObject(obj, "version", var->version);

    if (var->source_location) {
        cJSON_AddStringToObject(obj, "sourceLocation", var->source_location);
    }
    cJSON_AddNumberToObject(obj, "bindingCount", var->binding_count);

    // Serialize value based on type
    cJSON* value = NULL;
    switch (var->type) {
        case IR_REACTIVE_TYPE_INT:
            value = cJSON_CreateNumber(var->value.as_int);
            break;
        case IR_REACTIVE_TYPE_FLOAT:
            value = cJSON_CreateNumber(var->value.as_float);
            break;
        case IR_REACTIVE_TYPE_STRING:
            if (var->value.as_string) {
                value = cJSON_CreateString(var->value.as_string);
            } else {
                value = cJSON_CreateString("");
            }
            break;
        case IR_REACTIVE_TYPE_BOOL:
            value = cJSON_CreateBool(var->value.as_bool);
            break;
        case IR_REACTIVE_TYPE_CUSTOM:
            if (var->value.as_string) {
                value = cJSON_CreateString(var->value.as_string);
            } else {
                value = cJSON_CreateString("");
            }
            break;
    }

    if (value) {
        cJSON_AddItemToObject(obj, "value", value);
    }

    return obj;
}

/**
 * Serialize reactive binding to cJSON object
 */
static cJSON* json_serialize_reactive_binding(IRReactiveBinding* binding) {
    if (!binding) return NULL;

    cJSON* obj = cJSON_CreateObject();

    cJSON_AddNumberToObject(obj, "componentId", binding->component_id);
    cJSON_AddNumberToObject(obj, "varId", binding->reactive_var_id);
    cJSON_AddStringToObject(obj, "type", json_binding_type_to_string(binding->binding_type));

    if (binding->expression) {
        cJSON_AddStringToObject(obj, "expression", binding->expression);
    }

    if (binding->update_code) {
        cJSON_AddStringToObject(obj, "updateCode", binding->update_code);
    }

    return obj;
}

/**
 * Serialize reactive conditional to cJSON object
 */
static cJSON* json_serialize_reactive_conditional(IRReactiveConditional* cond) {
    if (!cond) return NULL;

    cJSON* obj = cJSON_CreateObject();

    cJSON_AddNumberToObject(obj, "componentId", cond->component_id);

    if (cond->condition) {
        cJSON_AddStringToObject(obj, "condition", cond->condition);
    }

    cJSON_AddBoolToObject(obj, "lastEvalResult", cond->last_eval_result);
    cJSON_AddBoolToObject(obj, "suspended", cond->suspended);

    // Serialize dependent var IDs array
    if (cond->dependent_var_count > 0 && cond->dependent_var_ids) {
        cJSON* deps = cJSON_CreateArray();
        for (uint32_t i = 0; i < cond->dependent_var_count; i++) {
            cJSON_AddItemToArray(deps, cJSON_CreateNumber(cond->dependent_var_ids[i]));
        }
        cJSON_AddItemToObject(obj, "dependentVarIds", deps);
    }

    return obj;
}

/**
 * Serialize reactive for-loop to cJSON object
 */
static cJSON* json_serialize_reactive_for_loop(IRReactiveForLoop* loop) {
    if (!loop) return NULL;

    cJSON* obj = cJSON_CreateObject();

    cJSON_AddNumberToObject(obj, "parentId", loop->parent_component_id);

    if (loop->collection_expr) {
        cJSON_AddStringToObject(obj, "collectionExpr", loop->collection_expr);
    }

    cJSON_AddNumberToObject(obj, "collectionVarId", loop->collection_var_id);

    if (loop->item_template) {
        cJSON_AddStringToObject(obj, "itemTemplate", loop->item_template);
    }

    // Serialize child component IDs array
    if (loop->child_count > 0 && loop->child_component_ids) {
        cJSON* children = cJSON_CreateArray();
        for (uint32_t i = 0; i < loop->child_count; i++) {
            cJSON_AddItemToArray(children, cJSON_CreateNumber(loop->child_component_ids[i]));
        }
        cJSON_AddItemToObject(obj, "childComponentIds", children);
    }

    return obj;
}

/**
 * Serialize complete reactive manifest to cJSON object
 */
static cJSON* json_serialize_reactive_manifest(IRReactiveManifest* manifest) {
    if (!manifest) return NULL;

    cJSON* obj = cJSON_CreateObject();

    cJSON_AddNumberToObject(obj, "version", manifest->format_version);
    cJSON_AddNumberToObject(obj, "nextVarId", manifest->next_var_id);

    // Serialize variables
    if (manifest->variable_count > 0) {
        cJSON* vars = cJSON_CreateArray();
        for (uint32_t i = 0; i < manifest->variable_count; i++) {
            cJSON* var = json_serialize_reactive_var(&manifest->variables[i]);
            if (var) {
                cJSON_AddItemToArray(vars, var);
            }
        }
        cJSON_AddItemToObject(obj, "variables", vars);
    }

    // Serialize bindings
    if (manifest->binding_count > 0) {
        cJSON* bindings = cJSON_CreateArray();
        for (uint32_t i = 0; i < manifest->binding_count; i++) {
            cJSON* binding = json_serialize_reactive_binding(&manifest->bindings[i]);
            if (binding) {
                cJSON_AddItemToArray(bindings, binding);
            }
        }
        cJSON_AddItemToObject(obj, "bindings", bindings);
    }

    // Serialize conditionals
    if (manifest->conditional_count > 0) {
        cJSON* conditionals = cJSON_CreateArray();
        for (uint32_t i = 0; i < manifest->conditional_count; i++) {
            cJSON* cond = json_serialize_reactive_conditional(&manifest->conditionals[i]);
            if (cond) {
                cJSON_AddItemToArray(conditionals, cond);
            }
        }
        cJSON_AddItemToObject(obj, "conditionals", conditionals);
    }

    // Serialize for-loops
    if (manifest->for_loop_count > 0) {
        cJSON* loops = cJSON_CreateArray();
        for (uint32_t i = 0; i < manifest->for_loop_count; i++) {
            cJSON* loop = json_serialize_reactive_for_loop(&manifest->for_loops[i]);
            if (loop) {
                cJSON_AddItemToArray(loops, loop);
            }
        }
        cJSON_AddItemToObject(obj, "forLoops", loops);
    }

    return obj;
}

/**
 * Serialize IR component tree with reactive manifest to JSON v2 format
 * @param root Root component to serialize
 * @param manifest Reactive manifest (can be NULL)
 * @return JSON string (caller must free), or NULL on error
 */
char* ir_serialize_json_v2_with_manifest(IRComponent* root, IRReactiveManifest* manifest) {
    if (!root) return NULL;

    cJSON* root_obj = cJSON_CreateObject();

    // Add component tree
    cJSON* component = json_serialize_component_recursive(root);
    if (component) {
        cJSON_AddItemToObject(root_obj, "component", component);
    }

    // Add reactive manifest
    if (manifest) {
        cJSON* manifest_obj = json_serialize_reactive_manifest(manifest);
        if (manifest_obj) {
            cJSON_AddItemToObject(root_obj, "manifest", manifest_obj);
        }
    }

    char* jsonStr = cJSON_Print(root_obj);
    cJSON_Delete(root_obj);

    return jsonStr;
}

/**
 * Write IR component tree with manifest to JSON v2 file
 * @param root Root component to serialize
 * @param manifest Reactive manifest (can be NULL)
 * @param filename Output file path
 * @return true on success, false on error
 */
bool ir_write_json_v2_file_with_manifest(IRComponent* root,
                                          IRReactiveManifest* manifest,
                                          const char* filename) {
    char* json = ir_serialize_json_v2_with_manifest(root, manifest);
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

// ============================================================================
// JSON Deserialization - Parsing Utilities
// ============================================================================

/**
 * Parse dimension string (e.g., "100px", "50%", "auto") into IRDimension
 */
static IRDimension json_parse_dimension(const char* str) {
    IRDimension dim = {0};

    if (!str || !*str) {
        dim.type = IR_DIMENSION_AUTO;
        return dim;
    }

    // Check for "auto"
    if (strcmp(str, "auto") == 0) {
        dim.type = IR_DIMENSION_AUTO;
        return dim;
    }

    // Parse numeric value
    char* endptr;
    float value = strtof(str, &endptr);

    // Check unit suffix
    if (endptr && *endptr) {
        if (strcmp(endptr, "px") == 0) {
            dim.type = IR_DIMENSION_PX;
            dim.value = value;
        } else if (strcmp(endptr, "%") == 0) {
            dim.type = IR_DIMENSION_PERCENT;
            dim.value = value;
        } else if (strcmp(endptr, "em") == 0) {
            dim.type = IR_DIMENSION_EM;
            dim.value = value;
        } else if (strcmp(endptr, "rem") == 0) {
            dim.type = IR_DIMENSION_REM;
            dim.value = value;
        } else if (strcmp(endptr, "vw") == 0) {
            dim.type = IR_DIMENSION_VW;
            dim.value = value;
        } else if (strcmp(endptr, "vh") == 0) {
            dim.type = IR_DIMENSION_VH;
            dim.value = value;
        } else if (strcmp(endptr, "fr") == 0) {
            dim.type = IR_DIMENSION_FLEX;
            dim.value = value;
        } else {
            // Default to px if unknown unit
            dim.type = IR_DIMENSION_PX;
            dim.value = value;
        }
    } else {
        // No unit, default to px
        dim.type = IR_DIMENSION_PX;
        dim.value = value;
    }

    return dim;
}

/**
 * Parse color string (e.g., "#rrggbb", "#rrggbbaa", "transparent") into IRColor
 */
static IRColor json_parse_color(const char* str) {
    IRColor color = {0};

    if (!str || !*str) {
        color.type = IR_COLOR_TRANSPARENT;
        return color;
    }

    // Check for "transparent"
    if (strcmp(str, "transparent") == 0) {
        color.type = IR_COLOR_TRANSPARENT;
        return color;
    }

    // Check for var() reference
    if (strncmp(str, "var(", 4) == 0) {
        color.type = IR_COLOR_VAR_REF;
        sscanf(str + 4, "%u", &color.data.var_id);
        return color;
    }

    // Parse hex color
    if (str[0] == '#') {
        color.type = IR_COLOR_SOLID;
        const char* hex = str + 1;
        size_t len = strlen(hex);

        unsigned int r = 0, g = 0, b = 0, a = 255;

        if (len == 6) {
            // #RRGGBB
            sscanf(hex, "%02x%02x%02x", &r, &g, &b);
        } else if (len == 8) {
            // #RRGGBBAA
            sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a);
        } else if (len == 3) {
            // #RGB - expand to RRGGBB
            sscanf(hex, "%1x%1x%1x", &r, &g, &b);
            r = (r << 4) | r;
            g = (g << 4) | g;
            b = (b << 4) | b;
        } else if (len == 4) {
            // #RGBA - expand to RRGGBBAA
            sscanf(hex, "%1x%1x%1x%1x", &r, &g, &b, &a);
            r = (r << 4) | r;
            g = (g << 4) | g;
            b = (b << 4) | b;
            a = (a << 4) | a;
        }

        color.data.r = (uint8_t)r;
        color.data.g = (uint8_t)g;
        color.data.b = (uint8_t)b;
        color.data.a = (uint8_t)a;

        return color;
    }

    // Default to black
    color.type = IR_COLOR_SOLID;
    color.data.r = 0;
    color.data.g = 0;
    color.data.b = 0;
    color.data.a = 255;

    return color;
}

/**
 * Parse spacing from JSON (number or array) into IRSpacing
 */
static IRSpacing json_parse_spacing(cJSON* json) {
    IRSpacing spacing = {0};

    if (!json) {
        return spacing;
    }

    if (cJSON_IsNumber(json)) {
        // Uniform spacing
        float value = (float)json->valuedouble;
        spacing.top = spacing.right = spacing.bottom = spacing.left = value;
    } else if (cJSON_IsArray(json)) {
        int count = cJSON_GetArraySize(json);
        if (count == 2) {
            // [vertical, horizontal]
            spacing.top = spacing.bottom = (float)cJSON_GetArrayItem(json, 0)->valuedouble;
            spacing.left = spacing.right = (float)cJSON_GetArrayItem(json, 1)->valuedouble;
        } else if (count == 4) {
            // [top, right, bottom, left]
            spacing.top = (float)cJSON_GetArrayItem(json, 0)->valuedouble;
            spacing.right = (float)cJSON_GetArrayItem(json, 1)->valuedouble;
            spacing.bottom = (float)cJSON_GetArrayItem(json, 2)->valuedouble;
            spacing.left = (float)cJSON_GetArrayItem(json, 3)->valuedouble;
        }
    }

    return spacing;
}

// ============================================================================
// Style & Layout Deserialization
// ============================================================================

/**
 * Parse text alignment from string
 */
static IRTextAlign json_parse_text_align(const char* str) {
    if (!str) return IR_TEXT_ALIGN_LEFT;
    if (strcmp(str, "center") == 0) return IR_TEXT_ALIGN_CENTER;
    if (strcmp(str, "right") == 0) return IR_TEXT_ALIGN_RIGHT;
    if (strcmp(str, "justify") == 0) return IR_TEXT_ALIGN_JUSTIFY;
    return IR_TEXT_ALIGN_LEFT;
}

/**
 * Parse alignment from string
 */
static IRAlignment json_parse_alignment(const char* str) {
    if (!str) return IR_ALIGNMENT_START;
    if (strcmp(str, "center") == 0) return IR_ALIGNMENT_CENTER;
    if (strcmp(str, "flex-end") == 0) return IR_ALIGNMENT_END;
    if (strcmp(str, "space-between") == 0) return IR_ALIGNMENT_SPACE_BETWEEN;
    if (strcmp(str, "space-around") == 0) return IR_ALIGNMENT_SPACE_AROUND;
    if (strcmp(str, "space-evenly") == 0) return IR_ALIGNMENT_SPACE_EVENLY;
    if (strcmp(str, "stretch") == 0) return IR_ALIGNMENT_STRETCH;
    return IR_ALIGNMENT_START;
}

/**
 * Deserialize style from JSON object into IRStyle
 */
static void json_deserialize_style(cJSON* obj, IRStyle* style) {
    if (!obj || !style) return;

    cJSON* item = NULL;

    // Dimensions
    if ((item = cJSON_GetObjectItem(obj, "width")) != NULL && cJSON_IsString(item)) {
        style->width = json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "height")) != NULL && cJSON_IsString(item)) {
        style->height = json_parse_dimension(item->valuestring);
    }

    // Visibility
    if ((item = cJSON_GetObjectItem(obj, "visible")) != NULL && cJSON_IsBool(item)) {
        style->visible = cJSON_IsTrue(item);
    }

    // Opacity
    if ((item = cJSON_GetObjectItem(obj, "opacity")) != NULL && cJSON_IsNumber(item)) {
        style->opacity = (float)item->valuedouble;
    }

    // Z-index
    if ((item = cJSON_GetObjectItem(obj, "zIndex")) != NULL && cJSON_IsNumber(item)) {
        style->z_index = item->valueint;
    }

    // Background color
    if ((item = cJSON_GetObjectItem(obj, "background")) != NULL && cJSON_IsString(item)) {
        style->background = json_parse_color(item->valuestring);
    }

    // Border
    if ((item = cJSON_GetObjectItem(obj, "border")) != NULL && cJSON_IsObject(item)) {
        cJSON* borderItem = NULL;
        if ((borderItem = cJSON_GetObjectItem(item, "width")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.width = (float)borderItem->valuedouble;
        }
        if ((borderItem = cJSON_GetObjectItem(item, "color")) != NULL && cJSON_IsString(borderItem)) {
            style->border.color = json_parse_color(borderItem->valuestring);
        }
        if ((borderItem = cJSON_GetObjectItem(item, "radius")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.radius = (float)borderItem->valuedouble;
        }
    }

    // Typography
    if ((item = cJSON_GetObjectItem(obj, "fontSize")) != NULL && cJSON_IsNumber(item)) {
        style->font.size = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "fontFamily")) != NULL && cJSON_IsString(item)) {
        if (style->font.family) free(style->font.family);
        style->font.family = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "fontWeight")) != NULL && cJSON_IsNumber(item)) {
        style->font.weight = item->valueint;
    }
    if ((item = cJSON_GetObjectItem(obj, "fontBold")) != NULL && cJSON_IsBool(item)) {
        style->font.bold = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(obj, "fontItalic")) != NULL && cJSON_IsBool(item)) {
        style->font.italic = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(obj, "color")) != NULL && cJSON_IsString(item)) {
        style->font.color = json_parse_color(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "textAlign")) != NULL && cJSON_IsString(item)) {
        style->font.align = json_parse_text_align(item->valuestring);
    }

    // Spacing
    if ((item = cJSON_GetObjectItem(obj, "padding")) != NULL) {
        style->padding = json_parse_spacing(item);
    }
    if ((item = cJSON_GetObjectItem(obj, "margin")) != NULL) {
        style->margin = json_parse_spacing(item);
    }

    // Transform
    if ((item = cJSON_GetObjectItem(obj, "transform")) != NULL && cJSON_IsObject(item)) {
        cJSON* transformItem = NULL;
        if ((transformItem = cJSON_GetObjectItem(item, "translate")) != NULL && cJSON_IsArray(transformItem)) {
            if (cJSON_GetArraySize(transformItem) >= 2) {
                style->transform.translate_x = (float)cJSON_GetArrayItem(transformItem, 0)->valuedouble;
                style->transform.translate_y = (float)cJSON_GetArrayItem(transformItem, 1)->valuedouble;
            }
        }
        if ((transformItem = cJSON_GetObjectItem(item, "scale")) != NULL && cJSON_IsArray(transformItem)) {
            if (cJSON_GetArraySize(transformItem) >= 2) {
                style->transform.scale_x = (float)cJSON_GetArrayItem(transformItem, 0)->valuedouble;
                style->transform.scale_y = (float)cJSON_GetArrayItem(transformItem, 1)->valuedouble;
            }
        }
        if ((transformItem = cJSON_GetObjectItem(item, "rotate")) != NULL && cJSON_IsNumber(transformItem)) {
            style->transform.rotate = (float)transformItem->valuedouble;
        }
    }
}

/**
 * Deserialize layout from JSON object into IRLayout
 */
static void json_deserialize_layout(cJSON* obj, IRLayout* layout) {
    if (!obj || !layout) return;

    cJSON* item = NULL;

    // Min/Max dimensions
    if ((item = cJSON_GetObjectItem(obj, "minWidth")) != NULL && cJSON_IsString(item)) {
        layout->min_width = json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "minHeight")) != NULL && cJSON_IsString(item)) {
        layout->min_height = json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "maxWidth")) != NULL && cJSON_IsString(item)) {
        layout->max_width = json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "maxHeight")) != NULL && cJSON_IsString(item)) {
        layout->max_height = json_parse_dimension(item->valuestring);
    }

    // Flexbox properties
    if ((item = cJSON_GetObjectItem(obj, "flexDirection")) != NULL && cJSON_IsString(item)) {
        layout->flex.direction = (strcmp(item->valuestring, "row") == 0) ? 1 : 0;
    }
    if ((item = cJSON_GetObjectItem(obj, "justifyContent")) != NULL && cJSON_IsString(item)) {
        layout->flex.justify_content = json_parse_alignment(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "alignItems")) != NULL && cJSON_IsString(item)) {
        layout->flex.cross_axis = json_parse_alignment(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "flexWrap")) != NULL && cJSON_IsBool(item)) {
        layout->flex.wrap = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(obj, "flexGrow")) != NULL && cJSON_IsNumber(item)) {
        layout->flex.grow = (uint8_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(obj, "flexShrink")) != NULL && cJSON_IsNumber(item)) {
        layout->flex.shrink = (uint8_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(obj, "gap")) != NULL && cJSON_IsNumber(item)) {
        layout->flex.gap = (uint32_t)item->valueint;
    }
}

// ============================================================================
// Component Tree Deserialization (Recursive)
// ============================================================================

/**
 * Convert component type string to enum
 */
static IRComponentType ir_string_to_component_type(const char* str) {
    if (!str) return IR_COMPONENT_CONTAINER;
    if (strcmp(str, "Container") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(str, "Row") == 0) return IR_COMPONENT_ROW;
    if (strcmp(str, "Column") == 0) return IR_COMPONENT_COLUMN;
    if (strcmp(str, "Center") == 0) return IR_COMPONENT_CENTER;
    if (strcmp(str, "Text") == 0) return IR_COMPONENT_TEXT;
    if (strcmp(str, "Button") == 0) return IR_COMPONENT_BUTTON;
    if (strcmp(str, "Input") == 0) return IR_COMPONENT_INPUT;
    if (strcmp(str, "Checkbox") == 0) return IR_COMPONENT_CHECKBOX;
    if (strcmp(str, "Image") == 0) return IR_COMPONENT_IMAGE;
    if (strcmp(str, "Canvas") == 0) return IR_COMPONENT_CANVAS;
    if (strcmp(str, "Dropdown") == 0) return IR_COMPONENT_DROPDOWN;
    if (strcmp(str, "Markdown") == 0) return IR_COMPONENT_MARKDOWN;
    if (strcmp(str, "Custom") == 0) return IR_COMPONENT_CUSTOM;
    return IR_COMPONENT_CONTAINER;
}

/**
 * Recursively deserialize component from JSON
 */
static IRComponent* json_deserialize_component_recursive(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    // Create component
    IRComponent* component = (IRComponent*)calloc(1, sizeof(IRComponent));
    if (!component) return NULL;

    // Initialize basic fields
    component->style = ir_create_style();
    component->layout = ir_create_layout();

    cJSON* item = NULL;

    // ID
    if ((item = cJSON_GetObjectItem(json, "id")) != NULL && cJSON_IsNumber(item)) {
        component->id = (uint32_t)item->valueint;
    }

    // Type
    if ((item = cJSON_GetObjectItem(json, "type")) != NULL && cJSON_IsString(item)) {
        component->type = ir_string_to_component_type(item->valuestring);
    }

    // Text content
    if ((item = cJSON_GetObjectItem(json, "text")) != NULL && cJSON_IsString(item)) {
        component->text_content = strdup(item->valuestring);
    }

    // Deserialize style and layout
    json_deserialize_style(json, component->style);
    json_deserialize_layout(json, component->layout);

    // Children
    if ((item = cJSON_GetObjectItem(json, "children")) != NULL && cJSON_IsArray(item)) {
        int childCount = cJSON_GetArraySize(item);
        if (childCount > 0) {
            component->children = (IRComponent**)calloc(childCount, sizeof(IRComponent*));
            component->child_count = 0;
            component->child_capacity = childCount;

            for (int i = 0; i < childCount; i++) {
                cJSON* childJson = cJSON_GetArrayItem(item, i);
                IRComponent* child = json_deserialize_component_recursive(childJson);
                if (child) {
                    child->parent = component;
                    component->children[component->child_count++] = child;
                }
            }
        }
    }

    return component;
}

// ============================================================================
// Reactive Manifest Deserialization
// ============================================================================

/**
 * Parse reactive variable type from string
 */
static IRReactiveVarType json_parse_reactive_type(const char* str) {
    if (!str) return IR_REACTIVE_TYPE_INT;
    if (strcmp(str, "int") == 0) return IR_REACTIVE_TYPE_INT;
    if (strcmp(str, "float") == 0) return IR_REACTIVE_TYPE_FLOAT;
    if (strcmp(str, "string") == 0) return IR_REACTIVE_TYPE_STRING;
    if (strcmp(str, "bool") == 0) return IR_REACTIVE_TYPE_BOOL;
    if (strcmp(str, "custom") == 0) return IR_REACTIVE_TYPE_CUSTOM;
    return IR_REACTIVE_TYPE_INT;
}

/**
 * Parse binding type from string
 */
static IRBindingType json_parse_binding_type(const char* str) {
    if (!str) return IR_BINDING_TEXT;
    if (strcmp(str, "text") == 0) return IR_BINDING_TEXT;
    if (strcmp(str, "conditional") == 0) return IR_BINDING_CONDITIONAL;
    if (strcmp(str, "attribute") == 0) return IR_BINDING_ATTRIBUTE;
    if (strcmp(str, "for_loop") == 0) return IR_BINDING_FOR_LOOP;
    if (strcmp(str, "custom") == 0) return IR_BINDING_CUSTOM;
    return IR_BINDING_TEXT;
}

/**
 * Deserialize reactive variable from JSON
 */
static IRReactiveVarDescriptor* json_deserialize_reactive_var(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    IRReactiveVarDescriptor* var = (IRReactiveVarDescriptor*)calloc(1, sizeof(IRReactiveVarDescriptor));
    if (!var) return NULL;

    cJSON* item = NULL;

    if ((item = cJSON_GetObjectItem(json, "id")) != NULL && cJSON_IsNumber(item)) {
        var->id = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "name")) != NULL && cJSON_IsString(item)) {
        var->name = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "type")) != NULL && cJSON_IsString(item)) {
        var->type = json_parse_reactive_type(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "version")) != NULL && cJSON_IsNumber(item)) {
        var->version = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "sourceLocation")) != NULL && cJSON_IsString(item)) {
        var->source_location = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "bindingCount")) != NULL && cJSON_IsNumber(item)) {
        var->binding_count = (uint32_t)item->valueint;
    }

    // Deserialize value based on type
    if ((item = cJSON_GetObjectItem(json, "value")) != NULL) {
        switch (var->type) {
            case IR_REACTIVE_TYPE_INT:
                if (cJSON_IsNumber(item)) {
                    var->value.as_int = item->valueint;
                }
                break;
            case IR_REACTIVE_TYPE_FLOAT:
                if (cJSON_IsNumber(item)) {
                    var->value.as_float = (float)item->valuedouble;
                }
                break;
            case IR_REACTIVE_TYPE_STRING:
            case IR_REACTIVE_TYPE_CUSTOM:
                if (cJSON_IsString(item)) {
                    var->value.as_string = strdup(item->valuestring);
                }
                break;
            case IR_REACTIVE_TYPE_BOOL:
                if (cJSON_IsBool(item)) {
                    var->value.as_bool = cJSON_IsTrue(item);
                }
                break;
        }
    }

    return var;
}

/**
 * Deserialize reactive binding from JSON
 */
static IRReactiveBinding* json_deserialize_reactive_binding(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    IRReactiveBinding* binding = (IRReactiveBinding*)calloc(1, sizeof(IRReactiveBinding));
    if (!binding) return NULL;

    cJSON* item = NULL;

    if ((item = cJSON_GetObjectItem(json, "componentId")) != NULL && cJSON_IsNumber(item)) {
        binding->component_id = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "reactiveVarId")) != NULL && cJSON_IsNumber(item)) {
        binding->reactive_var_id = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "bindingType")) != NULL && cJSON_IsString(item)) {
        binding->binding_type = json_parse_binding_type(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "expression")) != NULL && cJSON_IsString(item)) {
        binding->expression = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "updateCode")) != NULL && cJSON_IsString(item)) {
        binding->update_code = strdup(item->valuestring);
    }

    return binding;
}

/**
 * Deserialize reactive conditional from JSON
 */
static IRReactiveConditional* json_deserialize_reactive_conditional(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    IRReactiveConditional* cond = (IRReactiveConditional*)calloc(1, sizeof(IRReactiveConditional));
    if (!cond) return NULL;

    cJSON* item = NULL;

    if ((item = cJSON_GetObjectItem(json, "componentId")) != NULL && cJSON_IsNumber(item)) {
        cond->component_id = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "condition")) != NULL && cJSON_IsString(item)) {
        cond->condition = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "lastEvalResult")) != NULL && cJSON_IsBool(item)) {
        cond->last_eval_result = cJSON_IsTrue(item);
    }
    if ((item = cJSON_GetObjectItem(json, "suspended")) != NULL && cJSON_IsBool(item)) {
        cond->suspended = cJSON_IsTrue(item);
    }

    // Deserialize dependent var IDs
    if ((item = cJSON_GetObjectItem(json, "dependentVarIds")) != NULL && cJSON_IsArray(item)) {
        int count = cJSON_GetArraySize(item);
        if (count > 0) {
            cond->dependent_var_ids = (uint32_t*)calloc(count, sizeof(uint32_t));
            cond->dependent_var_count = count;
            for (int i = 0; i < count; i++) {
                cJSON* idItem = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(idItem)) {
                    cond->dependent_var_ids[i] = (uint32_t)idItem->valueint;
                }
            }
        }
    }

    return cond;
}

/**
 * Deserialize reactive for-loop from JSON
 */
static IRReactiveForLoop* json_deserialize_reactive_for_loop(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    IRReactiveForLoop* loop = (IRReactiveForLoop*)calloc(1, sizeof(IRReactiveForLoop));
    if (!loop) return NULL;

    cJSON* item = NULL;

    if ((item = cJSON_GetObjectItem(json, "parentComponentId")) != NULL && cJSON_IsNumber(item)) {
        loop->parent_component_id = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "collectionExpr")) != NULL && cJSON_IsString(item)) {
        loop->collection_expr = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "collectionVarId")) != NULL && cJSON_IsNumber(item)) {
        loop->collection_var_id = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "itemTemplate")) != NULL && cJSON_IsString(item)) {
        loop->item_template = strdup(item->valuestring);
    }

    // Deserialize child component IDs
    if ((item = cJSON_GetObjectItem(json, "childComponentIds")) != NULL && cJSON_IsArray(item)) {
        int count = cJSON_GetArraySize(item);
        if (count > 0) {
            loop->child_component_ids = (uint32_t*)calloc(count, sizeof(uint32_t));
            loop->child_count = count;
            for (int i = 0; i < count; i++) {
                cJSON* idItem = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(idItem)) {
                    loop->child_component_ids[i] = (uint32_t)idItem->valueint;
                }
            }
        }
    }

    return loop;
}

/**
 * Deserialize reactive manifest from JSON
 */
static IRReactiveManifest* json_deserialize_reactive_manifest(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    IRReactiveManifest* manifest = (IRReactiveManifest*)calloc(1, sizeof(IRReactiveManifest));
    if (!manifest) return NULL;

    cJSON* item = NULL;

    // Deserialize variables
    if ((item = cJSON_GetObjectItem(json, "variables")) != NULL && cJSON_IsArray(item)) {
        int count = cJSON_GetArraySize(item);
        if (count > 0) {
            manifest->variables = (IRReactiveVarDescriptor*)calloc(count, sizeof(IRReactiveVarDescriptor));
            manifest->variable_count = 0;
            for (int i = 0; i < count; i++) {
                cJSON* varJson = cJSON_GetArrayItem(item, i);
                IRReactiveVarDescriptor* var = json_deserialize_reactive_var(varJson);
                if (var) {
                    manifest->variables[manifest->variable_count++] = *var;
                    free(var);
                }
            }
        }
    }

    // Deserialize bindings
    if ((item = cJSON_GetObjectItem(json, "bindings")) != NULL && cJSON_IsArray(item)) {
        int count = cJSON_GetArraySize(item);
        if (count > 0) {
            manifest->bindings = (IRReactiveBinding*)calloc(count, sizeof(IRReactiveBinding));
            manifest->binding_count = 0;
            for (int i = 0; i < count; i++) {
                cJSON* bindingJson = cJSON_GetArrayItem(item, i);
                IRReactiveBinding* binding = json_deserialize_reactive_binding(bindingJson);
                if (binding) {
                    manifest->bindings[manifest->binding_count++] = *binding;
                    free(binding);
                }
            }
        }
    }

    // Deserialize conditionals
    if ((item = cJSON_GetObjectItem(json, "conditionals")) != NULL && cJSON_IsArray(item)) {
        int count = cJSON_GetArraySize(item);
        if (count > 0) {
            manifest->conditionals = (IRReactiveConditional*)calloc(count, sizeof(IRReactiveConditional));
            manifest->conditional_count = 0;
            for (int i = 0; i < count; i++) {
                cJSON* condJson = cJSON_GetArrayItem(item, i);
                IRReactiveConditional* cond = json_deserialize_reactive_conditional(condJson);
                if (cond) {
                    manifest->conditionals[manifest->conditional_count++] = *cond;
                    free(cond);
                }
            }
        }
    }

    // Deserialize for-loops
    if ((item = cJSON_GetObjectItem(json, "forLoops")) != NULL && cJSON_IsArray(item)) {
        int count = cJSON_GetArraySize(item);
        if (count > 0) {
            manifest->for_loops = (IRReactiveForLoop*)calloc(count, sizeof(IRReactiveForLoop));
            manifest->for_loop_count = 0;
            for (int i = 0; i < count; i++) {
                cJSON* loopJson = cJSON_GetArrayItem(item, i);
                IRReactiveForLoop* loop = json_deserialize_reactive_for_loop(loopJson);
                if (loop) {
                    manifest->for_loops[manifest->for_loop_count++] = *loop;
                    free(loop);
                }
            }
        }
    }

    return manifest;
}

// ============================================================================
// Main JSON v2 Deserialization Entry Points
// ============================================================================

/**
 * Deserialize IR component tree with reactive manifest from JSON v2 format
 * @param json_string JSON string to deserialize
 * @param manifest Output parameter for reactive manifest (can be NULL if not needed)
 * @return Deserialized component tree, or NULL on error
 */
IRComponent* ir_deserialize_json_v2_with_manifest(const char* json_string, IRReactiveManifest** manifest) {
    if (!json_string) return NULL;

    cJSON* root = cJSON_Parse(json_string);
    if (!root) return NULL;

    IRComponent* component = NULL;
    IRReactiveManifest* deserialized_manifest = NULL;

    // Check if root has "component" and "manifest" keys (wrapped format)
    cJSON* componentJson = cJSON_GetObjectItem(root, "component");
    cJSON* manifestJson = cJSON_GetObjectItem(root, "manifest");

    if (componentJson && cJSON_IsObject(componentJson)) {
        // Wrapped format: { "component": {...}, "manifest": {...} }
        component = json_deserialize_component_recursive(componentJson);

        if (manifestJson && cJSON_IsObject(manifestJson) && manifest) {
            deserialized_manifest = json_deserialize_reactive_manifest(manifestJson);
        }
    } else {
        // Unwrapped format: just component tree at root
        component = json_deserialize_component_recursive(root);
    }

    if (manifest) {
        *manifest = deserialized_manifest;
    }

    cJSON_Delete(root);
    return component;
}

/**
 * Deserialize IR component tree from JSON v2 format (no manifest)
 * @param json_string JSON string to deserialize
 * @return Deserialized component tree, or NULL on error
 */
IRComponent* ir_deserialize_json_v2(const char* json_string) {
    return ir_deserialize_json_v2_with_manifest(json_string, NULL);
}

/**
 * Read and deserialize IR component tree with manifest from JSON v2 file
 * @param filename Input file path
 * @param manifest Output parameter for reactive manifest (can be NULL if not needed)
 * @return Deserialized component tree, or NULL on error
 */
IRComponent* ir_read_json_v2_file_with_manifest(const char* filename, IRReactiveManifest** manifest) {
    if (!filename) return NULL;

    FILE* file = fopen(filename, "r");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }

    // Read file content
    char* content = (char*)malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(content, 1, file_size, file);
    content[read_size] = '\0';
    fclose(file);

    // Deserialize
    IRComponent* component = ir_deserialize_json_v2_with_manifest(content, manifest);
    free(content);

    return component;
}

/**
 * Read and deserialize IR component tree from JSON v2 file (no manifest)
 * @param filename Input file path
 * @return Deserialized component tree, or NULL on error
 */
IRComponent* ir_read_json_v2_file(const char* filename) {
    return ir_read_json_v2_file_with_manifest(filename, NULL);
}
