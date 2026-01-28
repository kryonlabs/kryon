/**
 * Target Handler Registry - Dynamic Implementation
 *
 * Provides a registry-based system for handling different build targets.
 * This eliminates hardcoded target checks and allows targets to be
 * registered dynamically with their capabilities and handler functions.
 *
 * KEY CHANGES:
 * - No hardcoded limits (MAX_TARGET_HANDLERS removed)
 * - Dynamic registry that grows as needed
 * - Proper cleanup of dynamically allocated handlers
 * - Thread-local context instead of global g_current_target_name
 */

#include "kryon_cli.h"
#include "build.h"
#include "../../../dis/include/dis_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Forward declare TargetHandler for use in registry */
struct TargetHandler;
typedef struct TargetHandler TargetHandler;

/* ============================================================================
 * Global Registry - Dynamic Allocation
 * ============================================================================ */

typedef struct TargetHandlerRegistry {
    TargetHandler** handlers;
    size_t count;
    size_t capacity;
} TargetHandlerRegistry;

static TargetHandlerRegistry g_registry = {
    .handlers = NULL,
    .count = 0,
    .capacity = 0
};

// Target capability flags
#define TARGET_CAN_BUILD  (1 << 0)
#define TARGET_CAN_RUN    (1 << 1)
#define TARGET_CAN_INSTALL (1 << 2)

static bool g_initialized = false;

/* ============================================================================
 * Handler Context (Explicit, not global)
 * ============================================================================ */

/**
 * Thread-local context for current target invocation
 * Replaces fragile g_current_target_name global
 */
static __thread TargetHandlerContext t_current_context = {0};

const char* target_handler_get_current_name(void) {
    return t_current_context.target_name;
}

/* ============================================================================
 * Registry Growth
 * ============================================================================ */

/**
 * Grow the handler registry by doubling capacity
 */
static void registry_grow(TargetHandlerRegistry* reg) {
    size_t new_capacity = reg->capacity == 0 ? 8 : reg->capacity * 2;
    TargetHandler** new_handlers = realloc(reg->handlers,
                                     new_capacity * sizeof(TargetHandler*));
    if (!new_handlers) {
        fprintf(stderr, "Error: Failed to grow target handler registry\n");
        return;
    }
    reg->handlers = new_handlers;
    reg->capacity = new_capacity;
    printf("[target] Registry grown to %zu handlers\n", new_capacity);
}

/* ============================================================================
 * Built-in Target Handler Implementations
 * ============================================================================ */

/**
 * Web target build handler
 */
static int web_target_build(const char* kir_file, const char* output_dir,
                            const char* project_name, const KryonConfig* config) {
    (void)config;  // Not used in web build
    return generate_html_from_kir(kir_file, output_dir, project_name, ".");
}

/**
 * Web target run handler
 */
static int web_target_run(const char* kir_file, const KryonConfig* config) {
    (void)kir_file;
    (void)config;
    fprintf(stderr, "Error: Web target should be run via dev server\n");
    fprintf(stderr, "Use 'kryon dev' or 'kryon run' without explicit target\n");
    return 1;
}

// Desktop and terminal targets removed - migrated to DIS VM target

// Android target removed - migrated to DIS VM target

/* ============================================================================
 * DIS Target Handler
 * ============================================================================ */

/**
 * DIS target build handler
 * Generates .dis bytecode from KIR
 */
static int dis_target_build(const char* kir_file, const char* output_dir,
                            const char* project_name, const KryonConfig* config) {
    (void)config;  // Not used in DIS build

    // Generate output path
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/%s.dis", output_dir, project_name);

    printf("Generating DIS bytecode: %s\n", output_path);

    // Call DIS codegen
    if (!dis_codegen_generate(kir_file, output_path)) {
        fprintf(stderr, "Error: DIS code generation failed: %s\n",
                dis_codegen_get_error());
        return 1;
    }

    printf("✓ DIS bytecode generated: %s\n", output_path);
    return 0;
}

/**
 * DIS target run handler
 * Generates .dis bytecode and executes with TaijiOS DIS VM
 */
