/*
 * Kryon Entity System Implementation
 */

#include "ir_entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Global State
// ============================================================================

typedef struct {
    bool initialized;

    // Entity storage
    IREntity entities[IR_MAX_ENTITIES];
    uint32_t entity_count;
    uint32_t next_entity_id;

    // Scene storage
    IRScene scenes[IR_MAX_SCENES];
    uint32_t scene_count;
    uint32_t next_scene_id;
    IRSceneID active_scene_id;

    // Callbacks
    IREntityCreateCallback create_callback;
    void* create_callback_user_data;

    IREntityDestroyCallback destroy_callback;
    void* destroy_callback_user_data;

    IRComponentAddCallback component_add_callback;
    void* component_add_callback_user_data;

    IRComponentRemoveCallback component_remove_callback;
    void* component_remove_callback_user_data;

    IRSceneActivateCallback scene_activate_callback;
    void* scene_activate_callback_user_data;

    IRSceneDeactivateCallback scene_deactivate_callback;
    void* scene_deactivate_callback_user_data;
} EntitySystem;

static EntitySystem g_entity_system = {0};

// ============================================================================
// Helper Functions
// ============================================================================

static IREntity* get_entity_by_id(IREntityID entity_id) {
    if (entity_id == IR_INVALID_ENTITY || entity_id == 0) return NULL;

    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        if (g_entity_system.entities[i].id == entity_id) {
            return &g_entity_system.entities[i];
        }
    }

    return NULL;
}

static IRScene* get_scene_by_id(IRSceneID scene_id) {
    if (scene_id == IR_INVALID_SCENE || scene_id == 0) return NULL;

    for (uint32_t i = 0; i < g_entity_system.scene_count; i++) {
        if (g_entity_system.scenes[i].id == scene_id) {
            return &g_entity_system.scenes[i];
        }
    }

    return NULL;
}

static void remove_child_from_parent(IREntity* parent, IREntityID child_id) {
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child_id) {
            // Shift remaining children
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            break;
        }
    }
}

