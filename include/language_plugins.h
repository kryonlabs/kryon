/**
 * @file language_plugins.h
 * @brief Kryon Language Plugins - Extensible scripting language support
 *
 * This header defines the language plugin system that allows Kryon functions
 * to be written in different scripting languages (native Kryon, Inferno sh, etc.)
 * while maintaining zero overhead when not used.
 *
 * Key Design Principles:
 * - Non-invasive: Additive layer, doesn't modify core semantics
 * - Opt-in: Functions declare their language explicitly
 * - Compile-time: Plugins compiled in based on build configuration
 * - Zero overhead: No penalty when plugins not used
 * - Runtime detection: Check language availability before execution
 * - Graceful fallback: Clear errors for unavailable languages
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_LANGUAGE_PLUGINS_H
#define KRYON_LANGUAGE_PLUGINS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

// Forward declarations
// Note: Do not typedef here - runtime.h/types.h already do this
struct KryonRuntime;
struct KryonScriptFunction;
struct KryonElement;

// =============================================================================
// EXECUTION RESULT
// =============================================================================

/**
 * @brief Result codes from language plugin execution
 */
typedef enum {
    KRYON_LANG_RESULT_SUCCESS = 0,      /**< Function executed successfully */
    KRYON_LANG_RESULT_ERROR,            /**< Runtime error during execution */
    KRYON_LANG_RESULT_NOT_AVAILABLE,    /**< Language plugin not available */
} KryonLanguageExecutionResult;

// =============================================================================
// LANGUAGE PLUGIN INTERFACE
// =============================================================================

/**
 * @brief Language plugin descriptor with vtable
 *
 * Each language plugin (native, sh, etc.) provides this structure
 * describing its capabilities and execution interface.
 */
typedef struct KryonLanguagePlugin {
    /** Language identifier (empty string "" for native Kryon, "sh" for Inferno shell, etc.) */
    const char *identifier;

    /** Plugin name for display purposes */
    const char *name;

    /** Plugin version string */
    const char *version;

    /**
     * Initialize the plugin
     * @param runtime The runtime context
     * @return true on success, false on failure
     */
    bool (*init)(KryonRuntime *runtime);

    /**
     * Shutdown the plugin and cleanup resources
     * @param runtime The runtime context
     */
    void (*shutdown)(KryonRuntime *runtime);

    /**
     * Check if this language plugin is currently available
     * @param runtime The runtime context
     * @return true if available (can execute), false otherwise
     */
    bool (*is_available)(KryonRuntime *runtime);

    /**
     * Execute a function in this language
     * @param runtime The runtime context
     * @param element The element context (can be NULL for global functions)
     * @param function The function to execute
     * @param error_buffer Buffer for error messages (can be NULL)
     * @param error_buffer_size Size of error buffer
     * @return Execution result code
     */
    KryonLanguageExecutionResult (*execute)(
        KryonRuntime *runtime,
        KryonElement *element,
        KryonScriptFunction *function,
        char *error_buffer,
        size_t error_buffer_size
    );
} KryonLanguagePlugin;

// =============================================================================
// LANGUAGE REGISTRY API
// =============================================================================

/**
 * @brief Register a language plugin
 *
 * Plugins register themselves using this function, typically via
 * __attribute__((constructor)) for automatic registration at load time.
 *
 * @param plugin Plugin descriptor to register
 * @return true on success, false if registration fails (e.g., duplicate identifier)
 *
 * @example
 * static KryonLanguagePlugin sh_plugin = { ... };
 *
 * __attribute__((constructor))
 * static void register_sh_plugin(void) {
 *     kryon_language_register(&sh_plugin);
 * }
 */
bool kryon_language_register(KryonLanguagePlugin *plugin);

/**
 * @brief Get a language plugin by identifier
 *
 * @param identifier Language identifier (e.g., "", "sh")
 * @return Plugin descriptor, or NULL if not found
 */
KryonLanguagePlugin* kryon_language_get(const char *identifier);

/**
 * @brief Check if a language is available
 *
 * This checks both that the plugin is registered AND that it reports
 * itself as available (e.g., external tools are present).
 *
 * @param runtime The runtime context
 * @param identifier Language identifier
 * @return true if language can be used, false otherwise
 */
bool kryon_language_available(KryonRuntime *runtime, const char *identifier);

/**
 * @brief Execute a function using its declared language
 *
 * This is the main dispatch function used by the runtime to execute
 * functions. It finds the appropriate language plugin and invokes it.
 *
 * @param runtime The runtime context
 * @param element The element context (can be NULL for global functions)
 * @param function The function to execute
 * @param error_buffer Buffer for error messages (can be NULL)
 * @param error_buffer_size Size of error buffer
 * @return Execution result code
 */
KryonLanguageExecutionResult kryon_language_execute_function(
    KryonRuntime *runtime,
    KryonElement *element,
    KryonScriptFunction *function,
    char *error_buffer,
    size_t error_buffer_size
);

/**
 * @brief Initialize the language plugin system
 *
 * Called by runtime initialization. Apps don't need to call this directly.
 *
 * @param runtime The runtime context
 * @return true on success
 */
bool kryon_language_init(KryonRuntime *runtime);

/**
 * @brief Shutdown the language plugin system
 *
 * Called by runtime shutdown. Apps don't need to call this directly.
 *
 * @param runtime The runtime context
 */
void kryon_language_shutdown(KryonRuntime *runtime);

/**
 * @brief List all registered language plugins
 *
 * Useful for debugging and diagnostics.
 *
 * @param plugins Output array of plugin pointers (can be NULL to query count)
 * @param max_plugins Size of plugins array
 * @return Number of registered plugins
 */
size_t kryon_language_list(KryonLanguagePlugin **plugins, size_t max_plugins);

#ifdef __cplusplus
}
#endif

#endif // KRYON_LANGUAGE_PLUGINS_H
