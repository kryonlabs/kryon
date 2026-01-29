/**
 * @file language_registry.c
 * @brief Language plugin registry implementation
 */

#include "runtime.h"
#include "language_plugins.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// REGISTRY STORAGE
// =============================================================================

#define MAX_LANGUAGE_PLUGINS 16

static struct {
    KryonLanguagePlugin *plugins[MAX_LANGUAGE_PLUGINS];
    size_t count;
    bool initialized;
} g_language_registry = {0};

// =============================================================================
// REGISTRATION
// =============================================================================

bool kryon_language_register(KryonLanguagePlugin *plugin) {
    if (!plugin || !plugin->identifier) {
        fprintf(stderr, "[Kryon/Language] Cannot register NULL plugin or plugin with NULL identifier\n");
        return false;
    }

    // Check for duplicate identifier
    for (size_t i = 0; i < g_language_registry.count; i++) {
        if (strcmp(g_language_registry.plugins[i]->identifier, plugin->identifier) == 0) {
            fprintf(stderr, "[Kryon/Language] Plugin with identifier '%s' already registered\n",
                    plugin->identifier);
            return false;
        }
    }

    // Check capacity
    if (g_language_registry.count >= MAX_LANGUAGE_PLUGINS) {
        fprintf(stderr, "[Kryon/Language] Registry full, cannot register plugin '%s'\n",
                plugin->identifier);
        return false;
    }

    // Register
    g_language_registry.plugins[g_language_registry.count++] = plugin;

    fprintf(stderr, "[Kryon/Language] Registered plugin: '%s' (%s)\n",
            plugin->name ? plugin->name : plugin->identifier,
            plugin->identifier[0] ? plugin->identifier : "native");

    return true;
}

// =============================================================================
// LOOKUP
// =============================================================================

KryonLanguagePlugin* kryon_language_get(const char *identifier) {
    if (!identifier) {
        return NULL;
    }

    for (size_t i = 0; i < g_language_registry.count; i++) {
        if (strcmp(g_language_registry.plugins[i]->identifier, identifier) == 0) {
            return g_language_registry.plugins[i];
        }
    }

    return NULL;
}

bool kryon_language_available(KryonRuntime *runtime, const char *identifier) {
    KryonLanguagePlugin *plugin = kryon_language_get(identifier);
    if (!plugin) {
        return false;
    }

    if (plugin->is_available) {
        return plugin->is_available(runtime);
    }

    // If no is_available function, assume available
    return true;
}

// =============================================================================
// EXECUTION DISPATCH
// =============================================================================

KryonLanguageExecutionResult kryon_language_execute_function(
    KryonRuntime *runtime,
    KryonElement *element,
    KryonScriptFunction *function,
    char *error_buffer,
    size_t error_buffer_size
) {
    if (!runtime || !function) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size, "Invalid runtime or function");
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    // Get language identifier (empty string for native)
    const char *lang_id = function->language ? function->language : "";

    // Find plugin
    KryonLanguagePlugin *plugin = kryon_language_get(lang_id);
    if (!plugin) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Language plugin not found: '%s'", lang_id);
        }
        return KRYON_LANG_RESULT_NOT_AVAILABLE;
    }

    // Check availability
    if (plugin->is_available && !plugin->is_available(runtime)) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Language '%s' is not available", lang_id);
        }
        return KRYON_LANG_RESULT_NOT_AVAILABLE;
    }

    // Execute
    if (!plugin->execute) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Language plugin '%s' has no execute function", lang_id);
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    return plugin->execute(runtime, element, function, error_buffer, error_buffer_size);
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool kryon_language_init(KryonRuntime *runtime) {
    if (g_language_registry.initialized) {
        return true;
    }

    // Initialize all registered plugins
    for (size_t i = 0; i < g_language_registry.count; i++) {
        KryonLanguagePlugin *plugin = g_language_registry.plugins[i];
        if (plugin->init) {
            if (!plugin->init(runtime)) {
                fprintf(stderr, "[Kryon/Language] Failed to initialize plugin '%s'\n",
                        plugin->name ? plugin->name : plugin->identifier);
                // Continue with other plugins
            }
        }
    }

    g_language_registry.initialized = true;
    return true;
}

void kryon_language_shutdown(KryonRuntime *runtime) {
    if (!g_language_registry.initialized) {
        return;
    }

    // Shutdown all plugins in reverse order
    for (size_t i = g_language_registry.count; i > 0; i--) {
        KryonLanguagePlugin *plugin = g_language_registry.plugins[i - 1];
        if (plugin->shutdown) {
            plugin->shutdown(runtime);
        }
    }

    g_language_registry.initialized = false;
}

// =============================================================================
// UTILITIES
// =============================================================================

size_t kryon_language_list(KryonLanguagePlugin **plugins, size_t max_plugins) {
    if (plugins && max_plugins > 0) {
        size_t to_copy = g_language_registry.count < max_plugins
            ? g_language_registry.count
            : max_plugins;

        for (size_t i = 0; i < to_copy; i++) {
            plugins[i] = g_language_registry.plugins[i];
        }
    }

    return g_language_registry.count;
}
