/**
 * Kryon Instance Management Implementation
 *
 * Implementation of multi-instance support for Kryon applications.
 */

#define _GNU_SOURCE
#include "../include/ir_instance.h"
#include "../include/ir_builder.h"
#include "../include/ir_executor.h"
#include "../include/ir_core.h"
#include "../utils/ir_memory.h"
#include "../include/ir_serialization.h"
#include "../utils/ir_asset.h"
#include "ir_hot_reload.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Forward declaration for desktop state (from desktop backend)
// Using weak symbol allows desktop backend to override without hard dependency
typedef struct IRDesktopState IRDesktopState;

// Weak symbols for desktop state management (provided by desktop backend)
// These are declared as weak so the linker won't fail if desktop backend isn't linked
#pragma weak ir_desktop_state_create
extern IRDesktopState* ir_desktop_state_create(uint32_t instance_id);

#pragma weak ir_desktop_state_destroy
extern void ir_desktop_state_destroy(IRDesktopState* state);

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

    // Note: Audio has been moved to the audio plugin
    // Each instance should use the audio plugin's API

    // Create desktop state for this instance (if desktop backend is linked)
    // The weak symbol check ensures this only works if desktop backend is available
    if (ir_desktop_state_create != NULL) {
        inst->render.desktop = ir_desktop_state_create(inst->instance_id);
        if (!inst->render.desktop) {
            fprintf(stderr, "Warning: Failed to create desktop state for instance %s\n", inst->name);
            // Continue anyway - will use shared desktop state
        }
    } else {
        inst->render.desktop = NULL;
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

    // Note: Audio state is managed by the audio plugin

    // Destroy desktop state (if desktop backend is linked)
    if (inst->render.desktop && ir_desktop_state_destroy != NULL) {
        ir_desktop_state_destroy(inst->render.desktop);
        inst->render.desktop = NULL;
    }

    // Destroy hot reload file watcher (if active)
    if (inst->hot_reload.watcher) {
        ir_file_watcher_destroy(inst->hot_reload.watcher);
        inst->hot_reload.watcher = NULL;
    }

    // Note: Command buffers are owned by the rendering backend, not the instance

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
    IRComponent* root = ir_read_json_file_with_manifest(filename, &manifest, NULL);

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

// ============================================================================
// Per-Instance Hot Reload Implementation
// ============================================================================

// Result codes for hot reload (matching IRReloadResult)
#define IR_RELOAD_SUCCESS 0
#define IR_RELOAD_BUILD_FAILED 1
#define IR_RELOAD_NO_CHANGES 2
#define IR_RELOAD_ERROR 3

bool ir_instance_enable_hot_reload(IRInstanceContext* inst, const char* watch_path) {
    if (!inst) return false;

    // Create file watcher if not exists
    if (!inst->hot_reload.watcher) {
        inst->hot_reload.watcher = ir_file_watcher_create();
        if (!inst->hot_reload.watcher) {
            fprintf(stderr, "[Instance] Failed to create file watcher for %s\n", inst->name);
            return false;
        }
    }

    // Set watch path
    if (watch_path) {
        strncpy(inst->hot_reload.watch_path, watch_path, sizeof(inst->hot_reload.watch_path) - 1);
        inst->hot_reload.watch_path[sizeof(inst->hot_reload.watch_path) - 1] = '\0';
    } else {
        // Use current directory
        strncpy(inst->hot_reload.watch_path, ".", sizeof(inst->hot_reload.watch_path) - 1);
    }

    // Watch the directory
    if (!ir_file_watcher_add_path(inst->hot_reload.watcher, inst->hot_reload.watch_path, true)) {
        fprintf(stderr, "[Instance] Failed to watch path: %s\n", inst->hot_reload.watch_path);
        return false;
    }

    inst->hot_reload.enabled = true;
    inst->hot_reload.last_reload_time = 0;
    inst->hot_reload.reload_count = 0;

    printf("[Instance] Hot reload enabled for %s (watching: %s)\n",
           inst->name, inst->hot_reload.watch_path);
    return true;
}

void ir_instance_disable_hot_reload(IRInstanceContext* inst) {
    if (!inst) return;

    inst->hot_reload.enabled = false;

    if (inst->hot_reload.watcher) {
        ir_file_watcher_destroy(inst->hot_reload.watcher);
        inst->hot_reload.watcher = NULL;
    }
}

bool ir_instance_hot_reload_enabled(IRInstanceContext* inst) {
    return inst && inst->hot_reload.enabled;
}

// Helper structure for hot reload user data
typedef struct {
    IRInstanceContext* inst;
    const char* filename;
} InstanceReloadData;

/**
 * Find a component in the new tree by matching scope
 * This allows preserving state across hot reload
 */
__attribute__((unused)) static IRComponent* find_component_by_scope(IRComponent* new_root, const char* scope) {
    if (!new_root || !scope) return NULL;

    if (new_root->scope && strcmp(new_root->scope, scope) == 0) {
        return new_root;
    }

    for (uint32_t i = 0; i < new_root->child_count; i++) {
        IRComponent* found = find_component_by_scope(new_root->children[i], scope);
        if (found) return found;
    }

    return NULL;
}

/**
 * Copy state from old component to new component based on scope match
 * Preserves:
 * - text_content (input values, text labels)
 * - tab_data->selected_index (tab selections)
 * - Component scope
 */
static void migrate_component_state(IRComponent* old_comp, IRComponent* new_comp,
                                     IRExecutorContext* executor) {
    if (!old_comp || !new_comp) return;

    // Migrate text_content (preserves input values, text labels)
    if (old_comp->text_content && new_comp->type == IR_COMPONENT_TEXT) {
        // Free existing text_content if any
        if (new_comp->text_content) {
            free(new_comp->text_content);
        }
        // Copy the text content
        new_comp->text_content = strdup(old_comp->text_content);
    }

    // Migrate tab selection state
    if (old_comp->tab_data && new_comp->tab_data) {
        new_comp->tab_data->selected_index = old_comp->tab_data->selected_index;
    }

    // Migrate scope (critical for reactive state matching)
    if (old_comp->scope) {
        if (new_comp->scope) {
            free(new_comp->scope);
        }
        new_comp->scope = strdup(old_comp->scope);
    }

    // Recursively migrate children with matching scopes
    // For each child in old component, find matching child in new component by scope
    for (uint32_t i = 0; i < old_comp->child_count; i++) {
        IRComponent* old_child = old_comp->children[i];
        if (!old_child->scope) continue;

        // Find matching child in new component by scope
        IRComponent* new_child = NULL;
        for (uint32_t j = 0; j < new_comp->child_count; j++) {
            if (new_comp->children[j]->scope &&
                strcmp(new_comp->children[j]->scope, old_child->scope) == 0) {
                new_child = new_comp->children[j];
                break;
            }
        }

        if (new_child) {
            migrate_component_state(old_child, new_child, executor);
        }
    }

    (void)executor;  // Reserved for future executor state migration
}

IRComponent* ir_instance_reload_kir(IRInstanceContext* inst, const char* filename) {
    if (!inst || !filename) {
        fprintf(stderr, "[Instance] Invalid parameters for reload\n");
        return NULL;
    }

    // Check if instance can reload via callback
    if (inst->callbacks && inst->callbacks->can_reload) {
        if (!inst->callbacks->can_reload(inst)) {
            printf("[Instance] %s cannot be reloaded right now\n", inst->name);
            return NULL;
        }
    }

    printf("[Instance] Reloading %s from %s...\n", inst->name, filename);

    // Get old root for state migration
    IRComponent* old_root = ir_instance_get_root(inst);

    // Trigger before_reload callback
    if (inst->callbacks && inst->callbacks->on_before_reload) {
        inst->callbacks->on_before_reload(inst, old_root);
    }

    // Load new KIR
    IRComponent* new_root = ir_instance_load_kir(inst, filename);
    if (!new_root) {
        fprintf(stderr, "[Instance] Failed to reload KIR file\n");

        if (inst->callbacks && inst->callbacks->on_error) {
            inst->callbacks->on_error(inst, "Failed to reload KIR file");
        }
        return NULL;
    }

    // Migrate state from old tree to new tree using scope matching
    if (old_root) {
        migrate_component_state(old_root, new_root, inst->executor);
    }

    // Set new root
    ir_instance_set_root(inst, new_root);

    // Increment version
    inst->version++;

    // Trigger after_reload callback
    if (inst->callbacks && inst->callbacks->on_after_reload) {
        inst->callbacks->on_after_reload(inst, new_root);
    }

    inst->hot_reload.reload_count++;
    printf("[Instance] %s reloaded successfully (version %u, reload #%u)\n",
           inst->name, inst->version, inst->hot_reload.reload_count);

    return new_root;
}

int ir_instance_hot_reload_poll(IRInstanceContext* inst) {
    if (!inst || !inst->hot_reload.enabled || !inst->hot_reload.watcher) {
        return IR_RELOAD_NO_CHANGES;
    }

    // Poll file watcher
    int events = ir_file_watcher_poll(inst->hot_reload.watcher, 0);
    if (events == 0) {
        return IR_RELOAD_NO_CHANGES;
    }

    // Debounce: don't reload too frequently
    uint64_t now = (uint64_t)time(NULL) * 1000;
    const uint64_t debounce_ms = 500;
    if (now - inst->hot_reload.last_reload_time < debounce_ms) {
        return IR_RELOAD_NO_CHANGES;
    }

    // Trigger reload using watch path as the KIR file
    // Assuming watch_path is a directory, we look for .kir files
    // For now, we use a simple heuristic: if watch_path ends with .kir, use it directly
    char kir_file[512];
    if (strstr(inst->hot_reload.watch_path, ".kir")) {
        strncpy(kir_file, inst->hot_reload.watch_path, sizeof(kir_file) - 1);
    } else {
        // Look for main.kir in watch directory
        snprintf(kir_file, sizeof(kir_file), "%s/main.kir", inst->hot_reload.watch_path);
    }

    IRComponent* new_root = ir_instance_reload_kir(inst, kir_file);
    if (new_root) {
        inst->hot_reload.last_reload_time = now;
        return IR_RELOAD_SUCCESS;
    }

    return IR_RELOAD_BUILD_FAILED;
}
