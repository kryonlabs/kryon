/**
 * Target Handler Registry
 *
 * Provides a registry-based system for handling different build targets.
 * This eliminates hardcoded target checks and allows targets to be
 * registered dynamically with their capabilities and handler functions.
 */

#include "kryon_cli.h"
#include "build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Global Registry
 * ============================================================================ */

#define MAX_TARGET_HANDLERS 16

static TargetHandler* g_target_handlers[MAX_TARGET_HANDLERS];
static int g_target_handler_count = 0;
static bool g_initialized = false;

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

/**
 * Desktop target build handler
 * Currently not implemented - returns error
 */
static int desktop_target_build(const char* kir_file, const char* output_dir,
                                const char* project_name, const KryonConfig* config) {
    (void)kir_file;
    (void)output_dir;
    (void)project_name;
    (void)config;
    fprintf(stderr, "Error: Desktop target build not yet implemented\n");
    return 1;
}

/**
 * Desktop target run handler
 * Runs KIR on desktop using the desktop renderer
 */
static int desktop_target_run(const char* kir_file, const KryonConfig* config) {
    (void)config;
    // Get renderer from config if available
    const char* renderer = NULL;
    if (config) {
        TargetConfig* desktop = config_get_target(config, "desktop");
        if (desktop) {
            renderer = target_config_get_option(desktop, "renderer");
        }
    }
    return run_kir_on_desktop(kir_file, NULL, renderer);
}

/**
 * Terminal target build handler
 */
static int terminal_target_build(const char* kir_file, const char* output_dir,
                                 const char* project_name, const KryonConfig* config) {
    (void)kir_file;
    (void)output_dir;
    (void)project_name;
    (void)config;
    fprintf(stderr, "Error: Terminal target build not yet implemented\n");
    return 1;
}

/**
 * Terminal target run handler
 */
static int terminal_target_run(const char* kir_file, const KryonConfig* config) {
    (void)kir_file;
    (void)config;
    fprintf(stderr, "Error: Terminal target not yet implemented\n");
    return 1;
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

static TargetHandler g_desktop_handler = {
    .name = "desktop",
    .capabilities = TARGET_CAN_INSTALL,  // Desktop can be installed
    .build_handler = desktop_target_build,
    .run_handler = desktop_target_run,
};

static TargetHandler g_terminal_handler = {
    .name = "terminal",
    .capabilities = 0,  // No special capabilities
    .build_handler = terminal_target_build,
    .run_handler = terminal_target_run,
};

/* ============================================================================
 * Registry API Implementation
 * ============================================================================ */

/**
 * Register a target handler
 * The handler pointer must remain valid for the lifetime of the program
 */
void target_handler_register(TargetHandler* handler) {
    if (!handler) {
        fprintf(stderr, "Error: Cannot register NULL target handler\n");
        return;
    }

    if (g_target_handler_count >= MAX_TARGET_HANDLERS) {
        fprintf(stderr, "Error: Maximum number of target handlers reached\n");
        return;
    }

    // Check for duplicate names
    for (int i = 0; i < g_target_handler_count; i++) {
        if (g_target_handlers[i] && strcmp(g_target_handlers[i]->name, handler->name) == 0) {
            fprintf(stderr, "Warning: Target handler '%s' already registered, skipping\n", handler->name);
            return;
        }
    }

    g_target_handlers[g_target_handler_count++] = handler;
}

/**
 * Find a target handler by name
 * Returns NULL if target not found
 */
TargetHandler* target_handler_find(const char* target_name) {
    if (!target_name) {
        return NULL;
    }

    for (int i = 0; i < g_target_handler_count; i++) {
        if (g_target_handlers[i] && strcmp(g_target_handlers[i]->name, target_name) == 0) {
            return g_target_handlers[i];
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

    // Register built-in handlers
    target_handler_register(&g_web_handler);
    target_handler_register(&g_desktop_handler);
    target_handler_register(&g_terminal_handler);

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
        for (int i = 0; i < g_target_handler_count; i++) {
            if (g_target_handlers[i]) {
                fprintf(stderr, "%s%s", i > 0 ? ", " : "", g_target_handlers[i]->name);
            }
        }
        fprintf(stderr, "\n");
        return 1;
    }

    if (!handler->build_handler) {
        fprintf(stderr, "Error: Target '%s' does not support build operation\n", target_name);
        return 1;
    }

    return handler->build_handler(kir_file, output_dir, project_name, config);
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

    return handler->run_handler(kir_file, config);
}

/**
 * Get list of all registered target names
 * Returns a NULL-terminated array of strings (caller should not free)
 */
const char** target_handler_list_names(void) {
    static const char* names[MAX_TARGET_HANDLERS + 1];  // +1 for NULL terminator
    int count = 0;

    for (int i = 0; i < g_target_handler_count && count < MAX_TARGET_HANDLERS; i++) {
        if (g_target_handlers[i]) {
            names[count++] = g_target_handlers[i]->name;
        }
    }
    names[count] = NULL;

    return names;
}
