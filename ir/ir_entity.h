/*
 * Kryon Entity System
 *
 * Entity Component System (ECS) for game object management.
 * Provides entity creation, component attachment, and scene management.
 */

#ifndef IR_ENTITY_H
#define IR_ENTITY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef uint32_t IREntityID;
typedef uint32_t IRSceneID;

// ============================================================================
// Constants
// ============================================================================

#define IR_INVALID_ENTITY 0
#define IR_INVALID_SCENE 0
#define IR_MAX_ENTITIES 4096
#define IR_MAX_SCENES 32
#define IR_MAX_COMPONENTS_PER_ENTITY 16
#define IR_MAX_CHILDREN_PER_ENTITY 64
#define IR_ENTITY_NAME_MAX_LEN 64

// ============================================================================
// Component Types
// ============================================================================

typedef enum {
    IR_COMPONENT_NONE = 0,
    IR_COMPONENT_TRANSFORM,      // Position, rotation, scale
    IR_COMPONENT_SPRITE,          // Sprite rendering
    IR_COMPONENT_PHYSICS_BODY,    // Physics body
    IR_COMPONENT_AUDIO_SOURCE,    // Audio emitter
    IR_COMPONENT_PARTICLE_EMITTER, // Particle system
    IR_COMPONENT_SCRIPT,          // Script component (future)
    IR_COMPONENT_CAMERA,          // Camera component (future)
    IR_COMPONENT_LIGHT,           // Light component (future)
    IR_COMPONENT_CUSTOM_BASE = 1000  // User-defined components start here
} IRComponentType;

// ============================================================================
// Transform Component
// ============================================================================

typedef struct {
    float x, y, z;        // Position
    float rotation;       // Rotation in radians (2D) or euler angles (3D)
    float scale_x;        // Scale X
    float scale_y;        // Scale Y
    float scale_z;        // Scale Z (for 3D)
} IRTransform;

// ============================================================================
// Component Storage
// ============================================================================

typedef struct {
    IRComponentType type;
    void* data;           // Pointer to component data
    uint32_t data_size;   // Size of component data
} IRComponent;

// ============================================================================
// Entity
// ============================================================================

typedef struct {
    IREntityID id;
    char name[IR_ENTITY_NAME_MAX_LEN];
    IRSceneID scene_id;   // Which scene this entity belongs to

    // Components
    IRComponent components[IR_MAX_COMPONENTS_PER_ENTITY];
    uint32_t component_count;

    // Hierarchy
    IREntityID parent;
    IREntityID children[IR_MAX_CHILDREN_PER_ENTITY];
    uint32_t child_count;

    // State
    bool active;
    bool visible;
    void* user_data;
} IREntity;

// ============================================================================
// Scene
// ============================================================================

typedef struct {
    IRSceneID id;
    char name[IR_ENTITY_NAME_MAX_LEN];

    // Root entities (no parent)
    IREntityID root_entities[IR_MAX_ENTITIES];
    uint32_t root_entity_count;

    // State
    bool active;
    bool loaded;
    void* user_data;
} IRScene;

// ============================================================================
// Query Results
// ============================================================================

typedef struct {
    IREntityID entities[IR_MAX_ENTITIES];
    uint32_t count;
} IREntityQuery;

// ============================================================================
// Callbacks
// ============================================================================

// Called when entity is created
typedef void (*IREntityCreateCallback)(IREntityID entity_id, void* user_data);

// Called when entity is destroyed
typedef void (*IREntityDestroyCallback)(IREntityID entity_id, void* user_data);

// Called when component is added
typedef void (*IRComponentAddCallback)(IREntityID entity_id, IRComponentType type, void* user_data);

// Called when component is removed
typedef void (*IRComponentRemoveCallback)(IREntityID entity_id, IRComponentType type, void* user_data);

// Called when scene is activated
typedef void (*IRSceneActivateCallback)(IRSceneID scene_id, void* user_data);

// Called when scene is deactivated
typedef void (*IRSceneDeactivateCallback)(IRSceneID scene_id, void* user_data);

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize the entity system.
 * Must be called before any other entity functions.
 *
 * @return true on success, false on failure
 */
bool ir_entity_init(void);

/**
 * Shutdown the entity system.
 * Destroys all entities and scenes.
 */
void ir_entity_shutdown(void);

/**
 * Update the entity system.
 * Call this every frame to update component systems.
 *
 * @param dt Delta time in seconds
 */
void ir_entity_update(float dt);

// ============================================================================
// Entity Management
// ============================================================================

/**
 * Create a new entity in the specified scene.
 *
 * @param scene_id Scene to create entity in (use active scene if IR_INVALID_SCENE)
 * @param name Optional entity name (can be NULL)
 * @return Entity ID or IR_INVALID_ENTITY on failure
 */
IREntityID ir_entity_create(IRSceneID scene_id, const char* name);

/**
 * Destroy an entity and all its components.
 * Also destroys all child entities recursively.
 *
 * @param entity_id Entity to destroy
 */
void ir_entity_destroy(IREntityID entity_id);

/**
 * Get entity by ID.
 *
 * @param entity_id Entity ID
 * @return Pointer to entity or NULL if not found
 */