static int dis_target_run(const char* kir_file, const KryonConfig* config) {
    // Build DIS file first
    const char* output_dir = ".";
    const char* project_name = "app";

    if (dis_target_build(kir_file, output_dir, project_name, config) != 0) {
        return 1;
    }

    // Get TaijiOS configuration
    const char* taiji_path = config && config->taiji_path
        ? config->taiji_path
        : "/home/wao/Projects/TaijiOS";

    bool use_run_app = config && config->taiji_use_run_app
        ? config->taiji_use_run_app
        : true;  // Default to run-app.sh

    const char* arch = config && config->taiji_arch
        ? config->taiji_arch
        : "x86_64";

    // Build DIS file path
    char dis_path[1024];
    snprintf(dis_path, sizeof(dis_path), "%s/%s.dis", output_dir, project_name);

    printf("Running DIS bytecode with TaijiOS\n");
    printf("DIS file: %s\n", dis_path);
    printf("TaijiOS path: %s\n", taiji_path);
    printf("Method: %s\n", use_run_app ? "run-app.sh" : "emu");

    // Validate TaijiOS installation
    char run_app_path[1024];
    snprintf(run_app_path, sizeof(run_app_path), "%s/run-app.sh", taiji_path);

    if (access(run_app_path, X_OK) != 0) {
        fprintf(stderr, "Error: TaijiOS run-app.sh not found at %s\n", run_app_path);
        fprintf(stderr, "Expected: %s\n", run_app_path);
        fprintf(stderr, "\n");
        fprintf(stderr, "To fix:\n");
        fprintf(stderr, "  1. Ensure TaijiOS is installed at: %s\n", taiji_path);
        fprintf(stderr, "  2. Or configure path in kryon.toml:\n");
        fprintf(stderr, "     [taiji]\n");
        fprintf(stderr, "     path = /path/to/TaijiOS\n");
        return 1;
    }

    char cmd[2048];

    if (use_run_app) {
        // Use run-app.sh wrapper (recommended)
        // run-app.sh expects: ./run-app.sh app.dis [args...]
        // It handles emu invocation, X11 setup, etc.
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./run-app.sh \"%s/%s.dis\"",
                taiji_path, output_dir, project_name);
    } else {
        // Direct emu invocation (for debugging)
        char emu_path[1024];
        snprintf(emu_path, sizeof(emu_path), "%s/Linux/%s/bin/emu",
                taiji_path, arch);

        if (access(emu_path, X_OK) != 0) {
            fprintf(stderr, "Error: emu binary not found at %s\n", emu_path);
            return 1;
        }

        snprintf(cmd, sizeof(cmd), "%s -r \"%s\" \"%s/%s.dis\"",
                emu_path, taiji_path, output_dir, project_name);
    }

    printf("Executing: %s\n", cmd);

    // Execute TaijiOS
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Warning: TaijiOS exited with code %d\n", result);
        return result;
    }

    printf("✓ DIS execution complete\n");
    return 0;
}

/* ============================================================================
 * Built-in Handler Definitions
 * ============================================================================ */

static TargetHandler g_web_handler = {
    .name = "web",
    .capabilities = TARGET_REQUIRES_CODEGEN,  // Web requires codegen
    .build_handler = web_target_build,
    .run_handler = web_target_run,
};

/* ============================================================================
 * Command Target Handlers
 * ============================================================================ */

/**
 * Variable expansion for command targets
 * Replaces ${KIR_FILE}, ${OUTPUT_DIR}, ${PROJECT_NAME}, ${ENTRY}
 */
