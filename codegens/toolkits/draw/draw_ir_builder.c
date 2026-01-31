/**
 * Kryon DrawIR Builder
 *
 * Converts KIR JSON to DrawIR (Draw toolkit-specific IR)
 * This fixes the Tk bias by using Draw-specific widget types!
 */

#define _POSIX_C_SOURCE 200809L

#include "draw_ir.h"
#include "codegen_common.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_ID_LENGTH 64

// ============================================================================
// Static Functions
// ============================================================================

static char* generate_id(const char* prefix, int counter);
static DrawIRProperty* parse_property(cJSON* prop_json);
static DrawIRWidget* parse_widget(cJSON* widget_json, int* id_counter);
static bool parse_properties(DrawIRWidget* widget, cJSON* props_json);

// ============================================================================
// KIR to DrawIR Type Conversion (CRITICAL - Fixes Tk Bias!)
// ============================================================================

DrawIRWidgetType drawir_type_from_kir(const char* kir_type) {
    if (!kir_type) return DRAW_IR_WIDGET_UNKNOWN;

    // Layout containers
    if (strcmp(kir_type, "Container") == 0) return DRAW_IR_WIDGET_FRAME;
    if (strcmp(kir_type, "Column") == 0) return DRAW_IR_WIDGET_COLUMN;
    if (strcmp(kir_type, "Row") == 0) return DRAW_IR_WIDGET_ROW;
    if (strcmp(kir_type, "Scroll") == 0) return DRAW_IR_WIDGET_SCROLL;

    // Basic widgets (NOTE: Draw uses proper capitalization!)
    if (strcmp(kir_type, "Button") == 0) return DRAW_IR_WIDGET_BUTTON;
    if (strcmp(kir_type, "Text") == 0) return DRAW_IR_WIDGET_TEXT;
    if (strcmp(kir_type, "Input") == 0) return DRAW_IR_WIDGET_TEXTFIELD;  // NOT "entry"!
    if (strcmp(kir_type, "TextArea") == 0) return DRAW_IR_WIDGET_TEXTAREA;

    // Selection widgets
    if (strcmp(kir_type, "Checkbox") == 0) return DRAW_IR_WIDGET_CHECKBOX;
    if (strcmp(kir_type, "Radio") == 0) return DRAW_IR_WIDGET_RADIO;

    // Display widgets
    if (strcmp(kir_type, "Image") == 0) return DRAW_IR_WIDGET_IMAGE;
    if (strcmp(kir_type, "Progress") == 0) return DRAW_IR_WIDGET_PROGRESSBAR;
    if (strcmp(kir_type, "Slider") == 0) return DRAW_IR_WIDGET_SLIDER;

    // Advanced widgets
    if (strcmp(kir_type, "Canvas") == 0) return DRAW_IR_WIDGET_CANVAS;
    if (strcmp(kir_type, "List") == 0) return DRAW_IR_WIDGET_LIST;

    return DRAW_IR_WIDGET_UNKNOWN;
}

const char* drawir_type_to_string(DrawIRWidgetType type) {
    switch (type) {
        case DRAW_IR_WIDGET_BUTTON: return "Button";
        case DRAW_IR_WIDGET_TEXT: return "Text";
        case DRAW_IR_WIDGET_TEXTFIELD: return "Textfield";  // NOT "entry"!
        case DRAW_IR_WIDGET_TEXTAREA: return "Textarea";
        case DRAW_IR_WIDGET_CHECKBOX: return "Checkbox";
        case DRAW_IR_WIDGET_RADIO: return "Radio";
        case DRAW_IR_WIDGET_IMAGE: return "Image";
        case DRAW_IR_WIDGET_CANVAS: return "Canvas";
        case DRAW_IR_WIDGET_FRAME: return "Frame";
        case DRAW_IR_WIDGET_COLUMN: return "Column";
        case DRAW_IR_WIDGET_ROW: return "Row";
        case DRAW_IR_WIDGET_SCROLL: return "Scroll";
        case DRAW_IR_WIDGET_LIST: return "List";
        case DRAW_IR_WIDGET_SLIDER: return "Slider";
        case DRAW_IR_WIDGET_PROGRESSBAR: return "Progressbar";
        default: return "Unknown";
    }
}

