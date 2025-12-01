#ifndef IR_PHYSICS_H
#define IR_PHYSICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Kryon Physics System
 *
 * Provides 2D physics simulation and collision detection.
 * Features:
 * - Plugin architecture for multiple backends (Simple AABB, Box2D, etc.)
 * - Rigid body dynamics
 * - AABB collision detection
 * - Collision callbacks
 * - Physics queries (raycast, overlap tests)
 * - Debug rendering
 * - Spatial hashing for performance
 *
 * Usage:
 *   ir_physics_init();
 *
 *   IRPhysicsBodyID player = ir_physics_create_body(100, 100, 32, 32);
 *   ir_physics_set_velocity(player, 50.0f, 0.0f);
 *   ir_physics_set_callback(player, on_collision, user_data);
 *
 *   // Every frame
 *   ir_physics_step(dt);
 *
 *   ir_physics_shutdown();
 */

// ============================================================================
// Type Definitions
// ============================================================================

typedef uint32_t IRPhysicsBodyID;
typedef uint32_t IRPhysicsWorldID;

#define IR_INVALID_BODY 0
#define IR_INVALID_WORLD 0

#define IR_MAX_BODIES 1024
#define IR_MAX_CONTACTS 512

// Physics backend type
typedef enum {
    IR_PHYSICS_BACKEND_NONE,
    IR_PHYSICS_BACKEND_AABB,     // Simple AABB collision
    IR_PHYSICS_BACKEND_BOX2D,    // Box2D integration
    IR_PHYSICS_BACKEND_CUSTOM    // User-defined
} IRPhysicsBackend;

// Body type
typedef enum {
    IR_BODY_STATIC,      // Never moves (walls, terrain)
    IR_BODY_KINEMATIC,   // Moves but not affected by forces (platforms)
    IR_BODY_DYNAMIC      // Fully simulated (player, enemies, objects)
} IRBodyType;

// Shape type
typedef enum {
    IR_SHAPE_AABB,       // Axis-aligned bounding box
    IR_SHAPE_CIRCLE,     // Circle
    IR_SHAPE_POLYGON     // Convex polygon (future)
} IRShapeType;

// Collision layer (bitfield)
typedef uint32_t IRCollisionLayer;

#define IR_LAYER_DEFAULT  (1 << 0)
#define IR_LAYER_PLAYER   (1 << 1)
#define IR_LAYER_ENEMY    (1 << 2)
#define IR_LAYER_PROJECTILE (1 << 3)
#define IR_LAYER_PICKUP   (1 << 4)
#define IR_LAYER_TRIGGER  (1 << 5)
#define IR_LAYER_ALL      0xFFFFFFFF

// Vector2
typedef struct {
    float x, y;
} IRVec2;

// AABB
typedef struct {
    float x, y;      // Center
    float width, height;
} IRAABB;

// Circle
typedef struct {
    float x, y;      // Center
    float radius;
} IRCircle;

// Contact point
typedef struct {
    IRPhysicsBodyID body_a;
    IRPhysicsBodyID body_b;
    IRVec2 point;
    IRVec2 normal;
    float depth;
} IRContact;

// Physics body
typedef struct {
    IRPhysicsBodyID id;
    IRBodyType type;
    IRShapeType shape;

    // Transform
    float x, y;
    float rotation;

    // Shape data
    union {
        IRAABB aabb;
        IRCircle circle;
    } shape_data;

    // Physics properties
    IRVec2 velocity;
    IRVec2 acceleration;
    float mass;
    float restitution;  // Bounciness (0-1)
    float friction;     // 0-1

    // Collision
    IRCollisionLayer layer;
    IRCollisionLayer mask;  // Which layers to collide with
    bool is_trigger;        // No physics response, only callbacks

    // User data
    void* user_data;
    bool active;
} IRPhysicsBody;

// Collision callback
typedef void (*IRCollisionCallback)(IRPhysicsBodyID body_a, IRPhysicsBodyID body_b,
                                    const IRContact* contact, void* user_data);

// Physics configuration
typedef struct {
    IRPhysicsBackend backend;
    float gravity_x;
    float gravity_y;
    uint32_t velocity_iterations;
    uint32_t position_iterations;
    bool enable_sleeping;  // Sleep inactive bodies for performance
    bool enable_debug;     // Enable debug rendering
} IRPhysicsConfig;

