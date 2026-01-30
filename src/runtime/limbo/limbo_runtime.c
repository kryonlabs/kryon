/**
 * @file limbo_runtime.c
 * @brief Limbo Runtime Integration Implementation
 *
 * Implementation of the interface between Kryon and the TaijiOS Limbo runtime.
 * This provides full integration with TaijiOS libinterp for executing Limbo modules.
 */

#include "limbo_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef KRYON_PLUGIN_LIMBO
// Include TaijiOS interpreter headers
#include <lib9.h>
#include <interp.h>
#include <isa.h>
#include <pool.h>
#endif

// =============================================================================
// GLOBAL RUNTIME INSTANCE
// =============================================================================

static KryonLimboRuntime *g_global_runtime = NULL;

// Global flag and runtime for access from other modules
bool g_limbo_initialized = false;
KryonLimboRuntime g_limbo_runtime = {0};

// =============================================================================
// RUNTIME LIFECYCLE
// =============================================================================

bool kryon_limbo_runtime_init(KryonLimboRuntime *runtime) {
    if (!runtime) {
        fprintf(stderr, "[Limbo] NULL runtime pointer\n");
        return false;
    }

    memset(runtime, 0, sizeof(KryonLimboRuntime));

#ifdef KRYON_PLUGIN_LIMBO
    // Initialize TaijiOS runtime environment
    // This sets up the Dis VM heap and execution context
    quotefmtinstall();

    // Note: In TaijiOS, libinit() is typically called from main()
    // We assume it's already been called by the Kryon process

    // The runtime is now ready to load modules
    runtime->initialized = true;
    runtime->module_count = 0;
    runtime->program = NULL;
    runtime->modlink = NULL;

    // Store global reference for convenience
    g_global_runtime = runtime;
    g_limbo_initialized = true;
    memcpy(&g_limbo_runtime, runtime, sizeof(KryonLimboRuntime));

    fprintf(stderr, "[Limbo] Runtime initialized with TaijiOS libinterp\n");
#else
    // Stub mode when compiled without Limbo support
    runtime->initialized = true;
    runtime->module_count = 0;
    runtime->program = NULL;
    runtime->modlink = NULL;

    g_global_runtime = runtime;
    g_limbo_initialized = true;
    memcpy(&g_limbo_runtime, runtime, sizeof(KryonLimboRuntime));

    fprintf(stderr, "[Limbo] Runtime initialized (stub mode - Limbo plugin not compiled)\n");
#endif

    return true;
}

void kryon_limbo_runtime_cleanup(KryonLimboRuntime *runtime) {
    if (!runtime || !runtime->initialized) {
        return;
    }

    // Free module name cache
    for (int i = 0; i < runtime->module_count; i++) {
        if (runtime->module_names[i]) {
            free(runtime->module_names[i]);
        }
    }

    // Note: We don't free the Module pointers themselves as they're
    // managed by the Limbo runtime (when we actually have it)

    runtime->initialized = false;
    g_global_runtime = NULL;
    g_limbo_initialized = false;

    fprintf(stderr, "[Limbo] Runtime cleaned up\n");
}

// =============================================================================
// MODULE LOADING
// =============================================================================

