/**
 * @file wir_composer.c
 * @brief WIR Composer Implementation
 *
 * Implementation of the WIR composer for language and toolkit composition.
 */

#define _POSIX_C_SOURCE 200809L

#include "wir_composer.h"
#include "../codegen_common.h"
#include "../wir/wir.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_LANGUAGES 16
#define MAX_TOOLKITS 16

// ============================================================================
// Registry State
// ============================================================================

typedef struct {
    const char* name;
    WIRLanguageEmitter* emitter;
} LanguageEntry;

typedef struct {
    const char* name;
    WIRToolkitEmitter* emitter;
} ToolkitEntry;

static struct {
    LanguageEntry languages[MAX_LANGUAGES];
    ToolkitEntry toolkits[MAX_TOOLKITS];
    int language_count;
    int toolkit_count;
    bool initialized;
} g_registry = {0};

// ============================================================================
// Registry Management
// ============================================================================

void wir_composer_init(void) {
    if (g_registry.initialized) {
        return;  // Already initialized
    }
    memset(&g_registry, 0, sizeof(g_registry));
    g_registry.initialized = true;
}

void wir_composer_cleanup(void) {
    // Note: We don't free the emitters themselves as they may be statically
    // allocated or managed by their respective modules
    memset(&g_registry, 0, sizeof(g_registry));
}

bool wir_composer_register_language(const char* name, WIRLanguageEmitter* emitter) {
    if (!name || !emitter) {
        return false;
    }

    if (g_registry.language_count >= MAX_LANGUAGES) {
        fprintf(stderr, "Error: Language registry full (max %d)\n", MAX_LANGUAGES);
        return false;
    }

    // Check for duplicate
    for (int i = 0; i < g_registry.language_count; i++) {
        if (strcmp(g_registry.languages[i].name, name) == 0) {
            fprintf(stderr, "Warning: Language '%s' already registered, replacing\n", name);
            g_registry.languages[i].emitter = emitter;
            return true;
        }
    }

    // Add new entry
    g_registry.languages[g_registry.language_count].name = name;
    g_registry.languages[g_registry.language_count].emitter = emitter;
    g_registry.language_count++;

    return true;
}

bool wir_composer_register_toolkit(const char* name, WIRToolkitEmitter* emitter) {
    if (!name || !emitter) {
        return false;
    }

    if (g_registry.toolkit_count >= MAX_TOOLKITS) {
        fprintf(stderr, "Error: Toolkit registry full (max %d)\n", MAX_TOOLKITS);
        return false;
    }

    // Check for duplicate
    for (int i = 0; i < g_registry.toolkit_count; i++) {
        if (strcmp(g_registry.toolkits[i].name, name) == 0) {
            fprintf(stderr, "Warning: Toolkit '%s' already registered, replacing\n", name);
            g_registry.toolkits[i].emitter = emitter;
            return true;
        }
    }

    // Add new entry
    g_registry.toolkits[g_registry.toolkit_count].name = name;
    g_registry.toolkits[g_registry.toolkit_count].emitter = emitter;
    g_registry.toolkit_count++;

    return true;
}

WIRLanguageEmitter* wir_composer_lookup_language(const char* name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < g_registry.language_count; i++) {
        if (strcmp(g_registry.languages[i].name, name) == 0) {
            return g_registry.languages[i].emitter;
        }
    }

    return NULL;
}

WIRToolkitEmitter* wir_composer_lookup_toolkit(const char* name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < g_registry.toolkit_count; i++) {
        if (strcmp(g_registry.toolkits[i].name, name) == 0) {
            return g_registry.toolkits[i].emitter;
        }
    }

    return NULL;
}

// ============================================================================
// Composed Emitter Creation
// ============================================================================