const char* drawir_type_to_limbo_class(DrawIRWidgetType type) {
    // Draw widget types ARE Limbo class names (capitalized)
    return drawir_type_to_string(type);
}

// ============================================================================
// ID Generation
// ============================================================================

static char* generate_id(const char* prefix, int counter) {
    char* id = malloc(MAX_ID_LENGTH);
    if (!id) return NULL;

    snprintf(id, MAX_ID_LENGTH, "%s_%d", prefix, counter);
    return id;
}

// ============================================================================
// DrawIR Widget API
// ============================================================================

DrawIRWidget* drawir_widget_new(DrawIRWidgetType type, const char* id) {
    DrawIRWidget* widget = calloc(1, sizeof(DrawIRWidget));
    if (!widget) return NULL;

    widget->type = type;
    widget->id = id ? strdup(id) : generate_id("widget", rand() % 10000);
    widget->properties = NULL;
    widget->events = NULL;
    widget->event_count = 0;
    widget->children = NULL;
    widget->child_count = 0;
    widget->parent = NULL;
    widget->x = 0;
    widget->y = 0;
    widget->width = 0;
    widget->height = 0;
    widget->has_position = false;

    return widget;
}

void drawir_widget_free(DrawIRWidget* widget) {
    if (!widget) return;

    // Free properties
    DrawIRProperty* prop = widget->properties;
    while (prop) {
        DrawIRProperty* next = prop->next;
        if (prop->name && prop->type == DRAW_IR_PROPERTY_STRING) {
            free((void*)prop->name);
            free((void*)prop->string_value);
        } else if (prop->name) {
            free((void*)prop->name);
        }
        free(prop);
        prop = next;
    }

    // Free events
    if (widget->events) {
        for (int i = 0; i < widget->event_count; i++) {
            free(widget->events[i]);
        }
        free(widget->events);
    }

    // Free children
    if (widget->children) {
        for (int i = 0; i < widget->child_count; i++) {
            drawir_widget_free(widget->children[i]);
        }
        free(widget->children);
    }

    // Free ID and KIR type
    if (widget->id) free((void*)widget->id);
    if (widget->kir_type) free((void*)widget->kir_type);

    free(widget);
}

bool drawir_widget_add_child(DrawIRWidget* parent, DrawIRWidget* child) {
    if (!parent || !child) return false;

    // Resize children array
    DrawIRWidget** new_children = realloc(parent->children,
                                         (parent->child_count + 1) * sizeof(DrawIRWidget*));
    if (!new_children) return false;

    parent->children = new_children;
    parent->children[parent->child_count++] = child;
    child->parent = parent;

    return true;
}

DrawIRProperty* drawir_widget_add_property(DrawIRWidget* widget,
                                          const char* name,
                                          DrawIRPropertyType type) {
    if (!widget || !name) return NULL;

    DrawIRProperty* prop = calloc(1, sizeof(DrawIRProperty));
    if (!prop) return NULL;

    prop->name = strdup(name);
    prop->type = type;
    prop->next = widget->properties;
    widget->properties = prop;

    return prop;
}

DrawIRProperty* drawir_widget_get_property(DrawIRWidget* widget, const char* name) {
    if (!widget || !name) return NULL;

    DrawIRProperty* prop = widget->properties;
    while (prop) {
        if (strcmp(prop->name, name) == 0) {
            return prop;
        }
        prop = prop->next;
    }

    return NULL;
}

bool drawir_widget_add_event(DrawIRWidget* widget, const char* event_name) {
    if (!widget || !event_name) return false;

    // Resize events array
    char** new_events = realloc(widget->events,
                               (widget->event_count + 1) * sizeof(char*));
    if (!new_events) return false;

    widget->events = new_events;
    widget->events[widget->event_count++] = strdup(event_name);

    return true;
}

// ============================================================================
// DrawIR Root API
// ============================================================================