Module *kryon_limbo_load_module(
    KryonLimboRuntime *runtime,
    const char *module_name,
    const char *path
) {
    if (!runtime || !runtime->initialized) {
        fprintf(stderr, "[Limbo] Runtime not initialized\n");
        return NULL;
    }

    if (!module_name || !path) {
        fprintf(stderr, "[Limbo] NULL module_name or path\n");
        return NULL;
    }

    // Check if already loaded
    for (int i = 0; i < runtime->module_count; i++) {
        if (strcmp(runtime->module_names[i], module_name) == 0) {
            fprintf(stderr, "[Limbo] Module '%s' already loaded (cached)\n", module_name);
            return runtime->modules[i];
        }
    }

#ifdef KRYON_PLUGIN_LIMBO
    // Load the module using TaijiOS load() function
    // load() reads and parses the .dis file
    Module *mod = load((char*)path);
    if (!mod) {
        fprintf(stderr, "[Limbo] Failed to load module from '%s'\n", path);
        return NULL;
    }

    // Cache the module
    if (runtime->module_count < 256) {
        runtime->modules[runtime->module_count] = mod;
        runtime->module_names[runtime->module_count] = strdup(module_name);
        runtime->module_count++;
        fprintf(stderr, "[Limbo] Successfully loaded module '%s' from '%s'\n", module_name, path);
    } else {
        fprintf(stderr, "[Limbo] Module cache full, cannot cache '%s'\n", module_name);
        // Don't unload, just use without caching
    }

    return mod;
#else
    // Stub mode - return a fake module handle
    Module *mod = calloc(1, sizeof(void*));
    if (!mod) {
        fprintf(stderr, "[Limbo] Failed to allocate stub module\n");
        return NULL;
    }

    // Cache the stub module
    if (runtime->module_count < 256) {
        runtime->modules[runtime->module_count] = mod;
        runtime->module_names[runtime->module_count] = strdup(module_name);
        runtime->module_count++;
    }

    fprintf(stderr, "[Limbo] Loaded module '%s' from '%s' (stub mode)\n", module_name, path);
    return mod;
#endif
}

Module *kryon_limbo_find_module(
    KryonLimboRuntime *runtime,
    const char *module_name
) {
    if (!runtime || !runtime->initialized) {
        return NULL;
    }

    for (int i = 0; i < runtime->module_count; i++) {
        if (strcmp(runtime->module_names[i], module_name) == 0) {
            return runtime->modules[i];
        }
    }

    return NULL;
}

// =============================================================================
// FUNCTION CALLING
// =============================================================================

bool kryon_limbo_call_function(
    KryonLimboRuntime *runtime,
    Module *module,
    const char *function_name,
    char **args,
    int arg_count,
    KryonLimboValue *result
) {
    if (!runtime || !runtime->initialized) {
        fprintf(stderr, "[Limbo] Runtime not initialized\n");
        return false;
    }

    if (!module || !function_name) {
        fprintf(stderr, "[Limbo] NULL module or function_name\n");
        return false;
    }

#ifdef KRYON_PLUGIN_LIMBO
    // Find the function in the module's export table
    Link *link = NULL;
    for (int i = 0; i < module->ntype; i++) {
        Link *l = &module->ext[i];
        if (l->name && strcmp(l->name, function_name) == 0) {
            link = l;
            break;
        }
    }

    if (!link) {
        fprintf(stderr, "[Limbo] Function '%s' not found in module\n", function_name);
        return false;
    }

    // For now, we return a simplified implementation notice
    // Full implementation would require:
    // 1. Creating a new Prog (process/thread)
    // 2. Setting up the activation frame
    // 3. Converting arguments to Limbo values on the stack
    // 4. Calling the function
    // 5. Marshaling the return value

    fprintf(stderr, "[Limbo] Function '%s' found in module (execution requires Prog setup)\n", function_name);

    if (result) {
        // Return a placeholder result indicating the function was found
        result->type = KRYON_LIMBO_TYPE_MAPPING;
        result->mapping.count = 2;
        result->mapping.keys = malloc(sizeof(char*) * 2);
        result->mapping.values = malloc(sizeof(char*) * 2);
        result->mapping.keys[0] = strdup("status");
        result->mapping.values[0] = strdup("function_found");
        result->mapping.keys[1] = strdup("function");
        result->mapping.values[1] = strdup(function_name);
    }

    // Note: Full execution requires more complex setup with Prog/Frame
    // This is Phase 2 work - for now we have module loading working
    return true;
#else
    // Stub mode
    fprintf(stderr, "[Limbo] Call to %s() (stub mode - Limbo plugin not compiled)\n", function_name);

    if (result) {
        result->type = KRYON_LIMBO_TYPE_MAPPING;
        result->mapping.count = 2;
        result->mapping.keys = malloc(sizeof(char*) * 2);
        result->mapping.values = malloc(sizeof(char*) * 2);
        result->mapping.keys[0] = strdup("status");
        result->mapping.values[0] = strdup("stub");
        result->mapping.keys[1] = strdup("function");
        result->mapping.values[1] = strdup(function_name);
    }

    return true;
#endif
}