WIRComposedEmitter* wir_compose(const char* language, const char* toolkit,
                                const WIRComposerOptions* options) {
    if (!language || !toolkit) {
        return NULL;
    }

    // Lookup language and toolkit
    WIRLanguageEmitter* lang_emitter = wir_composer_lookup_language(language);
    if (!lang_emitter) {
        fprintf(stderr, "Error: Language '%s' not registered\n", language);
        return NULL;
    }

    WIRToolkitEmitter* tk_emitter = wir_composer_lookup_toolkit(toolkit);
    if (!tk_emitter) {
        fprintf(stderr, "Error: Toolkit '%s' not registered\n", toolkit);
        return NULL;
    }

    // Create composed emitter
    WIRComposedEmitter* emitter = calloc(1, sizeof(WIRComposedEmitter));
    if (!emitter) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    emitter->language = lang_emitter;
    emitter->toolkit = tk_emitter;
    emitter->context = NULL;  // Could be used for private context data

    return emitter;
}

void wir_composer_free(WIRComposedEmitter* emitter) {
    if (!emitter) {
        return;
    }

    // Free language emitter if it has a free function
    if (emitter->language && emitter->language->free) {
        emitter->language->free(emitter->language);
    }

    // Free toolkit emitter if it has a free function
    if (emitter->toolkit && emitter->toolkit->free) {
        emitter->toolkit->free(emitter->toolkit);
    }

    // Free context if allocated
    if (emitter->context) {
        free(emitter->context);
    }

    free(emitter);
}

// ============================================================================
// Code Generation
// ============================================================================

