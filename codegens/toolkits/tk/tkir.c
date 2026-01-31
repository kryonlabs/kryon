/**
 * @file tkir.c
 * @brief TKIR - Toolkit Intermediate Representation Implementation
 *
 * Implementation of TKIR core functions.
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L

#include "tkir.h"
#include "../codegen_common.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Error Handling
// ============================================================================

static char tkir_error_buffer[1024];
static const char* tkir_error_prefix = "TKIR";

void tkir_set_error_prefix(const char* prefix) {
    tkir_error_prefix = prefix ? prefix : "TKIR";
}

const char* tkir_get_error(void) {
    return tkir_error_buffer;
}

void tkir_clear_error(void) {
    tkir_error_buffer[0] = '\0';
}

void tkir_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Write prefix
    snprintf(tkir_error_buffer, sizeof(tkir_error_buffer), "[%s] ", tkir_error_prefix);

    // Write error message
    vsnprintf(tkir_error_buffer + strlen(tkir_error_buffer),
              sizeof(tkir_error_buffer) - strlen(tkir_error_buffer),
              fmt, args);

    va_end(args);
}

void tkir_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Write to stderr
    fprintf(stderr, "[%s] WARNING: ", tkir_error_prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

// ============================================================================
// Layout Type Conversion
// ============================================================================

const char* tkir_layout_type_to_string(TKIRLayoutType type) {
    switch (type) {
        case TKIR_LAYOUT_PACK:  return "pack";
        case TKIR_LAYOUT_GRID:  return "grid";
        case TKIR_LAYOUT_PLACE: return "place";
        case TKIR_LAYOUT_AUTO:  return "auto";
        default:                return "unknown";
    }
}

TKIRLayoutType tkir_layout_type_from_string(const char* str) {
    if (!str) return TKIR_LAYOUT_AUTO;

    if (strcmp(str, "pack") == 0) return TKIR_LAYOUT_PACK;
    if (strcmp(str, "grid") == 0) return TKIR_LAYOUT_GRID;
    if (strcmp(str, "place") == 0) return TKIR_LAYOUT_PLACE;

    return TKIR_LAYOUT_AUTO;
}

// ============================================================================
// Alignment Conversion
// ============================================================================

const char* tkir_alignment_to_string(TKIRAlignment align) {
    switch (align) {
        case TKIR_ALIGN_START:          return "start";
        case TKIR_ALIGN_CENTER:         return "center";
        case TKIR_ALIGN_END:            return "end";
        case TKIR_ALIGN_STRETCH:        return "stretch";
        case TKIR_ALIGN_SPACE_BETWEEN:  return "space-between";
        case TKIR_ALIGN_SPACE_AROUND:   return "space-around";
        case TKIR_ALIGN_SPACE_EVENLY:   return "space-evenly";
        default:                        return "start";
    }
}

// ============================================================================
// TKIRWidget Implementation
// ============================================================================

TKIRWidget* tkir_widget_create(cJSON* component, const char* parent_id, int* id_counter) {
    if (!component || !id_counter) {
        tkir_error("Invalid arguments to tkir_widget_create");
        return NULL;
    }

    TKIRWidget* widget = calloc(1, sizeof(TKIRWidget));
    if (!widget) {
        tkir_error("Failed to allocate TKIRWidget");
        return NULL;
    }

    // Store reference to original JSON
    widget->json = component;
    widget->parent_id = parent_id;

    // Generate unique ID
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "widget-%d", (*id_counter)++);
    widget->id = strdup(id_buf);

    // Get component type
    const char* type = codegen_get_component_type(component);
    widget->kir_type = type;
    widget->tk_type = codegen_map_kir_to_tk_widget(type);

    // Extract text
    cJSON* text = cJSON_GetObjectItem(component, "text");
    widget->text = (text && cJSON_IsString(text)) ? text->valuestring : NULL;

    // Extract background color
    cJSON* background = cJSON_GetObjectItem(component, "background");
    widget->background = (background && cJSON_IsString(background)) ? background->valuestring : NULL;

    // Extract foreground color
    cJSON* color = cJSON_GetObjectItem(component, "color");
    widget->foreground = (color && cJSON_IsString(color)) ? color->valuestring : NULL;

    // Extract image
    cJSON* image = cJSON_GetObjectItem(component, "image");
    widget->image = (image && cJSON_IsString(image)) ? image->valuestring : NULL;

    // Width and height will be processed by the builder
    widget->width = NULL;
    widget->height = NULL;
    widget->font = NULL;
    widget->border = NULL;
    widget->layout = NULL;

    // Children and events will be populated by the builder
    widget->children_ids = NULL;
    widget->events = NULL;

    return widget;
}

void tkir_widget_free(TKIRWidget* widget) {
    if (!widget) return;

    // Free ID (only field we own)
    if (widget->id) {
        free((void*)widget->id);
    }

    // Free allocated size structures
    if (widget->width) {
        free(widget->width);
    }
    if (widget->height) {
        free(widget->height);
    }
    if (widget->font) {
        free(widget->font);
    }
    if (widget->border) {
        free(widget->border);
    }
    if (widget->layout) {
        free(widget->layout);
    }

    free(widget);
}

// ============================================================================
// TKIRHandler Implementation
// ============================================================================

TKIRHandler* tkir_handler_create(cJSON* json, const char* widget_id, int* id_counter) {
    if (!json || !widget_id || !id_counter) {
        return NULL;
    }

    TKIRHandler* handler = calloc(1, sizeof(TKIRHandler));
    if (!handler) {
        return NULL;
    }

    // Store reference to original JSON
    handler->json = json;
    handler->widget_id = widget_id;

    // Generate unique ID
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "handler-%d", (*id_counter)++);
    handler->id = strdup(id_buf);

    // Extract event type
    cJSON* event_type = cJSON_GetObjectItem(json, "event");
    handler->event_type = (event_type && cJSON_IsString(event_type)) ? event_type->valuestring : "unknown";

    // Implementations will be populated by the builder
    handler->implementations = NULL;

    return handler;
}

void tkir_handler_free(TKIRHandler* handler) {
    if (!handler) return;

    // Free ID (only field we own)
    if (handler->id) {
        free((void*)handler->id);
    }

    free(handler);
}

// ============================================================================
// TKIRRoot Implementation
// ============================================================================

TKIRRoot* tkir_root_create(const char* kir_json) {
    if (!kir_json) {
        tkir_error("NULL KIR JSON provided");
        return NULL;
    }

    // Parse KIR JSON
    cJSON* kir_root = codegen_parse_kir_json(kir_json);
    if (!kir_root) {
        tkir_error("Failed to parse KIR JSON");
        return NULL;
    }

    // Create TKIR from parsed JSON
    TKIRRoot* root = tkir_root_from_cJSON(kir_root);

    // Note: We don't delete kir_root here as it's referenced by the TKIR structure
    // The caller is responsible for managing the cJSON lifetime

    return root;
}

TKIRRoot* tkir_root_from_cJSON(cJSON* kir_root) {
    if (!kir_root) {
        tkir_error("NULL KIR root provided");
        return NULL;
    }

    TKIRRoot* root = calloc(1, sizeof(TKIRRoot));
    if (!root) {
        tkir_error("Failed to allocate TKIRRoot");
        return NULL;
    }

    // Store reference to KIR JSON
    root->json = kir_root;

    // Set metadata
    root->format = TKIR_FORMAT_STRING;
    root->version = "1.0";
    root->generator = TKIR_GENERATOR_NAME;

    // Extract window configuration
    cJSON* window = cJSON_GetObjectItem(kir_root, "window");
    if (window) {
        cJSON* title = cJSON_GetObjectItem(window, "title");
        root->title = (title && cJSON_IsString(title)) ? title->valuestring : "Kryon App";

        cJSON* width = cJSON_GetObjectItem(window, "width");
        root->width = (width && cJSON_IsNumber(width)) ? width->valueint : 800;

        cJSON* height = cJSON_GetObjectItem(window, "height");
        root->height = (height && cJSON_IsNumber(height)) ? height->valueint : 600;

        cJSON* resizable = cJSON_GetObjectItem(window, "resizable");
        root->resizable = (resizable && cJSON_IsBool(resizable)) ? cJSON_IsTrue(resizable) : true;

        cJSON* background = cJSON_GetObjectItem(window, "background");
        root->background = (background && cJSON_IsString(background)) ? background->valuestring : NULL;
    } else {
        root->title = "Kryon App";
        root->width = 800;
        root->height = 600;
        root->resizable = true;
        root->background = NULL;
    }

    // Read widgets, handlers, and data bindings from JSON if they exist
    // Otherwise create empty arrays for the builder to populate
    cJSON* widgets = cJSON_GetObjectItem(kir_root, "widgets");
    if (widgets && cJSON_IsArray(widgets)) {
        root->widgets = widgets;
    } else {
        root->widgets = cJSON_CreateArray();
    }

    cJSON* handlers = cJSON_GetObjectItem(kir_root, "handlers");
    if (handlers && cJSON_IsArray(handlers)) {
        root->handlers = handlers;
    } else {
        root->handlers = cJSON_CreateArray();
    }

    cJSON* data_bindings = cJSON_GetObjectItem(kir_root, "data_bindings");
    if (data_bindings && cJSON_IsArray(data_bindings)) {
        root->data_bindings = data_bindings;
    } else {
        root->data_bindings = cJSON_CreateArray();
    }

    return root;
}

void tkir_root_free(TKIRRoot* root) {
    if (!root) return;

    // Free widgets array only if it's not a borrowed reference from the JSON
    if (root->widgets && root->json) {
        cJSON* json_widgets = cJSON_GetObjectItem(root->json, "widgets");
        if (root->widgets != json_widgets) {
            cJSON_Delete(root->widgets);
        }
        // If they're the same, it's a borrowed reference - don't free
    } else if (root->widgets) {
        cJSON_Delete(root->widgets);
    }

    // Free handlers array only if it's not a borrowed reference from the JSON
    if (root->handlers && root->json) {
        cJSON* json_handlers = cJSON_GetObjectItem(root->json, "handlers");
        if (root->handlers != json_handlers) {
            cJSON_Delete(root->handlers);
        }
        // If they're the same, it's a borrowed reference - don't free
    } else if (root->handlers) {
        cJSON_Delete(root->handlers);
    }

    // Free data bindings array only if it's not a borrowed reference from the JSON
    if (root->data_bindings && root->json) {
        cJSON* json_bindings = cJSON_GetObjectItem(root->json, "data_bindings");
        if (root->data_bindings != json_bindings) {
            cJSON_Delete(root->data_bindings);
        }
        // If they're the same, it's a borrowed reference - don't free
    } else if (root->data_bindings) {
        cJSON_Delete(root->data_bindings);
    }

    // IMPORTANT: We don't free root->json here because it's a borrowed reference.
    // The caller who created the cJSON object is responsible for freeing it.
    // This function only frees the TKIRRoot structure itself, not the JSON it references.

    free(root);
}

char* tkir_root_to_json(TKIRRoot* root) {
    if (!root) {
        tkir_error("NULL TKIRRoot provided");
        return NULL;
    }

    cJSON* json = cJSON_CreateObject();

    // Add format and version
    cJSON_AddStringToObject(json, "format", root->format);
    cJSON_AddStringToObject(json, "version", root->version);

    // Add metadata
    cJSON* metadata = cJSON_CreateObject();
    if (root->source_kir) {
        cJSON_AddStringToObject(metadata, "source_kir", root->source_kir);
    }
    cJSON_AddStringToObject(metadata, "generator", root->generator);
    cJSON_AddItemToObject(json, "metadata", metadata);

    // Add window configuration
    cJSON* window = cJSON_CreateObject();
    cJSON_AddStringToObject(window, "title", root->title);
    cJSON_AddNumberToObject(window, "width", root->width);
    cJSON_AddNumberToObject(window, "height", root->height);
    cJSON_AddBoolToObject(window, "resizable", root->resizable);
    if (root->background) {
        cJSON_AddStringToObject(window, "background", root->background);
    }
    cJSON_AddItemToObject(json, "window", window);

    // Add widgets
    if (root->widgets) {
        cJSON_AddItemToObject(json, "widgets", cJSON_Duplicate(root->widgets, true));
    }

    // Add handlers
    if (root->handlers) {
        cJSON_AddItemToObject(json, "handlers", cJSON_Duplicate(root->handlers, true));
    }

    // Add data bindings
    if (root->data_bindings) {
        cJSON_AddItemToObject(json, "data_bindings", cJSON_Duplicate(root->data_bindings, true));
    }

    // Convert to string
    char* json_str = cJSON_Print(json);

    // Cleanup
    cJSON_Delete(json);

    return json_str;
}

TKIRWidget* tkir_root_find_widget(TKIRRoot* root, const char* id) {
    if (!root || !id) return NULL;

    cJSON* widget_array = root->widgets;
    if (!widget_array) return NULL;

    cJSON* widget_json = NULL;
    cJSON_ArrayForEach(widget_json, widget_array) {
        cJSON* id_item = cJSON_GetObjectItem(widget_json, "id");
        if (id_item && cJSON_IsString(id_item) && strcmp(id_item->valuestring, id) == 0) {
            // Found - create wrapper
            TKIRWidget* widget = calloc(1, sizeof(TKIRWidget));
            if (!widget) return NULL;

            widget->json = widget_json;
            widget->id = id_item->valuestring;

            cJSON* tk_type = cJSON_GetObjectItem(widget_json, "tk_type");
            widget->tk_type = (tk_type && cJSON_IsString(tk_type)) ? tk_type->valuestring : NULL;

            cJSON* kir_type = cJSON_GetObjectItem(widget_json, "kir_type");
            widget->kir_type = (kir_type && cJSON_IsString(kir_type)) ? kir_type->valuestring : NULL;

            // Extract other fields similarly...
            return widget;
        }
    }

    return NULL;
}

TKIRHandler* tkir_root_find_handler(TKIRRoot* root, const char* id) {
    if (!root || !id) return NULL;

    cJSON* handler_array = root->handlers;
    if (!handler_array) return NULL;

    cJSON* handler_json = NULL;
    cJSON_ArrayForEach(handler_json, handler_array) {
        cJSON* id_item = cJSON_GetObjectItem(handler_json, "id");
        if (id_item && cJSON_IsString(id_item) && strcmp(id_item->valuestring, id) == 0) {
            // Found
            TKIRHandler* handler = calloc(1, sizeof(TKIRHandler));
            if (!handler) return NULL;

            handler->json = handler_json;
            handler->id = id_item->valuestring;

            cJSON* event_type = cJSON_GetObjectItem(handler_json, "event_type");
            handler->event_type = (event_type && cJSON_IsString(event_type)) ? event_type->valuestring : NULL;

            cJSON* widget_id = cJSON_GetObjectItem(handler_json, "widget_id");
            handler->widget_id = (widget_id && cJSON_IsString(widget_id)) ? widget_id->valuestring : NULL;

            return handler;
        }
    }

    return NULL;
}
