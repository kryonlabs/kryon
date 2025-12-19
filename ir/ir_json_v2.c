// JSON v2 Serialization - Complete IR Property Coverage
// This file implements full JSON serialization using cJSON

#define _GNU_SOURCE
#include "ir_serialization.h"
#include "ir_builder.h"
#include "ir_logic.h"
#include "ir_plugin.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// Component Definition Context (for expansion during deserialization)
// ============================================================================

typedef struct {
    char* name;
    cJSON* definition;  // Full component definition (props, state, template)
} ComponentDefEntry;

typedef struct {
    ComponentDefEntry* entries;
    int count;
    int capacity;
} ComponentDefContext;

static ComponentDefContext* component_def_context_create(void) {
    ComponentDefContext* ctx = calloc(1, sizeof(ComponentDefContext));
    if (!ctx) return NULL;
    ctx->capacity = 8;
    ctx->entries = calloc(ctx->capacity, sizeof(ComponentDefEntry));
    return ctx;
}

static void component_def_context_free(ComponentDefContext* ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->count; i++) {
        free(ctx->entries[i].name);
        // Note: definition JSON is part of root cJSON, freed separately
    }
    free(ctx->entries);
    free(ctx);
}

static void component_def_context_add(ComponentDefContext* ctx, const char* name, cJSON* definition) {
    if (!ctx || !name || !definition) return;
    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->entries = realloc(ctx->entries, ctx->capacity * sizeof(ComponentDefEntry));
    }
    ctx->entries[ctx->count].name = strdup(name);
    ctx->entries[ctx->count].definition = definition;
    ctx->count++;
}

static cJSON* component_def_context_find(ComponentDefContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->entries[i].name, name) == 0) {
            return ctx->entries[i].definition;
        }
    }
    return NULL;
}

// ============================================================================
// Text Expression Substitution for Component Expansion
// ============================================================================

// Simple state context for template expansion
typedef struct {
    char* names[16];
    int values[16];
    int count;
} StateContext;

static void state_context_add(StateContext* sc, const char* name, int value) {
    if (sc->count >= 16) return;
    sc->names[sc->count] = strdup(name);
    sc->values[sc->count] = value;
    sc->count++;
}

static int state_context_get(StateContext* sc, const char* name, int* out) {
    for (int i = 0; i < sc->count; i++) {
        if (strcmp(sc->names[i], name) == 0) {
            *out = sc->values[i];
            return 1;
        }
    }
    return 0;
}

static void state_context_free(StateContext* sc) {
    for (int i = 0; i < sc->count; i++) {
        free(sc->names[i]);
    }
}

// Substitute {{varname}} in text with values from state context
static char* substitute_text_expressions(const char* text, StateContext* sc) {
    if (!text || !sc) return strdup(text ? text : "");

    // Find {{...}} pattern
    const char* start = strstr(text, "{{");
    if (!start) return strdup(text);

    const char* end = strstr(start, "}}");
    if (!end) return strdup(text);

    // Extract variable name
    int nameLen = end - start - 2;
    if (nameLen <= 0 || nameLen > 64) return strdup(text);

    char varName[65];
    strncpy(varName, start + 2, nameLen);
    varName[nameLen] = '\0';

    // Look up value
    int value;
    if (!state_context_get(sc, varName, &value)) {
        return strdup(text);  // Variable not found, keep original
    }

    // Build result string
    char result[256];
    int prefixLen = start - text;
    strncpy(result, text, prefixLen);
    result[prefixLen] = '\0';

    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), "%d", value);
    strcat(result, valueStr);
    strcat(result, end + 2);  // Rest after }}

    return strdup(result);
}

// Deep clone JSON and substitute text expressions
// The skipSubstitute flag is used for keys like "text_expression" that should keep {{var}}
static cJSON* clone_and_substitute_json_impl(cJSON* json, StateContext* sc, bool skipSubstitute) {
    if (!json) return NULL;

    if (cJSON_IsString(json)) {
        if (skipSubstitute) {
            // Keep original value (for text_expression)
            return cJSON_CreateString(json->valuestring);
        }
        // Substitute text expressions in string values
        char* newText = substitute_text_expressions(json->valuestring, sc);
        cJSON* result = cJSON_CreateString(newText);
        free(newText);
        return result;
    }

    if (cJSON_IsNumber(json)) {
        return cJSON_CreateNumber(json->valuedouble);
    }

    if (cJSON_IsBool(json)) {
        return cJSON_CreateBool(cJSON_IsTrue(json));
    }

    if (cJSON_IsNull(json)) {
        return cJSON_CreateNull();
    }

    if (cJSON_IsArray(json)) {
        cJSON* result = cJSON_CreateArray();
        if (!result) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) in clone_and_substitute_json_impl\n");
            return NULL;
        }
        cJSON* item;
        cJSON_ArrayForEach(item, json) {
            cJSON_AddItemToArray(result, clone_and_substitute_json_impl(item, sc, false));
        }
        return result;
    }

    if (cJSON_IsObject(json)) {
        cJSON* result = cJSON_CreateObject();
        if (!result) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) in clone_and_substitute_json_impl\n");
            return NULL;
        }
        cJSON* item;

        // Check if this object has text_expression - if so, use it to compute text
        cJSON* textExpr = cJSON_GetObjectItem(json, "text_expression");

        cJSON_ArrayForEach(item, json) {
            // Don't substitute text_expression - preserve it for reactive updates
            bool skip = (item->string && strcmp(item->string, "text_expression") == 0);

            // If this is "text" and we have a text_expression, substitute using that
            if (item->string && strcmp(item->string, "text") == 0 &&
                textExpr && cJSON_IsString(textExpr)) {
                // Use text_expression for substitution instead of current text value
                char* newText = substitute_text_expressions(textExpr->valuestring, sc);
                cJSON_AddItemToObject(result, item->string, cJSON_CreateString(newText));
                free(newText);
            } else {
                cJSON_AddItemToObject(result, item->string, clone_and_substitute_json_impl(item, sc, skip));
            }
        }
        return result;
    }

    return NULL;
}

static cJSON* clone_and_substitute_json(cJSON* json, StateContext* sc) {
    return clone_and_substitute_json_impl(json, sc, false);
}