DrawIRRoot* drawir_build_from_kir(const char* kir_json, bool compute_layout) {
    if (!kir_json) return NULL;

    // Parse KIR JSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        codegen_error("Failed to parse KIR JSON");
        return NULL;
    }

    // Create DrawIR root
    DrawIRRoot* drawir = calloc(1, sizeof(DrawIRRoot));
    if (!drawir) {
        cJSON_Delete(root);
        return NULL;
    }

    // Extract root component
    cJSON* component = codegen_extract_root_component(root);
    if (!component) {
        codegen_error("No root component found in KIR");
        drawir_free(drawir);
        cJSON_Delete(root);
        return NULL;
    }

    // Parse widgets
    int id_counter = 0;
    drawir->root_widget = parse_widget(component, &id_counter);
    if (!drawir->root_widget) {
        codegen_error("Failed to parse root widget");
        drawir_free(drawir);
        cJSON_Delete(root);
        return NULL;
    }

    // Set default window properties
    drawir->window_title = "Kryon Application";
    drawir->window_width = 800;
    drawir->window_height = 600;

    // Compute layout if requested
    if (compute_layout) {
        if (!drawir_compute_layout(drawir)) {
            codegen_warn("Failed to compute layout, using defaults");
        }
    }

    cJSON_Delete(root);
    return drawir;
}

void drawir_free(DrawIRRoot* root) {
    if (!root) return;

    if (root->root_widget) {
        drawir_widget_free(root->root_widget);
    }

    if (root->imports) {
        for (int i = 0; i < root->import_count; i++) {
            free(root->imports[i]);
        }
        free(root->imports);
    }

    if (root->handlers) {
        for (int i = 0; i < root->handler_count; i++) {
            free(root->handlers[i]);
        }
        free(root->handlers);
    }

    if (root->window_title) free((void*)root->window_title);

    free(root);
}

// ============================================================================
// Widget Parsing
// ============================================================================

static DrawIRWidget* parse_widget(cJSON* widget_json, int* id_counter) {
    if (!widget_json) return NULL;

    // Get widget type
    const char* type_str = codegen_get_component_type(widget_json);
    if (!type_str) {
        codegen_error("Widget has no type");
        return NULL;
    }

    // Convert to DrawIR type
    DrawIRWidgetType type = drawir_type_from_kir(type_str);
    if (type == DRAW_IR_WIDGET_UNKNOWN) {
        codegen_warn("Unknown KIR widget type: %s", type_str);
    }

    // Create widget
    char* id = generate_id("widget", (*id_counter)++);
    DrawIRWidget* widget = drawir_widget_new(type, id);
    free(id);  // Widget makes its own copy

    if (!widget) {
        return NULL;
    }

    widget->kir_type = strdup(type_str);

    // Parse properties
    cJSON* props = cJSON_GetObjectItem(widget_json, "properties");
    if (props) {
        parse_properties(widget, props);
    }

    // Parse children
    cJSON* children = cJSON_GetObjectItem(widget_json, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            DrawIRWidget* child_widget = parse_widget(child, id_counter);
            if (child_widget) {
                drawir_widget_add_child(widget, child_widget);
            }
        }
    }

    return widget;
}