// ============================================================================
// Backend Plugin Interface
// ============================================================================

typedef struct {
    // Initialization
    bool (*init)(IRPhysicsConfig* config);
    void (*shutdown)(void);

    // Body management
    IRPhysicsBodyID (*create_body)(IRBodyType type, float x, float y,
                                   IRShapeType shape, const void* shape_data);
    void (*destroy_body)(IRPhysicsBodyID body_id);
    void (*set_position)(IRPhysicsBodyID body_id, float x, float y);
    void (*set_velocity)(IRPhysicsBodyID body_id, float vx, float vy);
    void (*set_acceleration)(IRPhysicsBodyID body_id, float ax, float ay);
    void (*apply_force)(IRPhysicsBodyID body_id, float fx, float fy);
    void (*apply_impulse)(IRPhysicsBodyID body_id, float ix, float iy);

    // Simulation
    void (*step)(float dt);

    // Queries
    bool (*raycast)(float x1, float y1, float x2, float y2,
                    IRPhysicsBodyID* hit_body, IRVec2* hit_point, IRVec2* hit_normal);
    uint32_t (*query_aabb)(IRAABB aabb, IRPhysicsBodyID* out_bodies, uint32_t max_bodies);
    bool (*test_overlap)(IRPhysicsBodyID body_a, IRPhysicsBodyID body_b);

    // Debug
    void (*debug_draw)(void);
} IRPhysicsBackendPlugin;

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize physics system with default configuration
 * Uses simple AABB backend by default
 */
bool ir_physics_init(void);

/**
 * Initialize physics system with custom configuration
 */
bool ir_physics_init_ex(IRPhysicsConfig* config);

/**
 * Shutdown physics system
 */
void ir_physics_shutdown(void);

/**
 * Register custom physics backend
 */
bool ir_physics_register_backend(IRPhysicsBackend backend, IRPhysicsBackendPlugin* plugin);

// ============================================================================
// Body Management
// ============================================================================

/**
 * Create AABB body
 * @param type Body type (static, kinematic, dynamic)
 * @param x X position (center)
 * @param y Y position (center)
 * @param width AABB width
 * @param height AABB height
 * @return Body ID or IR_INVALID_BODY on failure
 */
IRPhysicsBodyID ir_physics_create_box(IRBodyType type, float x, float y,
                                       float width, float height);

/**
 * Create circle body
 */
IRPhysicsBodyID ir_physics_create_circle(IRBodyType type, float x, float y, float radius);

/**
 * Destroy physics body
 */
void ir_physics_destroy_body(IRPhysicsBodyID body_id);

/**
 * Get physics body
 */
IRPhysicsBody* ir_physics_get_body(IRPhysicsBodyID body_id);

/**
 * Set body position
 */
void ir_physics_set_position(IRPhysicsBodyID body_id, float x, float y);

/**
 * Get body position
 */
void ir_physics_get_position(IRPhysicsBodyID body_id, float* x, float* y);

/**
 * Set body velocity
 */
void ir_physics_set_velocity(IRPhysicsBodyID body_id, float vx, float vy);

/**
 * Get body velocity
 */
void ir_physics_get_velocity(IRPhysicsBodyID body_id, float* vx, float* vy);

/**
 * Set body acceleration
 */
void ir_physics_set_acceleration(IRPhysicsBodyID body_id, float ax, float ay);

/**
 * Set body mass
 */
void ir_physics_set_mass(IRPhysicsBodyID body_id, float mass);

/**
 * Set body restitution (bounciness, 0-1)
 */
void ir_physics_set_restitution(IRPhysicsBodyID body_id, float restitution);

/**
 * Set body friction (0-1)
 */
void ir_physics_set_friction(IRPhysicsBodyID body_id, float friction);

/**
 * Set body as trigger (no physics response, only callbacks)
 */
void ir_physics_set_trigger(IRPhysicsBodyID body_id, bool is_trigger);

/**
 * Set body collision layer
 */
void ir_physics_set_layer(IRPhysicsBodyID body_id, IRCollisionLayer layer);

/**
 * Set body collision mask (which layers to collide with)
 */
void ir_physics_set_mask(IRPhysicsBodyID body_id, IRCollisionLayer mask);

/**
 * Set body active state
 */
void ir_physics_set_active(IRPhysicsBodyID body_id, bool active);

/**
 * Set body user data
 */
