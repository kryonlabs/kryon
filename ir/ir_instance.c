/**
 * Kryon Instance Management Implementation
 *
 * Implementation of multi-instance support for Kryon applications.
 */

#define _GNU_SOURCE
#include "ir_instance.h"
#include "ir_builder.h"
#include "ir_executor.h"
#include "ir_core.h"
#include "ir_memory.h"
#include "ir_serialization.h"
#include "ir_asset.h"
#include "ir_audio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Global Instance Registry
// ============================================================================

IRInstanceRegistry g_instance_registry = {0};

// Thread-local current instance (for context access)
static __thread IRInstanceContext* t_current_instance = NULL;

// Thread-local current IRContext (for component creation)
static __thread IRContext* t_current_ir_context = NULL;

// ============================================================================
// Helper Functions
// ============================================================================

// Default instance name generator
static void generate_default_name(char* buffer, size_t buffer_size, uint32_t instance_id) {
    snprintf(buffer, buffer_size, "Instance_%u", instance_id);
}

// ============================================================================
// Registry Management
// ============================================================================

void ir_instance_registry_init(void) {
    memset(&g_instance_registry, 0, sizeof(g_instance_registry));
    g_instance_registry.next_id = 1;  // Instance 0 is reserved for "default/global"
}

void ir_instance_registry_shutdown(void) {
    // Destroy all active instances
    for (uint32_t i = 0; i < g_instance_registry.count; i++) {
        if (g_instance_registry.instances[i]) {
            ir_instance_destroy(g_instance_registry.instances[i]);
            g_instance_registry.instances[i] = NULL;
        }
    }
    g_instance_registry.count = 0;
    g_instance_registry.next_id = 1;
}

// ============================================================================
// Instance Lifecycle
// ============================================================================

IRInstanceContext* ir_instance_create(const char* name, IRInstanceCallbacks* callbacks) {
    // Check registry capacity
    if (g_instance_registry.count >= IR_MAX_INSTANCES) {
        fprintf(stderr, "Error: Maximum instances (%d) reached\n", IR_MAX_INSTANCES);
        return NULL;
    }

    // Allocate instance context
    IRInstanceContext* inst = calloc(1, sizeof(IRInstanceContext));
    if (!inst) {
        fprintf(stderr, "Error: Failed to allocate instance context\n");
        return NULL;
    }

    // Assign instance ID
    inst->instance_id = g_instance_registry.next_id++;

    // Set name
    if (name && name[0] != '\0') {
        strncpy(inst->name, name, IR_INSTANCE_NAME_MAX - 1);
    } else {
        generate_default_name(inst->name, IR_INSTANCE_NAME_MAX, inst->instance_id);
    }
    inst->name[IR_INSTANCE_NAME_MAX - 1] = '\0';

    // Create IRContext for this instance
    inst->ir_context = ir_create_context();
    if (!inst->ir_context) {
        fprintf(stderr, "Error: Failed to create IR context for instance %s\n", inst->name);
        free(inst);
        return NULL;
    }

    // Create IRExecutorContext for this instance
    inst->executor = ir_executor_create();
    if (!inst->executor) {
        fprintf(stderr, "Error: Failed to create executor context for instance %s\n", inst->name);
        ir_destroy_context(inst->ir_context);
        free(inst);
        return NULL;
    }

    // Create asset registry for this instance
    inst->resources.assets = ir_asset_registry_create(inst->instance_id);
    if (!inst->resources.assets) {
        fprintf(stderr, "Warning: Failed to create asset registry for instance %s\n", inst->name);
        // Continue anyway - assets will use global registry
    }

    // Create audio state for this instance
    inst->resources.audio = ir_audio_state_create(inst->instance_id);
    if (!inst->resources.audio) {
        fprintf(stderr, "Warning: Failed to create audio state for instance %s\n", inst->name);
        // Continue anyway - audio will use global state
    }

    // Initialize state
    inst->is_running = true;
    inst->is_suspended = false;
    inst->version = 1;

    // Store callbacks
    inst->callbacks = callbacks;

    // Register instance
    g_instance_registry.instances[g_instance_registry.count++] = inst;

    // Trigger create callback if provided
    if (inst->callbacks && inst->callbacks->on_create) {
        inst->callbacks->on_create(inst);
    }

    return inst;
}

