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

    // Emit header comment if language emitter supports it
    if (emitter->language->emit_comment) {
        sb_append_fmt(sb, "# Generated by Kryon WIR Composer\n");
        sb_append_fmt(sb, "# Language: %s, Toolkit: %s\n",
                     emitter->language->name, emitter->toolkit->name);
        sb_append_fmt(sb, "# Window: %s (%dx%d)\n\n",
                     root->title, root->width, root->height);
    }

    // Emit all widgets
    cJSON* widgets_array = root->widgets;
    if (widgets_array && cJSON_IsArray(widgets_array)) {
        cJSON* widget_json = NULL;
        cJSON_ArrayForEach(widget_json, widgets_array) {
            // Create temporary WIRWidget wrapper
            WIRWidget widget = {0};
            widget.json = widget_json;

            cJSON* id = cJSON_GetObjectItem(widget_json, "id");
            widget.id = id ? id->valuestring : NULL;

            cJSON* tk_type = cJSON_GetObjectItem(widget_json, "tk_type");
            widget.widget_type = tk_type ? tk_type->valuestring : NULL;

            cJSON* kir_type = cJSON_GetObjectItem(widget_json, "kir_type");
            widget.kir_type = kir_type ? kir_type->valuestring : NULL;

            // Emit widget creation
            if (emitter->toolkit->emit_widget_creation) {
                if (!emitter->toolkit->emit_widget_creation(sb, &widget)) {
                    fprintf(stderr, "Warning: Failed to emit widget '%s'\n", widget.id);
                }
            }

            // Emit properties
            cJSON* props = cJSON_GetObjectItem(widget_json, "properties");
            if (props && emitter->toolkit->emit_property_assignment) {
                cJSON* prop = NULL;
                cJSON_ArrayForEach(prop, props) {
                    if (prop->string && prop->child) {
                        emitter->toolkit->emit_property_assignment(sb, widget.id,
                                                                    prop->string, prop->child);
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