static void remove_entity_from_scene(IRScene* scene, IREntityID entity_id) {
    for (uint32_t i = 0; i < scene->root_entity_count; i++) {
        if (scene->root_entities[i] == entity_id) {
            // Shift remaining entities
            for (uint32_t j = i; j < scene->root_entity_count - 1; j++) {
                scene->root_entities[j] = scene->root_entities[j + 1];
            }
            scene->root_entity_count--;
            break;
        }
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool ir_entity_init(void) {
    if (g_entity_system.initialized) {
        return true;
    }

    memset(&g_entity_system, 0, sizeof(EntitySystem));
    g_entity_system.initialized = true;
    g_entity_system.next_entity_id = 1;
    g_entity_system.next_scene_id = 1;
    g_entity_system.active_scene_id = IR_INVALID_SCENE;

    return true;
}

void ir_entity_shutdown(void) {
    if (!g_entity_system.initialized) {
        return;
    }

    // Destroy all entities (also frees component data)
    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        IREntity* entity = &g_entity_system.entities[i];
        for (uint32_t j = 0; j < entity->component_count; j++) {
            if (entity->components[j].data) {
                free(entity->components[j].data);
            }
        }
    }

    memset(&g_entity_system, 0, sizeof(EntitySystem));
}

void ir_entity_update(float dt) {
    (void)dt;
    // Future: Update component systems here
    // - Update transforms with hierarchy
    // - Update physics bodies
    // - Update audio sources
    // - etc.
}

// ============================================================================
// Entity Management
// ============================================================================

IREntityID ir_entity_create(IRSceneID scene_id, const char* name) {
    if (!g_entity_system.initialized) {
        fprintf(stderr, "Entity system not initialized\n");
        return IR_INVALID_ENTITY;
    }

    if (g_entity_system.entity_count >= IR_MAX_ENTITIES) {
        fprintf(stderr, "Maximum entity count reached (%d)\n", IR_MAX_ENTITIES);
        return IR_INVALID_ENTITY;
    }

    // Use active scene if none specified
    if (scene_id == IR_INVALID_SCENE) {
        scene_id = g_entity_system.active_scene_id;
    }

    // Ensure scene exists
    IRScene* scene = get_scene_by_id(scene_id);
    if (!scene) {
        fprintf(stderr, "Scene %u not found\n", scene_id);
        return IR_INVALID_ENTITY;
    }

    // Create entity
    IREntity* entity = &g_entity_system.entities[g_entity_system.entity_count];
    memset(entity, 0, sizeof(IREntity));

    entity->id = g_entity_system.next_entity_id++;
    entity->scene_id = scene_id;
    entity->active = true;
    entity->visible = true;
    entity->parent = IR_INVALID_ENTITY;

    if (name) {
        strncpy(entity->name, name, IR_ENTITY_NAME_MAX_LEN - 1);
        entity->name[IR_ENTITY_NAME_MAX_LEN - 1] = '\0';
    }

    g_entity_system.entity_count++;

    // Add to scene as root entity
    if (scene->root_entity_count < IR_MAX_ENTITIES) {
        scene->root_entities[scene->root_entity_count++] = entity->id;
    }

    // Call create callback
    if (g_entity_system.create_callback) {
        g_entity_system.create_callback(entity->id, g_entity_system.create_callback_user_data);
    }

    return entity->id;
}

void ir_entity_destroy(IREntityID entity_id) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity) {
        return;
    }

    // Call destroy callback
    if (g_entity_system.destroy_callback) {
        g_entity_system.destroy_callback(entity_id, g_entity_system.destroy_callback_user_data);
    }

    // Recursively destroy children
    for (uint32_t i = 0; i < entity->child_count; i++) {
        ir_entity_destroy(entity->children[i]);
    }

    // Remove from parent
    if (entity->parent != IR_INVALID_ENTITY) {
        IREntity* parent = get_entity_by_id(entity->parent);
        if (parent) {
            remove_child_from_parent(parent, entity_id);
        }
    }

    // Remove from scene
    IRScene* scene = get_scene_by_id(entity->scene_id);
    if (scene) {
        remove_entity_from_scene(scene, entity_id);
    }

    // Free component data
    for (uint32_t i = 0; i < entity->component_count; i++) {
        if (entity->components[i].data) {
            free(entity->components[i].data);
        }
    }

    // Remove entity from array by shifting
    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        if (g_entity_system.entities[i].id == entity_id) {
            for (uint32_t j = i; j < g_entity_system.entity_count - 1; j++) {
                g_entity_system.entities[j] = g_entity_system.entities[j + 1];
            }
            g_entity_system.entity_count--;
            break;
        }
    }
}

IREntity* ir_entity_get(IREntityID entity_id) {
    return get_entity_by_id(entity_id);
}

IREntityID ir_entity_find_by_name(const char* name) {
    return ir_entity_find_by_name_in_scene(g_entity_system.active_scene_id, name);
}

IREntityID ir_entity_find_by_name_in_scene(IRSceneID scene_id, const char* name) {
    if (!name) return IR_INVALID_ENTITY;

    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        IREntity* entity = &g_entity_system.entities[i];
        if (entity->scene_id == scene_id && strcmp(entity->name, name) == 0) {
            return entity->id;
        }
    }

    return IR_INVALID_ENTITY;
}

void ir_entity_set_active(IREntityID entity_id, bool active) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (entity) {
        entity->active = active;
    }
}

bool ir_entity_is_active(IREntityID entity_id) {
    IREntity* entity = get_entity_by_id(entity_id);
    return entity ? entity->active : false;
}

void ir_entity_set_visible(IREntityID entity_id, bool visible) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (entity) {
        entity->visible = visible;
    }
}

bool ir_entity_is_visible(IREntityID entity_id) {
    IREntity* entity = get_entity_by_id(entity_id);
    return entity ? entity->visible : false;
}

void ir_entity_set_user_data(IREntityID entity_id, void* user_data) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (entity) {
        entity->user_data = user_data;
    }
}

void* ir_entity_get_user_data(IREntityID entity_id) {
    IREntity* entity = get_entity_by_id(entity_id);
    return entity ? entity->user_data : NULL;
}

// ============================================================================
// Component Management
// ============================================================================

