/**
 * @file ir_json_deserialize.c
 * @brief JSON deserialization functions for IR component trees
 *
 * This module contains all functions for converting KIR (Kryon Intermediate Representation)
 * JSON format into IR component trees.
 */

#define _GNU_SOURCE
#include "ir_json_deserialize.h"
#include "ir_json_context.h"
#include "../include/ir_core.h"
#include "../include/ir_builder.h"
#include "../include/ir_logic.h"
#include "../style/ir_stylesheet.h"
#include "../utils/ir_c_metadata.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

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

// External global C metadata from ir_c_metadata.c
extern CSourceMetadata g_c_metadata;

// Global module definition cache for component references
static ComponentDefContext* g_module_def_cache = NULL;

// Global counter for generating unique component IDs during expansion
static uint32_t g_next_expansion_id = 1000;  // Start high to avoid conflicts

// Forward declarations for helper functions
static IRComponent* json_deserialize_component_with_context(cJSON* json, ComponentDefContext* ctx);
static IRColor json_parse_color(const char* str);
static IRAlignment json_parse_alignment(const char* str);
static IRGridTrackType json_parse_grid_track_type(const char* str);
static IRTextAlign json_parse_text_align(const char* str);
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
// Property Parsing Helpers
// ============================================================================

/**
 * Parse dimension from string (e.g., "100px", "50%", "auto")
 */
IRDimension ir_json_parse_dimension(const char* str) {
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

    // Check for var() reference - preserve full var() string
    if (strncmp(str, "var(", 4) == 0) {
        color.type = IR_COLOR_VAR_REF;
        color.var_name = strdup(str);  // Store full "var(--border)"
        // Try to parse numeric var_id if present (legacy format "var(0)")
        if (str[4] >= '0' && str[4] <= '9') {
            sscanf(str + 4, "%hu", &color.data.var_id);
        }
        return color;
    }

    // Parse rgba(r,g,b,a) format
    if (strncmp(str, "rgba(", 5) == 0) {
        color.type = IR_COLOR_SOLID;
        float r = 0, g = 0, b = 0, a = 1.0f;
        sscanf(str + 5, "%f,%f,%f,%f", &r, &g, &b, &a);
        color.data.r = (uint8_t)r;
        color.data.g = (uint8_t)g;
        color.data.b = (uint8_t)b;
        color.data.a = (uint8_t)(a * 255.0f);
        return color;
    }

    // Parse rgb(r,g,b) format
    if (strncmp(str, "rgb(", 4) == 0) {
        color.type = IR_COLOR_SOLID;
        float r = 0, g = 0, b = 0;
        sscanf(str + 4, "%f,%f,%f", &r, &g, &b);
        color.data.r = (uint8_t)r;
        color.data.g = (uint8_t)g;
        color.data.b = (uint8_t)b;
        color.data.a = 255;
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
 * Wrapper for public API
 */
IRColor ir_json_parse_color(const char* str) {
    return json_parse_color(str);
}

/**
 * Parse spacing from JSON (number or array) into IRSpacing
 */
IRSpacing ir_json_parse_spacing(cJSON* json) {
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

IRTextAlign ir_json_parse_text_align(const char* str) {
    return json_parse_text_align(str);
}

/**
 * Parse alignment from string
 */
static IRAlignment json_parse_alignment(const char* str) {
    if (!str) return IR_ALIGNMENT_START;

    // Handle both bare values ("center", "end") and CSS values ("flex-end")
    IRAlignment result;
    if (strcmp(str, "center") == 0) result = IR_ALIGNMENT_CENTER;
    else if (strcmp(str, "start") == 0 || strcmp(str, "flex-start") == 0) result = IR_ALIGNMENT_START;
    else if (strcmp(str, "end") == 0 || strcmp(str, "flex-end") == 0) result = IR_ALIGNMENT_END;
    else if (strcmp(str, "space-between") == 0) result = IR_ALIGNMENT_SPACE_BETWEEN;
    else if (strcmp(str, "space-around") == 0) result = IR_ALIGNMENT_SPACE_AROUND;
    else if (strcmp(str, "space-evenly") == 0) result = IR_ALIGNMENT_SPACE_EVENLY;
    else if (strcmp(str, "stretch") == 0) result = IR_ALIGNMENT_STRETCH;
    else result = IR_ALIGNMENT_START;  // Default for unrecognized values

    return result;
}

IRAlignment ir_json_parse_alignment(const char* str) {
    return json_parse_alignment(str);
}

/**
 * Parse grid track type from string
 */
static IRGridTrackType json_parse_grid_track_type(const char* str) {
    if (!str) return IR_GRID_TRACK_AUTO;
    if (strcmp(str, "px") == 0) return IR_GRID_TRACK_PX;
    if (strcmp(str, "percent") == 0) return IR_GRID_TRACK_PERCENT;
    if (strcmp(str, "fr") == 0) return IR_GRID_TRACK_FR;
    if (strcmp(str, "auto") == 0) return IR_GRID_TRACK_AUTO;
    if (strcmp(str, "min-content") == 0) return IR_GRID_TRACK_MIN_CONTENT;
    if (strcmp(str, "max-content") == 0) return IR_GRID_TRACK_MAX_CONTENT;
    return IR_GRID_TRACK_AUTO;
}

IRGridTrackType ir_json_parse_grid_track_type(const char* str) {
    return json_parse_grid_track_type(str);
}

// ============================================================================
// Gradient Deserialization
// ============================================================================

/**
 * Deserialize gradient from JSON object
 */
IRGradient* ir_json_deserialize_gradient(cJSON* obj) {
    if (!obj || !cJSON_IsObject(obj)) return NULL;

    IRGradient* gradient = calloc(1, sizeof(IRGradient));
    if (!gradient) return NULL;

    // Type
    cJSON* type_item = cJSON_GetObjectItem(obj, "type");
    if (type_item && cJSON_IsString(type_item)) {
        if (strcmp(type_item->valuestring, "radial") == 0) {
            gradient->type = IR_GRADIENT_RADIAL;
        } else if (strcmp(type_item->valuestring, "conic") == 0) {
            gradient->type = IR_GRADIENT_CONIC;
        } else {
            gradient->type = IR_GRADIENT_LINEAR;
        }
    } else {
        gradient->type = IR_GRADIENT_LINEAR;
    }

    // Angle
    cJSON* angle_item = cJSON_GetObjectItem(obj, "angle");
    if (angle_item && cJSON_IsNumber(angle_item)) {
        gradient->angle = (float)angle_item->valuedouble;
    }

    // Center
    cJSON* cx = cJSON_GetObjectItem(obj, "centerX");
    cJSON* cy = cJSON_GetObjectItem(obj, "centerY");
    if (cx && cJSON_IsNumber(cx)) gradient->center_x = (float)cx->valuedouble;
    if (cy && cJSON_IsNumber(cy)) gradient->center_y = (float)cy->valuedouble;

    // Stops
    cJSON* stops = cJSON_GetObjectItem(obj, "stops");
    if (stops && cJSON_IsArray(stops)) {
        cJSON* stop_item;
        int idx = 0;
        cJSON_ArrayForEach(stop_item, stops) {
            if (idx >= 8) break;  // Max 8 stops

            cJSON* pos = cJSON_GetObjectItem(stop_item, "position");
            cJSON* color = cJSON_GetObjectItem(stop_item, "color");

            if (pos && cJSON_IsNumber(pos)) {
                gradient->stops[idx].position = (float)pos->valuedouble;
            }

            if (color && cJSON_IsString(color)) {
                // Parse hex color #RRGGBB or #RRGGBBAA
                const char* hex = color->valuestring;
                if (hex[0] == '#') {
                    unsigned int r = 0, g = 0, b = 0, a = 255;
                    size_t len = strlen(hex);
                    if (len == 7) {
                        unsigned int h;
                        if (sscanf(hex, "#%06x", &h) == 1) {
                            r = (h >> 16) & 0xFF;
                            g = (h >> 8) & 0xFF;
                            b = h & 0xFF;
                        }
                    } else if (len == 9) {
                        unsigned int h;
                        if (sscanf(hex, "#%08x", &h) == 1) {
                            r = (h >> 24) & 0xFF;
                            g = (h >> 16) & 0xFF;
                            b = (h >> 8) & 0xFF;
                            a = h & 0xFF;
                        }
                    }
                    gradient->stops[idx].r = (uint8_t)r;
                    gradient->stops[idx].g = (uint8_t)g;
                    gradient->stops[idx].b = (uint8_t)b;
                    gradient->stops[idx].a = (uint8_t)a;
                }
            }

            idx++;
        }
        gradient->stop_count = idx;
    }

    return gradient;
}

// ============================================================================
// Style & Layout Deserialization
// ============================================================================

/**
 * Deserialize style from JSON object into IRStyle
 */
static void json_deserialize_style(cJSON* obj, IRStyle* style) {
    if (!obj || !style) return;

    cJSON* item = NULL;

    // Dimensions
    if ((item = cJSON_GetObjectItem(obj, "width")) != NULL && cJSON_IsString(item)) {
        style->width = ir_json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "height")) != NULL && cJSON_IsString(item)) {
        style->height = ir_json_parse_dimension(item->valuestring);
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

    // Background color or gradient
    if ((item = cJSON_GetObjectItem(obj, "backgroundGradient")) != NULL && cJSON_IsObject(item)) {
        // Parse gradient object
        IRGradient* gradient = ir_json_deserialize_gradient(item);
        if (gradient) {
            style->background.type = IR_COLOR_GRADIENT;
            style->background.data.gradient = gradient;
        }
    } else if ((item = cJSON_GetObjectItem(obj, "background")) != NULL && cJSON_IsString(item)) {
        style->background = json_parse_color(item->valuestring);
    }

    // Border
    if ((item = cJSON_GetObjectItem(obj, "border")) != NULL && cJSON_IsObject(item)) {
        cJSON* borderItem = NULL;
        if ((borderItem = cJSON_GetObjectItem(item, "width")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.width = (float)borderItem->valuedouble;
        }
        // Per-side widths
        if ((borderItem = cJSON_GetObjectItem(item, "widthTop")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.width_top = (float)borderItem->valuedouble;
        }
        if ((borderItem = cJSON_GetObjectItem(item, "widthRight")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.width_right = (float)borderItem->valuedouble;
        }
        if ((borderItem = cJSON_GetObjectItem(item, "widthBottom")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.width_bottom = (float)borderItem->valuedouble;
        }
        if ((borderItem = cJSON_GetObjectItem(item, "widthLeft")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.width_left = (float)borderItem->valuedouble;
        }
        if ((borderItem = cJSON_GetObjectItem(item, "color")) != NULL && cJSON_IsString(borderItem)) {
            style->border.color = json_parse_color(borderItem->valuestring);
        }
        if ((borderItem = cJSON_GetObjectItem(item, "radius")) != NULL && cJSON_IsNumber(borderItem)) {
            style->border.radius = (uint8_t)borderItem->valuedouble;
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
    if ((item = cJSON_GetObjectItem(obj, "lineHeight")) != NULL && cJSON_IsNumber(item)) {
        style->font.line_height = (float)item->valuedouble;
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

    // Text effects
    if ((item = cJSON_GetObjectItem(obj, "maxTextWidth")) != NULL && cJSON_IsNumber(item)) {
        style->text_effect.max_width.type = IR_DIMENSION_PX;
        style->text_effect.max_width.value = (float)item->valuedouble;
    }

    // Spacing
    if ((item = cJSON_GetObjectItem(obj, "padding")) != NULL) {
        style->padding = ir_json_parse_spacing(item);
    }
    if ((item = cJSON_GetObjectItem(obj, "margin")) != NULL) {
        style->margin = ir_json_parse_spacing(item);
    }

    // Individual margin properties (override unified margin for specific sides)
    if ((item = cJSON_GetObjectItem(obj, "marginTop")) != NULL && cJSON_IsNumber(item)) {
        style->margin.top = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "marginRight")) != NULL && cJSON_IsNumber(item)) {
        style->margin.right = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "marginBottom")) != NULL && cJSON_IsNumber(item)) {
        style->margin.bottom = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "marginLeft")) != NULL && cJSON_IsNumber(item)) {
        style->margin.left = (float)item->valuedouble;
    }

    // Individual padding properties (override unified padding for specific sides)
    if ((item = cJSON_GetObjectItem(obj, "paddingTop")) != NULL && cJSON_IsNumber(item)) {
        style->padding.top = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "paddingRight")) != NULL && cJSON_IsNumber(item)) {
        style->padding.right = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "paddingBottom")) != NULL && cJSON_IsNumber(item)) {
        style->padding.bottom = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(obj, "paddingLeft")) != NULL && cJSON_IsNumber(item)) {
        style->padding.left = (float)item->valuedouble;
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

    // Background image (gradient string)
    if ((item = cJSON_GetObjectItem(obj, "backgroundImage")) != NULL && cJSON_IsString(item)) {
        if (style->background_image) free(style->background_image);
        style->background_image = strdup(item->valuestring);
    }

    // Background clip
    if ((item = cJSON_GetObjectItem(obj, "backgroundClip")) != NULL && cJSON_IsString(item)) {
        if (strcmp(item->valuestring, "text") == 0) {
            style->background_clip = IR_BACKGROUND_CLIP_TEXT;
        } else if (strcmp(item->valuestring, "content-box") == 0) {
            style->background_clip = IR_BACKGROUND_CLIP_CONTENT_BOX;
        } else if (strcmp(item->valuestring, "padding-box") == 0) {
            style->background_clip = IR_BACKGROUND_CLIP_PADDING_BOX;
        } else {
            style->background_clip = IR_BACKGROUND_CLIP_BORDER_BOX;
        }
    }

    // Text fill color
    if ((item = cJSON_GetObjectItem(obj, "textFillColor")) != NULL && cJSON_IsString(item)) {
        style->text_fill_color = json_parse_color(item->valuestring);
    }
}