// Build state context from component definition and instance props
static StateContext build_state_from_def(cJSON* definition, cJSON* instance) {
    StateContext sc = {0};

    if (!definition) return sc;

    // Get props definitions and state definitions
    cJSON* propsDefs = cJSON_GetObjectItem(definition, "props");
    cJSON* stateDefs = cJSON_GetObjectItem(definition, "state");

    // First, get prop values from instance (with defaults from definition)
    if (propsDefs && cJSON_IsArray(propsDefs)) {
        cJSON* propDef;
        cJSON_ArrayForEach(propDef, propsDefs) {
            cJSON* nameItem = cJSON_GetObjectItem(propDef, "name");
            cJSON* defaultItem = cJSON_GetObjectItem(propDef, "default");
            if (!nameItem || !cJSON_IsString(nameItem)) continue;

            const char* propName = nameItem->valuestring;
            int propValue = 0;

            // Check if instance provides this prop
            cJSON* instanceProp = instance ? cJSON_GetObjectItem(instance, propName) : NULL;
            if (instanceProp && cJSON_IsNumber(instanceProp)) {
                propValue = instanceProp->valueint;
            } else if (defaultItem && cJSON_IsNumber(defaultItem)) {
                propValue = defaultItem->valueint;
            }

            state_context_add(&sc, propName, propValue);
        }
    }

    // Then, compute initial state values
    if (stateDefs && cJSON_IsArray(stateDefs)) {
        cJSON* stateDef;
        cJSON_ArrayForEach(stateDef, stateDefs) {
            cJSON* nameItem = cJSON_GetObjectItem(stateDef, "name");
            cJSON* initialItem = cJSON_GetObjectItem(stateDef, "initial");
            if (!nameItem || !cJSON_IsString(nameItem)) continue;

            const char* stateName = nameItem->valuestring;
            int stateValue = 0;

            // Handle initial value - can be a number, JSON string like {"var":"propName"}, or plain prop name
            if (initialItem) {
                if (cJSON_IsNumber(initialItem)) {
                    stateValue = initialItem->valueint;
                } else if (cJSON_IsString(initialItem)) {
                    const char* initialStr = initialItem->valuestring;
                    bool found = false;

                    // First try to parse as JSON {"var":"propName"} reference
                    cJSON* parsed = cJSON_Parse(initialStr);
                    if (parsed) {
                        cJSON* varRef = cJSON_GetObjectItem(parsed, "var");
                        if (varRef && cJSON_IsString(varRef)) {
                            // Look up the referenced prop
                            found = state_context_get(&sc, varRef->valuestring, &stateValue);
                        }
                        cJSON_Delete(parsed);
                    }

                    // If not valid JSON or var not found, try as plain prop name
                    if (!found && initialStr && initialStr[0] != '{') {
                        state_context_get(&sc, initialStr, &stateValue);
                    }
                }
            }

            state_context_add(&sc, stateName, stateValue);
        }
    }

    return sc;
}

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

    // Border (width, color, radius - any can be set independently)
    if (style->border.width > 0 || style->border.radius > 0) {
        cJSON* border = cJSON_CreateObject();
        if (!border) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for border in json_style_to_json\n");
            return;
        }

        if (style->border.width > 0) {
            cJSON_AddNumberToObject(border, "width", style->border.width);
            char* colorStr = json_color_to_string(style->border.color);
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
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for transform in json_style_to_json\n");
            return;
        }

        if (fabs(style->transform.translate_x) > 0.001f || fabs(style->transform.translate_y) > 0.001f) {
            cJSON* translate = cJSON_CreateArray();
            if (!translate) {
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for translate in json_style_to_json\n");
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
                fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for scale in json_style_to_json\n");
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

    // Basic properties
    cJSON_AddNumberToObject(obj, "id", component->id);

    const char* typeStr = ir_component_type_to_string(component->type);
    cJSON_AddStringToObject(obj, "type", typeStr);

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
        char colorBuf[10];
        snprintf(colorBuf, sizeof(colorBuf), "#%02x%02x%02x",
                 style->border_color.data.r, style->border_color.data.g, style->border_color.data.b);
        cJSON_AddStringToObject(tableConfig, "borderColor", colorBuf);

        // Header background
        snprintf(colorBuf, sizeof(colorBuf), "#%02x%02x%02x",
                 style->header_background.data.r, style->header_background.data.g, style->header_background.data.b);
        cJSON_AddStringToObject(tableConfig, "headerBackground", colorBuf);

        // Row backgrounds (for striping)
        snprintf(colorBuf, sizeof(colorBuf), "#%02x%02x%02x",
                 style->even_row_background.data.r, style->even_row_background.data.g, style->even_row_background.data.b);
        cJSON_AddStringToObject(tableConfig, "evenRowBackground", colorBuf);
        snprintf(colorBuf, sizeof(colorBuf), "#%02x%02x%02x",
                 style->odd_row_background.data.r, style->odd_row_background.data.g, style->odd_row_background.data.b);
        cJSON_AddStringToObject(tableConfig, "oddRowBackground", colorBuf);

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
    }

    // Serialize flowchart data
    if (component->type == IR_COMPONENT_FLOWCHART && component->custom_data) {
        IRFlowchartState* state = (IRFlowchartState*)component->custom_data;
        cJSON* flowchartObj = cJSON_CreateObject();
        if (!flowchartObj) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for flowchart config\n");
            return obj;
        }

        // Direction
        cJSON_AddStringToObject(flowchartObj, "direction", ir_flowchart_direction_to_string(state->direction));

        // Layout parameters
        cJSON_AddNumberToObject(flowchartObj, "nodeSpacing", state->node_spacing);
        cJSON_AddNumberToObject(flowchartObj, "rankSpacing", state->rank_spacing);
        cJSON_AddNumberToObject(flowchartObj, "subgraphPadding", state->subgraph_padding);

        cJSON_AddItemToObject(obj, "flowchart_config", flowchartObj);
    }

    // Serialize flowchart node data
    if (component->type == IR_COMPONENT_FLOWCHART_NODE && component->custom_data) {
        IRFlowchartNodeData* nodeData = (IRFlowchartNodeData*)component->custom_data;
        cJSON* nodeObj = cJSON_CreateObject();
        if (!nodeObj) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for flowchart node\n");
            return obj;
        }

        if (nodeData->node_id) {
            cJSON_AddStringToObject(nodeObj, "id", nodeData->node_id);
        }
        cJSON_AddStringToObject(nodeObj, "shape", ir_flowchart_shape_to_string(nodeData->shape));
        if (nodeData->label) {
            cJSON_AddStringToObject(nodeObj, "label", nodeData->label);
        }

        // Colors (as hex strings)
        char colorBuf[12];
        snprintf(colorBuf, sizeof(colorBuf), "#%08x", nodeData->fill_color);
        cJSON_AddStringToObject(nodeObj, "fillColor", colorBuf);
        snprintf(colorBuf, sizeof(colorBuf), "#%08x", nodeData->stroke_color);
        cJSON_AddStringToObject(nodeObj, "strokeColor", colorBuf);
        cJSON_AddNumberToObject(nodeObj, "strokeWidth", nodeData->stroke_width);

        if (nodeData->subgraph_id) {
            cJSON_AddStringToObject(nodeObj, "subgraphId", nodeData->subgraph_id);
        }

        cJSON_AddItemToObject(obj, "flowchart_node", nodeObj);
    }

    // Serialize flowchart edge data
    if (component->type == IR_COMPONENT_FLOWCHART_EDGE && component->custom_data) {
        IRFlowchartEdgeData* edgeData = (IRFlowchartEdgeData*)component->custom_data;
        cJSON* edgeObj = cJSON_CreateObject();
        if (!edgeObj) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for flowchart edge\n");
            return obj;
        }

        if (edgeData->from_id) {
            cJSON_AddStringToObject(edgeObj, "from", edgeData->from_id);
        }
        if (edgeData->to_id) {
            cJSON_AddStringToObject(edgeObj, "to", edgeData->to_id);
        }
        cJSON_AddStringToObject(edgeObj, "type", ir_flowchart_edge_type_to_string(edgeData->type));
        cJSON_AddStringToObject(edgeObj, "startMarker", ir_flowchart_marker_to_string(edgeData->start_marker));
        cJSON_AddStringToObject(edgeObj, "endMarker", ir_flowchart_marker_to_string(edgeData->end_marker));
        if (edgeData->label) {
            cJSON_AddStringToObject(edgeObj, "label", edgeData->label);
        }

        cJSON_AddItemToObject(obj, "flowchart_edge", edgeObj);
    }

    // Serialize flowchart subgraph data
    if (component->type == IR_COMPONENT_FLOWCHART_SUBGRAPH && component->custom_data) {
        IRFlowchartSubgraphData* subgraphData = (IRFlowchartSubgraphData*)component->custom_data;
        cJSON* subgraphObj = cJSON_CreateObject();
        if (!subgraphObj) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for flowchart subgraph\n");
            return obj;
        }

        if (subgraphData->subgraph_id) {
            cJSON_AddStringToObject(subgraphObj, "id", subgraphData->subgraph_id);
        }
        if (subgraphData->title) {
            cJSON_AddStringToObject(subgraphObj, "title", subgraphData->title);
        }

        // Colors (as hex strings)
        char colorBuf[12];
        snprintf(colorBuf, sizeof(colorBuf), "#%08x", subgraphData->background_color);
        cJSON_AddStringToObject(subgraphObj, "backgroundColor", colorBuf);
        snprintf(colorBuf, sizeof(colorBuf), "#%08x", subgraphData->border_color);
        cJSON_AddStringToObject(subgraphObj, "borderColor", colorBuf);

        cJSON_AddItemToObject(obj, "flowchart_subgraph", subgraphObj);
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

    // Serialize animations (stored in style)
    if (component->style && component->style->animation_count > 0) {
        IRAnimation* anim = component->style->animations[0];
        if (anim && anim->name) {
            char animStr[128];
            // Format: "name(duration, iterations)" or "name(duration)"
            if (anim->iteration_count != 1) {
                snprintf(animStr, sizeof(animStr), "%s(%.1f, %d)",
                         anim->name, anim->duration, anim->iteration_count);
            } else {
                snprintf(animStr, sizeof(animStr), "%s(%.1f)", anim->name, anim->duration);
            }
            cJSON_AddStringToObject(obj, "animation", animStr);
        }
    }

    // Children
    if (component->child_count > 0) {
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
    if (!arr) {
        fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for component definitions\n");
        return NULL;
    }

    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        IRComponentDefinition* def = &manifest->component_defs[i];
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
    fprintf(stderr, "[DEBUG] json_serialize_reactive_manifest called\n");
    fflush(stderr);

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
        printf("[ir_json_v2] SERIALIZATION FUNCTION CALLED - %d conditionals\n", manifest->conditional_count);
        fflush(stdout);
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

    fprintf(stderr, "=== ir_serialize_json_v2: Starting serialization...\n");
    fflush(stderr);

    cJSON* json = json_serialize_component_recursive(root);
    if (!json) {
        fprintf(stderr, "=== ir_serialize_json_v2: Serialization failed!\n");
        fflush(stderr);
        return NULL;
    }

    fprintf(stderr, "=== ir_serialize_json_v2: Serialization succeeded, calling cJSON_Print...\n");
    fflush(stderr);

    char* jsonStr = cJSON_Print(json);  // Pretty-printed JSON

    fprintf(stderr, "=== ir_serialize_json_v2: cJSON_Print returned, length=%zu\n", jsonStr ? strlen(jsonStr) : 0);
    fprintf(stderr, "=== ir_serialize_json_v2: Calling cJSON_Delete... (THIS MIGHT CRASH)\n");
    fflush(stderr);

    cJSON_Delete(json);

    fprintf(stderr, "=== ir_serialize_json_v2: cJSON_Delete succeeded\n");
    fflush(stderr);

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

    // Scan and add plugin requirements
    uint32_t plugin_count = 0;
    char** required_plugins = ir_plugin_scan_requirements(root, &plugin_count);
    if (required_plugins && plugin_count > 0) {
        cJSON* pluginsArray = cJSON_CreateArray();
        if (!pluginsArray) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for plugins array\n");
            ir_plugin_free_requirements(required_plugins, plugin_count);
            cJSON_Delete(wrapper);
            return NULL;
        }
        for (uint32_t i = 0; i < plugin_count; i++) {
            cJSON_AddItemToArray(pluginsArray, cJSON_CreateString(required_plugins[i]));
        }
        cJSON_AddItemToObject(wrapper, "required_plugins", pluginsArray);

        // Clean up
        ir_plugin_free_requirements(required_plugins, plugin_count);
    }

    // Add sources if present (for round-trip preservation)
    if (manifest && manifest->source_count > 0) {
        cJSON* sources = cJSON_CreateObject();
        if (!sources) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for sources object\n");
            cJSON_Delete(wrapper);
            return NULL;
        }
        for (uint32_t i = 0; i < manifest->source_count; i++) {
            if (manifest->sources[i].lang && manifest->sources[i].code) {
                cJSON_AddStringToObject(sources, manifest->sources[i].lang, manifest->sources[i].code);
            }
        }
        cJSON_AddItemToObject(wrapper, "sources", sources);
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

    // Scan and add plugin requirements
    uint32_t plugin_count = 0;
    char** required_plugins = ir_plugin_scan_requirements(root, &plugin_count);
    if (required_plugins && plugin_count > 0) {
        cJSON* pluginsArray = cJSON_CreateArray();
        if (!pluginsArray) {
            fprintf(stderr, "ERROR: cJSON_CreateArray failed (OOM) for plugins array\n");
            ir_plugin_free_requirements(required_plugins, plugin_count);
            cJSON_Delete(wrapper);
            return NULL;
        }
        for (uint32_t i = 0; i < plugin_count; i++) {
            cJSON_AddItemToArray(pluginsArray, cJSON_CreateString(required_plugins[i]));
        }
        cJSON_AddItemToObject(wrapper, "required_plugins", pluginsArray);

        // Clean up
        ir_plugin_free_requirements(required_plugins, plugin_count);
    }

    // Add sources if present (for round-trip preservation)
    if (manifest && manifest->source_count > 0) {
        cJSON* sources = cJSON_CreateObject();
        if (!sources) {
            fprintf(stderr, "ERROR: cJSON_CreateObject failed (OOM) for sources object\n");
            cJSON_Delete(wrapper);
            return NULL;
        }
        for (uint32_t i = 0; i < manifest->source_count; i++) {
            if (manifest->sources[i].lang && manifest->sources[i].code) {
                cJSON_AddStringToObject(sources, manifest->sources[i].lang, manifest->sources[i].code);
            }
        }
        cJSON_AddItemToObject(wrapper, "sources", sources);
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
 * Parse color string into IRColor
 * Supports: "#rrggbb", "#rrggbbaa", "#rgb", "#rgba", "transparent", "var(id)",
 *           and named colors ("yellow", "red", "blue", etc.)
 */
// Forward declaration
static IRColor json_parse_color(const char* str);

// Convert IRColor to uint32_t RGBA format (for tab colors)
static uint32_t json_parse_color_rgba(const char* str) {
    IRColor color = json_parse_color(str);
    if (color.type == IR_COLOR_SOLID) {
        return (color.data.r << 24) | (color.data.g << 16) | (color.data.b << 8) | color.data.a;
    }
    return 0;  // Default to transparent
}

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

    // Fallback: try named color lookup (e.g., "yellow", "red", "blue")
    return ir_color_named(str);
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

    // Absolute positioning
    if ((item = cJSON_GetObjectItem(obj, "position")) != NULL && cJSON_IsString(item)) {
        if (strcmp(item->valuestring, "absolute") == 0) {
            style->position_mode = IR_POSITION_ABSOLUTE;
        } else if (strcmp(item->valuestring, "fixed") == 0) {
            style->position_mode = IR_POSITION_FIXED;
        } else {
            style->position_mode = IR_POSITION_RELATIVE;
        }
    }
    if ((item = cJSON_GetObjectItem(obj, "left")) != NULL && cJSON_IsNumber(item)) {
        style->absolute_x = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "top")) != NULL && cJSON_IsNumber(item)) {
        style->absolute_y = (float)item->valuedouble;
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
    if ((item = cJSON_GetObjectItem(obj, "letterSpacing")) != NULL) {
        if (cJSON_IsNumber(item)) {
            style->font.letter_spacing = (float)item->valuedouble;
        } else if (cJSON_IsObject(item)) {
            // Handle expression objects like {op: "neg", expr: 1.0}
            cJSON* op = cJSON_GetObjectItem(item, "op");
            cJSON* expr = cJSON_GetObjectItem(item, "expr");
            if (op && cJSON_IsString(op) && expr && cJSON_IsNumber(expr)) {
                float value = (float)expr->valuedouble;
                if (strcmp(op->valuestring, "neg") == 0) {
                    style->font.letter_spacing = -value;
                } else {
                    style->font.letter_spacing = value;
                }
            }
        }
    }
    if ((item = cJSON_GetObjectItem(obj, "wordSpacing")) != NULL) {
        if (cJSON_IsNumber(item)) {
            style->font.word_spacing = (float)item->valuedouble;
        } else if (cJSON_IsObject(item)) {
            // Handle expression objects like {op: "neg", expr: 1.0}
            cJSON* op = cJSON_GetObjectItem(item, "op");
            cJSON* expr = cJSON_GetObjectItem(item, "expr");
            if (op && cJSON_IsString(op) && expr && cJSON_IsNumber(expr)) {
                float value = (float)expr->valuedouble;
                if (strcmp(op->valuestring, "neg") == 0) {
                    style->font.word_spacing = -value;
                } else {
                    style->font.word_spacing = value;
                }
            }
        }
    }
    if ((item = cJSON_GetObjectItem(obj, "textDecoration")) != NULL && cJSON_IsString(item)) {
        const char* decoration = item->valuestring;
        style->font.decoration = IR_TEXT_DECORATION_NONE;

        // Parse decoration string (can be space-separated list)
        if (strstr(decoration, "underline")) {
            style->font.decoration |= IR_TEXT_DECORATION_UNDERLINE;
        }
        if (strstr(decoration, "overline")) {
            style->font.decoration |= IR_TEXT_DECORATION_OVERLINE;
        }
        if (strstr(decoration, "line-through")) {
            style->font.decoration |= IR_TEXT_DECORATION_LINE_THROUGH;
        }
        if (strcmp(decoration, "none") == 0) {
            style->font.decoration = IR_TEXT_DECORATION_NONE;
        }
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
 * Made public for use by HTML parser
 */
IRComponentType ir_string_to_component_type(const char* str) {
    if (!str) return IR_COMPONENT_CONTAINER;
    // CamelCase variants (from .kir files)
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
    if (strcmp(str, "TabGroup") == 0) return IR_COMPONENT_TAB_GROUP;
    if (strcmp(str, "TabBar") == 0) return IR_COMPONENT_TAB_BAR;
    if (strcmp(str, "Tab") == 0) return IR_COMPONENT_TAB;
    if (strcmp(str, "TabContent") == 0) return IR_COMPONENT_TAB_CONTENT;
    if (strcmp(str, "TabPanel") == 0) return IR_COMPONENT_TAB_PANEL;
    // UPPERCASE variants (from HTML transpiler data-ir-type attributes)
    if (strcmp(str, "CONTAINER") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(str, "ROW") == 0) return IR_COMPONENT_ROW;
    if (strcmp(str, "COLUMN") == 0) return IR_COMPONENT_COLUMN;
    if (strcmp(str, "CENTER") == 0) return IR_COMPONENT_CENTER;
    if (strcmp(str, "TEXT") == 0) return IR_COMPONENT_TEXT;
    if (strcmp(str, "BUTTON") == 0) return IR_COMPONENT_BUTTON;
    if (strcmp(str, "INPUT") == 0) return IR_COMPONENT_INPUT;
    if (strcmp(str, "CHECKBOX") == 0) return IR_COMPONENT_CHECKBOX;
    if (strcmp(str, "IMAGE") == 0) return IR_COMPONENT_IMAGE;
    if (strcmp(str, "CANVAS") == 0) return IR_COMPONENT_CANVAS;
    if (strcmp(str, "DROPDOWN") == 0) return IR_COMPONENT_DROPDOWN;
    if (strcmp(str, "MARKDOWN") == 0) return IR_COMPONENT_MARKDOWN;
    if (strcmp(str, "TAB_GROUP") == 0) return IR_COMPONENT_TAB_GROUP;
    if (strcmp(str, "TAB_BAR") == 0) return IR_COMPONENT_TAB_BAR;
    if (strcmp(str, "TAB") == 0) return IR_COMPONENT_TAB;
    if (strcmp(str, "TAB_CONTENT") == 0) return IR_COMPONENT_TAB_CONTENT;
    if (strcmp(str, "TAB_PANEL") == 0) return IR_COMPONENT_TAB_PANEL;
    // Table components
    if (strcmp(str, "Table") == 0) return IR_COMPONENT_TABLE;
    if (strcmp(str, "TableHead") == 0) return IR_COMPONENT_TABLE_HEAD;
    if (strcmp(str, "TableBody") == 0) return IR_COMPONENT_TABLE_BODY;
    if (strcmp(str, "TableFoot") == 0) return IR_COMPONENT_TABLE_FOOT;
    if (strcmp(str, "TableRow") == 0 || strcmp(str, "Tr") == 0) return IR_COMPONENT_TABLE_ROW;
    if (strcmp(str, "TableCell") == 0 || strcmp(str, "Td") == 0) return IR_COMPONENT_TABLE_CELL;
    if (strcmp(str, "TableHeaderCell") == 0 || strcmp(str, "Th") == 0) return IR_COMPONENT_TABLE_HEADER_CELL;
    // Flowchart components
    if (strcmp(str, "Flowchart") == 0) return IR_COMPONENT_FLOWCHART;
    if (strcmp(str, "FlowchartNode") == 0) return IR_COMPONENT_FLOWCHART_NODE;
    if (strcmp(str, "FlowchartEdge") == 0) return IR_COMPONENT_FLOWCHART_EDGE;
    if (strcmp(str, "FlowchartSubgraph") == 0) return IR_COMPONENT_FLOWCHART_SUBGRAPH;
    if (strcmp(str, "FlowchartLabel") == 0) return IR_COMPONENT_FLOWCHART_LABEL;
    // Edge type strings (used by transpiler as component types)
    if (strcmp(str, "arrow") == 0) return IR_COMPONENT_FLOWCHART_EDGE;
    if (strcmp(str, "dotted") == 0) return IR_COMPONENT_FLOWCHART_EDGE;
    if (strcmp(str, "open") == 0) return IR_COMPONENT_FLOWCHART_EDGE;
    if (strcmp(str, "thick") == 0) return IR_COMPONENT_FLOWCHART_EDGE;
    if (strcmp(str, "bidirectional") == 0) return IR_COMPONENT_FLOWCHART_EDGE;
    // Markdown components
    if (strcmp(str, "Heading") == 0) return IR_COMPONENT_HEADING;
    if (strcmp(str, "Paragraph") == 0) return IR_COMPONENT_PARAGRAPH;
    if (strcmp(str, "Blockquote") == 0) return IR_COMPONENT_BLOCKQUOTE;
    if (strcmp(str, "CodeBlock") == 0) return IR_COMPONENT_CODE_BLOCK;
    if (strcmp(str, "HorizontalRule") == 0) return IR_COMPONENT_HORIZONTAL_RULE;
    if (strcmp(str, "List") == 0) return IR_COMPONENT_LIST;
    if (strcmp(str, "ListItem") == 0) return IR_COMPONENT_LIST_ITEM;
    if (strcmp(str, "Link") == 0) return IR_COMPONENT_LINK;
    if (strcmp(str, "Custom") == 0) return IR_COMPONENT_CUSTOM;
    return IR_COMPONENT_CONTAINER;
}

/**
 * Parse animation string and apply to component
 * Formats: "pulse(duration, iterations)", "fadeInOut(duration, iterations)", "slideInLeft(duration)"
 */
static void apply_animation_from_string(IRComponent* component, const char* animSpec) {
    if (!component || !animSpec) return;

    char funcName[64] = {0};
    float param1 = 0, param2 = 0;
    int paramCount = 0;

    // Parse "funcName(p1, p2)" or "funcName(p1)"
    if (sscanf(animSpec, "%63[^(](%f, %f)", funcName, &param1, &param2) == 3) {
        paramCount = 2;
    } else if (sscanf(animSpec, "%63[^(](%f)", funcName, &param1) == 2) {
        paramCount = 1;
    } else {
        return;  // Invalid format
    }

    IRAnimation* anim = NULL;

    if (strcmp(funcName, "pulse") == 0) {
        anim = ir_animation_pulse(param1);
        if (anim && paramCount >= 2) ir_animation_set_iterations(anim, (int32_t)param2);
    } else if (strcmp(funcName, "fadeInOut") == 0) {
        anim = ir_animation_fade_in_out(param1);
        if (anim && paramCount >= 2) ir_animation_set_iterations(anim, (int32_t)param2);
    } else if (strcmp(funcName, "slideInLeft") == 0) {
        anim = ir_animation_slide_in_left(param1);
    }

    if (anim) {
        ir_component_add_animation(component, anim);
    }
}

// Forward declaration for context-aware deserialization
static IRComponent* json_deserialize_component_with_context(cJSON* json, ComponentDefContext* ctx);

/**
 * Recursively deserialize component from JSON (legacy wrapper)
 */
static IRComponent* json_deserialize_component_recursive(cJSON* json) {
    return json_deserialize_component_with_context(json, NULL);
}

/**
 * Helper: Set owner_instance_id recursively on all components in subtree
 */
static void set_owner_instance_recursive(IRComponent* comp, uint32_t owner_id) {
    if (!comp) return;
    comp->owner_instance_id = owner_id;
    for (uint32_t i = 0; i < comp->child_count; i++) {
        set_owner_instance_recursive(comp->children[i], owner_id);
    }
}

// Global counter for generating unique component IDs during expansion
static uint32_t g_next_expansion_id = 1000;  // Start high to avoid conflicts

/**
 * Helper: Remap component IDs recursively to make them unique
 */
static void remap_ids_recursive(IRComponent* comp) {
    if (!comp) return;
    comp->id = g_next_expansion_id++;
    for (uint32_t i = 0; i < comp->child_count; i++) {
        remap_ids_recursive(comp->children[i]);
    }
}

/**
 * Recursively deserialize component from JSON with component definition expansion
 */
static IRComponent* json_deserialize_component_with_context(cJSON* json, ComponentDefContext* ctx) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    // Check if this is a custom component that needs expansion
    cJSON* typeItem = cJSON_GetObjectItem(json, "type");
    if (typeItem && cJSON_IsString(typeItem) && ctx) {
        const char* typeName = typeItem->valuestring;
        cJSON* definition = component_def_context_find(ctx, typeName);
        if (definition) {
            // Found a component definition - expand the template
            cJSON* templateJson = cJSON_GetObjectItem(definition, "template");
            if (templateJson) {
                // Build state context from definition and instance props
                StateContext sc = build_state_from_def(definition, json);

                // Clone template and substitute text expressions
                cJSON* expandedTemplate = clone_and_substitute_json(templateJson, &sc);
                state_context_free(&sc);

                if (expandedTemplate) {
                    // Deserialize the expanded template
                    IRComponent* expanded = json_deserialize_component_with_context(expandedTemplate, ctx);
                    cJSON_Delete(expandedTemplate);  // Free the cloned JSON

                    if (expanded) {
                        // Copy ID from the instance to track it
                        cJSON* idItem = cJSON_GetObjectItem(json, "id");
                        if (idItem && cJSON_IsNumber(idItem)) {
                            uint32_t instanceId = (uint32_t)idItem->valueint;
                            // Remap all IDs to make them unique across instances
                            remap_ids_recursive(expanded);
                            // But use the original instance ID for the root
                            expanded->id = instanceId;
                            // Set owner_instance_id on entire subtree for state isolation
                            set_owner_instance_recursive(expanded, instanceId);
                        }
                        return expanded;
                    }
                }
            }
        }
    }

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

    // Text content (check both "text" and "label" properties)
    if ((item = cJSON_GetObjectItem(json, "text")) != NULL) {
        if (cJSON_IsString(item)) {
            // Simple string text
            component->text_content = strdup(item->valuestring);
        } else if (cJSON_IsObject(item)) {
            // Text is an expression object - serialize to JSON string for text_expression
            char* expr_str = cJSON_PrintUnformatted(item);
            if (expr_str) {
                component->text_expression = expr_str;
                printf("[deserialize] Text field is expression object, stored in text_expression\n");
            }
        }
    } else if ((item = cJSON_GetObjectItem(json, "label")) != NULL && cJSON_IsString(item)) {
        // "label" is an alias for "text" (used by Checkbox, Button, etc.)
        component->text_content = strdup(item->valuestring);
    }

    // Text expression (reactive text template) - can also be specified directly
    if ((item = cJSON_GetObjectItem(json, "text_expression")) != NULL && cJSON_IsString(item)) {
        component->text_expression = strdup(item->valuestring);
    }

    // Deserialize style and layout
    json_deserialize_style(json, component->style);
    json_deserialize_layout(json, component->layout);

    // Parse animation property
    if ((item = cJSON_GetObjectItem(json, "animation")) != NULL && cJSON_IsString(item)) {
        apply_animation_from_string(component, item->valuestring);
    }

    // Parse checkbox state
    if (component->type == IR_COMPONENT_CHECKBOX) {
        if ((item = cJSON_GetObjectItem(json, "checked")) != NULL && cJSON_IsBool(item)) {
            bool is_checked = cJSON_IsTrue(item);
            ir_set_custom_data(component, is_checked ? "checked" : "unchecked");
        }
    }

    // Parse Input value binding (e.g., "{{newTodo}}")
    if (component->type == IR_COMPONENT_INPUT) {
        if ((item = cJSON_GetObjectItem(json, "value")) != NULL && cJSON_IsString(item)) {
            // Store the binding expression (e.g., "{{newTodo}}")
            component->text_expression = strdup(item->valuestring);
            printf("[deserialize] Input value binding: %s\n", item->valuestring);
        }
    }

    // Parse Image src and alt
    if (component->type == IR_COMPONENT_IMAGE) {
        // Parse src (also accept "source" as alias)
        item = cJSON_GetObjectItem(json, "src");
        if (!item) item = cJSON_GetObjectItem(json, "source");
        if (item && cJSON_IsString(item)) {
            ir_set_custom_data(component, item->valuestring);
        }
        // Parse alt text
        if ((item = cJSON_GetObjectItem(json, "alt")) != NULL && cJSON_IsString(item)) {
            component->text_content = strdup(item->valuestring);
        }
    }

    // Parse dropdown state
    if (component->type == IR_COMPONENT_DROPDOWN) {
        IRDropdownState* state = (IRDropdownState*)calloc(1, sizeof(IRDropdownState));
        if (state) {
            // Check for nested dropdown_state object (new format) or fall back to top-level fields (legacy)
            cJSON* dropdownStateObj = cJSON_GetObjectItem(json, "dropdown_state");
            cJSON* sourceObj = (dropdownStateObj && cJSON_IsObject(dropdownStateObj)) ? dropdownStateObj : json;

            // Parse placeholder
            cJSON* placeholderItem = cJSON_GetObjectItem(sourceObj, "placeholder");
            if (placeholderItem && cJSON_IsString(placeholderItem)) {
                state->placeholder = strdup(placeholderItem->valuestring);
            } else {
                state->placeholder = strdup("Select...");
            }

            // Parse selectedIndex
            cJSON* selectedItem = cJSON_GetObjectItem(sourceObj, "selectedIndex");
            if (selectedItem && cJSON_IsNumber(selectedItem)) {
                state->selected_index = selectedItem->valueint;
            } else {
                state->selected_index = -1;
            }

            // Parse options array
            cJSON* optionsItem = cJSON_GetObjectItem(sourceObj, "options");
            if (optionsItem && cJSON_IsArray(optionsItem)) {
                int optionCount = cJSON_GetArraySize(optionsItem);
                state->option_count = optionCount;
                state->options = (char**)calloc(optionCount, sizeof(char*));
                if (state->options) {
                    for (int i = 0; i < optionCount; i++) {
                        cJSON* opt = cJSON_GetArrayItem(optionsItem, i);
                        if (opt && cJSON_IsString(opt)) {
                            state->options[i] = strdup(opt->valuestring);
                        }
                    }
                }
            }

            // Parse isOpen (from new format)
            cJSON* isOpenItem = cJSON_GetObjectItem(sourceObj, "isOpen");
            state->is_open = (isOpenItem && cJSON_IsBool(isOpenItem)) ? cJSON_IsTrue(isOpenItem) : false;

            state->hovered_index = -1;
            component->custom_data = (char*)state;
        }
    }

    // Parse tab-specific data
    if (component->type == IR_COMPONENT_TAB_GROUP || component->type == IR_COMPONENT_TAB_BAR ||
        component->type == IR_COMPONENT_TAB || component->type == IR_COMPONENT_TAB_CONTENT ||
        component->type == IR_COMPONENT_TAB_PANEL) {

        IRTabData* tab_data = (IRTabData*)calloc(1, sizeof(IRTabData));
        if (tab_data) {
            // Parse title (for Tab components)
            if ((item = cJSON_GetObjectItem(json, "title")) != NULL && cJSON_IsString(item)) {
                tab_data->title = strdup(item->valuestring);
            }

            // Parse reorderable (for TabBar)
            if ((item = cJSON_GetObjectItem(json, "reorderable")) != NULL && cJSON_IsBool(item)) {
                tab_data->reorderable = cJSON_IsTrue(item);
            }

            // Parse selectedIndex (for TabGroup)
            if ((item = cJSON_GetObjectItem(json, "selectedIndex")) != NULL && cJSON_IsNumber(item)) {
                tab_data->selected_index = item->valueint;
            } else {
                tab_data->selected_index = 0;  // Default to first tab
            }

            // Parse activeBackground (for Tab)
            if ((item = cJSON_GetObjectItem(json, "activeBackground")) != NULL && cJSON_IsString(item)) {
                tab_data->active_background = json_parse_color_rgba(item->valuestring);
            }

            // Parse textColor (for Tab)
            if ((item = cJSON_GetObjectItem(json, "textColor")) != NULL && cJSON_IsString(item)) {
                tab_data->text_color = json_parse_color_rgba(item->valuestring);
            }

            // Parse activeTextColor (for Tab)
            if ((item = cJSON_GetObjectItem(json, "activeTextColor")) != NULL && cJSON_IsString(item)) {
                tab_data->active_text_color = json_parse_color_rgba(item->valuestring);
            }

            component->tab_data = tab_data;
        }
    }

    // Set default properties for TabBar (horizontal layout + 100% width)
    if (component->type == IR_COMPONENT_TAB_BAR) {
        if (!component->layout) {
            component->layout = (IRLayout*)calloc(1, sizeof(IRLayout));
        }
        // Only set default direction if not already set from JSON
        if (component->layout && component->layout->flex.direction == 0) {
            // Check if direction was explicitly set to column, or just defaulted to 0
            // If no flexDirection was in JSON, default to row (horizontal)
            component->layout->flex.direction = 1;  // Row (horizontal)
            if (getenv("KRYON_TRACE_LAYOUT")) {
                fprintf(stderr, "[TabBar] Applied default flexDirection=1 (row)\n");
            }
        } else if (component->layout && getenv("KRYON_TRACE_LAYOUT")) {
            fprintf(stderr, "[TabBar] Already has flexDirection=%d from JSON\n", component->layout->flex.direction);
        }

        // Set default width to 100% to fill parent TabGroup
        bool style_created = false;
        if (!component->style) {
            component->style = (IRStyle*)calloc(1, sizeof(IRStyle));
            style_created = true;
        }
        // Apply default width if: style was just created (width.type=0=PX with value=0) OR explicitly AUTO
        if (component->style && (style_created || component->style->width.type == IR_DIMENSION_AUTO ||
            (component->style->width.type == IR_DIMENSION_PX && component->style->width.value == 0))) {
            component->style->width.type = IR_DIMENSION_PERCENT;
            component->style->width.value = 100.0f;
        }
    }

    // Set default flex properties for Tab components (grow and shrink to fill TabBar)
    if (component->type == IR_COMPONENT_TAB) {
        if (!component->layout) {
            component->layout = (IRLayout*)calloc(1, sizeof(IRLayout));
        }
        if (component->layout) {
            component->layout->flex.grow = 1;
            component->layout->flex.shrink = 1;
        }
    }

    // Parse table configuration (for Table components)
    if (component->type == IR_COMPONENT_TABLE) {
        cJSON* tableConfig = cJSON_GetObjectItem(json, "table_config");
        IRTableState* state = ir_table_create_state();

        if (state && tableConfig && cJSON_IsObject(tableConfig)) {
            // Parse columns
            cJSON* columnsArr = cJSON_GetObjectItem(tableConfig, "columns");
            if (columnsArr && cJSON_IsArray(columnsArr)) {
                int colCount = cJSON_GetArraySize(columnsArr);
                for (int i = 0; i < colCount; i++) {
                    cJSON* colObj = cJSON_GetArrayItem(columnsArr, i);
                    if (!colObj || !cJSON_IsObject(colObj)) continue;

                    IRTableColumnDef col = {0};
                    col.auto_size = true;  // Default

                    // Parse width
                    cJSON* widthItem = cJSON_GetObjectItem(colObj, "width");
                    if (widthItem && cJSON_IsString(widthItem)) {
                        col.width = json_parse_dimension(widthItem->valuestring);
                        if (col.width.type != IR_DIMENSION_AUTO) {
                            col.auto_size = false;
                        }
                    }

                    // Parse minWidth
                    cJSON* minWidthItem = cJSON_GetObjectItem(colObj, "minWidth");
                    if (minWidthItem && cJSON_IsString(minWidthItem)) {
                        col.min_width = json_parse_dimension(minWidthItem->valuestring);
                    }

                    // Parse maxWidth
                    cJSON* maxWidthItem = cJSON_GetObjectItem(colObj, "maxWidth");
                    if (maxWidthItem && cJSON_IsString(maxWidthItem)) {
                        col.max_width = json_parse_dimension(maxWidthItem->valuestring);
                    }

                    // Parse alignment
                    cJSON* alignItem = cJSON_GetObjectItem(colObj, "alignment");
                    if (alignItem && cJSON_IsString(alignItem)) {
                        if (strcmp(alignItem->valuestring, "center") == 0) {
                            col.alignment = IR_ALIGNMENT_CENTER;
                        } else if (strcmp(alignItem->valuestring, "end") == 0 ||
                                   strcmp(alignItem->valuestring, "right") == 0) {
                            col.alignment = IR_ALIGNMENT_END;
                        } else {
                            col.alignment = IR_ALIGNMENT_START;
                        }
                    }

                    // Parse autoSize
                    cJSON* autoSizeItem = cJSON_GetObjectItem(colObj, "autoSize");
                    if (autoSizeItem && cJSON_IsBool(autoSizeItem)) {
                        col.auto_size = cJSON_IsTrue(autoSizeItem);
                    }

                    ir_table_add_column(state, col);
                }
            }

            // Parse styling
            cJSON* borderColorItem = cJSON_GetObjectItem(tableConfig, "borderColor");
            if (borderColorItem && cJSON_IsString(borderColorItem)) {
                state->style.border_color = json_parse_color(borderColorItem->valuestring);
            }

            cJSON* headerBgItem = cJSON_GetObjectItem(tableConfig, "headerBackground");
            if (headerBgItem && cJSON_IsString(headerBgItem)) {
                state->style.header_background = json_parse_color(headerBgItem->valuestring);
            }

            cJSON* evenBgItem = cJSON_GetObjectItem(tableConfig, "evenRowBackground");
            if (evenBgItem && cJSON_IsString(evenBgItem)) {
                state->style.even_row_background = json_parse_color(evenBgItem->valuestring);
            }

            cJSON* oddBgItem = cJSON_GetObjectItem(tableConfig, "oddRowBackground");
            if (oddBgItem && cJSON_IsString(oddBgItem)) {
                state->style.odd_row_background = json_parse_color(oddBgItem->valuestring);
            }

            cJSON* borderWidthItem = cJSON_GetObjectItem(tableConfig, "borderWidth");
            if (borderWidthItem && cJSON_IsNumber(borderWidthItem)) {
                state->style.border_width = (float)borderWidthItem->valuedouble;
            }

            cJSON* cellPaddingItem = cJSON_GetObjectItem(tableConfig, "cellPadding");
            if (cellPaddingItem && cJSON_IsNumber(cellPaddingItem)) {
                state->style.cell_padding = (float)cellPaddingItem->valuedouble;
            }

            cJSON* showBordersItem = cJSON_GetObjectItem(tableConfig, "showBorders");
            if (showBordersItem && cJSON_IsBool(showBordersItem)) {
                state->style.show_borders = cJSON_IsTrue(showBordersItem);
            }

            cJSON* stripedItem = cJSON_GetObjectItem(tableConfig, "striped");
            if (stripedItem && cJSON_IsBool(stripedItem)) {
                state->style.striped_rows = cJSON_IsTrue(stripedItem);
            }

            cJSON* headerStickyItem = cJSON_GetObjectItem(tableConfig, "headerSticky");
            if (headerStickyItem && cJSON_IsBool(headerStickyItem)) {
                state->style.header_sticky = cJSON_IsTrue(headerStickyItem);
            }

            cJSON* collapseBordersItem = cJSON_GetObjectItem(tableConfig, "collapseBorders");
            if (collapseBordersItem && cJSON_IsBool(collapseBordersItem)) {
                state->style.collapse_borders = cJSON_IsTrue(collapseBordersItem);
            }
        }

        if (state) {
            component->custom_data = (char*)state;
        }
    }

    // Parse table cell data (for TableCell and TableHeaderCell)
    if (component->type == IR_COMPONENT_TABLE_CELL ||
        component->type == IR_COMPONENT_TABLE_HEADER_CELL) {
        cJSON* cellData = cJSON_GetObjectItem(json, "cell_data");

        IRTableCellData* data = (IRTableCellData*)calloc(1, sizeof(IRTableCellData));
        if (data) {
            // Default values
            data->colspan = 1;
            data->rowspan = 1;
            data->alignment = IR_ALIGNMENT_START;
            data->vertical_alignment = IR_ALIGNMENT_START;  // Top

            if (cellData && cJSON_IsObject(cellData)) {
                // Parse colspan
                cJSON* colspanItem = cJSON_GetObjectItem(cellData, "colspan");
                if (colspanItem && cJSON_IsNumber(colspanItem)) {
                    data->colspan = (uint16_t)colspanItem->valueint;
                }

                // Parse rowspan
                cJSON* rowspanItem = cJSON_GetObjectItem(cellData, "rowspan");
                if (rowspanItem && cJSON_IsNumber(rowspanItem)) {
                    data->rowspan = (uint16_t)rowspanItem->valueint;
                }

                // Parse alignment
                cJSON* alignItem = cJSON_GetObjectItem(cellData, "alignment");
                if (alignItem && cJSON_IsString(alignItem)) {
                    if (strcmp(alignItem->valuestring, "center") == 0) {
                        data->alignment = IR_ALIGNMENT_CENTER;
                    } else if (strcmp(alignItem->valuestring, "end") == 0 ||
                               strcmp(alignItem->valuestring, "right") == 0) {
                        data->alignment = IR_ALIGNMENT_END;
                    }
                }

                // Parse vertical alignment
                cJSON* vAlignItem = cJSON_GetObjectItem(cellData, "verticalAlignment");
                if (vAlignItem && cJSON_IsString(vAlignItem)) {
                    if (strcmp(vAlignItem->valuestring, "middle") == 0 ||
                        strcmp(vAlignItem->valuestring, "center") == 0) {
                        data->vertical_alignment = IR_ALIGNMENT_CENTER;
                    } else if (strcmp(vAlignItem->valuestring, "bottom") == 0 ||
                               strcmp(vAlignItem->valuestring, "end") == 0) {
                        data->vertical_alignment = IR_ALIGNMENT_END;
                    }
                }
            } else {
                // Check for inline colspan/rowspan on the component itself (shorthand)
                cJSON* colspanItem = cJSON_GetObjectItem(json, "colspan");
                if (colspanItem && cJSON_IsNumber(colspanItem)) {
                    data->colspan = (uint16_t)colspanItem->valueint;
                }
                cJSON* rowspanItem = cJSON_GetObjectItem(json, "rowspan");
                if (rowspanItem && cJSON_IsNumber(rowspanItem)) {
                    data->rowspan = (uint16_t)rowspanItem->valueint;
                }
            }

            component->custom_data = (char*)data;
        }
    }

    // Parse markdown component data
    if (component->type == IR_COMPONENT_HEADING) {
        IRHeadingData* data = (IRHeadingData*)calloc(1, sizeof(IRHeadingData));
        if (data) {
            cJSON* levelItem = cJSON_GetObjectItem(json, "level");
            if (levelItem && cJSON_IsNumber(levelItem)) {
                data->level = (uint8_t)levelItem->valueint;
            } else {
                data->level = 1;  // Default to H1
            }

            cJSON* textItem = cJSON_GetObjectItem(json, "text");
            if (textItem && cJSON_IsString(textItem)) {
                data->text = strdup(textItem->valuestring);
            }

            cJSON* idItem = cJSON_GetObjectItem(json, "id_attr");
            if (idItem && cJSON_IsString(idItem)) {
                data->id = strdup(idItem->valuestring);
            }

            component->custom_data = (char*)data;
        }
    }

    if (component->type == IR_COMPONENT_CODE_BLOCK) {
        IRCodeBlockData* data = (IRCodeBlockData*)calloc(1, sizeof(IRCodeBlockData));
        if (data) {
            cJSON* langItem = cJSON_GetObjectItem(json, "language");
            if (langItem && cJSON_IsString(langItem)) {
                data->language = strdup(langItem->valuestring);
            }

            cJSON* codeItem = cJSON_GetObjectItem(json, "code");
            if (codeItem && cJSON_IsString(codeItem)) {
                data->code = strdup(codeItem->valuestring);
                data->length = strlen(data->code);
            }

            cJSON* showLineNumsItem = cJSON_GetObjectItem(json, "showLineNumbers");
            if (showLineNumsItem && cJSON_IsBool(showLineNumsItem)) {
                data->show_line_numbers = cJSON_IsTrue(showLineNumsItem);
            }

            component->custom_data = (char*)data;
        }
    }

    if (component->type == IR_COMPONENT_LIST) {
        IRListData* data = (IRListData*)calloc(1, sizeof(IRListData));
        if (data) {
            cJSON* typeItem = cJSON_GetObjectItem(json, "listType");
            if (typeItem && cJSON_IsString(typeItem)) {
                data->type = (strcmp(typeItem->valuestring, "ordered") == 0) ?
                    IR_LIST_ORDERED : IR_LIST_UNORDERED;
            } else {
                data->type = IR_LIST_UNORDERED;  // Default
            }

            cJSON* startItem = cJSON_GetObjectItem(json, "start");
            if (startItem && cJSON_IsNumber(startItem)) {
                data->start = (uint32_t)startItem->valueint;
            } else {
                data->start = 1;
            }

            cJSON* tightItem = cJSON_GetObjectItem(json, "tight");
            if (tightItem && cJSON_IsBool(tightItem)) {
                data->tight = cJSON_IsTrue(tightItem);
            } else {
                data->tight = true;  // Default
            }

            component->custom_data = (char*)data;
        }
    }

    if (component->type == IR_COMPONENT_LIST_ITEM) {
        IRListItemData* data = (IRListItemData*)calloc(1, sizeof(IRListItemData));
        if (data) {
            cJSON* markerItem = cJSON_GetObjectItem(json, "marker");
            if (markerItem && cJSON_IsString(markerItem)) {
                data->marker = strdup(markerItem->valuestring);
            }

            cJSON* numberItem = cJSON_GetObjectItem(json, "number");
            if (numberItem && cJSON_IsNumber(numberItem)) {
                data->number = (uint32_t)numberItem->valueint;
            }

            component->custom_data = (char*)data;
        }
    }

    if (component->type == IR_COMPONENT_LINK) {
        IRLinkData* data = (IRLinkData*)calloc(1, sizeof(IRLinkData));
        if (data) {
            cJSON* urlItem = cJSON_GetObjectItem(json, "url");
            if (urlItem && cJSON_IsString(urlItem)) {
                data->url = strdup(urlItem->valuestring);
            }

            cJSON* titleItem = cJSON_GetObjectItem(json, "title");
            if (titleItem && cJSON_IsString(titleItem)) {
                data->title = strdup(titleItem->valuestring);
            }

            component->custom_data = (char*)data;
        }
    }

    // Parse flowchart config (for Flowchart components)
    if (component->type == IR_COMPONENT_FLOWCHART) {
        cJSON* flowchartConfig = cJSON_GetObjectItem(json, "flowchart_config");
        IRFlowchartState* state = ir_flowchart_create_state(IR_FLOWCHART_DIR_TB);  // Default

        if (flowchartConfig && cJSON_IsObject(flowchartConfig)) {
            // Parse direction from nested config object
            cJSON* dirItem = cJSON_GetObjectItem(flowchartConfig, "direction");
            if (dirItem && cJSON_IsString(dirItem)) {
                state->direction = ir_flowchart_parse_direction(dirItem->valuestring);
            }

            // Parse layout parameters
            cJSON* nodeSpacingItem = cJSON_GetObjectItem(flowchartConfig, "nodeSpacing");
            if (nodeSpacingItem && cJSON_IsNumber(nodeSpacingItem)) {
                state->node_spacing = (float)nodeSpacingItem->valuedouble;
            }

            cJSON* rankSpacingItem = cJSON_GetObjectItem(flowchartConfig, "rankSpacing");
            if (rankSpacingItem && cJSON_IsNumber(rankSpacingItem)) {
                state->rank_spacing = (float)rankSpacingItem->valuedouble;
            }

            cJSON* subgraphPaddingItem = cJSON_GetObjectItem(flowchartConfig, "subgraphPadding");
            if (subgraphPaddingItem && cJSON_IsNumber(subgraphPaddingItem)) {
                state->subgraph_padding = (float)subgraphPaddingItem->valuedouble;
            }
        } else {
            // Fallback: check for direct "direction" property (transpiler output format)
            cJSON* dirItem = cJSON_GetObjectItem(json, "direction");
            if (dirItem && cJSON_IsString(dirItem)) {
                state->direction = ir_flowchart_parse_direction(dirItem->valuestring);
            }
        }

        component->custom_data = (char*)state;
    }

    // Parse flowchart node data
    if (component->type == IR_COMPONENT_FLOWCHART_NODE) {
        cJSON* nodeData = cJSON_GetObjectItem(json, "flowchart_node");
        IRFlowchartNodeData* data = NULL;

        if (nodeData && cJSON_IsObject(nodeData)) {
            cJSON* idItem = cJSON_GetObjectItem(nodeData, "id");
            cJSON* shapeItem = cJSON_GetObjectItem(nodeData, "shape");
            cJSON* labelItem = cJSON_GetObjectItem(nodeData, "label");

            const char* nodeId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : NULL;
            IRFlowchartShape shape = IR_FLOWCHART_SHAPE_RECTANGLE;
            if (shapeItem && cJSON_IsString(shapeItem)) {
                shape = ir_flowchart_parse_shape(shapeItem->valuestring);
            }
            const char* label = (labelItem && cJSON_IsString(labelItem)) ? labelItem->valuestring : NULL;

            data = ir_flowchart_node_data_create(nodeId, shape, label);

            if (data) {
                // Parse colors
                cJSON* fillItem = cJSON_GetObjectItem(nodeData, "fillColor");
                if (fillItem && cJSON_IsString(fillItem)) {
                    const char* colorStr = fillItem->valuestring;
                    if (colorStr[0] == '#' && strlen(colorStr) == 9) {  // #RRGGBBAA
                        unsigned int rgba;
                        if (sscanf(colorStr + 1, "%x", &rgba) == 1) {
                            data->fill_color = rgba;
                        }
                    }
                }

                cJSON* strokeItem = cJSON_GetObjectItem(nodeData, "strokeColor");
                if (strokeItem && cJSON_IsString(strokeItem)) {
                    const char* colorStr = strokeItem->valuestring;
                    if (colorStr[0] == '#' && strlen(colorStr) == 9) {
                        unsigned int rgba;
                        if (sscanf(colorStr + 1, "%x", &rgba) == 1) {
                            data->stroke_color = rgba;
                        }
                    }
                }

                cJSON* strokeWidthItem = cJSON_GetObjectItem(nodeData, "strokeWidth");
                if (strokeWidthItem && cJSON_IsNumber(strokeWidthItem)) {
                    data->stroke_width = (float)strokeWidthItem->valuedouble;
                }

                cJSON* subgraphIdItem = cJSON_GetObjectItem(nodeData, "subgraphId");
                if (subgraphIdItem && cJSON_IsString(subgraphIdItem)) {
                    data->subgraph_id = strdup(subgraphIdItem->valuestring);
                }
            }
        } else {
            // Check for inline properties (transpiler output format)
            // Try both "id" (transpiler format) and "nodeId" (legacy format)
            cJSON* idItem = cJSON_GetObjectItem(json, "id");
            if (!idItem || !cJSON_IsString(idItem)) {
                idItem = cJSON_GetObjectItem(json, "nodeId");
            }
            cJSON* shapeItem = cJSON_GetObjectItem(json, "shape");
            cJSON* labelItem = cJSON_GetObjectItem(json, "label");

            const char* nodeId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : NULL;
            IRFlowchartShape shape = IR_FLOWCHART_SHAPE_RECTANGLE;
            if (shapeItem && cJSON_IsString(shapeItem)) {
                shape = ir_flowchart_parse_shape(shapeItem->valuestring);
            }
            const char* label = (labelItem && cJSON_IsString(labelItem)) ? labelItem->valuestring : NULL;

            data = ir_flowchart_node_data_create(nodeId, shape, label);
        }

        if (data) {
            component->custom_data = (char*)data;
        }
    }

    // Parse flowchart edge data
    if (component->type == IR_COMPONENT_FLOWCHART_EDGE) {
        cJSON* edgeData = cJSON_GetObjectItem(json, "flowchart_edge");
        IRFlowchartEdgeData* data = NULL;

        if (edgeData && cJSON_IsObject(edgeData)) {
            cJSON* fromItem = cJSON_GetObjectItem(edgeData, "from");
            cJSON* toItem = cJSON_GetObjectItem(edgeData, "to");
            cJSON* typeItem = cJSON_GetObjectItem(edgeData, "type");

            const char* fromId = (fromItem && cJSON_IsString(fromItem)) ? fromItem->valuestring : NULL;
            const char* toId = (toItem && cJSON_IsString(toItem)) ? toItem->valuestring : NULL;
            IRFlowchartEdgeType type = IR_FLOWCHART_EDGE_ARROW;
            if (typeItem && cJSON_IsString(typeItem)) {
                type = ir_flowchart_parse_edge_type(typeItem->valuestring);
            }

            data = ir_flowchart_edge_data_create(fromId, toId, type);

            if (data) {
                cJSON* startMarkerItem = cJSON_GetObjectItem(edgeData, "startMarker");
                if (startMarkerItem && cJSON_IsString(startMarkerItem)) {
                    data->start_marker = ir_flowchart_parse_marker(startMarkerItem->valuestring);
                }

                cJSON* endMarkerItem = cJSON_GetObjectItem(edgeData, "endMarker");
                if (endMarkerItem && cJSON_IsString(endMarkerItem)) {
                    data->end_marker = ir_flowchart_parse_marker(endMarkerItem->valuestring);
                }

                cJSON* labelItem = cJSON_GetObjectItem(edgeData, "label");
                if (labelItem && cJSON_IsString(labelItem)) {
                    data->label = strdup(labelItem->valuestring);
                }
            }
        } else {
            // Check for inline properties (transpiler output format)
            cJSON* fromItem = cJSON_GetObjectItem(json, "from");
            cJSON* toItem = cJSON_GetObjectItem(json, "to");
            cJSON* typeItem = cJSON_GetObjectItem(json, "type");
            cJSON* labelItem = cJSON_GetObjectItem(json, "label");

            const char* fromId = (fromItem && cJSON_IsString(fromItem)) ? fromItem->valuestring : NULL;
            const char* toId = (toItem && cJSON_IsString(toItem)) ? toItem->valuestring : NULL;

            // Parse edge type from the "type" field (used both as component type and edge type)
            IRFlowchartEdgeType edgeType = IR_FLOWCHART_EDGE_ARROW;
            if (typeItem && cJSON_IsString(typeItem)) {
                edgeType = ir_flowchart_parse_edge_type(typeItem->valuestring);
            }

            data = ir_flowchart_edge_data_create(fromId, toId, edgeType);

            // Handle label
            if (data && labelItem && cJSON_IsString(labelItem)) {
                data->label = strdup(labelItem->valuestring);
            }
        }

        if (data) {
            component->custom_data = (char*)data;
        }
    }

    // Parse flowchart subgraph data
    if (component->type == IR_COMPONENT_FLOWCHART_SUBGRAPH) {
        cJSON* subgraphData = cJSON_GetObjectItem(json, "flowchart_subgraph");
        IRFlowchartSubgraphData* data = NULL;

        if (subgraphData && cJSON_IsObject(subgraphData)) {
            cJSON* idItem = cJSON_GetObjectItem(subgraphData, "id");
            cJSON* titleItem = cJSON_GetObjectItem(subgraphData, "title");

            const char* subgraphId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : NULL;
            const char* title = (titleItem && cJSON_IsString(titleItem)) ? titleItem->valuestring : NULL;

            data = ir_flowchart_subgraph_data_create(subgraphId, title);

            if (data) {
                // Parse colors
                cJSON* bgItem = cJSON_GetObjectItem(subgraphData, "backgroundColor");
                if (bgItem && cJSON_IsString(bgItem)) {
                    const char* colorStr = bgItem->valuestring;
                    if (colorStr[0] == '#' && strlen(colorStr) == 9) {
                        unsigned int rgba;
                        if (sscanf(colorStr + 1, "%x", &rgba) == 1) {
                            data->background_color = rgba;
                        }
                    }
                }

                cJSON* borderItem = cJSON_GetObjectItem(subgraphData, "borderColor");
                if (borderItem && cJSON_IsString(borderItem)) {
                    const char* colorStr = borderItem->valuestring;
                    if (colorStr[0] == '#' && strlen(colorStr) == 9) {
                        unsigned int rgba;
                        if (sscanf(colorStr + 1, "%x", &rgba) == 1) {
                            data->border_color = rgba;
                        }
                    }
                }
            }
        } else {
            // Check for inline properties
            cJSON* idItem = cJSON_GetObjectItem(json, "subgraphId");
            cJSON* titleItem = cJSON_GetObjectItem(json, "title");

            const char* subgraphId = (idItem && cJSON_IsString(idItem)) ? idItem->valuestring : NULL;
            const char* title = (titleItem && cJSON_IsString(titleItem)) ? titleItem->valuestring : NULL;

            data = ir_flowchart_subgraph_data_create(subgraphId, title);
        }

        if (data) {
            component->custom_data = (char*)data;
        }
    }

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
                IRComponent* child = json_deserialize_component_with_context(childJson, ctx);
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

    // Parse component_definitions for expansion
    ComponentDefContext* ctx = NULL;
    cJSON* componentDefs = cJSON_GetObjectItem(root, "component_definitions");
    if (componentDefs && cJSON_IsArray(componentDefs)) {
        ctx = component_def_context_create();
        int defCount = cJSON_GetArraySize(componentDefs);
        for (int i = 0; i < defCount; i++) {
            cJSON* def = cJSON_GetArrayItem(componentDefs, i);
            if (!def || !cJSON_IsObject(def)) continue;

            cJSON* nameItem = cJSON_GetObjectItem(def, "name");
            cJSON* templateItem = cJSON_GetObjectItem(def, "template");

            if (nameItem && cJSON_IsString(nameItem) && templateItem && cJSON_IsObject(templateItem)) {
                // Add the full definition (not just template) so we can access props/state
                component_def_context_add(ctx, nameItem->valuestring, def);
            }
        }
    }

    IRComponent* component = NULL;

    // Check for "root" key first (new format), then "component" (legacy)
    cJSON* componentJson = cJSON_GetObjectItem(root, "root");
    if (!componentJson) {
        componentJson = cJSON_GetObjectItem(root, "component");
    }

    if (componentJson && cJSON_IsObject(componentJson)) {
        // Wrapped format: { "root": {...} } or { "component": {...} }
        component = json_deserialize_component_with_context(componentJson, ctx);
    } else {
        // Unwrapped format: just component tree at root
        component = json_deserialize_component_with_context(root, ctx);
    }

    // Parse plugin requirements if present
    cJSON* pluginsArray = cJSON_GetObjectItem(root, "required_plugins");
    if (pluginsArray && cJSON_IsArray(pluginsArray)) {
        int plugin_count = cJSON_GetArraySize(pluginsArray);
        if (plugin_count > 0) {
            // Store globally for desktop renderer to access
            char** plugin_names = malloc(sizeof(char*) * plugin_count);
            for (int i = 0; i < plugin_count; i++) {
                cJSON* plugin_name = cJSON_GetArrayItem(pluginsArray, i);
                if (plugin_name && cJSON_IsString(plugin_name)) {
                    plugin_names[i] = strdup(plugin_name->valuestring);
                }
            }
            ir_plugin_set_requirements(plugin_names, plugin_count);
        }
    }

    // Clean up context before deleting JSON (context references JSON nodes)
    component_def_context_free(ctx);
    cJSON_Delete(root);

    // Propagate animation flags after full tree construction
    if (component) {
        ir_animation_propagate_flags(component);
    }

    return component;
}

/**
 * Recursively finalize all TabGroup components in the tree
 * This is necessary for .kir files loaded from disk, where TabGroups
 * are deserialized but never have their TabGroupState runtime state created.
 * @param component Root of component tree to process
 */
static void ir_finalize_tabgroups_recursive(IRComponent* component) {
    if (!component) return;

    // Create and finalize state for TabGroup components
    if (component->type == IR_COMPONENT_TAB_GROUP) {
        // Check if TabGroupState already exists (Nim DSL path creates it)
        if (!component->custom_data) {
            // Find TabBar and TabContent children
            IRComponent* tab_bar = NULL;
            IRComponent* tab_content = NULL;

            for (uint32_t i = 0; i < component->child_count; i++) {
                IRComponent* child = component->children[i];
                if (child->type == IR_COMPONENT_TAB_BAR) {
                    tab_bar = child;
                } else if (child->type == IR_COMPONENT_TAB_CONTENT) {
                    tab_content = child;
                }
            }

            // Only create state if we found both required children
            if (tab_bar && tab_content) {
                // Get initial selectedIndex from tab_data (defaults to 0 if not set)
                int selected_index = component->tab_data ? component->tab_data->selected_index : 0;
                bool reorderable = component->tab_data ? component->tab_data->reorderable : false;

                // Create the TabGroupState
                TabGroupState* state = ir_tabgroup_create_state(
                    component,
                    tab_bar,
                    tab_content,
                    selected_index,
                    reorderable
                );

                if (state) {
                    // Store state in component's custom_data for renderer access
                    component->custom_data = (char*)state;

                    // Register all Tab components from TabBar
                    for (uint32_t i = 0; i < tab_bar->child_count; i++) {
                        IRComponent* tab = tab_bar->children[i];
                        if (tab->type == IR_COMPONENT_TAB) {
                            ir_tabgroup_register_tab(state, tab);
                        }
                    }

                    // Register all TabPanel components from TabContent
                    for (uint32_t i = 0; i < tab_content->child_count; i++) {
                        IRComponent* panel = tab_content->children[i];
                        if (panel->type == IR_COMPONENT_TAB_PANEL) {
                            ir_tabgroup_register_panel(state, panel);
                        }
                    }

                    // Finalize the state (sets initial panel visibility based on selectedIndex)
                    ir_tabgroup_finalize(state);
                }
            }
        } else {
            // State already exists (Nim DSL path), just finalize it
            TabGroupState* state = (TabGroupState*)component->custom_data;
            ir_tabgroup_finalize(state);
        }
    }

    // Recursively process all children
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_finalize_tabgroups_recursive(component->children[i]);
    }
}