bool ir_entity_add_component(IREntityID entity_id, IRComponentType type,
                             const void* data, uint32_t data_size) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity) {
        fprintf(stderr, "Entity %u not found\n", entity_id);
        return false;
    }

    if (entity->component_count >= IR_MAX_COMPONENTS_PER_ENTITY) {
        fprintf(stderr, "Entity %u has maximum components (%d)\n",
                entity_id, IR_MAX_COMPONENTS_PER_ENTITY);
        return false;
    }

    // Check if component already exists
    for (uint32_t i = 0; i < entity->component_count; i++) {
        if (entity->components[i].type == type) {
            fprintf(stderr, "Entity %u already has component type %d\n", entity_id, type);
            return false;
        }
    }

    // Allocate and copy component data
    void* component_data = malloc(data_size);
    if (!component_data) {
        fprintf(stderr, "Failed to allocate component data (%u bytes)\n", data_size);
        return false;
    }
    memcpy(component_data, data, data_size);

    // Add component
    IRComponent* component = &entity->components[entity->component_count];
    component->type = type;
    component->data = component_data;
    component->data_size = data_size;
    entity->component_count++;

    // Call component add callback
    if (g_entity_system.component_add_callback) {
        g_entity_system.component_add_callback(entity_id, type,
                                              g_entity_system.component_add_callback_user_data);
    }

    return true;
}

void ir_entity_remove_component(IREntityID entity_id, IRComponentType type) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity) {
        return;
    }

    for (uint32_t i = 0; i < entity->component_count; i++) {
        if (entity->components[i].type == type) {
            // Free component data
            if (entity->components[i].data) {
                free(entity->components[i].data);
            }

            // Shift remaining components
            for (uint32_t j = i; j < entity->component_count - 1; j++) {
                entity->components[j] = entity->components[j + 1];
            }
            entity->component_count--;

            // Call component remove callback
            if (g_entity_system.component_remove_callback) {
                g_entity_system.component_remove_callback(entity_id, type,
                    g_entity_system.component_remove_callback_user_data);
            }

            break;
        }
    }
}

bool ir_entity_has_component(IREntityID entity_id, IRComponentType type) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity) {
        return false;
    }

    for (uint32_t i = 0; i < entity->component_count; i++) {
        if (entity->components[i].type == type) {
            return true;
        }
    }

    return false;
}

void* ir_entity_get_component(IREntityID entity_id, IRComponentType type) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity) {
        return NULL;
    }

    for (uint32_t i = 0; i < entity->component_count; i++) {
        if (entity->components[i].type == type) {
            return entity->components[i].data;
        }
    }

    return NULL;
}

uint32_t ir_entity_get_components(IREntityID entity_id, IRComponent* components,
                                  uint32_t max_components) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity || !components) {
        return 0;
    }

    uint32_t count = entity->component_count < max_components ?
                     entity->component_count : max_components;

    for (uint32_t i = 0; i < count; i++) {
        components[i] = entity->components[i];
    }

    return count;
}

// ============================================================================
// Transform Component Helpers
// ============================================================================

bool ir_entity_add_transform(IREntityID entity_id, float x, float y, float rotation) {
    IRTransform transform = {
        .x = x,
        .y = y,
        .z = 0.0f,
        .rotation = rotation,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
        .scale_z = 1.0f
    };

    return ir_entity_add_component(entity_id, IR_COMPONENT_TRANSFORM,
                                   &transform, sizeof(IRTransform));
}

IRTransform* ir_entity_get_transform(IREntityID entity_id) {
    return (IRTransform*)ir_entity_get_component(entity_id, IR_COMPONENT_TRANSFORM);
}

void ir_entity_set_position(IREntityID entity_id, float x, float y) {
    IRTransform* transform = ir_entity_get_transform(entity_id);
    if (transform) {
        transform->x = x;
        transform->y = y;
    }
}

void ir_entity_get_position(IREntityID entity_id, float* x, float* y) {
    IRTransform* transform = ir_entity_get_transform(entity_id);
    if (transform) {
        if (x) *x = transform->x;
        if (y) *y = transform->y;
    } else {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
    }
}