IREntity* ir_entity_get(IREntityID entity_id);

/**
 * Find entity by name in the active scene.
 *
 * @param name Entity name
 * @return Entity ID or IR_INVALID_ENTITY if not found
 */
IREntityID ir_entity_find_by_name(const char* name);

/**
 * Find entity by name in a specific scene.
 *
 * @param scene_id Scene to search in
 * @param name Entity name
 * @return Entity ID or IR_INVALID_ENTITY if not found
 */
IREntityID ir_entity_find_by_name_in_scene(IRSceneID scene_id, const char* name);

/**
 * Set entity active state.
 *
 * @param entity_id Entity ID
 * @param active Active state
 */
void ir_entity_set_active(IREntityID entity_id, bool active);

/**
 * Get entity active state.
 *
 * @param entity_id Entity ID
 * @return true if active, false otherwise
 */
bool ir_entity_is_active(IREntityID entity_id);

/**
 * Set entity visible state.
 *
 * @param entity_id Entity ID
 * @param visible Visible state
 */
void ir_entity_set_visible(IREntityID entity_id, bool visible);

/**
 * Get entity visible state.
 *
 * @param entity_id Entity ID
 * @return true if visible, false otherwise
 */
bool ir_entity_is_visible(IREntityID entity_id);

/**
 * Set entity user data.
 *
 * @param entity_id Entity ID
 * @param user_data User data pointer
 */
void ir_entity_set_user_data(IREntityID entity_id, void* user_data);

/**
 * Get entity user data.
 *
 * @param entity_id Entity ID
 * @return User data pointer or NULL
 */
void* ir_entity_get_user_data(IREntityID entity_id);

// ============================================================================
// Component Management
// ============================================================================

/**
 * Add a component to an entity.
 * The component data is copied internally.
 *
 * @param entity_id Entity to add component to
 * @param type Component type
 * @param data Component data (copied)
 * @param data_size Size of component data in bytes
 * @return true on success, false on failure
 */
bool ir_entity_add_component(IREntityID entity_id, IRComponentType type,
                             const void* data, uint32_t data_size);

/**
 * Remove a component from an entity.
 *
 * @param entity_id Entity to remove component from
 * @param type Component type
 */
void ir_entity_remove_component(IREntityID entity_id, IRComponentType type);

/**
 * Check if entity has a component.
 *
 * @param entity_id Entity ID
 * @param type Component type
 * @return true if entity has component, false otherwise
 */
bool ir_entity_has_component(IREntityID entity_id, IRComponentType type);

/**
 * Get component data from an entity.
 *
 * @param entity_id Entity ID
 * @param type Component type
 * @return Pointer to component data or NULL if not found
 */
void* ir_entity_get_component(IREntityID entity_id, IRComponentType type);

/**
 * Get all components of an entity.
 *
 * @param entity_id Entity ID
 * @param components Output array of components
 * @param max_components Maximum number of components to return
 * @return Number of components returned
 */
uint32_t ir_entity_get_components(IREntityID entity_id, IRComponent* components,
                                  uint32_t max_components);

// ============================================================================
// Transform Component Helpers
// ============================================================================

/**
 * Add transform component to entity.
 *
 * @param entity_id Entity ID
 * @param x X position
 * @param y Y position
 * @param rotation Rotation in radians
 * @return true on success, false on failure
 */
bool ir_entity_add_transform(IREntityID entity_id, float x, float y, float rotation);

/**
 * Get entity transform.
 *
 * @param entity_id Entity ID
 * @return Pointer to transform or NULL if not found
 */
IRTransform* ir_entity_get_transform(IREntityID entity_id);

/**
 * Set entity position.
 *
 * @param entity_id Entity ID
 * @param x X position
 * @param y Y position
 */
void ir_entity_set_position(IREntityID entity_id, float x, float y);

/**
 * Get entity position.
 *
 * @param entity_id Entity ID
 * @param x Output X position (can be NULL)
 * @param y Output Y position (can be NULL)
 */
void ir_entity_get_position(IREntityID entity_id, float* x, float* y);

/**
 * Set entity rotation.
 *
 * @param entity_id Entity ID
 * @param rotation Rotation in radians
 */
void ir_entity_set_rotation(IREntityID entity_id, float rotation);

/**
 * Get entity rotation.
 *
 * @param entity_id Entity ID
 * @return Rotation in radians
 */
float ir_entity_get_rotation(IREntityID entity_id);

/**
 * Set entity scale.
 *
 * @param entity_id Entity ID
 * @param scale_x X scale
 * @param scale_y Y scale
 */
void ir_entity_set_scale(IREntityID entity_id, float scale_x, float scale_y);

/**
 * Get entity scale.
 *
 * @param entity_id Entity ID
 * @param scale_x Output X scale (can be NULL)
 * @param scale_y Output Y scale (can be NULL)
 */
void ir_entity_get_scale(IREntityID entity_id, float* scale_x, float* scale_y);

// ============================================================================
// Hierarchy Management
// ============================================================================

