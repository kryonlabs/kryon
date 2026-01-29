/**
 * @file native_language.c
 * @brief Native Kryon language plugin
 *
 * This plugin handles the default/native Kryon scripting language.
 * The actual execution logic remains in elements.c for now (execute_script_function_impl).
 */

#include "runtime.h"
#include "language_plugins.h"
#include <stdio.h>

// Forward declaration of the implementation in elements.c
// This will be properly exposed via a header later
extern bool execute_script_function_impl(
    KryonRuntime* runtime,
    KryonElement* element,
    KryonScriptFunction* function
);

// =============================================================================
// NATIVE LANGUAGE EXECUTION
// =============================================================================

/**
 * Execute a function written in native Kryon scripting language
 */
static KryonLanguageExecutionResult execute_native_function(
    KryonRuntime *runtime,
    KryonElement *element,
    KryonScriptFunction *function,
    char *error_buffer,
    size_t error_buffer_size
) {
    if (!runtime || !function || !function->code) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size, "Invalid runtime or function");
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    // Call the implementation in elements.c
    bool result = execute_script_function_impl(runtime, element, function);

    if (result) {
        return KRYON_LANG_RESULT_SUCCESS;
    } else {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Function '%s' executed without supported operations",
                    function->name ? function->name : "(unnamed)");
        }
        return KRYON_LANG_RESULT_ERROR;
    }
}

// =============================================================================
// PLUGIN INTERFACE
// =============================================================================

static bool native_init(KryonRuntime *runtime) {
    (void)runtime;
    // Native language needs no initialization
    return true;
}

static void native_shutdown(KryonRuntime *runtime) {
    (void)runtime;
    // Native language needs no cleanup
}

static bool native_is_available(KryonRuntime *runtime) {
    (void)runtime;
    // Native language is always available
    return true;
}

static KryonLanguagePlugin native_plugin = {
    .identifier = "",  // Empty string = native/default
    .name = "Native Kryon",
    .version = "1.0.0",
    .init = native_init,
    .shutdown = native_shutdown,
    .is_available = native_is_available,
    .execute = execute_native_function,
};

// =============================================================================
// AUTO-REGISTRATION
// =============================================================================

__attribute__((constructor))
static void register_native_language(void) {
    kryon_language_register(&native_plugin);
}