void ir_instance_destroy(IRInstanceContext* inst) {
    if (!inst) return;

    // Trigger destroy callback if provided
    if (inst->callbacks && inst->callbacks->on_destroy) {
        inst->callbacks->on_destroy(inst);
    }

    // Destroy executor context
    if (inst->executor) {
        ir_executor_destroy(inst->executor);
        inst->executor = NULL;
    }

    // Destroy IR context
    if (inst->ir_context) {
        ir_destroy_context(inst->ir_context);
        inst->ir_context = NULL;
    }

    // Destroy asset registry
    if (inst->resources.assets) {
        ir_asset_registry_destroy(inst->resources.assets);
        inst->resources.assets = NULL;
    }

    // Destroy audio state
    if (inst->resources.audio) {
        ir_audio_state_destroy(inst->resources.audio);
        inst->resources.audio = NULL;
    }

    // TODO: Destroy commands when Phase 4 implemented
    // TODO: Destroy desktop state when Phase 5 implemented
    // TODO: Destroy hot reload watcher when Phase 6 implemented

    // Remove from registry
    for (uint32_t i = 0; i < g_instance_registry.count; i++) {
        if (g_instance_registry.instances[i] == inst) {
            // Shift remaining instances down
            for (uint32_t j = i; j < g_instance_registry.count - 1; j++) {
                g_instance_registry.instances[j] = g_instance_registry.instances[j + 1];
            }
            g_instance_registry.instances[g_instance_registry.count - 1] = NULL;
            g_instance_registry.count--;
            break;
        }
    }

    // Free instance context
    free(inst);
}

void ir_instance_suspend(IRInstanceContext* inst) {
    if (!inst || inst->is_suspended) return;

    inst->is_suspended = true;

    if (inst->callbacks && inst->callbacks->on_suspend) {
        inst->callbacks->on_suspend(inst);
    }
}

void ir_instance_resume(IRInstanceContext* inst) {
    if (!inst || !inst->is_suspended) return;

    inst->is_suspended = false;

    if (inst->callbacks && inst->callbacks->on_resume) {
        inst->callbacks->on_resume(inst);
    }
}

// ============================================================================
// Registry Lookup
// ============================================================================

IRInstanceContext* ir_instance_get_by_id(uint32_t instance_id) {
    // Instance ID 0 is a special case - return NULL or the "default" instance
    // For now, we treat 0 as "no instance" (global context)
    if (instance_id == 0) return NULL;

    for (uint32_t i = 0; i < g_instance_registry.count; i++) {
        if (g_instance_registry.instances[i] &&
            g_instance_registry.instances[i]->instance_id == instance_id) {
            return g_instance_registry.instances[i];
        }
    }
    return NULL;
}

IRInstanceContext* ir_instance_get_by_name(const char* name) {
    if (!name) return NULL;

    for (uint32_t i = 0; i < g_instance_registry.count; i++) {
        if (g_instance_registry.instances[i] &&
            strcmp(g_instance_registry.instances[i]->name, name) == 0) {
            return g_instance_registry.instances[i];
        }
    }
    return NULL;
}

uint32_t ir_instance_get_count(void) {
    return g_instance_registry.count;
}

bool ir_instance_is_running(IRInstanceContext* inst) {
    return inst && inst->is_running && !inst->is_suspended;
}

// ============================================================================
// Thread-Local Context Access
// ============================================================================

IRInstanceContext* ir_instance_get_current(void) {
    return t_current_instance;
}

void ir_instance_set_current(IRInstanceContext* inst) {
    t_current_instance = inst;
}

IRInstanceContext* ir_instance_push_context(IRInstanceContext* inst) {
    IRInstanceContext* prev = t_current_instance;
    t_current_instance = inst;
    return prev;
}

void ir_instance_pop_context(IRInstanceContext* restore) {
    t_current_instance = restore;
}

// ============================================================================
// Component Query Helpers
// ============================================================================

uint32_t ir_instance_get_owner(IRComponent* comp) {
    if (!comp) return 0;
    return comp->owner_instance_id;
}

void ir_instance_set_owner(IRComponent* comp, uint32_t instance_id) {
    if (!comp) return;
    comp->owner_instance_id = instance_id;
}

const char* ir_instance_get_scope(IRComponent* comp) {
    if (!comp) return NULL;
    return comp->scope;
}

void ir_instance_set_scope(IRComponent* comp, char* scope) {
    if (!comp) return;

    // Free existing scope if present
    if (comp->scope) {
        free(comp->scope);
    }
    comp->scope = scope;
}

// ============================================================================
// IR Context Access (for loading components into specific contexts)
// ============================================================================

IRContext* ir_context_get_current(void) {
    return t_current_ir_context;
}

IRContext* ir_context_set_current(IRContext* ctx) {
    IRContext* prev = t_current_ir_context;
    t_current_ir_context = ctx;
    return prev;
}

// ============================================================================
// KIR Loading Functions
// ============================================================================

/**
 * Helper: Set owner_instance_id recursively on all components in subtree
 */