void ir_entity_set_rotation(IREntityID entity_id, float rotation) {
    IRTransform* transform = ir_entity_get_transform(entity_id);
    if (transform) {
        transform->rotation = rotation;
    }
}

float ir_entity_get_rotation(IREntityID entity_id) {
    IRTransform* transform = ir_entity_get_transform(entity_id);
    return transform ? transform->rotation : 0.0f;
}

void ir_entity_set_scale(IREntityID entity_id, float scale_x, float scale_y) {
    IRTransform* transform = ir_entity_get_transform(entity_id);
    if (transform) {
        transform->scale_x = scale_x;
        transform->scale_y = scale_y;
    }
}

void ir_entity_get_scale(IREntityID entity_id, float* scale_x, float* scale_y) {
    IRTransform* transform = ir_entity_get_transform(entity_id);
    if (transform) {
        if (scale_x) *scale_x = transform->scale_x;
        if (scale_y) *scale_y = transform->scale_y;
    } else {
        if (scale_x) *scale_x = 1.0f;
        if (scale_y) *scale_y = 1.0f;
    }
}

// ============================================================================
// Hierarchy Management
// ============================================================================

void ir_entity_set_parent(IREntityID entity_id, IREntityID parent_id) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity) {
        return;
    }

    // Remove from old parent
    if (entity->parent != IR_INVALID_ENTITY) {
        IREntity* old_parent = get_entity_by_id(entity->parent);
        if (old_parent) {
            remove_child_from_parent(old_parent, entity_id);
        }

        // Add back to scene root
        IRScene* scene = get_scene_by_id(entity->scene_id);
        if (scene && scene->root_entity_count < IR_MAX_ENTITIES) {
            scene->root_entities[scene->root_entity_count++] = entity_id;
        }
    }

    // Set new parent
    if (parent_id != IR_INVALID_ENTITY) {
        IREntity* new_parent = get_entity_by_id(parent_id);
        if (!new_parent) {
            fprintf(stderr, "Parent entity %u not found\n", parent_id);
            return;
        }

        if (new_parent->child_count >= IR_MAX_CHILDREN_PER_ENTITY) {
            fprintf(stderr, "Parent entity %u has maximum children (%d)\n",
                    parent_id, IR_MAX_CHILDREN_PER_ENTITY);
            return;
        }

        // Remove from scene root
        IRScene* scene = get_scene_by_id(entity->scene_id);
        if (scene) {
            remove_entity_from_scene(scene, entity_id);
        }

        // Add to parent
        new_parent->children[new_parent->child_count++] = entity_id;
        entity->parent = parent_id;
    } else {
        entity->parent = IR_INVALID_ENTITY;
    }
}

IREntityID ir_entity_get_parent(IREntityID entity_id) {
    IREntity* entity = get_entity_by_id(entity_id);
    return entity ? entity->parent : IR_INVALID_ENTITY;
}

uint32_t ir_entity_get_children(IREntityID entity_id, IREntityID* children,
                                uint32_t max_children) {
    IREntity* entity = get_entity_by_id(entity_id);
    if (!entity || !children) {
        return 0;
    }

    uint32_t count = entity->child_count < max_children ?
                     entity->child_count : max_children;

    for (uint32_t i = 0; i < count; i++) {
        children[i] = entity->children[i];
    }

    return count;
}

uint32_t ir_entity_get_child_count(IREntityID entity_id) {
    IREntity* entity = get_entity_by_id(entity_id);
    return entity ? entity->child_count : 0;
}

// ============================================================================
// Scene Management
// ============================================================================

IRSceneID ir_scene_create(const char* name) {
    if (!g_entity_system.initialized) {
        fprintf(stderr, "Entity system not initialized\n");
        return IR_INVALID_SCENE;
    }

    if (g_entity_system.scene_count >= IR_MAX_SCENES) {
        fprintf(stderr, "Maximum scene count reached (%d)\n", IR_MAX_SCENES);
        return IR_INVALID_SCENE;
    }

    IRScene* scene = &g_entity_system.scenes[g_entity_system.scene_count];
    memset(scene, 0, sizeof(IRScene));

    scene->id = g_entity_system.next_scene_id++;
    scene->active = false;
    scene->loaded = false;

    if (name) {
        strncpy(scene->name, name, IR_ENTITY_NAME_MAX_LEN - 1);
        scene->name[IR_ENTITY_NAME_MAX_LEN - 1] = '\0';
    }

    g_entity_system.scene_count++;

    // If this is the first scene, make it active
    if (g_entity_system.scene_count == 1) {
        ir_scene_set_active(scene->id);
    }

    return scene->id;
}