void ir_json_deserialize_style(cJSON* obj, IRStyle* style) {
    json_deserialize_style(obj, style);
}

/**
 * Deserialize layout from JSON object into IRLayout
 */
static void json_deserialize_layout(cJSON* obj, IRLayout* layout) {
    if (!obj || !layout) return;

    cJSON* item = NULL;

    // Display mode
    if ((item = cJSON_GetObjectItem(obj, "display")) != NULL && cJSON_IsString(item)) {
        layout->display_explicit = true;
        if (strcmp(item->valuestring, "flex") == 0) {
            layout->mode = IR_LAYOUT_MODE_FLEX;
        } else if (strcmp(item->valuestring, "grid") == 0) {
            layout->mode = IR_LAYOUT_MODE_GRID;
        } else if (strcmp(item->valuestring, "block") == 0) {
            layout->mode = IR_LAYOUT_MODE_BLOCK;
        }
    }

    // Min/Max dimensions
    if ((item = cJSON_GetObjectItem(obj, "minWidth")) != NULL && cJSON_IsString(item)) {
        layout->min_width = ir_json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "minHeight")) != NULL && cJSON_IsString(item)) {
        layout->min_height = ir_json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "maxWidth")) != NULL && cJSON_IsString(item)) {
        layout->max_width = ir_json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(obj, "maxHeight")) != NULL && cJSON_IsString(item)) {
        layout->max_height = ir_json_parse_dimension(item->valuestring);
    }

    // Flexbox properties
    if ((item = cJSON_GetObjectItem(obj, "flexDirection")) != NULL && cJSON_IsString(item)) {
        layout->flex.direction = (strcmp(item->valuestring, "row") == 0) ? 1 : 0;
    }
    if ((item = cJSON_GetObjectItem(obj, "justifyContent")) != NULL && cJSON_IsString(item)) {
        layout->flex.justify_content = json_parse_alignment(item->valuestring);
        // NOTE: main_axis field was removed - justify_content is used directly
    }
    if ((item = cJSON_GetObjectItem(obj, "alignItems")) != NULL && cJSON_IsString(item)) {
        layout->flex.cross_axis = json_parse_alignment(item->valuestring);
        if (getenv("KRYON_TRACE_MAXWIDTH")) {
            fprintf(stderr, "[MAXWIDTH] JSON: Parsed alignItems=\"%s\" -> cross_axis=%d\n",
                    item->valuestring, layout->flex.cross_axis);
        }
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
        // Also set grid gaps if in grid mode
        if (layout->mode == IR_LAYOUT_MODE_GRID) {
            layout->grid.row_gap = (float)item->valuedouble;
            layout->grid.column_gap = (float)item->valuedouble;
        }
    }

    // Grid-specific properties (only if grid mode)
    if (layout->mode == IR_LAYOUT_MODE_GRID) {
        IRGrid* grid = &layout->grid;

        // Grid row/column gaps
        if ((item = cJSON_GetObjectItem(obj, "rowGap")) != NULL && cJSON_IsNumber(item)) {
            grid->row_gap = (float)item->valuedouble;
        }
        if ((item = cJSON_GetObjectItem(obj, "columnGap")) != NULL && cJSON_IsNumber(item)) {
            grid->column_gap = (float)item->valuedouble;
        }

        // Grid column repeat
        cJSON* col_repeat = cJSON_GetObjectItem(obj, "gridColumnRepeat");
        if (col_repeat && cJSON_IsObject(col_repeat)) {
            grid->use_column_repeat = true;

            // Mode
            cJSON* mode = cJSON_GetObjectItem(col_repeat, "mode");
            if (mode && cJSON_IsString(mode)) {
                if (strcmp(mode->valuestring, "auto-fit") == 0) {
                    grid->column_repeat.mode = IR_GRID_REPEAT_AUTO_FIT;
                } else if (strcmp(mode->valuestring, "auto-fill") == 0) {
                    grid->column_repeat.mode = IR_GRID_REPEAT_AUTO_FILL;
                } else if (strcmp(mode->valuestring, "count") == 0) {
                    grid->column_repeat.mode = IR_GRID_REPEAT_COUNT;
                }
            }

            // Count
            cJSON* count = cJSON_GetObjectItem(col_repeat, "count");
            if (count && cJSON_IsNumber(count)) {
                grid->column_repeat.count = (uint8_t)count->valueint;
            }

            // Minmax
            cJSON* minmax = cJSON_GetObjectItem(col_repeat, "minmax");
            if (minmax && cJSON_IsObject(minmax)) {
                grid->column_repeat.has_minmax = true;

                cJSON* minType = cJSON_GetObjectItem(minmax, "minType");
                cJSON* minVal = cJSON_GetObjectItem(minmax, "minValue");
                cJSON* maxType = cJSON_GetObjectItem(minmax, "maxType");
                cJSON* maxVal = cJSON_GetObjectItem(minmax, "maxValue");

                if (minType && cJSON_IsString(minType)) {
                    grid->column_repeat.minmax.min_type = json_parse_grid_track_type(minType->valuestring);
                }
                if (minVal && cJSON_IsNumber(minVal)) {
                    grid->column_repeat.minmax.min_value = (float)minVal->valuedouble;
                }
                if (maxType && cJSON_IsString(maxType)) {
                    grid->column_repeat.minmax.max_type = json_parse_grid_track_type(maxType->valuestring);
                }
                if (maxVal && cJSON_IsNumber(maxVal)) {
                    grid->column_repeat.minmax.max_value = (float)maxVal->valuedouble;
                }
            } else {
                // Simple track
                cJSON* trackType = cJSON_GetObjectItem(col_repeat, "trackType");
                cJSON* trackVal = cJSON_GetObjectItem(col_repeat, "trackValue");
                if (trackType && cJSON_IsString(trackType)) {
                    grid->column_repeat.track.type = json_parse_grid_track_type(trackType->valuestring);
                }
                if (trackVal && cJSON_IsNumber(trackVal)) {
                    grid->column_repeat.track.value = (float)trackVal->valuedouble;
                }
            }
        }

        // Explicit grid columns
        cJSON* cols = cJSON_GetObjectItem(obj, "gridColumns");
        if (cols && cJSON_IsArray(cols) && !grid->use_column_repeat) {
            int count = cJSON_GetArraySize(cols);
            if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;
            grid->column_count = (uint8_t)count;

            for (int i = 0; i < count; i++) {
                cJSON* col = cJSON_GetArrayItem(cols, i);
                if (col && cJSON_IsObject(col)) {
                    cJSON* type = cJSON_GetObjectItem(col, "type");
                    cJSON* val = cJSON_GetObjectItem(col, "value");
                    if (type && cJSON_IsString(type)) {
                        grid->columns[i].type = json_parse_grid_track_type(type->valuestring);
                    }
                    if (val && cJSON_IsNumber(val)) {
                        grid->columns[i].value = (float)val->valuedouble;
                    }
                }
            }
        }

        // Grid row repeat
        cJSON* row_repeat = cJSON_GetObjectItem(obj, "gridRowRepeat");
        if (row_repeat && cJSON_IsObject(row_repeat)) {
            grid->use_row_repeat = true;

            cJSON* mode = cJSON_GetObjectItem(row_repeat, "mode");
            if (mode && cJSON_IsString(mode)) {
                if (strcmp(mode->valuestring, "auto-fit") == 0) {
                    grid->row_repeat.mode = IR_GRID_REPEAT_AUTO_FIT;
                } else if (strcmp(mode->valuestring, "auto-fill") == 0) {
                    grid->row_repeat.mode = IR_GRID_REPEAT_AUTO_FILL;
                } else if (strcmp(mode->valuestring, "count") == 0) {
                    grid->row_repeat.mode = IR_GRID_REPEAT_COUNT;
                }
            }

            cJSON* count = cJSON_GetObjectItem(row_repeat, "count");
            if (count && cJSON_IsNumber(count)) {
                grid->row_repeat.count = (uint8_t)count->valueint;
            }

            cJSON* minmax = cJSON_GetObjectItem(row_repeat, "minmax");
            if (minmax && cJSON_IsObject(minmax)) {
                grid->row_repeat.has_minmax = true;

                cJSON* minType = cJSON_GetObjectItem(minmax, "minType");
                cJSON* minVal = cJSON_GetObjectItem(minmax, "minValue");
                cJSON* maxType = cJSON_GetObjectItem(minmax, "maxType");
                cJSON* maxVal = cJSON_GetObjectItem(minmax, "maxValue");

                if (minType && cJSON_IsString(minType)) {
                    grid->row_repeat.minmax.min_type = json_parse_grid_track_type(minType->valuestring);
                }
                if (minVal && cJSON_IsNumber(minVal)) {
                    grid->row_repeat.minmax.min_value = (float)minVal->valuedouble;
                }
                if (maxType && cJSON_IsString(maxType)) {
                    grid->row_repeat.minmax.max_type = json_parse_grid_track_type(maxType->valuestring);
                }
                if (maxVal && cJSON_IsNumber(maxVal)) {
                    grid->row_repeat.minmax.max_value = (float)maxVal->valuedouble;
                }
            } else {
                cJSON* trackType = cJSON_GetObjectItem(row_repeat, "trackType");
                cJSON* trackVal = cJSON_GetObjectItem(row_repeat, "trackValue");
                if (trackType && cJSON_IsString(trackType)) {
                    grid->row_repeat.track.type = json_parse_grid_track_type(trackType->valuestring);
                }
                if (trackVal && cJSON_IsNumber(trackVal)) {
                    grid->row_repeat.track.value = (float)trackVal->valuedouble;
                }
            }
        }

        // Explicit grid rows
        cJSON* rows = cJSON_GetObjectItem(obj, "gridRows");
        if (rows && cJSON_IsArray(rows) && !grid->use_row_repeat) {
            int count = cJSON_GetArraySize(rows);
            if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;
            grid->row_count = (uint8_t)count;

            for (int i = 0; i < count; i++) {
                cJSON* row = cJSON_GetArrayItem(rows, i);
                if (row && cJSON_IsObject(row)) {
                    cJSON* type = cJSON_GetObjectItem(row, "type");
                    cJSON* val = cJSON_GetObjectItem(row, "value");
                    if (type && cJSON_IsString(type)) {
                        grid->rows[i].type = json_parse_grid_track_type(type->valuestring);
                    }
                    if (val && cJSON_IsNumber(val)) {
                        grid->rows[i].value = (float)val->valuedouble;
                    }
                }
            }
        }

        // Grid alignment
        if ((item = cJSON_GetObjectItem(obj, "justifyItems")) != NULL && cJSON_IsString(item)) {
            grid->justify_items = json_parse_alignment(item->valuestring);
        }
        if ((item = cJSON_GetObjectItem(obj, "gridAlignItems")) != NULL && cJSON_IsString(item)) {
            grid->align_items = json_parse_alignment(item->valuestring);
        }
    }
}