void ir_physics_set_user_data(IRPhysicsBodyID body_id, void* user_data);

/**
 * Get body user data
 */
void* ir_physics_get_user_data(IRPhysicsBodyID body_id);

// ============================================================================
// Forces & Impulses
// ============================================================================

/**
 * Apply force to body (affects velocity over time)
 */
void ir_physics_apply_force(IRPhysicsBodyID body_id, float fx, float fy);

/**
 * Apply impulse to body (instant velocity change)
 */
void ir_physics_apply_impulse(IRPhysicsBodyID body_id, float ix, float iy);

/**
 * Apply force at world point (creates torque if not at center)
 */
void ir_physics_apply_force_at_point(IRPhysicsBodyID body_id, float fx, float fy,
                                      float px, float py);

// ============================================================================
// Simulation
// ============================================================================

/**
 * Step physics simulation
 * @param dt Time step in seconds (usually 1/60 for 60 FPS)
 */
void ir_physics_step(float dt);

/**
 * Set gravity
 */
void ir_physics_set_gravity(float gx, float gy);

/**
 * Get gravity
 */
void ir_physics_get_gravity(float* gx, float* gy);

// ============================================================================
// Collision Callbacks
// ============================================================================

/**
 * Set collision callback for specific body
 */
void ir_physics_set_body_callback(IRPhysicsBodyID body_id, IRCollisionCallback callback,
                                   void* user_data);

/**
 * Set global collision callback (called for all collisions)
 */
void ir_physics_set_global_callback(IRCollisionCallback callback, void* user_data);

// ============================================================================
// Queries
// ============================================================================

/**
 * Raycast from point to point
 * @return true if hit, false otherwise
 */
bool ir_physics_raycast(float x1, float y1, float x2, float y2,
                        IRPhysicsBodyID* hit_body, IRVec2* hit_point, IRVec2* hit_normal);

/**
 * Query all bodies overlapping AABB
 * @return Number of bodies found
 */
uint32_t ir_physics_query_aabb(float x, float y, float width, float height,
                                IRPhysicsBodyID* out_bodies, uint32_t max_bodies);

/**
 * Query all bodies overlapping circle
 */
uint32_t ir_physics_query_circle(float x, float y, float radius,
                                  IRPhysicsBodyID* out_bodies, uint32_t max_bodies);

/**
 * Test if two bodies overlap
 */
bool ir_physics_test_overlap(IRPhysicsBodyID body_a, IRPhysicsBodyID body_b);

/**
 * Get all current contacts
 */
uint32_t ir_physics_get_contacts(IRContact* out_contacts, uint32_t max_contacts);

// ============================================================================
// Debug Rendering
// ============================================================================

/**
 * Enable/disable debug rendering
 */
void ir_physics_set_debug_draw(bool enabled);

/**
 * Render debug visualization
 * Call this after ir_physics_step() to draw collision shapes
 */
void ir_physics_debug_draw(void);

/**
 * Set debug draw callback (user provides drawing functions)
 */
typedef void (*IRPhysicsDebugDrawCallback)(const IRPhysicsBody* body, void* user_data);
void ir_physics_set_debug_draw_callback(IRPhysicsDebugDrawCallback callback, void* user_data);

// ============================================================================
// Statistics
// ============================================================================

typedef struct {
    uint32_t body_count;
    uint32_t active_bodies;
    uint32_t static_bodies;
    uint32_t dynamic_bodies;
    uint32_t collision_checks;
    uint32_t collisions;
    float step_time_ms;
    IRPhysicsBackend backend;
} IRPhysicsStats;

/**
 * Get physics statistics
 */
void ir_physics_get_stats(IRPhysicsStats* stats);

/**
 * Print physics statistics for debugging
 */
void ir_physics_print_stats(void);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Test AABB vs AABB collision
 */
bool ir_aabb_vs_aabb(const IRAABB* a, const IRAABB* b);

/**
 * Test Circle vs Circle collision
 */
bool ir_circle_vs_circle(const IRCircle* a, const IRCircle* b);

/**
 * Test AABB vs Circle collision
 */
bool ir_aabb_vs_circle(const IRAABB* aabb, const IRCircle* circle);

/**
 * Calculate AABB for body
 */
void ir_physics_get_aabb(IRPhysicsBodyID body_id, IRAABB* out_aabb);

#ifdef __cplusplus
}
#endif

#endif // IR_PHYSICS_H