void ir_scene_destroy(IRSceneID scene_id) {
    IRScene* scene = get_scene_by_id(scene_id);
    if (!scene) {
        return;
    }

    // Deactivate if active
    if (g_entity_system.active_scene_id == scene_id) {
        g_entity_system.active_scene_id = IR_INVALID_SCENE;
    }

    // Destroy all entities in scene
    for (uint32_t i = 0; i < scene->root_entity_count; i++) {
        ir_entity_destroy(scene->root_entities[i]);
    }

    // Remove scene from array
    for (uint32_t i = 0; i < g_entity_system.scene_count; i++) {
        if (g_entity_system.scenes[i].id == scene_id) {
            for (uint32_t j = i; j < g_entity_system.scene_count - 1; j++) {
                g_entity_system.scenes[j] = g_entity_system.scenes[j + 1];
            }
            g_entity_system.scene_count--;
            break;
        }
    }
}

IRScene* ir_scene_get(IRSceneID scene_id) {
    return get_scene_by_id(scene_id);
}

IRSceneID ir_scene_find_by_name(const char* name) {
    if (!name) return IR_INVALID_SCENE;

    for (uint32_t i = 0; i < g_entity_system.scene_count; i++) {
        if (strcmp(g_entity_system.scenes[i].name, name) == 0) {
            return g_entity_system.scenes[i].id;
        }
    }

    return IR_INVALID_SCENE;
}

bool ir_scene_set_active(IRSceneID scene_id) {
    IRScene* scene = get_scene_by_id(scene_id);
    if (!scene) {
        fprintf(stderr, "Scene %u not found\n", scene_id);
        return false;
    }

    // Deactivate current scene
    if (g_entity_system.active_scene_id != IR_INVALID_SCENE) {
        IRScene* old_scene = get_scene_by_id(g_entity_system.active_scene_id);
        if (old_scene) {
            old_scene->active = false;

            // Call deactivate callback
            if (g_entity_system.scene_deactivate_callback) {
                g_entity_system.scene_deactivate_callback(
                    g_entity_system.active_scene_id,
                    g_entity_system.scene_deactivate_callback_user_data);
            }
        }
    }

    // Activate new scene
    scene->active = true;
    scene->loaded = true;
    g_entity_system.active_scene_id = scene_id;

    // Call activate callback
    if (g_entity_system.scene_activate_callback) {
        g_entity_system.scene_activate_callback(scene_id,
            g_entity_system.scene_activate_callback_user_data);
    }

    return true;
}

IRSceneID ir_scene_get_active(void) {
    return g_entity_system.active_scene_id;
}

void ir_scene_load(IRSceneID scene_id) {
    IRScene* scene = get_scene_by_id(scene_id);
    if (scene) {
        scene->loaded = true;
    }
}

void ir_scene_unload(IRSceneID scene_id) {
    IRScene* scene = get_scene_by_id(scene_id);
    if (scene) {
        scene->loaded = false;
    }
}

bool ir_scene_is_loaded(IRSceneID scene_id) {
    IRScene* scene = get_scene_by_id(scene_id);
    return scene ? scene->loaded : false;
}

uint32_t ir_scene_get_entities(IRSceneID scene_id, IREntityID* entities,
                               uint32_t max_entities) {
    if (!entities) return 0;

    uint32_t count = 0;

    // Gather all entities (root and children) in the scene
    for (uint32_t i = 0; i < g_entity_system.entity_count && count < max_entities; i++) {
        if (g_entity_system.entities[i].scene_id == scene_id) {
            entities[count++] = g_entity_system.entities[i].id;
        }
    }

    return count;
}

// ============================================================================
// Query System
// ============================================================================