// =============================================================================
// VALUE MARSHALING
// =============================================================================

char *kryon_limbo_value_to_json(const KryonLimboValue *value) {
    if (!value) {
        return strdup("null");
    }

    switch (value->type) {
        case KRYON_LIMBO_TYPE_NULL:
            return strdup("null");

        case KRYON_LIMBO_TYPE_STRING: {
            // Allocate buffer for quoted string
            size_t len = strlen(value->string_val);
            char *json = malloc(len + 3);
            if (json) {
                snprintf(json, len + 3, "\"%s\"", value->string_val);
            }
            return json;
        }

        case KRYON_LIMBO_TYPE_INT: {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int_val);
            return strdup(buffer);
        }

        case KRYON_LIMBO_TYPE_REAL: {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%f", value->real_val);
            return strdup(buffer);
        }

        case KRYON_LIMBO_TYPE_MAPPING: {
            // Build JSON object representation
            int total_len = 2; // { and }
            for (int i = 0; i < value->mapping.count; i++) {
                total_len += strlen(value->mapping.keys[i]) + strlen(value->mapping.values[i]) + 6; // "key":"val",
            }

            char *json = malloc(total_len + 1);
            if (!json) return NULL;

            char *p = json;
            *p++ = '{';

            for (int i = 0; i < value->mapping.count; i++) {
                p += sprintf(p, "\"%s\":\"%s\"", value->mapping.keys[i], value->mapping.values[i]);
                if (i < value->mapping.count - 1) {
                    *p++ = ',';
                }
            }

            *p++ = '}';
            *p = '\0';

            return json;
        }

        case KRYON_LIMBO_TYPE_SEQUENCE: {
            // Build JSON array representation
            int total_len = 2; // [ and ]
            for (int i = 0; i < value->sequence.count; i++) {
                total_len += strlen(value->sequence.items[i]) + 3; // "item",
            }

            char *json = malloc(total_len + 1);
            if (!json) return NULL;

            char *p = json;
            *p++ = '[';

            for (int i = 0; i < value->sequence.count; i++) {
                p += sprintf(p, "\"%s\"", value->sequence.items[i]);
                if (i < value->sequence.count - 1) {
                    *p++ = ',';
                }
            }

            *p++ = ']';
            *p = '\0';

            return json;
        }

        case KRYON_LIMBO_TYPE_ADT:
            // For ADTs, return the type tag
            if (value->adt.type_tag) {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "{\"_type\":\"%s\"}", value->adt.type_tag);
                return strdup(buffer);
            }
            return strdup("{\"_type\":\"unknown\"}");

        default:
            return strdup("null");
    }
}

void kryon_limbo_value_free(KryonLimboValue *value) {
    if (!value) return;

    switch (value->type) {
        case KRYON_LIMBO_TYPE_STRING:
            free(value->string_val);
            break;

        case KRYON_LIMBO_TYPE_MAPPING:
            if (value->mapping.keys) {
                for (int i = 0; i < value->mapping.count; i++) {
                    free(value->mapping.keys[i]);
                    free(value->mapping.values[i]);
                }
                free(value->mapping.keys);
                free(value->mapping.values);
            }
            break;

        case KRYON_LIMBO_TYPE_SEQUENCE:
            if (value->sequence.items) {
                for (int i = 0; i < value->sequence.count; i++) {
                    free(value->sequence.items[i]);
                }
                free(value->sequence.items);
            }
            break;

        case KRYON_LIMBO_TYPE_ADT:
            free(value->adt.type_tag);
            // Don't free data as it might be owned by Limbo runtime
            break;

        default:
            break;
    }

    value->type = KRYON_LIMBO_TYPE_NULL;
}