char* wir_composer_emit(WIRComposedEmitter* emitter, WIRRoot* root) {
    if (!emitter || !root) {
        return NULL;
    }

    // Create string builder for output
    StringBuilder* sb = sb_create(8192);
    if (!sb) {
        fprintf(stderr, "Error: Failed to create string builder\n");
        return NULL;
    }

    // Build widget lookup map and hierarchical paths
    typedef struct {
        const char* id;
        cJSON* widget;
    } WidgetMapEntry;

    typedef struct {
        const char* widget_id;
        char* full_path;
    } WidgetPath;

    WidgetMapEntry* widget_map = NULL;
    WidgetPath* paths = NULL;
    int widget_map_count = 0;
    int path_count = 0;

    cJSON* widgets_array = root->widgets;
    if (widgets_array && cJSON_IsArray(widgets_array)) {
        // Build hierarchical widget paths for proper Tk nesting
        int widget_map_size = cJSON_GetArraySize(widgets_array);
        widget_map = calloc(widget_map_size, sizeof(WidgetMapEntry));
        paths = calloc(widget_map_size, sizeof(WidgetPath));

        if (!widget_map || !paths) {
            fprintf(stderr, "Error: Failed to allocate memory for widget map\n");
            free(widget_map);
            free(paths);
            widget_map = NULL;
            paths = NULL;
        }

        // Step 1: Build widget lookup map (simple array, not cJSON object to avoid ownership issues)
        if (widget_map && paths) {
        fprintf(stderr, "[DEBUG] wir_composer_emit: Building paths for %d widgets\n", widget_map_size);
        cJSON* w = NULL;
        cJSON_ArrayForEach(w, widgets_array) {
            cJSON* id = cJSON_GetObjectItem(w, "id");
            if (id && id->valuestring) {
                widget_map[widget_map_count].id = id->valuestring;
                widget_map[widget_map_count].widget = w;
                widget_map_count++;
            }
        }

        // Step 2: Build full hierarchical paths for all widgets
        int idx = 0;
        cJSON_ArrayForEach(w, widgets_array) {
            cJSON* id = cJSON_GetObjectItem(w, "id");
            if (!id || !id->valuestring) continue;

            // Build path by walking up parent chain (WITHOUT leading dot - Tk emitter adds it)
            char path[512];
            snprintf(path, sizeof(path), "%s", id->valuestring);

            // Walk up the parent hierarchy with cycle detection
            cJSON* current = w;
            int max_depth = 100;  // Prevent infinite loops
            int depth = 0;
            while (current && depth < max_depth) {
                cJSON* parent_id_json = cJSON_GetObjectItem(current, "parent_id");
                if (!parent_id_json || !parent_id_json->valuestring) break;

                const char* parent_id = parent_id_json->valuestring;

                // Check for circular reference
                if (strcmp(parent_id, id->valuestring) == 0) {
                    fprintf(stderr, "[ERROR] Circular reference detected in widget %s\n", id->valuestring);
                    break;
                }

                // Prepend parent to path: "parent" + "child" = "parent.child"
                char new_path[512];
                int current_len = strlen(path);
                int parent_len = strlen(parent_id);

                // Check if we have space for "parent.child"
                if (parent_len + 1 + current_len + 1 >= (int)sizeof(new_path)) {
                    fprintf(stderr, "[ERROR] Widget path too long for widget %s (parent=%s, current=%s)\n",
                            id->valuestring, parent_id, path);
                    break;
                }
                snprintf(new_path, sizeof(new_path), "%s.%s", parent_id, path);

                // Copy back to path buffer
                snprintf(path, sizeof(path), "%s", new_path);

                // Find parent widget in map (linear search)
                current = NULL;
                for (int j = 0; j < widget_map_count; j++) {
                    if (strcmp(widget_map[j].id, parent_id) == 0) {
                        current = widget_map[j].widget;
                        break;
                    }
                }
                depth++;
            }
            if (depth >= max_depth) {
                fprintf(stderr, "[ERROR] Parent hierarchy too deep for widget %s (depth >= %d)\n",
                        id->valuestring, max_depth);
            }

            paths[idx].widget_id = id->valuestring;
            paths[idx].full_path = strdup(path);
            if (!paths[idx].full_path) {
                fprintf(stderr, "[ERROR] Failed to allocate memory for path: %s\n", path);
                continue;  // Skip this widget
            }
            fprintf(stderr, "[DEBUG] Built path: %s -> %s\n", id->valuestring, path);
            idx++;
        }
            path_count = idx;
        fprintf(stderr, "[DEBUG] Built %d hierarchical paths\n", path_count);

        // Step 3: Sort widgets by path depth for proper Tk parent-before-child ordering
        // Tk requires parents to be created before children, so sort by depth (fewer dots = shallower)
        if (strcmp(emitter->toolkit->name, "tk") == 0) {
            fprintf(stderr, "[DEBUG] Sorting %d widgets by depth for Tk\n", path_count);

            // Simple bubble sort by path depth (number of dots in path)
            for (int i = 0; i < path_count - 1; i++) {
                for (int j = 0; j < path_count - i - 1; j++) {
                    // Count dots in each path
                    int dots_j = 0, dots_j1 = 0;
                    for (const char* p = paths[j].full_path; *p; p++) if (*p == '.') dots_j++;
                    for (const char* p = paths[j+1].full_path; *p; p++) if (*p == '.') dots_j1++;

                    // Swap if paths[j] has more dots (deeper) than paths[j+1]
                    if (dots_j > dots_j1) {
                        WidgetPath temp = paths[j];
                        paths[j] = paths[j+1];
                        paths[j+1] = temp;
                    }
                }
            }

            fprintf(stderr, "[DEBUG] Widget order after sorting:\n");
            for (int i = 0; i < path_count; i++) {
                int dots = 0;
                for (const char* p = paths[i].full_path; *p; p++) if (*p == '.') dots++;
                fprintf(stderr, "[DEBUG]   %d: %s (depth=%d)\n", i, paths[i].full_path, dots);
            }
        }
        }  // End of if (widget_map && paths)
    }

    // Emit header comment if language emitter supports it
    if (emitter->language->emit_comment) {
        sb_append_fmt(sb, "# Generated by Kryon WIR Composer\n");
        sb_append_fmt(sb, "# Language: %s, Toolkit: %s\n",
                     emitter->language->name, emitter->toolkit->name);
        sb_append_fmt(sb, "# Window: %s (%dx%d)\n\n",
                     root->title, root->width, root->height);
    }

    // Emit window setup for Tcl/Tk
    if (strcmp(emitter->toolkit->name, "tk") == 0) {
        // Set window title
        if (root->title) {
            sb_append_fmt(sb, "wm title . {%s}\n", root->title);
        }

        // Set window geometry
        if (root->width > 0 && root->height > 0) {
            sb_append_fmt(sb, "wm geometry . %dx%d\n", root->width, root->height);
        }

        // Set window background color
        if (root->background) {
            sb_append_fmt(sb, ". configure -background %s\n", root->background);
        }

        sb_append_fmt(sb, "\n");
    }

    // Emit all widgets
    if (widgets_array && cJSON_IsArray(widgets_array)) {
        // For Tk, use sorted order to ensure parents are created before children
        // For other toolkits, use original order
        bool use_sorted_order = (strcmp(emitter->toolkit->name, "tk") == 0) &&
                                (paths && path_count > 0);

        if (use_sorted_order) {
            // Emit in sorted order (by depth)
            for (int i = 0; i < path_count; i++) {
                // Find widget in widget_map by widget_id
                cJSON* widget_json = NULL;
                for (int j = 0; j < widget_map_count; j++) {
                    if (strcmp(widget_map[j].id, paths[i].widget_id) == 0) {
                        widget_json = widget_map[j].widget;
                        break;
                    }
                }

                if (!widget_json) {
                    fprintf(stderr, "Warning: Could not find widget '%s' in map\n", paths[i].widget_id);
                    continue;
                }

                // Create temporary WIRWidget wrapper
                WIRWidget widget = {0};
                widget.json = widget_json;
                widget.id = paths[i].full_path;  // Use pre-computed hierarchical path

                cJSON* tk_type = cJSON_GetObjectItem(widget_json, "tk_type");
                widget.widget_type = tk_type ? tk_type->valuestring : NULL;

                cJSON* kir_type = cJSON_GetObjectItem(widget_json, "kir_type");
                widget.kir_type = kir_type ? kir_type->valuestring : NULL;

                // Emit widget creation
                if (!emitter->toolkit->emit_widget_creation) {
                    fprintf(stderr, "Warning: emit_widget_creation is NULL\n");
                    continue;
                }

                if (!emitter->toolkit->emit_widget_creation(sb, &widget)) {
                    fprintf(stderr, "Warning: Failed to emit widget '%s'\n", widget.id ? widget.id : "NULL");
                }

                // Emit properties
                cJSON* props = cJSON_GetObjectItem(widget_json, "properties");
                if (props && emitter->toolkit->emit_property_assignment) {
                    // Iterate over object items (properties is an object, not array)
                    cJSON* prop = NULL;
                    for (prop = props->child; prop != NULL; prop = prop->next) {
                        if (prop->string) {
                            // Pass the property value (prop itself) to the emitter
                            // Also pass the widget type so the emitter can filter valid properties
                            emitter->toolkit->emit_property_assignment(sb, widget.id,
                                                                        prop->string, prop,
                                                                        widget.widget_type);
                        }
                    }
                }

                // Emit layout
                if (emitter->toolkit->emit_layout_command) {
                    emitter->toolkit->emit_layout_command(sb, &widget);
                }

                sb_append_fmt(sb, "\n");
            }
        } else {
            // Original order for non-Tk toolkits
            cJSON* widget_json = NULL;
            cJSON_ArrayForEach(widget_json, widgets_array) {
                // Create temporary WIRWidget wrapper
                WIRWidget widget = {0};
                widget.json = widget_json;

                cJSON* id = cJSON_GetObjectItem(widget_json, "id");
                const char* simple_id = id ? id->valuestring : NULL;

                // Find and use the full hierarchical path
                widget.id = simple_id;  // Default to simple ID

                // Look up hierarchical path if available
                if (paths && path_count > 0) {
                    for (int i = 0; i < path_count; i++) {
                        if (strcmp(paths[i].widget_id, simple_id) == 0) {
                            widget.id = paths[i].full_path;
                            break;
                        }
                    }
                }

                cJSON* tk_type = cJSON_GetObjectItem(widget_json, "tk_type");
                widget.widget_type = tk_type ? tk_type->valuestring : NULL;

                cJSON* kir_type = cJSON_GetObjectItem(widget_json, "kir_type");
                widget.kir_type = kir_type ? kir_type->valuestring : NULL;

                // Emit widget creation
                if (!emitter->toolkit->emit_widget_creation) {
                    fprintf(stderr, "Warning: emit_widget_creation is NULL\n");
                    continue;
                }

                if (!emitter->toolkit->emit_widget_creation(sb, &widget)) {
                    fprintf(stderr, "Warning: Failed to emit widget '%s'\n", widget.id ? widget.id : "NULL");
                }

                // Emit properties
                cJSON* props = cJSON_GetObjectItem(widget_json, "properties");
                if (props && emitter->toolkit->emit_property_assignment) {
                    // Iterate over object items (properties is an object, not array)
                    cJSON* prop = NULL;
                    for (prop = props->child; prop != NULL; prop = prop->next) {
                        if (prop->string) {
                            // Pass the property value (prop itself) to the emitter
                            // Also pass the widget type so the emitter can filter valid properties
                            emitter->toolkit->emit_property_assignment(sb, widget.id,
                                                                        prop->string, prop,
                                                                        widget.widget_type);
                        }
                    }
                }

                // Emit layout
                if (emitter->toolkit->emit_layout_command) {
                    emitter->toolkit->emit_layout_command(sb, &widget);
                }

                sb_append_fmt(sb, "\n");
            }
        }
    }

    // Pack root widget for Tcl/Tk
    if (strcmp(emitter->toolkit->name, "tk") == 0) {
        // Find root widget (widget with no parent_id)
        const char* root_widget_id = NULL;
        if (widgets_array && cJSON_IsArray(widgets_array)) {
            cJSON* widget_json = NULL;
            cJSON_ArrayForEach(widget_json, widgets_array) {
                cJSON* parent_id = cJSON_GetObjectItem(widget_json, "parent_id");
                if (!parent_id || !parent_id->valuestring) {
                    // This is the root widget
                    cJSON* id = cJSON_GetObjectItem(widget_json, "id");
                    if (id && id->valuestring) {
                        root_widget_id = id->valuestring;
                        break;
                    }
                }
            }
        }

        if (root_widget_id) {
            sb_append_fmt(sb, "pack .%s -fill both -expand true\n", root_widget_id);
            sb_append_fmt(sb, "\n");
        }
    }

    // Free paths and widget map
    if (paths) {
        for (int i = 0; i < path_count; i++) {
            if (paths[i].full_path) {
                free(paths[i].full_path);
            }
        }
        free(paths);
    }

    if (widget_map) {
        free(widget_map);
    }

    // Emit handlers
    cJSON* handlers_array = root->handlers;
    if (handlers_array && cJSON_IsArray(handlers_array)) {
        sb_append_fmt(sb, "\n");

        if (emitter->language->emit_comment) {
            emitter->language->emit_comment(sb, "Event Handlers");
        }

        cJSON* handler_json = NULL;
        cJSON_ArrayForEach(handler_json, handlers_array) {
            // Create temporary WIRHandler wrapper
            WIRHandler handler = {0};
            handler.json = handler_json;

            cJSON* id = cJSON_GetObjectItem(handler_json, "id");
            handler.id = id ? id->valuestring : NULL;

            cJSON* event_type = cJSON_GetObjectItem(handler_json, "event_type");
            handler.event_type = event_type ? event_type->valuestring : NULL;

            cJSON* widget_id = cJSON_GetObjectItem(handler_json, "widget_id");
            handler.widget_id = widget_id ? widget_id->valuestring : NULL;

            // Emit handler
            if (emitter->toolkit->emit_event_binding) {
                emitter->toolkit->emit_event_binding(sb, &handler);
            }
        }
    }

    // Get final output
    char* output = sb_get(sb);
    sb_free(sb);

    return output;
}

char* wir_compose_and_emit(const char* language, const char* toolkit,
                           WIRRoot* root, const WIRComposerOptions* options) {
    WIRComposedEmitter* emitter = wir_compose(language, toolkit, options);
    if (!emitter) {
        return NULL;
    }

    char* output = wir_composer_emit(emitter, root);
    wir_composer_free(emitter);

    return output;
}
