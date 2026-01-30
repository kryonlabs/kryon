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
#include "../../../codegens/limbo/limbo_codegen.h"
#include "../../../codegens/c/ir_c_codegen.h"
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

// Desktop and terminal targets removed - migrated to Limbo/DIS target

// Android target removed - migrated to Limbo/DIS target

// DIS target removed - all DIS generation now goes through Limbo compilation

/* ============================================================================
 * Limbo Target Handler
 * ============================================================================ */

/**
 * Limbo target build handler
 * Pipeline: KIR → .b (Limbo source) → .dis (bytecode)
 */
static int limbo_target_build(const char* kir_file, const char* output_dir,
                              const char* project_name, const KryonConfig* config) {
    // Step 1: Generate Limbo source (.b) from KIR
    char limbo_source[1024];
    snprintf(limbo_source, sizeof(limbo_source), "%s/%s.b", output_dir, project_name);

    printf("Generating Limbo source: %s\n", limbo_source);
    if (!limbo_codegen_generate_multi(kir_file, output_dir)) {
        fprintf(stderr, "Error: Limbo code generation failed\n");
        return 1;
    }

    // Limbo codegen generates main.b, rename to project_name.b
    char main_b[1024];
    snprintf(main_b, sizeof(main_b), "%s/main.b", output_dir);

    if (strcmp(main_b, limbo_source) != 0 && access(main_b, F_OK) == 0) {
        rename(main_b, limbo_source);
    }

    printf("✓ Generated Limbo source: %s\n", limbo_source);

    // Step 2: Compile Limbo source to DIS bytecode
    char dis_output[1024];
    snprintf(dis_output, sizeof(dis_output), "%s/%s.dis", output_dir, project_name);

    // Get limbo compiler path
    const char* taiji_path = config && config->taiji_path
        ? config->taiji_path
        : getenv("TAIJI_PATH");

    if (!taiji_path) {
        taiji_path = "/home/wao/Projects/TaijiOS";
    }

    // Verify TaijiOS installation
    char limbo_compiler[1024];
    snprintf(limbo_compiler, sizeof(limbo_compiler), "%s/Linux/amd64/bin/limbo", taiji_path);

    if (access(limbo_compiler, X_OK) != 0) {
        fprintf(stderr, "Error: Limbo compiler not found at %s\n", limbo_compiler);
        fprintf(stderr, "  Set TAIJI_PATH environment variable or install TaijiOS\n");
        return 1;
    }

    // Compile: limbo -o output.dis source.b
    char compile_cmd[2048];
    snprintf(compile_cmd, sizeof(compile_cmd),
             "cd \"%s\" && \"%s\" -o \"%s\" \"%s\"",
             output_dir, limbo_compiler, dis_output, limbo_source);

    printf("Compiling Limbo to DIS: %s\n", compile_cmd);
    int result = system(compile_cmd);

    if (result != 0) {
        fprintf(stderr, "Error: Limbo compilation failed with exit code %d\n", result);
        return 1;
    }

    printf("✓ Compiled to DIS bytecode: %s\n", dis_output);
    return 0;
}

/**
 * Limbo target run handler
 * Builds if needed, then executes in TaijiOS emu
 */
static int limbo_target_run(const char* kir_file, const KryonConfig* config) {
    // Determine output directory and project name
    const char* output_dir = ".";
    const char* project_name = "app";

    // Build first (generates .dis file)
    if (limbo_target_build(kir_file, output_dir, project_name, config) != 0) {
        return 1;
    }

    // Get TaijiOS configuration
    const char* taiji_path = config && config->taiji_path
        ? config->taiji_path
        : getenv("TAIJI_PATH");

    if (!taiji_path) {
        taiji_path = "/home/wao/Projects/TaijiOS";
    }

    bool use_run_app = config && config->taiji_use_run_app
        ? config->taiji_use_run_app
        : false;  // Default to direct emu (run-app.sh expects TaijiOS-internal paths)

    // Copy .dis file to TaijiOS directory for execution
    // Emu can't access arbitrary host paths, only paths within its namespace
    char source_dis[1024];
    snprintf(source_dis, sizeof(source_dis), "%s/%s.dis", output_dir, project_name);

    char target_dis[1024];
    snprintf(target_dis, sizeof(target_dis), "%s/dis/kryon/%s.dis", taiji_path, project_name);

    // Create kryon directory in TaijiOS/dis if it doesn't exist
    char kryon_dis_dir[1024];
    snprintf(kryon_dis_dir, sizeof(kryon_dis_dir), "%s/dis/kryon", taiji_path);

    char mkdir_cmd[2048];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", kryon_dis_dir);
    system(mkdir_cmd);

    // Copy the .dis file
    char cp_cmd[2048];
    snprintf(cp_cmd, sizeof(cp_cmd), "cp \"%s\" \"%s\"", source_dis, target_dis);
    printf("Copying DIS to TaijiOS: %s\n", target_dis);

    if (system(cp_cmd) != 0) {
        fprintf(stderr, "Error: Failed to copy .dis file to TaijiOS directory\n");
        return 1;
    }

    // Validate TaijiOS installation
    if (access(taiji_path, F_OK) != 0) {
        fprintf(stderr, "Error: TaijiOS not found at %s\n", taiji_path);
        return 1;
    }

    // Use relative path within TaijiOS namespace
    char dis_file_rel[1024];
    snprintf(dis_file_rel, sizeof(dis_file_rel), "kryon/%s.dis", project_name);

    char cmd[2048];

    if (use_run_app) {
        // Use run-app.sh wrapper (recommended)
        char run_app_path[1024];
        snprintf(run_app_path, sizeof(run_app_path), "%s/run-app.sh", taiji_path);

        if (access(run_app_path, X_OK) != 0) {
            fprintf(stderr, "Error: run-app.sh not found at %s\n", run_app_path);
            fprintf(stderr, "  Falling back to direct emu invocation\n");
            use_run_app = false;
        } else {
            snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./run-app.sh \"%s\"",
                    taiji_path, dis_file_rel);
        }
    }

    if (!use_run_app) {
        // Direct emu invocation
        char emu_path[1024];
        snprintf(emu_path, sizeof(emu_path), "%s/Linux/amd64/bin/emu", taiji_path);

        if (access(emu_path, X_OK) != 0) {
            fprintf(stderr, "Error: emu binary not found at %s\n", emu_path);
            return 1;
        }

        // emu -r <ROOT> /dis/wminit.dis <app.dis>
        // wminit.dis sets up the window manager and graphics context
        snprintf(cmd, sizeof(cmd),
                "cd \"%s\" && ROOT=\"%s\" \"%s\" -r \"%s\" /dis/wminit.dis %s",
                taiji_path, taiji_path, emu_path, taiji_path, dis_file_rel);
    }

    printf("Executing in TaijiOS: %s\n", cmd);
    int result = system(cmd);

    if (result != 0) {
        fprintf(stderr, "Warning: TaijiOS emu exited with code %d\n", result);
    } else {
        printf("✓ Execution complete\n");
    }

    return result;
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

    // Register Limbo handler (generates .b from KIR, then compiles to .dis)
    static TargetHandler g_limbo_handler = {
        .name = "limbo",
        .capabilities = TARGET_CAN_BUILD | TARGET_CAN_RUN,
        .build_handler = limbo_target_build,
        .run_handler = limbo_target_run,
    };
    target_handler_register(&g_limbo_handler);

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