static char* expand_command_vars(const char* cmd_template,
                                 const char* kir_file,
                                 const char* output_dir,
                                 const char* project_name,
                                 const KryonConfig* config) {
    if (!cmd_template) return NULL;

    char* result = strdup(cmd_template);

    // Expand ${KIR_FILE}
    if (kir_file && strstr(result, "${KIR_FILE}")) {
        char* temp = result;
        result = string_replace(temp, "${KIR_FILE}", kir_file);
        free(temp);
    }

    // Expand ${OUTPUT_DIR}
    if (output_dir && strstr(result, "${OUTPUT_DIR}")) {
        char* temp = result;
        result = string_replace(temp, "${OUTPUT_DIR}", output_dir);
        free(temp);
    }

    // Expand ${PROJECT_NAME}
    if (project_name && strstr(result, "${PROJECT_NAME}")) {
        char* temp = result;
        result = string_replace(temp, "${PROJECT_NAME}", project_name);
        free(temp);
    }

    // Expand ${ENTRY}
    if (config && config->build_entry && strstr(result, "${ENTRY}")) {
        char* temp = result;
        result = string_replace(temp, "${ENTRY}", config->build_entry);
        free(temp);
    }

    return result;
}

/**
 * Generic command target build handler
 * Uses thread-local context to identify which specific target config to use
 */
static int command_target_build(const char* kir_file, const char* output_dir,
                                const char* project_name, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for command targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the specific target config by name
    TargetConfig* target = config_get_target(config, target_name);
    if (!target) {
        fprintf(stderr, "Error: Target config not found: %s\n", target_name);
        return 1;
    }

    if (!target->build_cmd) {
        fprintf(stderr, "Error: Target '%s' missing build command\n", target_name);
        return 1;
    }

    char* cmd = expand_command_vars(target->build_cmd, kir_file,
                                    output_dir, project_name, config);

    printf("[command:%s] Building: %s\n", target->name, cmd);
    int result = system(cmd);

    free(cmd);
    return result;
}

/**
 * Generic command target run handler
 * Uses thread-local context to identify which specific target config to use
 */
static int command_target_run(const char* kir_file, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for command targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the specific target config by name
    TargetConfig* target = config_get_target(config, target_name);
    if (!target) {
        fprintf(stderr, "Error: Target config not found: %s\n", target_name);
        return 1;
    }

    if (!target->run_cmd) {
        fprintf(stderr, "Error: Target '%s' missing run command\n", target_name);
        return 1;
    }

    char* cmd = expand_command_vars(target->run_cmd, kir_file,
                                    NULL, NULL, config);

    printf("[command:%s] Running: %s\n", target->name, cmd);
    int result = system(cmd);

    free(cmd);
    return result;
}

/**
 * Alias target build handler
 * Delegates to the base target's build handler
 */
static int alias_target_build(const char* kir_file, const char* output_dir,
                             const char* project_name, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for alias targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the alias target config
    TargetConfig* alias = config_get_target(config, target_name);
    if (!alias || !alias->alias_target) {
        fprintf(stderr, "Error: Alias target config not found: %s\n", target_name);
        return 1;
    }

    printf("[alias:%s] Delegating to base target: %s\n", target_name, alias->alias_target);

    // Call the base target's build handler
    return target_handler_build(alias->alias_target, kir_file, output_dir, project_name, config);
}

/**
 * Alias target run handler
 * Delegates to the base target's run handler
 */
static int alias_target_run(const char* kir_file, const KryonConfig* config) {
    if (!config) {
        fprintf(stderr, "Error: Config required for alias targets\n");
        return 1;
    }

    const char* target_name = target_handler_get_current_name();
    if (!target_name) {
        fprintf(stderr, "Error: Target name not set in context\n");
        return 1;
    }

    // Find the alias target config
    TargetConfig* alias = config_get_target(config, target_name);
    if (!alias || !alias->alias_target) {
        fprintf(stderr, "Error: Alias target config not found: %s\n", target_name);
        return 1;
    }

    printf("[alias:%s] Delegating to base target: %s\n", target_name, alias->alias_target);

    // Call the base target's run handler
    return target_handler_run(alias->alias_target, kir_file, config);
}

/* ============================================================================
 * Registry API Implementation
 * ============================================================================ */

/**
 * Register a target handler
 * The handler pointer must remain valid for the lifetime of the program
 */