static bool parse_properties(DrawIRWidget* widget, cJSON* props_json) {
    if (!widget || !props_json) return false;

    cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, props_json) {
        const char* prop_name = prop->string;
        if (!prop_name) continue;

        // Size properties (special handling)
        if (strcmp(prop_name, "width") == 0) {
            if (cJSON_IsString(prop)) {
                widget->width = codegen_parse_size_value(prop->valuestring);
            } else if (cJSON_IsNumber(prop)) {
                widget->width = prop->valueint;
            }
            continue;
        }

        if (strcmp(prop_name, "height") == 0) {
            if (cJSON_IsString(prop)) {
                widget->height = codegen_parse_size_value(prop->valuestring);
            } else if (cJSON_IsNumber(prop)) {
                widget->height = prop->valueint;
            }
            continue;
        }

        if (strcmp(prop_name, "x") == 0) {
            if (cJSON_IsNumber(prop)) {
                widget->x = prop->valueint;
                widget->has_position = true;
            }
            continue;
        }

        if (strcmp(prop_name, "y") == 0) {
            if (cJSON_IsNumber(prop)) {
                widget->y = prop->valueint;
                widget->has_position = true;
            }
            continue;
        }

        // Generic properties
        if (cJSON_IsString(prop)) {
            DrawIRProperty* ir_prop = drawir_widget_add_property(
                widget, prop_name, DRAW_IR_PROPERTY_STRING);
            if (ir_prop) {
                ir_prop->string_value = strdup(prop->valuestring);
            }
        } else if (cJSON_IsNumber(prop)) {
            DrawIRProperty* ir_prop = drawir_widget_add_property(
                widget, prop_name, DRAW_IR_PROPERTY_FLOAT);
            if (ir_prop) {
                ir_prop->float_value = prop->valuedouble;
            }
        } else if (cJSON_IsBool(prop)) {
            DrawIRProperty* ir_prop = drawir_widget_add_property(
                widget, prop_name, DRAW_IR_PROPERTY_BOOLEAN);
            if (ir_prop) {
                ir_prop->bool_value = cJSON_IsTrue(prop);
            }
        }
    }

    return true;
}

// ============================================================================
// Layout Computation
// ============================================================================

bool drawir_compute_layout(DrawIRRoot* root) {
    if (!root || !root->root_widget) return false;

    int next_x = 0;
    int next_y = 0;

    return drawir_compute_widget_layout(root->root_widget,
                                       root->window_width,
                                       root->window_height,
                                       &next_x,
                                       &next_y);
}

bool drawir_compute_widget_layout(DrawIRWidget* widget,
                                  int parent_width,
                                  int parent_height,
                                  int* next_x,
                                  int* next_y) {
    if (!widget || !next_x || !next_y) return false;

    // If widget already has position, use it
    if (widget->has_position) {
        *next_x = widget->x;
        *next_y = widget->y;
        return true;
    }

    // Default position
    widget->x = *next_x;
    widget->y = *next_y;

    // Default size if not set
    if (widget->width == 0) {
        widget->width = 100;
    }
    if (widget->height == 0) {
        widget->height = 30;
    }

    // Layout children based on type
    int child_x = widget->x;
    int child_y = widget->y;

    for (int i = 0; i < widget->child_count; i++) {
        DrawIRWidget* child = widget->children[i];
        if (!child) continue;

        if (widget->type == DRAW_IR_WIDGET_COLUMN) {
            // Vertical layout
            drawir_compute_widget_layout(child, widget->width, widget->height,
                                       &child_x, &child_y);
            child_y += child->height + 10;  // Spacing
        } else if (widget->type == DRAW_IR_WIDGET_ROW) {
            // Horizontal layout
            drawir_compute_widget_layout(child, widget->width, widget->height,
                                       &child_x, &child_y);
            child_x += child->width + 10;  // Spacing
        } else {
            // Default: place children below
            drawir_compute_widget_layout(child, widget->width, widget->height,
                                       &child_x, &child_y);
        }
    }

    // Update next position
    *next_x = widget->x;
    *next_y = widget->y + widget->height + 10;

    return true;
}

// ============================================================================
// Debug Utilities
// ============================================================================

void drawir_print_widget(DrawIRWidget* widget, int indent) {
    if (!widget) return;

    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s (%s) [%d,%d %dx%d]\n",
           drawir_type_to_string(widget->type),
           widget->id,
           widget->x, widget->y,
           widget->width, widget->height);

    for (int i = 0; i < widget->child_count; i++) {
        drawir_print_widget(widget->children[i], indent + 1);
    }
}

void drawir_print_tree(DrawIRRoot* root) {
    if (!root) return;

    printf("DrawIR Tree: %s [%dx%d]\n",
           root->window_title,
           root->window_width,
           root->window_height);

    if (root->root_widget) {
        drawir_print_widget(root->root_widget, 0);
    }
}
