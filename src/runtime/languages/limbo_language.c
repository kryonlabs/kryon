/**
 * @file limbo_language.c
 * @brief Limbo language plugin for Kryon
 *
 * This plugin allows Kryon functions to call Limbo modules from TaijiOS.
 * It enables seamless integration between Kryon and the TaijiOS module ecosystem.
 *
 * Usage in Kryon:
 *   function "limbo" loadData() {
 *       # Limbo code goes here
 *   }
 *
 * Or use built-in functions:
 *   data = loadModule("YAML", "/dis/lib/yaml.dis")
 *   result = callModuleFunction("YAML", "load", "data.yaml")
 */

#ifdef KRYON_PLUGIN_LIMBO

#include "runtime.h"
#include "language_plugins.h"
#include "../limbo/limbo_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// GLOBAL STATE
// =============================================================================

static KryonLimboRuntime g_limbo_runtime = {0};
static bool g_limbo_initialized = false;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * Check if the Limbo runtime is available
 */
static bool check_limbo_available(void) {
    // Try to load a simple module to test availability
    // For now, just check if we can initialize the runtime
    return true;
}

/**
 * Parse a Limbo function declaration to extract module and function
 *
 * Expected format in function code:
 *   ModuleName.functionName(args...)
 *
 * For example:
 *   YAML.loadfile("data.yaml")
 */
static bool parse_limbo_call(
    const char *code,
    char *module_name,
    size_t module_name_size,
    char *function_name,
    size_t function_name_size,
    char *args,
    size_t args_size
) {
    if (!code) return false;

    // Simple parser: look for "Module.func(" pattern
    const char *dot = strchr(code, '.');
    const char *paren = strchr(code, '(');

    if (!dot || !paren || dot >= paren) {
        fprintf(stderr, "[Limbo] Invalid call format, expected 'Module.function(...)'\n");
        return false;
    }

    // Extract module name
    size_t module_len = dot - code;
    if (module_len >= module_name_size) {
        fprintf(stderr, "[Limbo] Module name too long\n");
        return false;
    }
    strncpy(module_name, code, module_len);
    module_name[module_len] = '\0';

    // Extract function name
    size_t func_len = paren - dot - 1;
    if (func_len >= function_name_size) {
        fprintf(stderr, "[Limbo] Function name too long\n");
        return false;
    }
    strncpy(function_name, dot + 1, func_len);
    function_name[func_len] = '\0';

    // Extract arguments (everything between parentheses)
    const char *end = strchr(paren + 1, ')');
    if (!end) {
        fprintf(stderr, "[Limbo] Unclosed parenthesis\n");
        return false;
    }

    size_t args_len = end - paren - 1;
    if (args_len >= args_size) {
        fprintf(stderr, "[Limbo] Arguments too long\n");
        return false;
    }
    strncpy(args, paren + 1, args_len);
    args[args_len] = '\0';

    fprintf(stderr, "[Limbo] Parsed: module='%s', function='%s', args='%s'\n",
            module_name, function_name, args);

    return true;
}

/**
 * Convert string to lowercase
 */
static void strlower(char *str) {
    if (!str) return;
    for (; *str; str++) {
        if (*str >= 'A' && *str <= 'Z') {
            *str = *str + 32;
        }
    }
}

/**
 * Parse string argument from args string
 *
 * Extracts a quoted string like "data.yaml"
 */
static bool parse_string_arg(const char *args, char *buffer, size_t buffer_size) {
    if (!args || !buffer) return false;

    // Skip leading whitespace
    while (*args == ' ' || *args == '\t') args++;

    // Check for opening quote
    if (*args != '"') {
        fprintf(stderr, "[Limbo] Expected quoted string argument\n");
        return false;
    }
    args++;

    // Find closing quote
    const char *end = strchr(args, '"');
    if (!end) {
        fprintf(stderr, "[Limbo] Unclosed string argument\n");
        return false;
    }

    size_t len = end - args;
    if (len >= buffer_size) {
        fprintf(stderr, "[Limbo] String argument too long\n");
        return false;
    }

    strncpy(buffer, args, len);
    buffer[len] = '\0';

    return true;
}