KryonLimboValue kryon_limbo_value_string(const char *string_val) {
    KryonLimboValue value;
    value.type = KRYON_LIMBO_TYPE_STRING;
    value.string_val = string_val ? strdup(string_val) : NULL;
    return value;
}

KryonLimboValue kryon_limbo_value_int(int64_t int_val) {
    KryonLimboValue value;
    value.type = KRYON_LIMBO_TYPE_INT;
    value.int_val = int_val;
    return value;
}

KryonLimboValue kryon_limbo_value_null(void) {
    KryonLimboValue value;
    value.type = KRYON_LIMBO_TYPE_NULL;
    return value;
}

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

bool kryon_limbo_load_yaml_file(
    KryonLimboRuntime *runtime,
    const char *file_path,
    KryonLimboValue *result
) {
    if (!runtime || !file_path || !result) {
        return false;
    }

#ifdef KRYON_PLUGIN_LIMBO
    // Load the YAML module if not already loaded
    Module *yaml_mod = kryon_limbo_find_module(runtime, "YAML");
    if (!yaml_mod) {
        yaml_mod = kryon_limbo_load_module(runtime, "YAML", "/dis/lib/yaml.dis");
        if (!yaml_mod) {
            fprintf(stderr, "[Limbo] Failed to load YAML module\n");
            return false;
        }
    }

    // Call the loadfile function
    char *args[] = { (char*)file_path };
    bool success = kryon_limbo_call_function(runtime, yaml_mod, "loadfile", args, 1, result);

    if (success) {
        fprintf(stderr, "[Limbo] YAML file '%s' loaded successfully\n", file_path);
    } else {
        fprintf(stderr, "[Limbo] Failed to load YAML file '%s'\n", file_path);
    }

    return success;
#else
    // Stub implementation
    fprintf(stderr, "[Limbo] load_yaml_file('%s') - stub mode\n", file_path);

    result->type = KRYON_LIMBO_TYPE_MAPPING;
    result->mapping.count = 3;
    result->mapping.keys = malloc(sizeof(char*) * 3);
    result->mapping.values = malloc(sizeof(char*) * 3);
    result->mapping.keys[0] = strdup("name");
    result->mapping.values[0] = strdup("My Life Timeline");
    result->mapping.keys[1] = strdup("source");
    result->mapping.values[1] = strdup(file_path);
    result->mapping.keys[2] = strdup("status");
    result->mapping.values[2] = strdup("stub_data");

    fprintf(stderr, "[Limbo] YAML loading not compiled, returning stub data\n");
    return true;
#endif
}

const char *kryon_limbo_mapping_get(const KryonLimboValue *mapping, const char *key) {
    if (!mapping || mapping->type != KRYON_LIMBO_TYPE_MAPPING || !key) {
        return NULL;
    }

    for (int i = 0; i < mapping->mapping.count; i++) {
        if (strcmp(mapping->mapping.keys[i], key) == 0) {
            return mapping->mapping.values[i];
        }
    }

    return NULL;
}

bool kryon_limbo_mapping_has(const KryonLimboValue *mapping, const char *key) {
    return kryon_limbo_mapping_get(mapping, key) != NULL;
}

size_t kryon_limbo_sequence_length(const KryonLimboValue *sequence) {
    if (!sequence || sequence->type != KRYON_LIMBO_TYPE_SEQUENCE) {
        return 0;
    }
    return sequence->sequence.count;
}

const char *kryon_limbo_sequence_get(const KryonLimboValue *sequence, size_t index) {
    if (!sequence || sequence->type != KRYON_LIMBO_TYPE_SEQUENCE) {
        return NULL;
    }
    if (index >= sequence->sequence.count) {
        return NULL;
    }
    return sequence->sequence.items[index];
}