void ir_json_deserialize_layout(cJSON* obj, IRLayout* layout) {
    json_deserialize_layout(obj, layout);
}

// ============================================================================
// Style Properties Deserialization
// ============================================================================

/**
 * Deserialize style properties from JSON object
 * Matches format from json_serialize_style_properties()
 */
void ir_json_deserialize_style_properties(cJSON* propsObj, IRStyleProperties* props) {
    if (!propsObj || !cJSON_IsObject(propsObj) || !props) return;

    cJSON* item;

    // Display
    if ((item = cJSON_GetObjectItem(propsObj, "display")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_DISPLAY;
        const char* val = item->valuestring;
        if (strcmp(val, "flex") == 0) props->display = IR_LAYOUT_MODE_FLEX;
        else if (strcmp(val, "inline-flex") == 0) props->display = IR_LAYOUT_MODE_INLINE_FLEX;
        else if (strcmp(val, "grid") == 0) props->display = IR_LAYOUT_MODE_GRID;
        else if (strcmp(val, "inline-grid") == 0) props->display = IR_LAYOUT_MODE_INLINE_GRID;
        else if (strcmp(val, "inline") == 0) props->display = IR_LAYOUT_MODE_INLINE;
        else if (strcmp(val, "inline-block") == 0) props->display = IR_LAYOUT_MODE_INLINE_BLOCK;
        else if (strcmp(val, "none") == 0) props->display = IR_LAYOUT_MODE_NONE;
        else props->display = IR_LAYOUT_MODE_BLOCK;
    }

    // Flex direction
    if ((item = cJSON_GetObjectItem(propsObj, "flexDirection")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_FLEX_DIRECTION;
        props->flex_direction = (strcmp(item->valuestring, "row") == 0) ? 1 : 0;
    }

    // Flex wrap
    if ((item = cJSON_GetObjectItem(propsObj, "flexWrap")) && cJSON_IsBool(item)) {
        props->set_flags |= IR_PROP_FLEX_WRAP;
        props->flex_wrap = cJSON_IsTrue(item);
    }

    // Justify content
    if ((item = cJSON_GetObjectItem(propsObj, "justifyContent")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_JUSTIFY_CONTENT;
        if (strcmp(item->valuestring, "center") == 0) props->justify_content = IR_ALIGNMENT_CENTER;
        else if (strcmp(item->valuestring, "flex-end") == 0) props->justify_content = IR_ALIGNMENT_END;
        else if (strcmp(item->valuestring, "space-between") == 0) props->justify_content = IR_ALIGNMENT_SPACE_BETWEEN;
        else if (strcmp(item->valuestring, "space-around") == 0) props->justify_content = IR_ALIGNMENT_SPACE_AROUND;
        else if (strcmp(item->valuestring, "space-evenly") == 0) props->justify_content = IR_ALIGNMENT_SPACE_EVENLY;
        else props->justify_content = IR_ALIGNMENT_START;
    }

    // Align items
    if ((item = cJSON_GetObjectItem(propsObj, "alignItems")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_ALIGN_ITEMS;
        if (strcmp(item->valuestring, "center") == 0) props->align_items = IR_ALIGNMENT_CENTER;
        else if (strcmp(item->valuestring, "flex-end") == 0) props->align_items = IR_ALIGNMENT_END;
        else if (strcmp(item->valuestring, "stretch") == 0) props->align_items = IR_ALIGNMENT_STRETCH;
        else props->align_items = IR_ALIGNMENT_START;
    }

    // Gap
    if ((item = cJSON_GetObjectItem(propsObj, "gap")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_GAP;
        props->gap = (float)item->valuedouble;
    }

    // Colors
    if ((item = cJSON_GetObjectItem(propsObj, "color")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_COLOR;
        props->color = json_parse_color(item->valuestring);
    }

    if ((item = cJSON_GetObjectItem(propsObj, "background")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_BACKGROUND;
        props->background = json_parse_color(item->valuestring);
    }
    // Also check for "backgroundColor" key (used in stylesheet rules)
    if ((item = cJSON_GetObjectItem(propsObj, "backgroundColor")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_BACKGROUND;
        props->background = json_parse_color(item->valuestring);
    }

    // Font properties
    if ((item = cJSON_GetObjectItem(propsObj, "fontSize"))) {
        if (cJSON_IsString(item)) {
            // New format: "1.5rem", "16px"
            props->font_size = ir_json_parse_dimension(item->valuestring);
            props->set_flags |= IR_PROP_FONT_SIZE;
        } else if (cJSON_IsNumber(item)) {
            // Legacy format: plain number (assume px)
            props->font_size.type = IR_DIMENSION_PX;
            props->font_size.value = (float)item->valuedouble;
            props->set_flags |= IR_PROP_FONT_SIZE;
        }
    }

    if ((item = cJSON_GetObjectItem(propsObj, "fontWeight")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_FONT_WEIGHT;
        props->font_weight = (uint16_t)item->valueint;
    }

    if ((item = cJSON_GetObjectItem(propsObj, "lineHeight")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_LINE_HEIGHT;
        props->line_height = (float)item->valuedouble;
    }

    if ((item = cJSON_GetObjectItem(propsObj, "letterSpacing")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_LETTER_SPACING;
        props->letter_spacing = (float)item->valuedouble;
    }

    if ((item = cJSON_GetObjectItem(propsObj, "fontFamily")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_FONT_FAMILY;
        props->font_family = strdup(item->valuestring);
    }

    // Text align
    if ((item = cJSON_GetObjectItem(propsObj, "textAlign")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_TEXT_ALIGN;
        if (strcmp(item->valuestring, "center") == 0) props->text_align = IR_TEXT_ALIGN_CENTER;
        else if (strcmp(item->valuestring, "right") == 0) props->text_align = IR_TEXT_ALIGN_RIGHT;
        else if (strcmp(item->valuestring, "justify") == 0) props->text_align = IR_TEXT_ALIGN_JUSTIFY;
        else props->text_align = IR_TEXT_ALIGN_LEFT;
    }

    // Opacity
    if ((item = cJSON_GetObjectItem(propsObj, "opacity")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_OPACITY;
        props->opacity = (float)item->valuedouble;
    }

    // Border - only set IR_PROP_BORDER if borderWidth is present, not just borderColor
    // borderColor alone can be used with individual border sides (borderTopWidth, etc.)
    if ((item = cJSON_GetObjectItem(propsObj, "borderColor")) && cJSON_IsString(item)) {
        props->border_color = json_parse_color(item->valuestring);
    }

    if ((item = cJSON_GetObjectItem(propsObj, "borderWidth")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_BORDER;
        props->border_width = (float)item->valuedouble;
    }

    if ((item = cJSON_GetObjectItem(propsObj, "borderRadius")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_BORDER_RADIUS;
        props->border_radius = (uint8_t)item->valueint;
    }

    // Border color only (for hover states, etc.)
    if ((item = cJSON_GetObjectItem(propsObj, "borderColorOnly")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_BORDER_COLOR;
        props->border_color = json_parse_color(item->valuestring);
    }

    // Individual border sides
    if ((item = cJSON_GetObjectItem(propsObj, "borderTopWidth")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_BORDER_TOP;
        props->border_width_top = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(propsObj, "borderRightWidth")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_BORDER_RIGHT;
        props->border_width_right = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(propsObj, "borderBottomWidth")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_BORDER_BOTTOM;
        props->border_width_bottom = (float)item->valuedouble;
    }
    if ((item = cJSON_GetObjectItem(propsObj, "borderLeftWidth")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_BORDER_LEFT;
        props->border_width_left = (float)item->valuedouble;
    }

    // Padding - support both array [top, right, bottom, left] and object formats
    cJSON* paddingObj = cJSON_GetObjectItem(propsObj, "padding");
    if (paddingObj) {
        props->set_flags |= IR_PROP_PADDING;
        props->padding.set_flags = IR_SPACING_SET_ALL;
        if (cJSON_IsArray(paddingObj) && cJSON_GetArraySize(paddingObj) >= 4) {
            props->padding.top = (float)cJSON_GetArrayItem(paddingObj, 0)->valuedouble;
            props->padding.right = (float)cJSON_GetArrayItem(paddingObj, 1)->valuedouble;
            props->padding.bottom = (float)cJSON_GetArrayItem(paddingObj, 2)->valuedouble;
            props->padding.left = (float)cJSON_GetArrayItem(paddingObj, 3)->valuedouble;
        } else if (cJSON_IsObject(paddingObj)) {
            if ((item = cJSON_GetObjectItem(paddingObj, "top"))) props->padding.top = (float)item->valuedouble;
            if ((item = cJSON_GetObjectItem(paddingObj, "right"))) props->padding.right = (float)item->valuedouble;
            if ((item = cJSON_GetObjectItem(paddingObj, "bottom"))) props->padding.bottom = (float)item->valuedouble;
            if ((item = cJSON_GetObjectItem(paddingObj, "left"))) props->padding.left = (float)item->valuedouble;
        }
    }

    // Margin - support both array [top, right, bottom, left] and object formats
    cJSON* marginObj = cJSON_GetObjectItem(propsObj, "margin");
    if (marginObj) {
        props->set_flags |= IR_PROP_MARGIN;
        props->margin.set_flags = IR_SPACING_SET_ALL;
        if (cJSON_IsArray(marginObj) && cJSON_GetArraySize(marginObj) >= 4) {
            props->margin.top = (float)cJSON_GetArrayItem(marginObj, 0)->valuedouble;
            props->margin.right = (float)cJSON_GetArrayItem(marginObj, 1)->valuedouble;
            props->margin.bottom = (float)cJSON_GetArrayItem(marginObj, 2)->valuedouble;
            props->margin.left = (float)cJSON_GetArrayItem(marginObj, 3)->valuedouble;
        } else if (cJSON_IsObject(marginObj)) {
            if ((item = cJSON_GetObjectItem(marginObj, "top"))) props->margin.top = (float)item->valuedouble;
            if ((item = cJSON_GetObjectItem(marginObj, "right"))) props->margin.right = (float)item->valuedouble;
            if ((item = cJSON_GetObjectItem(marginObj, "bottom"))) props->margin.bottom = (float)item->valuedouble;
            if ((item = cJSON_GetObjectItem(marginObj, "left"))) props->margin.left = (float)item->valuedouble;
        }
    }

    // Dimensions - handle both string ("56px") and numeric (56) values
    if ((item = cJSON_GetObjectItem(propsObj, "width"))) {
        if (cJSON_IsString(item)) {
            props->set_flags |= IR_PROP_WIDTH;
            props->width = ir_json_parse_dimension(item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            props->set_flags |= IR_PROP_WIDTH;
            props->width.value = (float)item->valuedouble;
            props->width.type = IR_DIMENSION_PX;
        }
    }
    if ((item = cJSON_GetObjectItem(propsObj, "height"))) {
        if (cJSON_IsString(item)) {
            props->set_flags |= IR_PROP_HEIGHT;
            props->height = ir_json_parse_dimension(item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            props->set_flags |= IR_PROP_HEIGHT;
            props->height.value = (float)item->valuedouble;
            props->height.type = IR_DIMENSION_PX;
        }
    }
    if ((item = cJSON_GetObjectItem(propsObj, "minWidth")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_MIN_WIDTH;
        props->min_width = ir_json_parse_dimension(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(propsObj, "maxWidth")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_MAX_WIDTH;
        props->max_width = ir_json_parse_dimension(item->valuestring);
    }

    // Background image (gradient string)
    if ((item = cJSON_GetObjectItem(propsObj, "backgroundImage")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_BACKGROUND_IMAGE;
        if (props->background_image) free(props->background_image);
        props->background_image = strdup(item->valuestring);
    }

    // Background clip
    if ((item = cJSON_GetObjectItem(propsObj, "backgroundClip")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_BACKGROUND_CLIP;
        if (strcmp(item->valuestring, "text") == 0) {
            props->background_clip = IR_BACKGROUND_CLIP_TEXT;
        } else if (strcmp(item->valuestring, "content-box") == 0) {
            props->background_clip = IR_BACKGROUND_CLIP_CONTENT_BOX;
        } else if (strcmp(item->valuestring, "padding-box") == 0) {
            props->background_clip = IR_BACKGROUND_CLIP_PADDING_BOX;
        } else {
            props->background_clip = IR_BACKGROUND_CLIP_BORDER_BOX;
        }
    }

    // Text fill color
    if ((item = cJSON_GetObjectItem(propsObj, "textFillColor")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_TEXT_FILL_COLOR;
        props->text_fill_color = json_parse_color(item->valuestring);
    }

    // Grid template columns (raw CSS string for roundtrip)
    if ((item = cJSON_GetObjectItem(propsObj, "gridTemplateColumns")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_GRID_TEMPLATE_COLUMNS;
        if (props->grid_template_columns) free(props->grid_template_columns);
        props->grid_template_columns = strdup(item->valuestring);
    }

    // Grid template rows (raw CSS string for roundtrip)
    if ((item = cJSON_GetObjectItem(propsObj, "gridTemplateRows")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_GRID_TEMPLATE_ROWS;
        if (props->grid_template_rows) free(props->grid_template_rows);
        props->grid_template_rows = strdup(item->valuestring);
    }

    // Transform - parse IRTransform object
    if ((item = cJSON_GetObjectItem(propsObj, "transform")) && cJSON_IsObject(item)) {
        cJSON* prop_item;
        // Initialize to identity
        props->transform.scale_x = 1.0f;
        props->transform.scale_y = 1.0f;

        if ((prop_item = cJSON_GetObjectItem(item, "translateX")) && cJSON_IsNumber(prop_item)) {
            props->transform.translate_x = (float)prop_item->valuedouble;
        }
        if ((prop_item = cJSON_GetObjectItem(item, "translateY")) && cJSON_IsNumber(prop_item)) {
            props->transform.translate_y = (float)prop_item->valuedouble;
        }
        if ((prop_item = cJSON_GetObjectItem(item, "scaleX")) && cJSON_IsNumber(prop_item)) {
            props->transform.scale_x = (float)prop_item->valuedouble;
        }
        if ((prop_item = cJSON_GetObjectItem(item, "scaleY")) && cJSON_IsNumber(prop_item)) {
            props->transform.scale_y = (float)prop_item->valuedouble;
        }
        if ((prop_item = cJSON_GetObjectItem(item, "rotate")) && cJSON_IsNumber(prop_item)) {
            props->transform.rotate = (float)prop_item->valuedouble;
        }
        props->set_flags |= IR_PROP_TRANSFORM;
    }

    // Text decoration - parse as number (bitmask)
    if ((item = cJSON_GetObjectItem(propsObj, "textDecoration")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_TEXT_DECORATION;
        props->text_decoration = (uint8_t)item->valueint;
    }

    // Box sizing - parse as number
    if ((item = cJSON_GetObjectItem(propsObj, "boxSizing")) && cJSON_IsNumber(item)) {
        props->set_flags |= IR_PROP_BOX_SIZING;
        props->box_sizing = (uint8_t)item->valueint;
    }

    if ((item = cJSON_GetObjectItem(propsObj, "objectFit")) && cJSON_IsString(item)) {
        props->set_flags |= IR_PROP_OBJECT_FIT;
        const char* val = item->valuestring;
        if (strcmp(val, "contain") == 0) {
            props->object_fit = IR_OBJECT_FIT_CONTAIN;
        } else if (strcmp(val, "cover") == 0) {
            props->object_fit = IR_OBJECT_FIT_COVER;
        } else if (strcmp(val, "none") == 0) {
            props->object_fit = IR_OBJECT_FIT_NONE;
        } else if (strcmp(val, "scale-down") == 0) {
            props->object_fit = IR_OBJECT_FIT_SCALE_DOWN;
        } else {
            props->object_fit = IR_OBJECT_FIT_FILL;
        }
    }
}

// ============================================================================
// Stylesheet Deserialization
// ============================================================================

/**
 * Deserialize stylesheet from JSON object
 * Matches format from json_serialize_stylesheet()
 */
static IRStylesheet* json_deserialize_stylesheet(cJSON* obj) {
    if (!obj || !cJSON_IsObject(obj)) return NULL;

    IRStylesheet* stylesheet = ir_stylesheet_create();
    if (!stylesheet) return NULL;

    // Deserialize CSS variables
    cJSON* variables = cJSON_GetObjectItem(obj, "variables");
    if (variables && cJSON_IsObject(variables)) {
        cJSON* var = NULL;
        cJSON_ArrayForEach(var, variables) {
            if (cJSON_IsString(var) && var->string) {
                ir_stylesheet_add_variable(stylesheet, var->string, var->valuestring);
            }
        }
    }

    // Deserialize rules
    cJSON* rules = cJSON_GetObjectItem(obj, "rules");
    if (rules && cJSON_IsArray(rules)) {
        cJSON* rule = NULL;
        cJSON_ArrayForEach(rule, rules) {
            if (!cJSON_IsObject(rule)) continue;

            // Get selector string
            cJSON* selectorJson = cJSON_GetObjectItem(rule, "selector");
            if (!selectorJson || !cJSON_IsString(selectorJson)) continue;

            const char* selector_str = selectorJson->valuestring;
            if (!selector_str || !selector_str[0]) continue;

            // Get properties object
            cJSON* propsJson = cJSON_GetObjectItem(rule, "properties");
            IRStyleProperties props = {0};
            ir_json_deserialize_style_properties(propsJson, &props);

            // Add rule (ir_stylesheet_add_rule parses the selector string)
            ir_stylesheet_add_rule(stylesheet, selector_str, &props);

            // Free allocated memory (ir_stylesheet_add_rule deep copies them)
            if (props.font_family) {
                free(props.font_family);
            }
            if (props.background_image) {
                free(props.background_image);
            }
            if (props.grid_template_columns) {
                free(props.grid_template_columns);
            }
            if (props.grid_template_rows) {
                free(props.grid_template_rows);
            }
        }
    }

    // Deserialize media queries
    cJSON* media_queries = cJSON_GetObjectItem(obj, "mediaQueries");
    if (media_queries && cJSON_IsArray(media_queries)) {
        cJSON* mq = NULL;
        cJSON_ArrayForEach(mq, media_queries) {
            if (!cJSON_IsObject(mq)) continue;

            cJSON* conditionJson = cJSON_GetObjectItem(mq, "condition");
            cJSON* cssContentJson = cJSON_GetObjectItem(mq, "cssContent");

            if (conditionJson && cJSON_IsString(conditionJson) &&
                cssContentJson && cJSON_IsString(cssContentJson)) {
                ir_stylesheet_add_media_query(stylesheet,
                    conditionJson->valuestring,
                    cssContentJson->valuestring);
            }
        }
    }

    return stylesheet;
}

IRStylesheet* ir_json_deserialize_stylesheet(cJSON* obj) {
    return json_deserialize_stylesheet(obj);
}

// ============================================================================
// C Metadata Deserialization (for C->KIR->C round-trip)
// ============================================================================

/**
 * Deserialize C metadata from JSON into global g_c_metadata
 * Used for C->KIR->C round-trip to restore event handlers and other C code
 */
static void json_deserialize_c_metadata_helper(cJSON* c_meta_obj) {
    if (!c_meta_obj || !cJSON_IsObject(c_meta_obj)) return;

    // Clean up existing metadata before loading new data
    ir_c_metadata_cleanup(&g_c_metadata);

    // Deserialize event_handlers
    cJSON* handlers = cJSON_GetObjectItem(c_meta_obj, "event_handlers");
    if (handlers && cJSON_IsArray(handlers)) {
        int count = cJSON_GetArraySize(handlers);
        if (count > 0) {
            g_c_metadata.event_handlers = (CEventHandlerDecl*)calloc(count, sizeof(CEventHandlerDecl));
            g_c_metadata.event_handler_count = 0;
            g_c_metadata.event_handler_capacity = count;

            for (int i = 0; i < count; i++) {
                cJSON* handler_obj = cJSON_GetArrayItem(handlers, i);
                if (!handler_obj || !cJSON_IsObject(handler_obj)) continue;

                CEventHandlerDecl* handler = &g_c_metadata.event_handlers[g_c_metadata.event_handler_count];

                cJSON* item = cJSON_GetObjectItem(handler_obj, "logic_id");
                if (item && cJSON_IsString(item)) {
                    handler->logic_id = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(handler_obj, "function_name");
                if (item && cJSON_IsString(item)) {
                    handler->function_name = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(handler_obj, "return_type");
                if (item && cJSON_IsString(item)) {
                    handler->return_type = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(handler_obj, "parameters");
                if (item && cJSON_IsString(item)) {
                    handler->parameters = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(handler_obj, "body");
                if (item && cJSON_IsString(item)) {
                    handler->body = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(handler_obj, "line_number");
                if (item && cJSON_IsNumber(item)) {
                    handler->line_number = (int)item->valuedouble;
                }

                g_c_metadata.event_handler_count++;
            }
        }
    }

    // Deserialize variables
    cJSON* variables = cJSON_GetObjectItem(c_meta_obj, "variables");
    if (variables && cJSON_IsArray(variables)) {
        int count = cJSON_GetArraySize(variables);
        if (count > 0) {
            g_c_metadata.variables = (CVariableDecl*)calloc(count, sizeof(CVariableDecl));
            g_c_metadata.variable_count = 0;
            g_c_metadata.variable_capacity = count;

            for (int i = 0; i < count; i++) {
                cJSON* var_obj = cJSON_GetArrayItem(variables, i);
                if (!var_obj || !cJSON_IsObject(var_obj)) continue;

                CVariableDecl* var = &g_c_metadata.variables[g_c_metadata.variable_count];

                cJSON* item = cJSON_GetObjectItem(var_obj, "name");
                if (item && cJSON_IsString(item)) {
                    var->name = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(var_obj, "type");
                if (item && cJSON_IsString(item)) {
                    var->type = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(var_obj, "storage");
                if (item && cJSON_IsString(item)) {
                    var->storage = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(var_obj, "initial_value");
                if (item && cJSON_IsString(item)) {
                    var->initial_value = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(var_obj, "component_id");
                if (item && cJSON_IsNumber(item)) {
                    var->component_id = (uint32_t)item->valuedouble;
                }

                item = cJSON_GetObjectItem(var_obj, "line_number");
                if (item && cJSON_IsNumber(item)) {
                    var->line_number = (int)item->valuedouble;
                }

                g_c_metadata.variable_count++;
            }
        }
    }

    // Deserialize helper_functions
    cJSON* helpers = cJSON_GetObjectItem(c_meta_obj, "helper_functions");
    if (helpers && cJSON_IsArray(helpers)) {
        int count = cJSON_GetArraySize(helpers);
        if (count > 0) {
            g_c_metadata.helper_functions = (CHelperFunction*)calloc(count, sizeof(CHelperFunction));
            g_c_metadata.helper_function_count = 0;
            g_c_metadata.helper_function_capacity = count;

            for (int i = 0; i < count; i++) {
                cJSON* helper_obj = cJSON_GetArrayItem(helpers, i);
                if (!helper_obj || !cJSON_IsObject(helper_obj)) continue;

                CHelperFunction* helper = &g_c_metadata.helper_functions[g_c_metadata.helper_function_count];

                cJSON* item = cJSON_GetObjectItem(helper_obj, "name");
                if (item && cJSON_IsString(item)) {
                    helper->name = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(helper_obj, "return_type");
                if (item && cJSON_IsString(item)) {
                    helper->return_type = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(helper_obj, "parameters");
                if (item && cJSON_IsString(item)) {
                    helper->parameters = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(helper_obj, "body");
                if (item && cJSON_IsString(item)) {
                    helper->body = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(helper_obj, "line_number");
                if (item && cJSON_IsNumber(item)) {
                    helper->line_number = (int)item->valuedouble;
                }

                g_c_metadata.helper_function_count++;
            }
        }
    }

    // Deserialize includes
    cJSON* includes = cJSON_GetObjectItem(c_meta_obj, "includes");
    if (includes && cJSON_IsArray(includes)) {
        int count = cJSON_GetArraySize(includes);
        if (count > 0) {
            g_c_metadata.includes = (CInclude*)calloc(count, sizeof(CInclude));
            g_c_metadata.include_count = 0;
            g_c_metadata.include_capacity = count;

            for (int i = 0; i < count; i++) {
                cJSON* inc_obj = cJSON_GetArrayItem(includes, i);
                if (!inc_obj || !cJSON_IsObject(inc_obj)) continue;

                CInclude* inc = &g_c_metadata.includes[g_c_metadata.include_count];

                cJSON* item = cJSON_GetObjectItem(inc_obj, "include_string");
                if (item && cJSON_IsString(item)) {
                    inc->include_string = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(inc_obj, "is_system");
                if (item && cJSON_IsBool(item)) {
                    inc->is_system = cJSON_IsTrue(item);
                }

                item = cJSON_GetObjectItem(inc_obj, "line_number");
                if (item && cJSON_IsNumber(item)) {
                    inc->line_number = (int)item->valuedouble;
                }

                g_c_metadata.include_count++;
            }
        }
    }

    // Deserialize preprocessor_directives
    cJSON* directives = cJSON_GetObjectItem(c_meta_obj, "preprocessor_directives");
    if (directives && cJSON_IsArray(directives)) {
        int count = cJSON_GetArraySize(directives);
        if (count > 0) {
            g_c_metadata.preprocessor_directives = (CPreprocessorDirective*)calloc(count, sizeof(CPreprocessorDirective));
            g_c_metadata.preprocessor_directive_count = 0;
            g_c_metadata.preprocessor_directive_capacity = count;

            for (int i = 0; i < count; i++) {
                cJSON* dir_obj = cJSON_GetArrayItem(directives, i);
                if (!dir_obj || !cJSON_IsObject(dir_obj)) continue;

                CPreprocessorDirective* dir = &g_c_metadata.preprocessor_directives[g_c_metadata.preprocessor_directive_count];

                cJSON* item = cJSON_GetObjectItem(dir_obj, "directive_type");
                if (item && cJSON_IsString(item)) {
                    dir->directive_type = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(dir_obj, "condition");
                if (item && cJSON_IsString(item)) {
                    dir->condition = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(dir_obj, "value");
                if (item && cJSON_IsString(item)) {
                    dir->value = strdup(item->valuestring);
                }

                item = cJSON_GetObjectItem(dir_obj, "line_number");
                if (item && cJSON_IsNumber(item)) {
                    dir->line_number = (int)item->valuedouble;
                }

                g_c_metadata.preprocessor_directive_count++;
            }
        }
    }
}

void ir_json_deserialize_c_metadata(cJSON* c_meta_obj) {
    json_deserialize_c_metadata_helper(c_meta_obj);
}

// ============================================================================
// Component Type Conversion
// ============================================================================

/**
 * Convert component type string to enum
 * Made public for use by HTML parser
 */
IRComponentType ir_string_to_component_type(const char* str) {
    if (!str) return IR_COMPONENT_CONTAINER;
    // CamelCase variants (from .kir files)
    if (strcmp(str, "Container") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(str, "Body") == 0) return IR_COMPONENT_CONTAINER;  // Body is an alias for Container
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
    if (strcmp(str, "Modal") == 0) return IR_COMPONENT_MODAL;
    if (strcmp(str, "Markdown") == 0) return IR_COMPONENT_MARKDOWN;
    if (strcmp(str, "TabGroup") == 0) return IR_COMPONENT_TAB_GROUP;
    if (strcmp(str, "TabBar") == 0) return IR_COMPONENT_TAB_BAR;
    if (strcmp(str, "Tab") == 0) return IR_COMPONENT_TAB;
    if (strcmp(str, "TabContent") == 0) return IR_COMPONENT_TAB_CONTENT;
    if (strcmp(str, "TabPanel") == 0) return IR_COMPONENT_TAB_PANEL;
    // UPPERCASE variants (from HTML transpiler data-ir-type attributes)
    if (strcmp(str, "CONTAINER") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(str, "BODY") == 0) return IR_COMPONENT_CONTAINER;  // Body is an alias for Container
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
    if (strcmp(str, "MODAL") == 0) return IR_COMPONENT_MODAL;
    if (strcmp(str, "MARKDOWN") == 0) return IR_COMPONENT_MARKDOWN;
    if (strcmp(str, "TAB_GROUP") == 0) return IR_COMPONENT_TAB_GROUP;
    if (strcmp(str, "TAB_BAR") == 0) return IR_COMPONENT_TAB_BAR;
    if (strcmp(str, "TAB") == 0) return IR_COMPONENT_TAB;
    if (strcmp(str, "TAB_CONTENT") == 0) return IR_COMPONENT_TAB_CONTENT;
    if (strcmp(str, "TAB_PANEL") == 0) return IR_COMPONENT_TAB_PANEL;
    if (strcmp(str, "TABGROUP") == 0) return IR_COMPONENT_TAB_GROUP;
    if (strcmp(str, "TABBAR") == 0) return IR_COMPONENT_TAB_BAR;
    if (strcmp(str, "TABCONTENT") == 0) return IR_COMPONENT_TAB_CONTENT;
    if (strcmp(str, "TABPANEL") == 0) return IR_COMPONENT_TAB_PANEL;
    // Table components
    if (strcmp(str, "Table") == 0) return IR_COMPONENT_TABLE;
    if (strcmp(str, "TableHead") == 0) return IR_COMPONENT_TABLE_HEAD;
    if (strcmp(str, "TableBody") == 0) return IR_COMPONENT_TABLE_BODY;
    if (strcmp(str, "TableFoot") == 0) return IR_COMPONENT_TABLE_FOOT;
    if (strcmp(str, "TableRow") == 0 || strcmp(str, "Tr") == 0) return IR_COMPONENT_TABLE_ROW;
    if (strcmp(str, "TableCell") == 0 || strcmp(str, "Td") == 0) return IR_COMPONENT_TABLE_CELL;
    if (strcmp(str, "TableHeaderCell") == 0 || strcmp(str, "Th") == 0) return IR_COMPONENT_TABLE_HEADER_CELL;
    if (strcmp(str, "TABLE_HEAD") == 0) return IR_COMPONENT_TABLE_HEAD;
    if (strcmp(str, "TABLE_BODY") == 0) return IR_COMPONENT_TABLE_BODY;
    if (strcmp(str, "TABLE_FOOT") == 0) return IR_COMPONENT_TABLE_FOOT;
    if (strcmp(str, "TABLE_ROW") == 0) return IR_COMPONENT_TABLE_ROW;
    if (strcmp(str, "TABLE_CELL") == 0) return IR_COMPONENT_TABLE_CELL;
    if (strcmp(str, "TABLE_HEADER_CELL") == 0) return IR_COMPONENT_TABLE_HEADER_CELL;
    if (strcmp(str, "TABLEHEAD") == 0) return IR_COMPONENT_TABLE_HEAD;
    if (strcmp(str, "TABLEBODY") == 0) return IR_COMPONENT_TABLE_BODY;
    if (strcmp(str, "TABLEFOOT") == 0) return IR_COMPONENT_TABLE_FOOT;
    if (strcmp(str, "TABLEROW") == 0) return IR_COMPONENT_TABLE_ROW;
    if (strcmp(str, "TABLECELL") == 0) return IR_COMPONENT_TABLE_CELL;
    if (strcmp(str, "TABLEHEADERCELL") == 0) return IR_COMPONENT_TABLE_HEADER_CELL;
    if (strcmp(str, "Heading") == 0) return IR_COMPONENT_HEADING;
    if (strcmp(str, "Paragraph") == 0) return IR_COMPONENT_PARAGRAPH;
    if (strcmp(str, "Blockquote") == 0) return IR_COMPONENT_BLOCKQUOTE;
    if (strcmp(str, "CodeBlock") == 0) return IR_COMPONENT_CODE_BLOCK;
    if (strcmp(str, "HorizontalRule") == 0) return IR_COMPONENT_HORIZONTAL_RULE;
    if (strcmp(str, "List") == 0) return IR_COMPONENT_LIST;
    if (strcmp(str, "ListItem") == 0) return IR_COMPONENT_LIST_ITEM;
    if (strcmp(str, "Link") == 0) return IR_COMPONENT_LINK;
    if (strcmp(str, "Custom") == 0) return IR_COMPONENT_CUSTOM;
    // Uppercase markdown components (from HTML transpiler)
    if (strcmp(str, "CODE_BLOCK") == 0) return IR_COMPONENT_CODE_BLOCK;
    if (strcmp(str, "CODEBLOCK") == 0) return IR_COMPONENT_CODE_BLOCK;
    if (strcmp(str, "HORIZONTAL_RULE") == 0) return IR_COMPONENT_HORIZONTAL_RULE;
    if (strcmp(str, "HORIZONTALRULE") == 0) return IR_COMPONENT_HORIZONTAL_RULE;
    if (strcmp(str, "LIST_ITEM") == 0) return IR_COMPONENT_LIST_ITEM;
    if (strcmp(str, "LISTITEM") == 0) return IR_COMPONENT_LIST_ITEM;
    // Inline semantic components (for rich text)
    if (strcmp(str, "Span") == 0) return IR_COMPONENT_SPAN;
    if (strcmp(str, "Strong") == 0) return IR_COMPONENT_STRONG;
    if (strcmp(str, "Em") == 0) return IR_COMPONENT_EM;
    if (strcmp(str, "CodeInline") == 0) return IR_COMPONENT_CODE_INLINE;
    if (strcmp(str, "Small") == 0) return IR_COMPONENT_SMALL;
    if (strcmp(str, "Mark") == 0) return IR_COMPONENT_MARK;
    // Uppercase variants for inline semantic components
    if (strcmp(str, "SPAN") == 0) return IR_COMPONENT_SPAN;
    if (strcmp(str, "STRONG") == 0) return IR_COMPONENT_STRONG;
    if (strcmp(str, "EM") == 0) return IR_COMPONENT_EM;
    if (strcmp(str, "CODE_INLINE") == 0) return IR_COMPONENT_CODE_INLINE;
    if (strcmp(str, "CODEINLINE") == 0) return IR_COMPONENT_CODE_INLINE;
    if (strcmp(str, "SMALL") == 0) return IR_COMPONENT_SMALL;
    if (strcmp(str, "MARK") == 0) return IR_COMPONENT_MARK;
    // Source structure types (for round-trip codegen)
    if (strcmp(str, "StaticBlock") == 0) return IR_COMPONENT_STATIC_BLOCK;
    if (strcmp(str, "ForLoop") == 0) return IR_COMPONENT_FOR_LOOP;
    if (strcmp(str, "ForEach") == 0) return IR_COMPONENT_FOR_EACH;
    if (strcmp(str, "VarDecl") == 0) return IR_COMPONENT_VAR_DECL;
    if (strcmp(str, "Placeholder") == 0) return IR_COMPONENT_PLACEHOLDER;
    // Uppercase variants for source structure types
    if (strcmp(str, "STATICBLOCK") == 0) return IR_COMPONENT_STATIC_BLOCK;
    if (strcmp(str, "FORLOOP") == 0) return IR_COMPONENT_FOR_LOOP;
    if (strcmp(str, "FOREACH") == 0) return IR_COMPONENT_FOR_EACH;
    if (strcmp(str, "VARDECL") == 0) return IR_COMPONENT_VAR_DECL;
    if (strcmp(str, "PLACEHOLDER") == 0) return IR_COMPONENT_PLACEHOLDER;
    return IR_COMPONENT_CONTAINER;
}

// ============================================================================
// ============================================================================
// Component ID Remapping Helpers
// ============================================================================

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

// ============================================================================
// Component Tree Deserialization (Recursive)
// ============================================================================

/**
 * Recursively deserialize component from JSON with component definition expansion
 * This is the main deserialization function
 */
static IRComponent* json_deserialize_component_with_context(cJSON* json, ComponentDefContext* ctx) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    // Track if we created the context (for cleanup)
    int ctx_created = 0;

    // Check if this is a custom component that needs expansion
    cJSON* typeItem = cJSON_GetObjectItem(json, "type");
    if (typeItem && cJSON_IsString(typeItem)) {
        const char* typeName = typeItem->valuestring;

        // Check for module reference: $module:components/tabs or $module:components/tabs#buildTabsAndPanels
        // Note: We use a global cache for module definitions to avoid reloading
        if (strncmp(typeName, "$module:", 8) == 0) {
            // Initialize global cache if needed
            if (!g_module_def_cache) {
                g_module_def_cache = ir_json_context_create();
            }
            // Use global cache as context
            ctx = g_module_def_cache;
            ctx_created = 0;  // Never free the global cache here

            const char* moduleRef = typeName + 8;  // Skip "$module:" prefix

            // Parse module reference (format: "module_id" or "module_id#export_name")
            char* moduleId = strdup(moduleRef);
            char* exportName = NULL;
            char* hash = strchr(moduleId, '#');
            if (hash) {
                *hash = '\0';
                exportName = hash + 1;
            }

            // Try to load module KIR file from cache directory
            const char* cacheDir = getenv("KRYON_CACHE_DIR");
            if (!cacheDir) cacheDir = ".kryon_cache";

            char kirPath[4096];
            snprintf(kirPath, sizeof(kirPath), "%s/%s.kir", cacheDir, moduleId);

            // Check if module KIR file exists
            FILE* f = fopen(kirPath, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);

                char* kirContent = malloc(size + 1);
                if (kirContent) {
                    fread(kirContent, 1, size, f);
                    kirContent[size] = '\0';

                    cJSON* moduleKir = cJSON_Parse(kirContent);
                    if (moduleKir) {
                        // Get component_definitions from module KIR
                        cJSON* compDefs = cJSON_GetObjectItem(moduleKir, "component_definitions");
                        if (compDefs && cJSON_IsArray(compDefs)) {
                            // Add each component definition to the global cache
                            cJSON* def = compDefs->child;
                            while (def) {
                                // Get the component name from the definition
                                cJSON* nameItem = cJSON_GetObjectItem(def, "name");
                                if (nameItem && cJSON_IsString(nameItem)) {
                                    // Create a scoped name for the definition
                                    char scopedName[1024];
                                    if (exportName) {
                                        // If export name specified, only add that one
                                        if (strcmp(nameItem->valuestring, exportName) == 0) {
                                            snprintf(scopedName, sizeof(scopedName), "%s/%s", moduleId, nameItem->valuestring);
                                            ir_json_context_add(ctx, scopedName, def);
                                        }
                                    } else {
                                        // Add all exports from this module
                                        snprintf(scopedName, sizeof(scopedName), "%s/%s", moduleId, nameItem->valuestring);
                                        ir_json_context_add(ctx, scopedName, def);
                                    }
                                }
                                def = def->next;
                            }
                        }
                        // Don't delete moduleKir - the cJSON pointers are stored in the cache
                        // and will be cleaned up when the cache is freed
                    }
                    free(kirContent);
                }
                fclose(f);
            }

            // Now look up the definition using the module reference
            // The typeName is "$module:..." but we need to look it up as "moduleId/exportName"
            char lookupName[512];
            if (exportName) {
                snprintf(lookupName, sizeof(lookupName), "%s/%s", moduleId, exportName);
            } else {
                // Use the full module ref as lookup
                strncpy(lookupName, moduleRef, sizeof(lookupName) - 1);
                lookupName[sizeof(lookupName) - 1] = '\0';
            }

            cJSON* definition = ir_json_context_lookup(ctx, lookupName);
            // Fallback: if no export name was specified, try to find any definition from this module
            if (!definition && !exportName) {
                definition = ir_json_context_lookup_by_module(ctx, moduleId);
            }
            if (definition) {
                // Found the module component definition - expand it
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
                        cJSON_Delete(expandedTemplate);

                        if (expanded) {
                            // Copy ID from the instance
                            cJSON* idItem = cJSON_GetObjectItem(json, "id");
                            if (idItem && cJSON_IsNumber(idItem)) {
                                uint32_t instanceId = (uint32_t)idItem->valueint;
                                remap_ids_recursive(expanded);
                                expanded->id = instanceId;
                                set_owner_instance_recursive(expanded, instanceId);
                            }
                            // Set module_ref on the expanded component
                            expanded->module_ref = strdup(moduleId);
                            if (exportName) {
                                expanded->export_name = strdup(exportName);
                            }

                            // Apply instance-specific properties from the module reference
                            // These override template values (e.g., tab title, colors)
                            cJSON* textItem = cJSON_GetObjectItem(json, "text");
                            if (textItem && cJSON_IsString(textItem)) {
                                if (expanded->text_content) free(expanded->text_content);
                                expanded->text_content = strdup(textItem->valuestring);
                            }

                            // Apply background color if specified
                            cJSON* bgItem = cJSON_GetObjectItem(json, "background");
                            if (bgItem && cJSON_IsString(bgItem) && expanded->style) {
                                IRColor color = json_parse_color(bgItem->valuestring);
                                expanded->style->background = color;
                            }

                            // Apply text color if specified
                            cJSON* colorItem = cJSON_GetObjectItem(json, "color");
                            if (colorItem && cJSON_IsString(colorItem) && expanded->style) {
                                IRColor color = json_parse_color(colorItem->valuestring);
                                expanded->style->font.color = color;
                            }

                            free(moduleId);
                            return expanded;
                        }
                    }
                }
            }

            free(moduleId);

            // If we get here, module loading failed - check for actual_type fallback
            // This preserves the original component type (e.g., Button) instead of defaulting to Container
            cJSON* actualTypeItem = cJSON_GetObjectItem(json, "actual_type");
            if (actualTypeItem && cJSON_IsString(actualTypeItem)) {
                // Use the preserved actual type - replace the type item temporarily
                // so the normal type parsing below uses the correct type
                cJSON_ReplaceItemInObject(json, "type", cJSON_CreateString(actualTypeItem->valuestring));
            }
            // Fall through to create component with correct type
        }

        cJSON* definition = ir_json_context_lookup(ctx, typeName);
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

    // Mark as externally allocated (not from component pool)
    // This ensures ir_destroy_component will free it directly instead of returning to pool
    component->is_externally_allocated = true;

    // Initialize basic fields
    component->style = ir_create_style();
    component->layout = ir_create_layout();

    // Initialize layout state for two-pass layout system
    component->layout_state = calloc(1, sizeof(IRLayoutState));
    if (component->layout_state) {
        component->layout_state->dirty = true;  // Start dirty, needs initial layout
        // intrinsic_valid = false (default from calloc)
        // layout_valid = false (default from calloc)
    }

    // Initialize layout cache to dirty state (uncached)
    component->layout_cache.dirty = true;
    component->layout_cache.cached_intrinsic_width = -1.0f;
    component->layout_cache.cached_intrinsic_height = -1.0f;

    cJSON* item = NULL;

    // ID
    if ((item = cJSON_GetObjectItem(json, "id")) != NULL && cJSON_IsNumber(item)) {
        component->id = (uint32_t)item->valueint;
    }

    // Type
    if ((item = cJSON_GetObjectItem(json, "type")) != NULL && cJSON_IsString(item)) {
        const char* type_str = item->valuestring;
        IRComponentType parsed_type = ir_string_to_component_type(type_str);
        component->type = parsed_type;

        if (getenv("KRYON_DEBUG_DESER")) {
            fprintf(stderr, "[DESER] Component id=%u type_str='%s' -> type=%d\n",
                    component->id, type_str, parsed_type);
        }

        // Preserve "Body" tag for HTML generator
        if (strcmp(type_str, "Body") == 0) {
            component->tag = strdup("Body");
        }
    }

    // Semantic tag (for roundtrip HTML preservation)
    if ((item = cJSON_GetObjectItem(json, "tag")) != NULL && cJSON_IsString(item)) {
        // Only set if not already set (e.g., by "Body" above)
        if (!component->tag) {
            component->tag = strdup(item->valuestring);
        }
    }

    // CSS class name (for styling, separate from semantic tag)
    if ((item = cJSON_GetObjectItem(json, "css_class")) != NULL && cJSON_IsString(item)) {
        component->css_class = strdup(item->valuestring);
    }

    // CSS selector type (for accurate roundtrip)
    if ((item = cJSON_GetObjectItem(json, "selector_type")) != NULL && cJSON_IsString(item)) {
        if (strcmp(item->valuestring, "element") == 0) {
            component->selector_type = IR_SELECTOR_ELEMENT;
        } else if (strcmp(item->valuestring, "class") == 0) {
            component->selector_type = IR_SELECTOR_CLASS;
        } else if (strcmp(item->valuestring, "id") == 0) {
            component->selector_type = IR_SELECTOR_ID;
        }
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

    // Scope (for scoped variable lookups)
    if ((item = cJSON_GetObjectItem(json, "scope")) != NULL && cJSON_IsString(item)) {
        component->scope = strdup(item->valuestring);
    }

    // Deserialize style and layout
    json_deserialize_style(json, component->style);

    // Ensure layout exists before deserializing (needed for alignItems, justifyContent, etc.)
    if (!component->layout) {
        component->layout = ir_get_layout(component);
    }
    json_deserialize_layout(json, component->layout);

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

    // Parse modal state
    if (component->type == IR_COMPONENT_MODAL) {
        cJSON* modalStateObj = cJSON_GetObjectItem(json, "modal_state");
        if (modalStateObj && cJSON_IsObject(modalStateObj)) {
            cJSON* isOpenItem = cJSON_GetObjectItem(modalStateObj, "isOpen");
            cJSON* titleItem = cJSON_GetObjectItem(modalStateObj, "title");

            bool is_open = (isOpenItem && cJSON_IsBool(isOpenItem)) ? cJSON_IsTrue(isOpenItem) : false;
            const char* title = (titleItem && cJSON_IsString(titleItem)) ? titleItem->valuestring : NULL;

            // Build state string: "open|title" or "closed|title"
            char state_str[256];
            if (title) {
                snprintf(state_str, sizeof(state_str), "%s|%s", is_open ? "open" : "closed", title);
            } else {
                snprintf(state_str, sizeof(state_str), "%s", is_open ? "open" : "closed");
            }
            ir_set_custom_data(component, state_str);
        }
    }

    // Parse Link component data (url, target, rel)
    if (component->type == IR_COMPONENT_LINK && !component->custom_data) {
        typedef struct { char* url; char* title; char* target; char* rel; } IRLinkData;
        IRLinkData* link_data = (IRLinkData*)calloc(1, sizeof(IRLinkData));
        if (link_data) {
            // Read url field (new format)
            cJSON* urlItem = cJSON_GetObjectItem(json, "url");
            if (urlItem && cJSON_IsString(urlItem)) {
                link_data->url = strdup(urlItem->valuestring);
            }
            // Read title field
            cJSON* titleItem = cJSON_GetObjectItem(json, "title");
            if (titleItem && cJSON_IsString(titleItem)) {
                link_data->title = strdup(titleItem->valuestring);
            }
            // Read target field
            cJSON* targetItem = cJSON_GetObjectItem(json, "target");
            if (targetItem && cJSON_IsString(targetItem)) {
                link_data->target = strdup(targetItem->valuestring);
            }
            // Read rel field
            cJSON* relItem = cJSON_GetObjectItem(json, "rel");
            if (relItem && cJSON_IsString(relItem)) {
                link_data->rel = strdup(relItem->valuestring);
            }
            component->custom_data = (char*)link_data;
        }
    }

    // Parse Heading component data (level, text, id)
    if (component->type == IR_COMPONENT_HEADING && !component->custom_data) {
        IRHeadingData* heading_data = (IRHeadingData*)calloc(1, sizeof(IRHeadingData));
        if (heading_data) {
            // Read level field
            cJSON* levelItem = cJSON_GetObjectItem(json, "level");
            if (levelItem && cJSON_IsNumber(levelItem)) {
                heading_data->level = (uint8_t)levelItem->valueint;
                if (heading_data->level < 1) heading_data->level = 1;
                if (heading_data->level > 6) heading_data->level = 6;
            } else {
                heading_data->level = 1;  // Default to h1
            }
            // Read text field
            cJSON* textItem = cJSON_GetObjectItem(json, "text");
            if (textItem && cJSON_IsString(textItem)) {
                heading_data->text = strdup(textItem->valuestring);
            }
            // Read id field
            cJSON* idItem = cJSON_GetObjectItem(json, "id");
            if (idItem && cJSON_IsString(idItem)) {
                heading_data->id = strdup(idItem->valuestring);
            }
            component->custom_data = (char*)heading_data;
        }
    }

    // Parse List component data (type, start, tight)
    if (component->type == IR_COMPONENT_LIST && !component->custom_data) {
        IRListData* list_data = (IRListData*)calloc(1, sizeof(IRListData));
        if (list_data) {
            // Read type field (0=unordered, 1=ordered)
            cJSON* typeItem = cJSON_GetObjectItem(json, "list_type");
            if (typeItem && cJSON_IsNumber(typeItem)) {
                list_data->type = (IRListType)typeItem->valueint;
            } else {
                list_data->type = IR_LIST_UNORDERED;  // Default to unordered
            }
            // Read start field
            cJSON* startItem = cJSON_GetObjectItem(json, "start");
            if (startItem && cJSON_IsNumber(startItem)) {
                list_data->start = (uint32_t)startItem->valueint;
            } else {
                list_data->start = 1;
            }
            // Read tight field
            cJSON* tightItem = cJSON_GetObjectItem(json, "tight");
            if (tightItem && cJSON_IsBool(tightItem)) {
                list_data->tight = cJSON_IsTrue(tightItem);
            } else {
                list_data->tight = false;
            }
            component->custom_data = (char*)list_data;
        }
    }

    // Parse CodeBlock component data (language, code, line numbers)
    if (component->type == IR_COMPONENT_CODE_BLOCK && !component->custom_data) {
        FILE* debug_file = fopen("/tmp/ir_deserialize_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "[CODE_BLOCK deserialize] component=%p, type=%d, parsing custom_data\n",
                    (void*)component, component->type);
            fflush(debug_file);
            fclose(debug_file);
        }

        typedef struct {
            char* language;
            char* code;
            size_t length;
            bool show_line_numbers;
            uint32_t start_line;
        } IRCodeBlockData;

        IRCodeBlockData* code_data = (IRCodeBlockData*)calloc(1, sizeof(IRCodeBlockData));
        if (code_data) {
            // Read language field
            cJSON* langItem = cJSON_GetObjectItem(json, "language");
            if (langItem && cJSON_IsString(langItem)) {
                code_data->language = strdup(langItem->valuestring);
            }

            // Read code field
            cJSON* codeItem = cJSON_GetObjectItem(json, "code");
            if (codeItem && cJSON_IsString(codeItem)) {
                code_data->code = strdup(codeItem->valuestring);
                code_data->length = strlen(codeItem->valuestring);
            }

            // Read showLineNumbers field
            cJSON* showLineNumbersItem = cJSON_GetObjectItem(json, "showLineNumbers");
            if (showLineNumbersItem && cJSON_IsBool(showLineNumbersItem)) {
                code_data->show_line_numbers = cJSON_IsTrue(showLineNumbersItem);
            } else {
                code_data->show_line_numbers = false;
            }

            // Read startLine field
            cJSON* startLineItem = cJSON_GetObjectItem(json, "startLine");
            if (startLineItem && cJSON_IsNumber(startLineItem)) {
                code_data->start_line = (uint32_t)startLineItem->valueint;
            } else {
                code_data->start_line = 1;
            }

            component->custom_data = (char*)code_data;

            debug_file = fopen("/tmp/ir_deserialize_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "[CODE_BLOCK deserialize] component=%p, custom_data=%p, language=%s, code=%p, length=%zu\n",
                        (void*)component, (void*)component->custom_data,
                        code_data->language ? code_data->language : "(null)",
                        (void*)code_data->code, code_data->length);
                fclose(debug_file);
            }
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

                // Core events - fast path
                if (strcmp(typeStr, "click") == 0) event->type = IR_EVENT_CLICK;
                else if (strcmp(typeStr, "hover") == 0) event->type = IR_EVENT_HOVER;
                else if (strcmp(typeStr, "focus") == 0) event->type = IR_EVENT_FOCUS;
                else if (strcmp(typeStr, "blur") == 0) event->type = IR_EVENT_BLUR;
                else if (strcmp(typeStr, "text_change") == 0) event->type = IR_EVENT_TEXT_CHANGE;
                else if (strcmp(typeStr, "key") == 0) event->type = IR_EVENT_KEY;
                else if (strcmp(typeStr, "scroll") == 0) event->type = IR_EVENT_SCROLL;
                else if (strcmp(typeStr, "timer") == 0) event->type = IR_EVENT_TIMER;
                else if (strcmp(typeStr, "custom") == 0) event->type = IR_EVENT_CUSTOM;
                else {
                    // Plugin events are now handled by the capability system
                    // For now, treat unknown events as custom with the name preserved
                    event->type = IR_EVENT_CUSTOM;
                    event->event_name = strdup(typeStr);
                }
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

            // Handler source (IR v2.2: Lua source preservation)
            cJSON* handlerSourceItem = cJSON_GetObjectItem(eventJson, "handler_source");
            if (handlerSourceItem && cJSON_IsObject(handlerSourceItem)) {
                event->handler_source = calloc(1, sizeof(IRHandlerSource));
                if (event->handler_source) {
                    cJSON* langItem = cJSON_GetObjectItem(handlerSourceItem, "language");
                    if (langItem && cJSON_IsString(langItem)) {
                        event->handler_source->language = strdup(langItem->valuestring);
                    }
                    cJSON* codeItem = cJSON_GetObjectItem(handlerSourceItem, "code");
                    if (codeItem && cJSON_IsString(codeItem)) {
                        event->handler_source->code = strdup(codeItem->valuestring);
                    }
                    cJSON* fileItem = cJSON_GetObjectItem(handlerSourceItem, "file");
                    if (fileItem && cJSON_IsString(fileItem)) {
                        event->handler_source->file = strdup(fileItem->valuestring);
                    }
                    cJSON* lineItem = cJSON_GetObjectItem(handlerSourceItem, "line");
                    if (lineItem && cJSON_IsNumber(lineItem)) {
                        event->handler_source->line = lineItem->valueint;
                    }

                    // Closure metadata (IR v2.3: Target-agnostic KIR)
                    cJSON* usesClosuresItem = cJSON_GetObjectItem(handlerSourceItem, "uses_closures");
                    if (usesClosuresItem && cJSON_IsBool(usesClosuresItem)) {
                        event->handler_source->uses_closures = (usesClosuresItem->type == cJSON_True);

                        if (event->handler_source->uses_closures) {
                            cJSON* closureArrayItem = cJSON_GetObjectItem(handlerSourceItem, "closure_vars");
                            if (closureArrayItem && cJSON_IsArray(closureArrayItem)) {
                                int closureCount = cJSON_GetArraySize(closureArrayItem);
                                if (closureCount > 0) {
                                    event->handler_source->closure_vars = calloc(closureCount, sizeof(char*));
                                    event->handler_source->closure_var_count = closureCount;

                                    for (int i = 0; i < closureCount; i++) {
                                        cJSON* closureItem = cJSON_GetArrayItem(closureArrayItem, i);
                                        if (closureItem && cJSON_IsString(closureItem)) {
                                            event->handler_source->closure_vars[i] = strdup(closureItem->valuestring);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
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

    // Conditional visibility
    if ((item = cJSON_GetObjectItem(json, "visible_condition")) != NULL && cJSON_IsString(item)) {
        component->visible_condition = strdup(item->valuestring);
        // Default to true if not specified
        component->visible_when_true = true;

        cJSON* when_true = cJSON_GetObjectItem(json, "visible_when_true");
        if (when_true && cJSON_IsBool(when_true)) {
            component->visible_when_true = cJSON_IsTrue(when_true);
        }
    }

    // ForEach (dynamic list rendering) properties
    if ((item = cJSON_GetObjectItem(json, "each_source")) != NULL && cJSON_IsString(item)) {
        component->each_source = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "each_item_name")) != NULL && cJSON_IsString(item)) {
        component->each_item_name = strdup(item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(json, "each_index_name")) != NULL && cJSON_IsString(item)) {
        component->each_index_name = strdup(item->valuestring);
    }

    // Generic custom_data (for elementId, data-* attributes, etc.)
    if ((item = cJSON_GetObjectItem(json, "custom_data")) != NULL) {
        if (cJSON_IsString(item)) {
            component->custom_data = strdup(item->valuestring);
        } else if (cJSON_IsObject(item) || cJSON_IsArray(item)) {
            // custom_data can be a JSON object/array (e.g., ForEach metadata)
            // Serialize it back to string for later parsing
            char* json_str = cJSON_PrintUnformatted(item);
            if (json_str) {
                component->custom_data = json_str;
            }
        }
    }

    // Parse ForEach metadata from custom_data if not already set via direct properties
    if (component->type == IR_COMPONENT_FOR_EACH && !component->each_source && component->custom_data) {
        cJSON* custom_json = cJSON_Parse(component->custom_data);
        if (custom_json) {
            cJSON* forEach = cJSON_GetObjectItem(custom_json, "forEach");
            if (forEach && cJSON_IsTrue(forEach)) {
                cJSON* each_source = cJSON_GetObjectItem(custom_json, "each_source");
                if (each_source) {
                    // each_source can be either a string (variable reference) or an array (literal data)
                    if (cJSON_IsString(each_source)) {
                        component->each_source = strdup(each_source->valuestring);
                    } else if (cJSON_IsArray(each_source)) {
                        // Serialize array back to JSON string for runtime expansion
                        char* array_str = cJSON_PrintUnformatted(each_source);
                        if (array_str) {
                            component->each_source = array_str;
                        }
                    }
                }
                cJSON* each_item_name = cJSON_GetObjectItem(custom_json, "each_item_name");
                if (each_item_name && cJSON_IsString(each_item_name)) {
                    component->each_item_name = strdup(each_item_name->valuestring);
                }
                cJSON* each_index_name = cJSON_GetObjectItem(custom_json, "each_index_name");
                if (each_index_name && cJSON_IsString(each_index_name)) {
                    component->each_index_name = strdup(each_index_name->valuestring);
                }
            }
            cJSON_Delete(custom_json);
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

IRComponent* ir_json_deserialize_component_with_context(cJSON* json, ComponentDefContext* ctx) {
    return json_deserialize_component_with_context(json, ctx);
}

// ============================================================================
// Main Deserialization Functions
// ============================================================================

/**
 * Deserialize JSON string to component tree
 */
IRComponent* ir_json_deserialize_component_tree(const char* json_string) {
    if (!json_string) {
        return NULL;
    }

    cJSON* root = cJSON_Parse(json_string);
    if (!root) {
        return NULL;
    }

    return ir_json_deserialize_from_cjson(root);
}

/**
 * Deserialize cJSON object to component tree
 */
IRComponent* ir_json_deserialize_from_cjson(cJSON* root) {
    if (!root) return NULL;

    // Parse component_definitions for expansion
    ComponentDefContext* ctx = NULL;
    cJSON* componentDefs = cJSON_GetObjectItem(root, "component_definitions");
    if (componentDefs && cJSON_IsArray(componentDefs)) {
        ctx = ir_json_context_create();
        int defCount = cJSON_GetArraySize(componentDefs);
        for (int i = 0; i < defCount; i++) {
            cJSON* def = cJSON_GetArrayItem(componentDefs, i);
            if (!def || !cJSON_IsObject(def)) continue;

            cJSON* nameItem = cJSON_GetObjectItem(def, "name");
            cJSON* templateItem = cJSON_GetObjectItem(def, "template");

            if (nameItem && cJSON_IsString(nameItem) && templateItem && cJSON_IsObject(templateItem)) {
                // Add the full definition (not just template) so we can access props/state
                ir_json_context_add(ctx, nameItem->valuestring, def);
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

    // Plugin requirements are now handled by the capability system
    // The capability system will automatically load required plugins

    // Parse window properties from "app" object (canonical format)
    cJSON* appJson = cJSON_GetObjectItem(root, "app");

    if (appJson) {
        IRContext* ir_ctx = g_ir_context ?: (ir_set_context(ir_create_context()), g_ir_context);
        if (!ir_ctx->metadata) {
            ir_ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
        }

        // Window title: app.windowTitle
        cJSON* titleItem = cJSON_GetObjectItem(appJson, "windowTitle");
        if (titleItem && cJSON_IsString(titleItem)) {
            free(ir_ctx->metadata->window_title);
            ir_ctx->metadata->window_title = strdup(titleItem->valuestring);
        }

        // Window dimensions: app.windowWidth, app.windowHeight
        cJSON* widthItem = cJSON_GetObjectItem(appJson, "windowWidth");
        if (widthItem && cJSON_IsNumber(widthItem)) {
            ir_ctx->metadata->window_width = (int)widthItem->valuedouble;
        }

        cJSON* heightItem = cJSON_GetObjectItem(appJson, "windowHeight");
        if (heightItem && cJSON_IsNumber(heightItem)) {
            ir_ctx->metadata->window_height = (int)heightItem->valuedouble;
        }
    }

    // Parse C metadata for C->KIR->C round-trip
    cJSON* c_metadata = cJSON_GetObjectItem(root, "c_metadata");
    if (c_metadata) {
        json_deserialize_c_metadata_helper(c_metadata);
    }

    // Parse stylesheet for CSS rules with complex selectors (e.g., ".logo span")
    cJSON* stylesheetJson = cJSON_GetObjectItem(root, "stylesheet");
    if (stylesheetJson && cJSON_IsObject(stylesheetJson)) {
        IRStylesheet* stylesheet = json_deserialize_stylesheet(stylesheetJson);
        if (stylesheet) {
            IRContext* ir_ctx = g_ir_context ?: (ir_set_context(ir_create_context()), g_ir_context);
            if (ir_ctx->stylesheet) {
                ir_stylesheet_free(ir_ctx->stylesheet);
            }
            ir_ctx->stylesheet = stylesheet;
        }
    }

    // Clean up context before deleting JSON (context references JSON nodes)
    ir_json_context_free(ctx);
    cJSON_Delete(root);

    return component;
}