// =============================================================================
// PLUGIN INTERFACE IMPLEMENTATION
// =============================================================================

/**
 * Initialize the Limbo plugin
 */
static bool limbo_init(KryonRuntime *runtime) {
    if (g_limbo_initialized) {
        return true;
    }

    fprintf(stderr, "[Limbo] Initializing plugin...\n");

    if (!kryon_limbo_runtime_init(&g_limbo_runtime)) {
        fprintf(stderr, "[Limbo] Failed to initialize runtime\n");
        return false;
    }

    g_limbo_initialized = true;
    fprintf(stderr, "[Limbo] Plugin initialized successfully\n");
    return true;
}

/**
 * Shutdown the Limbo plugin
 */
static void limbo_shutdown(KryonRuntime *runtime) {
    if (!g_limbo_initialized) {
        return;
    }

    fprintf(stderr, "[Limbo] Shutting down plugin...\n");
    kryon_limbo_runtime_cleanup(&g_limbo_runtime);
    g_limbo_initialized = false;
}

/**
 * Check if Limbo is available
 */
static bool limbo_is_available(KryonRuntime *runtime) {
    return g_limbo_initialized && check_limbo_available();
}

/**
 * Execute a Limbo function
 */
static KryonLanguageExecutionResult limbo_execute(
    KryonRuntime *runtime,
    KryonElement *element,
    KryonScriptFunction *function,
    char *error_buffer,
    size_t error_buffer_size
) {
    if (!g_limbo_initialized) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size, "Limbo runtime not initialized");
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    if (!function || !function->code) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size, "No function code provided");
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    fprintf(stderr, "[Limbo] Executing function: %s\n", function->name);

    // Parse the Limbo call
    char module_name[256];
    char function_name[256];
    char args[1024];

    if (!parse_limbo_call(function->code, module_name, sizeof(module_name),
                         function_name, sizeof(function_name),
                         args, sizeof(args))) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Failed to parse Limbo call: %s", function->code);
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    // Build module path
    char module_path[512];
    char lower_name[256];
    strncpy(lower_name, module_name, sizeof(lower_name) - 1);
    strlower(lower_name);
    snprintf(module_path, sizeof(module_path), "/dis/lib/%s.dis", lower_name);

    // Load the module
    Module *mod = kryon_limbo_load_module(&g_limbo_runtime, module_name, module_path);
    if (!mod) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Failed to load Limbo module: %s", module_name);
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    // Parse arguments
    char *argv[16];
    int argc = 0;

    char arg_str[512];
    if (parse_string_arg(args, arg_str, sizeof(arg_str))) {
        argv[argc++] = arg_str;
    }

    // Call the function
    KryonLimboValue result;
    if (!kryon_limbo_call_function(&g_limbo_runtime, mod, function_name,
                                   argv, argc, &result)) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Failed to call Limbo function: %s.%s",
                    module_name, function_name);
        }
        return KRYON_LANG_RESULT_ERROR;
    }

    // Convert result to JSON and store it
    char *result_json = kryon_limbo_value_to_json(&result);
    if (result_json) {
        // Store result in runtime variable if function has a name
        if (function->name && runtime) {
            // TODO: Add variable to runtime state
            fprintf(stderr, "[Limbo] Result: %s\n", result_json);
        }
        free(result_json);
    }

    kryon_limbo_value_free(&result);

    return KRYON_LANG_RESULT_SUCCESS;
}

// =============================================================================
// PLUGIN REGISTRATION
// =============================================================================

static KryonLanguagePlugin limbo_plugin = {
    .identifier = "limbo",
    .name = "Limbo",
    .version = "1.0.0",
    .init = limbo_init,
    .shutdown = limbo_shutdown,
    .is_available = limbo_is_available,
    .execute = limbo_execute,
};

__attribute__((constructor))
static void register_limbo_plugin(void) {
    fprintf(stderr, "[Limbo] Registering plugin...\n");
    if (kryon_language_register(&limbo_plugin)) {
        fprintf(stderr, "[Limbo] Plugin registered successfully\n");
    } else {
        fprintf(stderr, "[Limbo] Failed to register plugin\n");
    }
}

#endif // KRYON_PLUGIN_LIMBO