static void set_owner_instance_recursive_internal(IRComponent* comp, uint32_t owner_id) {
    if (!comp) return;
    comp->owner_instance_id = owner_id;
    for (uint32_t i = 0; i < comp->child_count; i++) {
        set_owner_instance_recursive_internal(comp->children[i], owner_id);
    }
}

void ir_instance_set_owner_recursive(IRComponent* comp, uint32_t instance_id) {
    set_owner_instance_recursive_internal(comp, instance_id);
}

IRComponent* ir_instance_load_kir(IRInstanceContext* inst, const char* filename) {
    if (!inst || !filename) {
        fprintf(stderr, "Error: Invalid instance or filename\n");
        return NULL;
    }

    // Save current context
    IRContext* prev_ctx = ir_context_set_current(inst->ir_context);
    IRInstanceContext* prev_inst = ir_instance_push_context(inst);

    // Load KIR file - components will be created with instance's context
    IRReactiveManifest* manifest = NULL;
    IRComponent* root = ir_read_json_file_with_manifest(filename, &manifest);

    // Restore previous context
    ir_context_set_current(prev_ctx);
    ir_instance_pop_context(prev_inst);

    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file '%s'\n", filename);
        return NULL;
    }

    // Set owner_instance_id on all components
    set_owner_instance_recursive_internal(root, inst->instance_id);

    // Store manifest if loaded
    if (manifest && inst->ir_context) {
        // Free existing manifest if present
        if (inst->ir_context->reactive_manifest) {
            ir_reactive_manifest_destroy(inst->ir_context->reactive_manifest);
        }
        inst->ir_context->reactive_manifest = manifest;
    }

    // Set as root if not already set
    if (inst->ir_context && !inst->ir_context->root) {
        inst->ir_context->root = root;
    }

    return root;
}

IRComponent* ir_instance_load_kir_string(IRInstanceContext* inst, const char* json_string) {
    if (!inst || !json_string) {
        fprintf(stderr, "Error: Invalid instance or JSON string\n");
        return NULL;
    }

    // Save current context
    IRContext* prev_ctx = ir_context_set_current(inst->ir_context);
    IRInstanceContext* prev_inst = ir_instance_push_context(inst);

    // Deserialize from JSON string
    IRComponent* root = ir_deserialize_json(json_string);

    // Restore previous context
    ir_context_set_current(prev_ctx);
    ir_instance_pop_context(prev_inst);

    if (!root) {
        fprintf(stderr, "Error: Failed to deserialize KIR JSON\n");
        return NULL;
    }

    // Set owner_instance_id on all components
    set_owner_instance_recursive_internal(root, inst->instance_id);

    // Set as root if not already set
    if (inst->ir_context && !inst->ir_context->root) {
        inst->ir_context->root = root;
    }

    return root;
}

void ir_instance_set_root(IRInstanceContext* inst, IRComponent* root) {
    if (!inst) return;
    if (inst->ir_context) {
        // Free existing root if present
        if (inst->ir_context->root && inst->ir_context->root != root) {
            ir_destroy_component(inst->ir_context->root);
        }
        inst->ir_context->root = root;
    }
}

IRComponent* ir_instance_get_root(IRInstanceContext* inst) {
    if (!inst || !inst->ir_context) return NULL;
    return inst->ir_context->root;
}

// ============================================================================
// Debug/Logging
// ============================================================================

void ir_instance_registry_print(void) {
    printf("=== Instance Registry ===\n");
    printf("Count: %u/%d\n", g_instance_registry.count, IR_MAX_INSTANCES);
    printf("Next ID: %u\n", g_instance_registry.next_id);
    printf("\n");

    for (uint32_t i = 0; i < g_instance_registry.count; i++) {
        IRInstanceContext* inst = g_instance_registry.instances[i];
        if (inst) {
            ir_instance_print(inst);
        }
    }
    printf("========================\n");
}

void ir_instance_print(IRInstanceContext* inst) {
    if (!inst) {
        printf("Instance: NULL\n");
        return;
    }

    printf("--- Instance %s ---\n", inst->name);
    printf("  ID: %u\n", inst->instance_id);
    printf("  Running: %s\n", inst->is_running ? "yes" : "no");
    printf("  Suspended: %s\n", inst->is_suspended ? "yes" : "no");
    printf("  Version: %u\n", inst->version);
    printf("  IR Context: %p\n", (void*)inst->ir_context);
    printf("  Executor: %p\n", (void*)inst->executor);

    if (inst->ir_context && inst->ir_context->root) {
        printf("  Root Component: ID %u, Type %s\n",
               inst->ir_context->root->id,
               ir_component_type_to_string(inst->ir_context->root->type));
    } else {
        printf("  Root Component: (none)\n");
    }

    printf("----------------------\n");
}
