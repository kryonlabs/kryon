/**
 * @file wir.c
 * @brief WIR - Toolkit Intermediate Representation Implementation
 *
 * Implementation of WIR core functions.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#define _POSIX_C_SOURCE 200809L

#include "wir.h"
#include "../codegen_common.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Error Handling
// ============================================================================

static char wir_error_buffer[1024];
static const char* wir_error_prefix = "WIR";

void wir_set_error_prefix(const char* prefix) {
    wir_error_prefix = prefix ? prefix : "WIR";
}

const char* wir_get_error(void) {
    return wir_error_buffer;
}

void wir_clear_error(void) {
    wir_error_buffer[0] = '\0';
}

void wir_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Write prefix
    snprintf(wir_error_buffer, sizeof(wir_error_buffer), "[%s] ", wir_error_prefix);

    // Write error message
    vsnprintf(wir_error_buffer + strlen(wir_error_buffer),
              sizeof(wir_error_buffer) - strlen(wir_error_buffer),
              fmt, args);

    va_end(args);
}

void wir_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Write to stderr
    fprintf(stderr, "[%s] WARNING: ", wir_error_prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

// ============================================================================
// Layout Type Conversion
// ============================================================================

const char* wir_layout_type_to_string(WIRLayoutType type) {
    switch (type) {
        case WIR_LAYOUT_PACK:  return "pack";
        case WIR_LAYOUT_GRID:  return "grid";
        case WIR_LAYOUT_PLACE: return "place";
        case WIR_LAYOUT_AUTO:  return "auto";
        default:                return "unknown";
    }
}

WIRLayoutType wir_layout_type_from_string(const char* str) {
    if (!str) return WIR_LAYOUT_AUTO;

    if (strcmp(str, "pack") == 0) return WIR_LAYOUT_PACK;
    if (strcmp(str, "grid") == 0) return WIR_LAYOUT_GRID;
    if (strcmp(str, "place") == 0) return WIR_LAYOUT_PLACE;

    return WIR_LAYOUT_AUTO;
}

// ============================================================================
// Alignment Conversion
// ============================================================================

const char* wir_alignment_to_string(WIRAlignment align) {
    switch (align) {
        case WIR_ALIGN_START:          return "start";
        case WIR_ALIGN_CENTER:         return "center";
        case WIR_ALIGN_END:            return "end";
        case WIR_ALIGN_STRETCH:        return "stretch";
        case WIR_ALIGN_SPACE_BETWEEN:  return "space-between";
        case WIR_ALIGN_SPACE_AROUND:   return "space-around";
        case WIR_ALIGN_SPACE_EVENLY:   return "space-evenly";
        default:                        return "start";
    }
}

// ============================================================================
// WIRWidget Implementation
// ============================================================================

WIRWidget* wir_widget_create(cJSON* component, const char* parent_id, int* id_counter) {
    if (!component || !id_counter) {
        wir_error("Invalid arguments to wir_widget_create");
        return NULL;
    }

    WIRWidget* widget = calloc(1, sizeof(WIRWidget));
    if (!widget) {
        wir_error("Failed to allocate WIRWidget");
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
    widget->widget_type = codegen_map_kir_to_tk_widget(type);

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

void wir_widget_free(WIRWidget* widget) {
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
// WIRHandler Implementation
// ============================================================================

WIRHandler* wir_handler_create(cJSON* json, const char* widget_id, int* id_counter) {
    if (!json || !widget_id || !id_counter) {
        return NULL;
    }

    WIRHandler* handler = calloc(1, sizeof(WIRHandler));
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

void wir_handler_free(WIRHandler* handler) {
    if (!handler) return;

    // Free ID (only field we own)
    if (handler->id) {
        free((void*)handler->id);
    }

    free(handler);
}

// ============================================================================
// WIRRoot Implementation
// ============================================================================

WIRRoot* wir_root_create(const char* kir_json) {
    if (!kir_json) {
        wir_error("NULL KIR JSON provided");
        return NULL;
    }

    // Parse KIR JSON
    cJSON* kir_root = codegen_parse_kir_json(kir_json);
    if (!kir_root) {
        wir_error("Failed to parse KIR JSON");
        return NULL;
    }

    // Create WIR from parsed JSON
    WIRRoot* root = wir_root_from_cJSON(kir_root);

    // Note: We don't delete kir_root here as it's referenced by the WIR structure
    // The caller is responsible for managing the cJSON lifetime

    return root;
}

WIRRoot* wir_root_from_cJSON(cJSON* kir_root) {
    if (!kir_root) {
        wir_error("NULL KIR root provided");
        return NULL;
    }

    WIRRoot* root = calloc(1, sizeof(WIRRoot));
    if (!root) {
        wir_error("Failed to allocate WIRRoot");
        return NULL;
    }

    // Store reference to KIR JSON
    root->json = kir_root;

    // Set metadata
    root->format = WIR_FORMAT_STRING;
    root->version = "alpha";
    root->generator = WIR_GENERATOR_NAME;

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

void wir_root_free(WIRRoot* root) {
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

    // Free the JSON root if we own it
    // When created via wir_build_from_kir(), root->json is the parsed KIR cJSON
    // that we need to free. When created via wir_root_from_cJSON(), it's a
    // borrowed reference that the caller owns - in that case, we should NOT free it.
    //
    // To distinguish: if widgets/handlers are the same as what's in root->json,
    // then root->json is a borrowed reference (from wir_root_from_cJSON).
    // Otherwise, we own it (from wir_build_from_kir).

    bool json_is_borrowed = false;
    if (root->json && root->widgets) {
        cJSON* json_widgets = cJSON_GetObjectItem(root->json, "widgets");
        if (root->widgets == json_widgets) {
            json_is_borrowed = true;  // Same reference - borrowed
        }
    }

    if (!json_is_borrowed && root->json) {
        cJSON_Delete(root->json);
    }

    free(root);
}

char* wir_root_to_json(WIRRoot* root) {
    if (!root) {
        wir_error("NULL WIRRoot provided");
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

WIRWidget* wir_root_find_widget(WIRRoot* root, const char* id) {
    if (!root || !id) return NULL;

    cJSON* widget_array = root->widgets;
    if (!widget_array) return NULL;

    cJSON* widget_json = NULL;
    cJSON_ArrayForEach(widget_json, widget_array) {
        cJSON* id_item = cJSON_GetObjectItem(widget_json, "id");
        if (id_item && cJSON_IsString(id_item) && strcmp(id_item->valuestring, id) == 0) {
            // Found - create wrapper
            WIRWidget* widget = calloc(1, sizeof(WIRWidget));
            if (!widget) return NULL;

            widget->json = widget_json;
            widget->id = id_item->valuestring;

            cJSON* tk_type = cJSON_GetObjectItem(widget_json, "tk_type");
            widget->widget_type = (tk_type && cJSON_IsString(tk_type)) ? tk_type->valuestring : NULL;

            cJSON* kir_type = cJSON_GetObjectItem(widget_json, "kir_type");
            widget->kir_type = (kir_type && cJSON_IsString(kir_type)) ? kir_type->valuestring : NULL;

            // Extract other fields similarly...
            return widget;
        }
    }

    return NULL;
}

WIRHandler* wir_root_find_handler(WIRRoot* root, const char* id) {
    if (!root || !id) return NULL;

    cJSON* handler_array = root->handlers;
    if (!handler_array) return NULL;

    cJSON* handler_json = NULL;
    cJSON_ArrayForEach(handler_json, handler_array) {
        cJSON* id_item = cJSON_GetObjectItem(handler_json, "id");
        if (id_item && cJSON_IsString(id_item) && strcmp(id_item->valuestring, id) == 0) {
            // Found
            WIRHandler* handler = calloc(1, sizeof(WIRHandler));
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