/**
 * Set entity parent.
 * Automatically removes from previous parent and adds to new parent.
 *
 * @param entity_id Entity ID
 * @param parent_id Parent entity ID (IR_INVALID_ENTITY to unparent)
 */
void ir_entity_set_parent(IREntityID entity_id, IREntityID parent_id);

/**
 * Get entity parent.
 *
 * @param entity_id Entity ID
 * @return Parent entity ID or IR_INVALID_ENTITY if no parent
 */
IREntityID ir_entity_get_parent(IREntityID entity_id);

/**
 * Get entity children.
 *
 * @param entity_id Entity ID
 * @param children Output array of child entity IDs
 * @param max_children Maximum number of children to return
 * @return Number of children returned
 */
uint32_t ir_entity_get_children(IREntityID entity_id, IREntityID* children,
                                uint32_t max_children);

/**
 * Get number of children.
 *
 * @param entity_id Entity ID
 * @return Number of children
 */
uint32_t ir_entity_get_child_count(IREntityID entity_id);

// ============================================================================
// Scene Management
// ============================================================================

/**
 * Create a new scene.
 *
 * @param name Scene name (can be NULL)
 * @return Scene ID or IR_INVALID_SCENE on failure
 */
IRSceneID ir_scene_create(const char* name);

/**
 * Destroy a scene and all its entities.
 *
 * @param scene_id Scene ID
 */
void ir_scene_destroy(IRSceneID scene_id);

/**
 * Get scene by ID.
 *
 * @param scene_id Scene ID
 * @return Pointer to scene or NULL if not found
 */
IRScene* ir_scene_get(IRSceneID scene_id);

/**
 * Find scene by name.
 *
 * @param name Scene name
 * @return Scene ID or IR_INVALID_SCENE if not found
 */
IRSceneID ir_scene_find_by_name(const char* name);

/**
 * Set active scene.
 * Deactivates the current active scene and activates the new one.
 *
 * @param scene_id Scene ID
 * @return true on success, false on failure
 */
bool ir_scene_set_active(IRSceneID scene_id);

/**
 * Get active scene.
 *
 * @return Active scene ID or IR_INVALID_SCENE if none active
 */
IRSceneID ir_scene_get_active(void);

/**
 * Load a scene (marks as loaded without activating).
 *
 * @param scene_id Scene ID
 */
void ir_scene_load(IRSceneID scene_id);

/**
 * Unload a scene (marks as unloaded).
 *
 * @param scene_id Scene ID
 */
void ir_scene_unload(IRSceneID scene_id);

/**
 * Check if scene is loaded.
 *
 * @param scene_id Scene ID
 * @return true if loaded, false otherwise
 */
bool ir_scene_is_loaded(IRSceneID scene_id);

/**
 * Get all entities in a scene.
 *
 * @param scene_id Scene ID
 * @param entities Output array of entity IDs
 * @param max_entities Maximum number of entities to return
 * @return Number of entities returned
 */
uint32_t ir_scene_get_entities(IRSceneID scene_id, IREntityID* entities,
                               uint32_t max_entities);

// ============================================================================
// Query System
// ============================================================================

/**
 * Find all entities with a specific component type.
 *
 * @param type Component type
 * @param result Output query result
 */
void ir_entity_query_by_component(IRComponentType type, IREntityQuery* result);

/**
 * Find all entities with multiple component types (AND query).
 *
 * @param types Array of component types
 * @param type_count Number of component types
 * @param result Output query result
 */
void ir_entity_query_by_components(const IRComponentType* types, uint32_t type_count,
                                   IREntityQuery* result);

/**
 * Find all active entities.
 *
 * @param result Output query result
 */
void ir_entity_query_active(IREntityQuery* result);

/**
 * Find all visible entities.
 *
 * @param result Output query result
 */
void ir_entity_query_visible(IREntityQuery* result);

// ============================================================================
// Callbacks
// ============================================================================

/**
 * Set entity create callback.
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void ir_entity_set_create_callback(IREntityCreateCallback callback, void* user_data);

/**
 * Set entity destroy callback.
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void ir_entity_set_destroy_callback(IREntityDestroyCallback callback, void* user_data);

/**
 * Set component add callback.
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void ir_entity_set_component_add_callback(IRComponentAddCallback callback, void* user_data);

/**
 * Set component remove callback.
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void ir_entity_set_component_remove_callback(IRComponentRemoveCallback callback, void* user_data);

/**
 * Set scene activate callback.
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void ir_entity_set_scene_activate_callback(IRSceneActivateCallback callback, void* user_data);

/**
 * Set scene deactivate callback.
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void ir_entity_set_scene_deactivate_callback(IRSceneDeactivateCallback callback, void* user_data);

// ============================================================================
// Statistics
// ============================================================================

typedef struct {
    uint32_t entity_count;
    uint32_t active_entity_count;
    uint32_t scene_count;
    uint32_t active_scene_count;
    uint32_t total_component_count;
} IREntityStats;

/**
 * Get entity system statistics.
 *
 * @param stats Output statistics structure
 */
void ir_entity_get_stats(IREntityStats* stats);

/**
 * Print entity system statistics to stdout.
 */
void ir_entity_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif // IR_ENTITY_H
