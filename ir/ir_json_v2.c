// JSON v2 Serialization - Complete IR Property Coverage
// This file implements full JSON serialization using cJSON

#define _GNU_SOURCE
#include "ir_serialization.h"
#include "ir_builder.h"
#include "ir_logic.h"
#include "cJSON.h"
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

/**
 * Serialize component - with option to serialize as reference or full tree
 * @param component Component to serialize
 * @param as_template If true, serialize full tree (for templates). If false, output reference when component_ref is set.
 */
static cJSON* json_serialize_component_impl(IRComponent* component, bool as_template) {
    if (!component) return NULL;

    cJSON* obj = cJSON_CreateObject();

    // If this is a component instance (has component_ref) and we're not serializing as template,
    // output just a reference instead of the full tree
    if (!as_template && component->component_ref && component->component_ref[0] != '\0') {
        cJSON_AddStringToObject(obj, "component", component->component_ref);
        cJSON_AddNumberToObject(obj, "id", component->id);

        // Add props if present
        if (component->component_props && component->component_props[0] != '\0') {
            cJSON* propsJson = cJSON_Parse(component->component_props);
            if (propsJson) {
                cJSON_AddItemToObject(obj, "props", propsJson);
            }
        }
        return obj;
    }

    // Basic properties
    cJSON_AddNumberToObject(obj, "id", component->id);

    const char* typeStr = ir_component_type_to_string(component->type);
    cJSON_AddStringToObject(obj, "type", typeStr);

    // Text content
    if (component->text_content && component->text_content[0] != '\0') {
        cJSON_AddStringToObject(obj, "text", component->text_content);
    }

    // Reactive text expression (for declarative .kir files)
    if (component->text_expression && component->text_expression[0] != '\0') {
        cJSON_AddStringToObject(obj, "text_expression", component->text_expression);
    }

    // Component reference (for custom components) - only in template mode
    if (as_template && component->component_ref && component->component_ref[0] != '\0') {
        cJSON_AddStringToObject(obj, "component_ref", component->component_ref);
    }

    // Serialize style properties
    if (component->style) {
        json_serialize_style(obj, component->style);
    }

    // Serialize layout properties
    if (component->layout) {
        json_serialize_layout(obj, component->layout);
    }

    // Serialize events (IR v2.1: with bytecode support)
    if (component->events) {
        cJSON* events = cJSON_CreateArray();
        IREvent* event = component->events;
        while (event) {
            cJSON* eventObj = cJSON_CreateObject();

            // Event type
            const char* eventType = "unknown";
            switch (event->type) {
                case IR_EVENT_CLICK: eventType = "click"; break;
                case IR_EVENT_HOVER: eventType = "hover"; break;
                case IR_EVENT_FOCUS: eventType = "focus"; break;
                case IR_EVENT_BLUR: eventType = "blur"; break;
                case IR_EVENT_KEY: eventType = "key"; break;
                case IR_EVENT_SCROLL: eventType = "scroll"; break;
                case IR_EVENT_TIMER: eventType = "timer"; break;
                case IR_EVENT_CUSTOM: eventType = "custom"; break;
            }
            cJSON_AddStringToObject(eventObj, "type", eventType);

            // Legacy logic ID (for Nim/C callbacks)
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

            cJSON_AddItemToArray(events, eventObj);
            event = event->next;
        }
        cJSON_AddItemToObject(obj, "events", events);
    }

    // Children
    if (component->child_count > 0) {
        cJSON* children = cJSON_CreateArray();
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
static cJSON* json_serialize_component_recursive(IRComponent* component) {
    return json_serialize_component_impl(component, false);
}

// Wrapper for template serialization (full tree)
static cJSON* json_serialize_component_as_template(IRComponent* component) {
    return json_serialize_component_impl(component, true);
}

// ============================================================================
// Reactive Manifest Serialization (v2.1 POC)
// ============================================================================

/**
 * Serialize reactive manifest to JSON
 * @param manifest Reactive manifest to serialize
 * @return cJSON object (caller must delete), or NULL if no manifest data
 */
/**
 * Serialize component definitions to JSON
 */
static cJSON* json_serialize_component_definitions(IRReactiveManifest* manifest) {
    if (!manifest || manifest->component_def_count == 0) return NULL;

    cJSON* arr = cJSON_CreateArray();

    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        IRComponentDefinition* def = &manifest->component_defs[i];
        cJSON* defObj = cJSON_CreateObject();

        if (def->name) {
            cJSON_AddStringToObject(defObj, "name", def->name);
        }

        // Serialize props
        if (def->prop_count > 0 && def->props) {
            cJSON* propsArr = cJSON_CreateArray();
            for (uint32_t j = 0; j < def->prop_count; j++) {
                IRComponentProp* prop = &def->props[j];
                cJSON* propObj = cJSON_CreateObject();
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
            for (uint32_t j = 0; j < def->state_var_count; j++) {
                IRComponentStateVar* sv = &def->state_vars[j];
                cJSON* svObj = cJSON_CreateObject();
                if (sv->name) cJSON_AddStringToObject(svObj, "name", sv->name);
                if (sv->type) cJSON_AddStringToObject(svObj, "type", sv->type);
                if (sv->initial_expr) cJSON_AddStringToObject(svObj, "initial", sv->initial_expr);
                cJSON_AddItemToArray(stateArr, svObj);
            }
            cJSON_AddItemToObject(defObj, "state", stateArr);
        }

        // Serialize template if present (use template mode for full tree)
        if (def->template_root) {
            cJSON* templateJson = json_serialize_component_as_template(def->template_root);
            if (templateJson) {
                cJSON_AddItemToObject(defObj, "template", templateJson);
            }
        }

        cJSON_AddItemToArray(arr, defObj);
    }

    return arr;
}

static cJSON* json_serialize_reactive_manifest(IRReactiveManifest* manifest) {
    if (!manifest || (manifest->variable_count == 0 && manifest->component_def_count == 0 &&
                      manifest->conditional_count == 0 && manifest->for_loop_count == 0)) return NULL;

    cJSON* obj = cJSON_CreateObject();

    // Serialize variables
    if (manifest->variable_count > 0) {
        cJSON* vars = cJSON_CreateArray();
        for (uint32_t i = 0; i < manifest->variable_count; i++) {
            IRReactiveVarDescriptor* var = &manifest->variables[i];
            cJSON* varObj = cJSON_CreateObject();

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
        for (uint32_t i = 0; i < manifest->binding_count; i++) {
            IRReactiveBinding* binding = &manifest->bindings[i];
            cJSON* bindingObj = cJSON_CreateObject();

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
        for (uint32_t i = 0; i < manifest->conditional_count; i++) {
            IRReactiveConditional* cond = &manifest->conditionals[i];
            cJSON* condObj = cJSON_CreateObject();

            cJSON_AddNumberToObject(condObj, "component_id", cond->component_id);

            if (cond->condition) {
                cJSON_AddStringToObject(condObj, "condition", cond->condition);
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
                for (uint32_t j = 0; j < cond->then_children_count; j++) {
                    cJSON_AddItemToArray(thenIds, cJSON_CreateNumber(cond->then_children_ids[j]));
                }
                cJSON_AddItemToObject(condObj, "then_children_ids", thenIds);
            }

            if (cond->else_children_count > 0 && cond->else_children_ids) {
                cJSON* elseIds = cJSON_CreateArray();
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
        for (uint32_t i = 0; i < manifest->for_loop_count; i++) {
            IRReactiveForLoop* loop = &manifest->for_loops[i];
            cJSON* loopObj = cJSON_CreateObject();

            cJSON_AddNumberToObject(loopObj, "parent_component_id", loop->parent_component_id);
            cJSON_AddNumberToObject(loopObj, "collection_var_id", loop->collection_var_id);

            if (loop->collection_expr) {
                cJSON_AddStringToObject(loopObj, "collection_expr", loop->collection_expr);
            }

            cJSON_AddItemToArray(loops, loopObj);
        }
        cJSON_AddItemToObject(obj, "for_loops", loops);
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
 * Serialize IR component tree with reactive manifest to JSON v2.1 format
 * @param root Root component to serialize
 * @param manifest Reactive manifest to include (can be NULL)
 * @return JSON string (caller must free), or NULL on error
 */
char* ir_serialize_json_v2_with_manifest(IRComponent* root, IRReactiveManifest* manifest) {
    if (!root) return NULL;

    // Create wrapper object
    cJSON* wrapper = cJSON_CreateObject();

    // Add format version - v2.1 when we have component definitions, conditionals, or for-loops
    if (manifest && (manifest->component_def_count > 0 || manifest->conditional_count > 0 || manifest->for_loop_count > 0)) {
        cJSON_AddStringToObject(wrapper, "format_version", "2.1");
    }

    // Add component definitions FIRST (at the top)
    if (manifest && manifest->component_def_count > 0) {
        cJSON* componentDefsJson = json_serialize_component_definitions(manifest);
        if (componentDefsJson) {
            cJSON_AddItemToObject(wrapper, "component_definitions", componentDefsJson);
        }
    }

    // Add component tree
    cJSON* componentJson = json_serialize_component_recursive(root);
    if (!componentJson) {
        cJSON_Delete(wrapper);
        return NULL;
    }
    cJSON_AddItemToObject(wrapper, "root", componentJson);

    // Add reactive manifest if present (variables, bindings, conditionals, for-loops)
    if (manifest && (manifest->variable_count > 0 || manifest->binding_count > 0 ||
                     manifest->conditional_count > 0 || manifest->for_loop_count > 0)) {
        cJSON* manifestJson = json_serialize_reactive_manifest(manifest);
        if (manifestJson) {
            cJSON_AddItemToObject(wrapper, "reactive_manifest", manifestJson);
        }
    }

    char* jsonStr = cJSON_Print(wrapper);  // Pretty-printed JSON
    cJSON_Delete(wrapper);

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

/**
 * Write IR component tree with reactive manifest to JSON v2.1 file
 * @param root Root component to serialize
 * @param manifest Reactive manifest to include (can be NULL)
 * @param filename Output file path
 * @return true on success, false on error
 */
bool ir_write_json_v2_with_manifest_file(IRComponent* root, IRReactiveManifest* manifest, const char* filename) {
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
// JSON v3.0 Serialization - With Logic Block Support
// ============================================================================

/**
 * Serialize IR component tree with reactive manifest and logic block to JSON v3.0 format
 * @param root Root component to serialize
 * @param manifest Reactive manifest to include (can be NULL)
 * @param logic Logic block with functions and event bindings (can be NULL)
 * @return JSON string (caller must free), or NULL on error
 */
char* ir_serialize_json_v3(IRComponent* root, IRReactiveManifest* manifest, struct IRLogicBlock* logic) {
    if (!root) return NULL;

    // Create wrapper object
    cJSON* wrapper = cJSON_CreateObject();

    // Add format version
    cJSON_AddStringToObject(wrapper, "format_version", "3.0");

    // Add component definitions FIRST (at the top)
    if (manifest && manifest->component_def_count > 0) {
        cJSON* componentDefsJson = json_serialize_component_definitions(manifest);
        if (componentDefsJson) {
            cJSON_AddItemToObject(wrapper, "component_definitions", componentDefsJson);
        }
    }

    // Add component tree
    cJSON* componentJson = json_serialize_component_recursive(root);
    if (!componentJson) {
        cJSON_Delete(wrapper);
        return NULL;
    }
    cJSON_AddItemToObject(wrapper, "root", componentJson);

    // Add logic block if present
    if (logic && (logic->function_count > 0 || logic->event_binding_count > 0)) {
        cJSON* logicJson = ir_logic_block_to_json(logic);
        if (logicJson) {
            cJSON_AddItemToObject(wrapper, "logic", logicJson);
        }
    }

    // Add reactive manifest if present (variables, bindings, conditionals, for-loops)
    if (manifest && (manifest->variable_count > 0 || manifest->binding_count > 0 ||
                     manifest->conditional_count > 0 || manifest->for_loop_count > 0)) {
        cJSON* manifestJson = json_serialize_reactive_manifest(manifest);
        if (manifestJson) {
            cJSON_AddItemToObject(wrapper, "reactive_manifest", manifestJson);
        }
    }

    char* jsonStr = cJSON_Print(wrapper);  // Pretty-printed JSON
    cJSON_Delete(wrapper);

    return jsonStr;
}

/**
 * Write IR component tree with logic block to JSON v3.0 file
 * @param root Root component to serialize
 * @param manifest Reactive manifest to include (can be NULL)
 * @param logic Logic block with functions and event bindings (can be NULL)
 * @param filename Output file path
 * @return true on success, false on error
 */
bool ir_write_json_v3_file(IRComponent* root, IRReactiveManifest* manifest, struct IRLogicBlock* logic, const char* filename) {
    char* json = ir_serialize_json_v3(root, manifest, logic);
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

    // Deserialize events (IR v2.1: with bytecode support)
    if ((item = cJSON_GetObjectItem(json, "events")) != NULL && cJSON_IsArray(item)) {
        int eventCount = cJSON_GetArraySize(item);
        IREvent* lastEvent = NULL;

        for (int i = 0; i < eventCount; i++) {
            cJSON* eventJson = cJSON_GetArrayItem(item, i);
            if (!eventJson || !cJSON_IsObject(eventJson)) continue;

            IREvent* event = (IREvent*)calloc(1, sizeof(IREvent));
            if (!event) continue;

            // Event type
            cJSON* typeItem = cJSON_GetObjectItem(eventJson, "type");
            if (typeItem && cJSON_IsString(typeItem)) {
                const char* typeStr = typeItem->valuestring;
                if (strcmp(typeStr, "click") == 0) event->type = IR_EVENT_CLICK;
                else if (strcmp(typeStr, "hover") == 0) event->type = IR_EVENT_HOVER;
                else if (strcmp(typeStr, "focus") == 0) event->type = IR_EVENT_FOCUS;
                else if (strcmp(typeStr, "blur") == 0) event->type = IR_EVENT_BLUR;
                else if (strcmp(typeStr, "key") == 0) event->type = IR_EVENT_KEY;
                else if (strcmp(typeStr, "scroll") == 0) event->type = IR_EVENT_SCROLL;
                else if (strcmp(typeStr, "timer") == 0) event->type = IR_EVENT_TIMER;
                else if (strcmp(typeStr, "custom") == 0) event->type = IR_EVENT_CUSTOM;
            }

            // Logic ID (legacy)
            cJSON* logicIdItem = cJSON_GetObjectItem(eventJson, "logic_id");
            if (logicIdItem && cJSON_IsString(logicIdItem)) {
                event->logic_id = strdup(logicIdItem->valuestring);
            }

            // Handler data
            cJSON* handlerDataItem = cJSON_GetObjectItem(eventJson, "handler_data");
            if (handlerDataItem && cJSON_IsString(handlerDataItem)) {
                event->handler_data = strdup(handlerDataItem->valuestring);
            }

            // Bytecode function ID (IR v2.1)
            cJSON* funcIdItem = cJSON_GetObjectItem(eventJson, "bytecode_function_id");
            if (funcIdItem && cJSON_IsNumber(funcIdItem)) {
                event->bytecode_function_id = (uint32_t)funcIdItem->valueint;
            }

            // Add to linked list
            if (!component->events) {
                component->events = event;
            } else {
                lastEvent->next = event;
            }
            lastEvent = event;
        }
    }

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
// JSON v2 Deserialization Functions
// ============================================================================

/**
 * Deserialize IR component tree from JSON v2 format
 * @param json_string JSON string to deserialize
 * @return Deserialized component tree, or NULL on error
 */
IRComponent* ir_deserialize_json_v2(const char* json_string) {
    if (!json_string) return NULL;

    cJSON* root = cJSON_Parse(json_string);
    if (!root) return NULL;

    IRComponent* component = NULL;

    // Check for "root" key first (new format), then "component" (legacy)
    cJSON* componentJson = cJSON_GetObjectItem(root, "root");
    if (!componentJson) {
        componentJson = cJSON_GetObjectItem(root, "component");
    }

    if (componentJson && cJSON_IsObject(componentJson)) {
        // Wrapped format: { "root": {...} } or { "component": {...} }
        component = json_deserialize_component_recursive(componentJson);
    } else {
        // Unwrapped format: just component tree at root
        component = json_deserialize_component_recursive(root);
    }

    cJSON_Delete(root);
    return component;
}

/**
 * Read and deserialize IR component tree from JSON v2 file
 * @param filename Input file path
 * @return Deserialized component tree, or NULL on error
 */
IRComponent* ir_read_json_v2_file(const char* filename) {
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
    IRComponent* component = ir_deserialize_json_v2(content);
    free(content);

    return component;
}