void ir_entity_query_by_component(IRComponentType type, IREntityQuery* result) {
    if (!result) return;

    result->count = 0;

    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        IREntity* entity = &g_entity_system.entities[i];
        if (ir_entity_has_component(entity->id, type)) {
            if (result->count < IR_MAX_ENTITIES) {
                result->entities[result->count++] = entity->id;
            }
        }
    }
}

void ir_entity_query_by_components(const IRComponentType* types, uint32_t type_count,
                                   IREntityQuery* result) {
    if (!result || !types || type_count == 0) return;

    result->count = 0;

    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        IREntity* entity = &g_entity_system.entities[i];

        // Check if entity has all specified components
        bool has_all = true;
        for (uint32_t j = 0; j < type_count; j++) {
            if (!ir_entity_has_component(entity->id, types[j])) {
                has_all = false;
                break;
            }
        }

        if (has_all) {
            if (result->count < IR_MAX_ENTITIES) {
                result->entities[result->count++] = entity->id;
            }
        }
    }
}

void ir_entity_query_active(IREntityQuery* result) {
    if (!result) return;

    result->count = 0;

    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        IREntity* entity = &g_entity_system.entities[i];
        if (entity->active) {
            if (result->count < IR_MAX_ENTITIES) {
                result->entities[result->count++] = entity->id;
            }
        }
    }
}

void ir_entity_query_visible(IREntityQuery* result) {
    if (!result) return;

    result->count = 0;

    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        IREntity* entity = &g_entity_system.entities[i];
        if (entity->visible) {
            if (result->count < IR_MAX_ENTITIES) {
                result->entities[result->count++] = entity->id;
            }
        }
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void ir_entity_set_create_callback(IREntityCreateCallback callback, void* user_data) {
    g_entity_system.create_callback = callback;
    g_entity_system.create_callback_user_data = user_data;
}

void ir_entity_set_destroy_callback(IREntityDestroyCallback callback, void* user_data) {
    g_entity_system.destroy_callback = callback;
    g_entity_system.destroy_callback_user_data = user_data;
}

void ir_entity_set_component_add_callback(IRComponentAddCallback callback, void* user_data) {
    g_entity_system.component_add_callback = callback;
    g_entity_system.component_add_callback_user_data = user_data;
}

void ir_entity_set_component_remove_callback(IRComponentRemoveCallback callback, void* user_data) {
    g_entity_system.component_remove_callback = callback;
    g_entity_system.component_remove_callback_user_data = user_data;
}

void ir_entity_set_scene_activate_callback(IRSceneActivateCallback callback, void* user_data) {
    g_entity_system.scene_activate_callback = callback;
    g_entity_system.scene_activate_callback_user_data = user_data;
}

void ir_entity_set_scene_deactivate_callback(IRSceneDeactivateCallback callback, void* user_data) {
    g_entity_system.scene_deactivate_callback = callback;
    g_entity_system.scene_deactivate_callback_user_data = user_data;
}

// ============================================================================
// Statistics
// ============================================================================

void ir_entity_get_stats(IREntityStats* stats) {
    if (!stats) return;

    stats->entity_count = g_entity_system.entity_count;
    stats->scene_count = g_entity_system.scene_count;
    stats->active_scene_count = 0;
    stats->active_entity_count = 0;
    stats->total_component_count = 0;

    // Count active entities and components
    for (uint32_t i = 0; i < g_entity_system.entity_count; i++) {
        IREntity* entity = &g_entity_system.entities[i];
        if (entity->active) {
            stats->active_entity_count++;
        }
        stats->total_component_count += entity->component_count;
    }

    // Count active scenes
    for (uint32_t i = 0; i < g_entity_system.scene_count; i++) {
        if (g_entity_system.scenes[i].active) {
            stats->active_scene_count++;
        }
    }
}

void ir_entity_print_stats(void) {
    IREntityStats stats;
    ir_entity_get_stats(&stats);

    printf("=== Entity System Statistics ===\n");
    printf("Entities: %u (%u active)\n", stats.entity_count, stats.active_entity_count);
    printf("Scenes: %u (%u active)\n", stats.scene_count, stats.active_scene_count);
    printf("Total Components: %u\n", stats.total_component_count);
    printf("Active Scene ID: %u\n", g_entity_system.active_scene_id);
    printf("\n");
}