void target_handler_register(TargetHandler* handler) {
    if (!handler || !handler->name) {
        fprintf(stderr, "Error: Invalid handler (must have name)\n");
        return;
    }

    // Check for duplicates
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i] &&
            strcmp(g_registry.handlers[i]->name, handler->name) == 0) {
            fprintf(stderr, "Warning: Handler '%s' already registered\n", handler->name);
            return;
        }
    }

    // Grow if needed
    if (g_registry.count >= g_registry.capacity) {
        registry_grow(&g_registry);
    }

    // Take ownership of handler (caller should NOT free it)
    g_registry.handlers[g_registry.count++] = handler;
    printf("[target] Registered: %s\n", handler->name);
}

/**
 * Unregister a handler by name (with cleanup)
 */
void target_handler_unregister(const char* name) {
    if (!name) return;

    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i] &&
            strcmp(g_registry.handlers[i]->name, name) == 0) {

            // Free handler memory
            if (g_registry.handlers[i]->cleanup) {
                g_registry.handlers[i]->cleanup(g_registry.handlers[i]);
            }

            free(g_registry.handlers[i]->name);
            free(g_registry.handlers[i]);

            // Shift remaining handlers
            for (size_t j = i; j < g_registry.count - 1; j++) {
                g_registry.handlers[j] = g_registry.handlers[j + 1];
            }

            g_registry.count--;
            printf("[target] Unregistered: %s\n", name);
            return;
        }
    }
}

/**
 * Cleanup all dynamic handlers
 * Should be called at program shutdown
 */
void target_handler_cleanup(void) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i]) {
            if (g_registry.handlers[i]->cleanup) {
                g_registry.handlers[i]->cleanup(g_registry.handlers[i]);
            }
            free(g_registry.handlers[i]->name);
            free(g_registry.handlers[i]);
        }
    }
    free(g_registry.handlers);
    g_registry.handlers = NULL;
    g_registry.count = 0;
    g_registry.capacity = 0;
}

/**
 * Find a target handler by name
 * Returns NULL if target not found
 */
TargetHandler* target_handler_find(const char* target_name) {
    if (!target_name) {
        return NULL;
    }

    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i] &&
            strcmp(g_registry.handlers[i]->name, target_name) == 0) {
            return g_registry.handlers[i];
        }
    }

    return NULL;
}

/**
 * Check if a target has a specific capability
 */
bool target_has_capability(const char* target_name, TargetCapability capability) {
    TargetHandler* handler = target_handler_find(target_name);
    if (!handler) {
        return false;
    }
    return (handler->capabilities & capability) != 0;
}

/**
 * Initialize the target handler registry with built-in handlers
 * This function is idempotent - calling it multiple times is safe
 */
void target_handler_initialize(void) {
    if (g_initialized) {
        return;  // Already initialized
    }

    // Register built-in handlers (static, not freed)
    target_handler_register(&g_web_handler);

    // Register DIS handler
    static TargetHandler g_dis_handler = {
        .name = "dis",
        .capabilities = TARGET_CAN_BUILD | TARGET_CAN_RUN,
        .build_handler = dis_target_build,
        .run_handler = dis_target_run,
    };
    target_handler_register(&g_dis_handler);

    g_initialized = true;
}

/* ============================================================================
 * Helper Functions for Commands
 * ============================================================================ */

/**
 * Invoke the build handler for a target
 * Returns 0 on success, non-zero on failure
 */
int target_handler_build(const char* target_name, const char* kir_file,
                         const char* output_dir, const char* project_name,
                         const KryonConfig* config) {
    TargetHandler* handler = target_handler_find(target_name);
    if (!handler) {
        fprintf(stderr, "Error: Unknown target '%s'\n", target_name);
        fprintf(stderr, "Valid targets: ");
        for (size_t i = 0; i < g_registry.count; i++) {
            if (g_registry.handlers[i]) {
                fprintf(stderr, "%s%s", i > 0 ? ", " : "", g_registry.handlers[i]->name);
            }
        }
        fprintf(stderr, "\n");
        return 1;
    }

    if (!handler->build_handler) {
        fprintf(stderr, "Error: Target '%s' does not support build operation\n", target_name);
        return 1;
    }

    // Set thread-local context
    t_current_context.target_name = target_name;
    t_current_context.config = config;
    t_current_context.kir_file = kir_file;
    t_current_context.output_dir = output_dir;
    t_current_context.project_name = project_name;

    int result = handler->build_handler(kir_file, output_dir, project_name, config);

    // Clear context
    memset(&t_current_context, 0, sizeof(t_current_context));

    return result;
}