/**
 * Recursively finalize all Table components in the tree
 * This builds the span map for colspan/rowspan support after deserialization.
 * @param component Root of component tree to process
 */
static void ir_finalize_tables_recursive(IRComponent* component) {
    if (!component) return;

    // Finalize Table components
    if (component->type == IR_COMPONENT_TABLE && component->custom_data) {
        ir_table_finalize(component);
    }

    // Recursively process all children
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_finalize_tables_recursive(component->children[i]);
    }
}

/**
 * Recursively finalize all Flowchart components in the tree
 * This registers nodes/edges in the flowchart state and computes layout.
 * @param component Root of component tree to process
 */
static void ir_finalize_flowcharts_recursive(IRComponent* component) {
    if (!component) return;

    // Finalize Flowchart components
    if (component->type == IR_COMPONENT_FLOWCHART) {
        IRFlowchartState* state = ir_get_flowchart_state(component);

        // Create state if not present
        if (!state) {
            // Get direction from flowchart_config if present
            IRFlowchartDirection direction = IR_FLOWCHART_DIR_TB;
            // State may have been created during deserialization
            state = ir_flowchart_create_state(direction);
            if (state) {
                component->custom_data = (char*)state;
            }
        }

        if (state) {
            // Register all FlowchartNode and FlowchartEdge children
            for (uint32_t i = 0; i < component->child_count; i++) {
                IRComponent* child = component->children[i];

                if (child->type == IR_COMPONENT_FLOWCHART_NODE) {
                    IRFlowchartNodeData* nodeData = ir_get_flowchart_node_data(child);
                    if (nodeData) {
                        ir_flowchart_register_node(state, nodeData);
                    }
                } else if (child->type == IR_COMPONENT_FLOWCHART_EDGE) {
                    IRFlowchartEdgeData* edgeData = ir_get_flowchart_edge_data(child);
                    if (edgeData) {
                        ir_flowchart_register_edge(state, edgeData);
                    }
                } else if (child->type == IR_COMPONENT_FLOWCHART_SUBGRAPH) {
                    IRFlowchartSubgraphData* subgraphData = ir_get_flowchart_subgraph_data(child);
                    if (subgraphData) {
                        ir_flowchart_register_subgraph(state, subgraphData);
                    }
                    // Also register nodes within subgraph
                    for (uint32_t j = 0; j < child->child_count; j++) {
                        IRComponent* subchild = child->children[j];
                        if (subchild->type == IR_COMPONENT_FLOWCHART_NODE) {
                            IRFlowchartNodeData* nodeData = ir_get_flowchart_node_data(subchild);
                            if (nodeData) {
                                ir_flowchart_register_node(state, nodeData);
                            }
                        }
                    }
                }
            }

            // Finalize the flowchart (registers nodes/edges, sets layout_computed=false)
            ir_flowchart_finalize(component);

            // Mark the flowchart as dirty so layout will be computed during rendering
            component->dirty_flags |= IR_DIRTY_LAYOUT | IR_DIRTY_SUBTREE;
        }
    }

    // Recursively process all children
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_finalize_flowcharts_recursive(component->children[i]);
    }
}

/**
 * Recursively mark a component and all its descendants as dirty.
 * This ensures layout will be computed for the entire tree.
 * @param component Root of subtree to mark dirty
 */
static void ir_mark_dirty_recursive(IRComponent* component) {
    if (!component) return;

    component->dirty_flags |= IR_DIRTY_LAYOUT | IR_DIRTY_SUBTREE;

    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_mark_dirty_recursive(component->children[i]);
    }
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

    // Finalize all TabGroups in the tree (sets initial panel visibility)
    if (component) {
        ir_finalize_tabgroups_recursive(component);
    }

    // Finalize all Tables in the tree (builds span map for colspan/rowspan)
    if (component) {
        ir_finalize_tables_recursive(component);
    }

    // Finalize all Flowcharts in the tree (registers nodes/edges, computes layout)
    if (component) {
        ir_finalize_flowcharts_recursive(component);
    }

    // Mark entire tree as dirty so layout will be computed
    if (component) {
        ir_mark_dirty_recursive(component);
    }

    return component;
}