/**
 * Invoke the run handler for a target
 * Returns 0 on success, non-zero on failure
 */
int target_handler_run(const char* target_name, const char* kir_file,
                       const KryonConfig* config) {
    TargetHandler* handler = target_handler_find(target_name);
    if (!handler) {
        fprintf(stderr, "Error: Unknown target '%s'\n", target_name);
        return 1;
    }

    if (!handler->run_handler) {
        fprintf(stderr, "Error: Target '%s' does not support run operation\n", target_name);
        return 1;
    }

    // Set thread-local context
    t_current_context.target_name = target_name;
    t_current_context.config = config;
    t_current_context.kir_file = kir_file;

    int result = handler->run_handler(kir_file, config);

    // Clear context
    memset(&t_current_context, 0, sizeof(t_current_context));

    return result;
}

/**
 * Get list of all registered target names
 * Returns a NULL-terminated array of strings (caller should not free)
 */
const char** target_handler_list_names(void) {
    static const char** names = NULL;
    static size_t names_capacity = 0;

    // Free previous allocation
    if (names) {
        free(names);
    }

    // Allocate new array
    names = calloc(g_registry.count + 1, sizeof(const char*));
    if (!names) return NULL;

    names_capacity = g_registry.count + 1;

    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.handlers[i]) {
            names[i] = g_registry.handlers[i]->name;
        }
    }
    names[g_registry.count] = NULL;

    return names;
}

/**
 * Register command targets dynamically from config
 * Called after config is loaded to register targets with build/run commands or aliases
 */
void target_handler_register_command_targets(const KryonConfig* config) {
    if (!config) return;

    for (int i = 0; i < config->targets_count; i++) {
        TargetConfig* target = &config->targets[i];

        // Check if this is an alias target
        bool is_alias = (target->alias_target != NULL);

        // Check if this is a command target
        bool is_command = (target->build_cmd != NULL || target->run_cmd != NULL);

        // Skip if this is neither a command target nor an alias
        if (!is_command && !is_alias) {
            continue;
        }

        // Check if already registered
        bool already_registered = false;
        for (size_t j = 0; j < g_registry.count; j++) {
            if (g_registry.handlers[j] &&
                strcmp(g_registry.handlers[j]->name, target->name) == 0) {
                already_registered = true;
                break;
            }
        }

        if (already_registered) continue;

        // Grow if needed
        if (g_registry.count >= g_registry.capacity) {
            registry_grow(&g_registry);
        }

        // Allocate new handler (dynamically allocated, will be freed on cleanup)
        TargetHandler* handler = (TargetHandler*)calloc(1, sizeof(TargetHandler));
        if (!handler) {
            fprintf(stderr, "Warning: Failed to allocate handler for target %s\n", target->name);
            continue;
        }

        handler->name = strdup(target->name);
        handler->context = NULL;
        handler->cleanup = NULL;  // Will be freed by cleanup function

        if (is_alias) {
            // Alias target - delegates to base target
            handler->capabilities = 0;  // Inherits from base target
            handler->build_handler = alias_target_build;
            handler->run_handler = alias_target_run;
            printf("[target] Registered alias target: %s -> %s\n", target->name, target->alias_target);
        } else {
            // Command target - executes shell commands
            handler->capabilities = 0;  // No special capabilities for command targets
            handler->build_handler = command_target_build;
            handler->run_handler = command_target_run;
            printf("[target] Registered command target: %s\n", target->name);
        }

        // Register the handler
        g_registry.handlers[g_registry.count++] = handler;
    }
}
