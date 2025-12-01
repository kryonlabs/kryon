/*
 * Kryon Physics System Implementation
 *
 * Simple 2D physics with AABB collision detection
 */

#define _POSIX_C_SOURCE 199309L

#include "ir_physics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// ============================================================================
// Global State
// ============================================================================

static struct {
    bool initialized;

    // Backend
    IRPhysicsBackend backend_type;
    IRPhysicsBackendPlugin backend;

    // Bodies
    IRPhysicsBody bodies[IR_MAX_BODIES];
    uint32_t body_count;
    uint32_t next_body_id;

    // Contacts
    IRContact contacts[IR_MAX_CONTACTS];
    uint32_t contact_count;

    // Callbacks
    IRCollisionCallback global_callback;
    void* global_callback_data;

    // Configuration
    IRPhysicsConfig config;
    IRVec2 gravity;

    // Statistics
    uint32_t collision_checks;
    uint32_t collisions;
    float step_time_ms;

    // Debug
    bool debug_draw_enabled;
    IRPhysicsDebugDrawCallback debug_draw_callback;
    void* debug_draw_user_data;
} g_physics = {0};

// ============================================================================
// Helper Functions
// ============================================================================

// Find body by ID
static IRPhysicsBody* find_body(IRPhysicsBodyID body_id) {
    if (body_id == IR_INVALID_BODY) return NULL;

    for (uint32_t i = 0; i < g_physics.body_count; i++) {
        if (g_physics.bodies[i].id == body_id && g_physics.bodies[i].active) {
            return &g_physics.bodies[i];
        }
    }
    return NULL;
}

// Check if layers should collide
static bool layers_collide(IRCollisionLayer layer_a, IRCollisionLayer mask_a,
                           IRCollisionLayer layer_b, IRCollisionLayer mask_b) {
    return (layer_a & mask_b) && (layer_b & mask_a);
}

// Get current time in milliseconds
static uint64_t get_time_ms(void) {
#ifndef _WIN32
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#else
    return (uint64_t)GetTickCount64();
#endif
}

// ============================================================================
// Collision Detection
// ============================================================================

bool ir_aabb_vs_aabb(const IRAABB* a, const IRAABB* b) {
    float a_min_x = a->x - a->width / 2.0f;
    float a_max_x = a->x + a->width / 2.0f;
    float a_min_y = a->y - a->height / 2.0f;
    float a_max_y = a->y + a->height / 2.0f;

    float b_min_x = b->x - b->width / 2.0f;
    float b_max_x = b->x + b->width / 2.0f;
    float b_min_y = b->y - b->height / 2.0f;
    float b_max_y = b->y + b->height / 2.0f;

    return (a_min_x < b_max_x && a_max_x > b_min_x &&
            a_min_y < b_max_y && a_max_y > b_min_y);
}

bool ir_circle_vs_circle(const IRCircle* a, const IRCircle* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dist_sq = dx * dx + dy * dy;
    float radius_sum = a->radius + b->radius;
    return dist_sq < (radius_sum * radius_sum);
}

bool ir_aabb_vs_circle(const IRAABB* aabb, const IRCircle* circle) {
    // Find closest point on AABB to circle center
    float closest_x = fmaxf(aabb->x - aabb->width/2.0f,
                           fminf(circle->x, aabb->x + aabb->width/2.0f));
    float closest_y = fmaxf(aabb->y - aabb->height/2.0f,
                           fminf(circle->y, aabb->y + aabb->height/2.0f));

    float dx = circle->x - closest_x;
    float dy = circle->y - closest_y;
    float dist_sq = dx * dx + dy * dy;

    return dist_sq < (circle->radius * circle->radius);
}

// Calculate collision normal and depth for AABB vs AABB
static void calculate_aabb_collision(const IRAABB* a, const IRAABB* b,
                                     IRVec2* normal, float* depth) {
    float a_min_x = a->x - a->width / 2.0f;
    float a_max_x = a->x + a->width / 2.0f;
    float a_min_y = a->y - a->height / 2.0f;
    float a_max_y = a->y + a->height / 2.0f;

    float b_min_x = b->x - b->width / 2.0f;
    float b_max_x = b->x + b->width / 2.0f;
    float b_min_y = b->y - b->height / 2.0f;
    float b_max_y = b->y + b->height / 2.0f;

    float overlap_x = fminf(a_max_x - b_min_x, b_max_x - a_min_x);
    float overlap_y = fminf(a_max_y - b_min_y, b_max_y - a_min_y);

    if (overlap_x < overlap_y) {
        *depth = overlap_x;
        normal->x = (a->x < b->x) ? -1.0f : 1.0f;
        normal->y = 0.0f;
    } else {
        *depth = overlap_y;
        normal->x = 0.0f;
        normal->y = (a->y < b->y) ? -1.0f : 1.0f;
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool ir_physics_init(void) {
    IRPhysicsConfig default_config = {
        .backend = IR_PHYSICS_BACKEND_AABB,
        .gravity_x = 0.0f,
        .gravity_y = 980.0f,  // 9.8 m/s^2 * 100 (pixels)
        .velocity_iterations = 8,
        .position_iterations = 3,
        .enable_sleeping = false,
        .enable_debug = false
    };

    return ir_physics_init_ex(&default_config);
}

bool ir_physics_init_ex(IRPhysicsConfig* config) {
    if (g_physics.initialized) {
        fprintf(stderr, "[Physics] Already initialized\n");
        return false;
    }

    memset(&g_physics, 0, sizeof(g_physics));

    g_physics.config = *config;
    g_physics.backend_type = config->backend;
    g_physics.gravity.x = config->gravity_x;
    g_physics.gravity.y = config->gravity_y;
    g_physics.next_body_id = 1;

    g_physics.initialized = true;

    printf("[Physics] System initialized (backend: %d, gravity: %.1f, %.1f)\n",
           config->backend, config->gravity_x, config->gravity_y);

    return true;
}

void ir_physics_shutdown(void) {
    if (!g_physics.initialized) return;

    // Shutdown backend
    if (g_physics.backend.shutdown) {
        g_physics.backend.shutdown();
    }

    memset(&g_physics, 0, sizeof(g_physics));

    printf("[Physics] System shutdown\n");
}

bool ir_physics_register_backend(IRPhysicsBackend backend, IRPhysicsBackendPlugin* plugin) {
    if (!g_physics.initialized) {
        fprintf(stderr, "[Physics] Not initialized\n");
        return false;
    }

    if (!plugin) {
        fprintf(stderr, "[Physics] Invalid backend plugin\n");
        return false;
    }

    g_physics.backend_type = backend;
    g_physics.backend = *plugin;

    // Initialize backend
    if (g_physics.backend.init) {
        if (!g_physics.backend.init(&g_physics.config)) {
            fprintf(stderr, "[Physics] Backend initialization failed\n");
            return false;
        }
    }

    printf("[Physics] Registered backend: %d\n", backend);
    return true;
}

// ============================================================================
// Body Management
// ============================================================================

IRPhysicsBodyID ir_physics_create_box(IRBodyType type, float x, float y,
                                       float width, float height) {
    if (!g_physics.initialized) {
        fprintf(stderr, "[Physics] Not initialized\n");
        return IR_INVALID_BODY;
    }

    if (g_physics.body_count >= IR_MAX_BODIES) {
        fprintf(stderr, "[Physics] Max bodies reached\n");
        return IR_INVALID_BODY;
    }

    // Allocate body
    IRPhysicsBody* body = &g_physics.bodies[g_physics.body_count];
    memset(body, 0, sizeof(IRPhysicsBody));

    body->id = g_physics.next_body_id++;
    body->type = type;
    body->shape = IR_SHAPE_AABB;
    body->x = x;
    body->y = y;
    body->shape_data.aabb.x = x;
    body->shape_data.aabb.y = y;
    body->shape_data.aabb.width = width;
    body->shape_data.aabb.height = height;

    // Default properties
    body->mass = 1.0f;
    body->restitution = 0.0f;
    body->friction = 0.3f;
    body->layer = IR_LAYER_DEFAULT;
    body->mask = IR_LAYER_ALL;
    body->is_trigger = false;
    body->active = true;

    g_physics.body_count++;

    return body->id;
}

IRPhysicsBodyID ir_physics_create_circle(IRBodyType type, float x, float y, float radius) {
    if (!g_physics.initialized) {
        fprintf(stderr, "[Physics] Not initialized\n");
        return IR_INVALID_BODY;
    }

    if (g_physics.body_count >= IR_MAX_BODIES) {
        fprintf(stderr, "[Physics] Max bodies reached\n");
        return IR_INVALID_BODY;
    }

    IRPhysicsBody* body = &g_physics.bodies[g_physics.body_count];
    memset(body, 0, sizeof(IRPhysicsBody));

    body->id = g_physics.next_body_id++;
    body->type = type;
    body->shape = IR_SHAPE_CIRCLE;
    body->x = x;
    body->y = y;
    body->shape_data.circle.x = x;
    body->shape_data.circle.y = y;
    body->shape_data.circle.radius = radius;

    body->mass = 1.0f;
    body->restitution = 0.0f;
    body->friction = 0.3f;
    body->layer = IR_LAYER_DEFAULT;
    body->mask = IR_LAYER_ALL;
    body->is_trigger = false;
    body->active = true;

    g_physics.body_count++;

    return body->id;
}

void ir_physics_destroy_body(IRPhysicsBodyID body_id) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->active = false;
}

IRPhysicsBody* ir_physics_get_body(IRPhysicsBodyID body_id) {
    return find_body(body_id);
}

void ir_physics_set_position(IRPhysicsBodyID body_id, float x, float y) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->x = x;
    body->y = y;

    if (body->shape == IR_SHAPE_AABB) {
        body->shape_data.aabb.x = x;
        body->shape_data.aabb.y = y;
    } else if (body->shape == IR_SHAPE_CIRCLE) {
        body->shape_data.circle.x = x;
        body->shape_data.circle.y = y;
    }
}

void ir_physics_get_position(IRPhysicsBodyID body_id, float* x, float* y) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        return;
    }

    if (x) *x = body->x;
    if (y) *y = body->y;
}

void ir_physics_set_velocity(IRPhysicsBodyID body_id, float vx, float vy) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->velocity.x = vx;
    body->velocity.y = vy;
}

void ir_physics_get_velocity(IRPhysicsBodyID body_id, float* vx, float* vy) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) {
        if (vx) *vx = 0.0f;
        if (vy) *vy = 0.0f;
        return;
    }

    if (vx) *vx = body->velocity.x;
    if (vy) *vy = body->velocity.y;
}

void ir_physics_set_acceleration(IRPhysicsBodyID body_id, float ax, float ay) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->acceleration.x = ax;
    body->acceleration.y = ay;
}

void ir_physics_set_mass(IRPhysicsBodyID body_id, float mass) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->mass = mass;
}

void ir_physics_set_restitution(IRPhysicsBodyID body_id, float restitution) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->restitution = restitution;
}

void ir_physics_set_friction(IRPhysicsBodyID body_id, float friction) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->friction = friction;
}

void ir_physics_set_trigger(IRPhysicsBodyID body_id, bool is_trigger) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->is_trigger = is_trigger;
}

void ir_physics_set_layer(IRPhysicsBodyID body_id, IRCollisionLayer layer) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->layer = layer;
}

void ir_physics_set_mask(IRPhysicsBodyID body_id, IRCollisionLayer mask) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->mask = mask;
}

void ir_physics_set_active(IRPhysicsBodyID body_id, bool active) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->active = active;
}

void ir_physics_set_user_data(IRPhysicsBodyID body_id, void* user_data) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body) return;

    body->user_data = user_data;
}

void* ir_physics_get_user_data(IRPhysicsBodyID body_id) {
    IRPhysicsBody* body = find_body(body_id);
    return body ? body->user_data : NULL;
}

// ============================================================================
// Forces & Impulses
// ============================================================================

void ir_physics_apply_force(IRPhysicsBodyID body_id, float fx, float fy) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body || body->type != IR_BODY_DYNAMIC) return;

    // F = ma -> a = F/m
    body->acceleration.x += fx / body->mass;
    body->acceleration.y += fy / body->mass;
}

void ir_physics_apply_impulse(IRPhysicsBodyID body_id, float ix, float iy) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body || body->type != IR_BODY_DYNAMIC) return;

    // J = mv -> v = J/m
    body->velocity.x += ix / body->mass;
    body->velocity.y += iy / body->mass;
}

void ir_physics_apply_force_at_point(IRPhysicsBodyID body_id, float fx, float fy,
                                      float px, float py) {
    (void)px; (void)py;  // TODO: Calculate torque
    ir_physics_apply_force(body_id, fx, fy);
}

// ============================================================================
// Simulation
// ============================================================================

void ir_physics_step(float dt) {
    if (!g_physics.initialized) return;

    uint64_t start_time = get_time_ms();

    g_physics.collision_checks = 0;
    g_physics.contact_count = 0;

    // Update dynamic bodies
    for (uint32_t i = 0; i < g_physics.body_count; i++) {
        IRPhysicsBody* body = &g_physics.bodies[i];
        if (!body->active || body->type != IR_BODY_DYNAMIC) continue;

        // Apply gravity
        body->acceleration.x += g_physics.gravity.x * dt;
        body->acceleration.y += g_physics.gravity.y * dt;

        // Update velocity
        body->velocity.x += body->acceleration.x * dt;
        body->velocity.y += body->acceleration.y * dt;

        // Update position
        body->x += body->velocity.x * dt;
        body->y += body->velocity.y * dt;

        // Update shape position
        if (body->shape == IR_SHAPE_AABB) {
            body->shape_data.aabb.x = body->x;
            body->shape_data.aabb.y = body->y;
        } else if (body->shape == IR_SHAPE_CIRCLE) {
            body->shape_data.circle.x = body->x;
            body->shape_data.circle.y = body->y;
        }

        // Reset acceleration
        body->acceleration.x = 0.0f;
        body->acceleration.y = 0.0f;
    }

    // Collision detection
    for (uint32_t i = 0; i < g_physics.body_count; i++) {
        IRPhysicsBody* body_a = &g_physics.bodies[i];
        if (!body_a->active) continue;

        for (uint32_t j = i + 1; j < g_physics.body_count; j++) {
            IRPhysicsBody* body_b = &g_physics.bodies[j];
            if (!body_b->active) continue;

            g_physics.collision_checks++;

            // Skip if both are static
            if (body_a->type == IR_BODY_STATIC && body_b->type == IR_BODY_STATIC) {
                continue;
            }

            // Check collision layers
            if (!layers_collide(body_a->layer, body_a->mask, body_b->layer, body_b->mask)) {
                continue;
            }

            // Check collision based on shape types
            bool colliding = false;

            if (body_a->shape == IR_SHAPE_AABB && body_b->shape == IR_SHAPE_AABB) {
                colliding = ir_aabb_vs_aabb(&body_a->shape_data.aabb, &body_b->shape_data.aabb);
            } else if (body_a->shape == IR_SHAPE_CIRCLE && body_b->shape == IR_SHAPE_CIRCLE) {
                colliding = ir_circle_vs_circle(&body_a->shape_data.circle, &body_b->shape_data.circle);
            } else if (body_a->shape == IR_SHAPE_AABB && body_b->shape == IR_SHAPE_CIRCLE) {
                colliding = ir_aabb_vs_circle(&body_a->shape_data.aabb, &body_b->shape_data.circle);
            } else if (body_a->shape == IR_SHAPE_CIRCLE && body_b->shape == IR_SHAPE_AABB) {
                colliding = ir_aabb_vs_circle(&body_b->shape_data.aabb, &body_a->shape_data.circle);
            }

            if (colliding) {
                g_physics.collisions++;

                // Create contact
                if (g_physics.contact_count < IR_MAX_CONTACTS) {
                    IRContact* contact = &g_physics.contacts[g_physics.contact_count++];
                    contact->body_a = body_a->id;
                    contact->body_b = body_b->id;
                    contact->point.x = (body_a->x + body_b->x) / 2.0f;
                    contact->point.y = (body_a->y + body_b->y) / 2.0f;

                    // Calculate normal and depth for AABB collision
                    if (body_a->shape == IR_SHAPE_AABB && body_b->shape == IR_SHAPE_AABB) {
                        calculate_aabb_collision(&body_a->shape_data.aabb,
                                                &body_b->shape_data.aabb,
                                                &contact->normal, &contact->depth);
                    } else {
                        contact->normal.x = body_b->x - body_a->x;
                        contact->normal.y = body_b->y - body_a->y;
                        float len = sqrtf(contact->normal.x * contact->normal.x +
                                        contact->normal.y * contact->normal.y);
                        if (len > 0.0f) {
                            contact->normal.x /= len;
                            contact->normal.y /= len;
                        }
                        contact->depth = 0.0f;
                    }

                    // Call global callback
                    if (g_physics.global_callback) {
                        g_physics.global_callback(body_a->id, body_b->id, contact,
                                                 g_physics.global_callback_data);
                    }
                }

                // Resolve collision if not triggers
                if (!body_a->is_trigger && !body_b->is_trigger) {
                    // Simple separation
                    if (body_a->type == IR_BODY_DYNAMIC && body_b->type == IR_BODY_STATIC) {
                        // Move body_a out of body_b
                        if (body_a->shape == IR_SHAPE_AABB && body_b->shape == IR_SHAPE_AABB) {
                            IRVec2 normal;
                            float depth;
                            calculate_aabb_collision(&body_a->shape_data.aabb,
                                                    &body_b->shape_data.aabb,
                                                    &normal, &depth);
                            body_a->x += normal.x * depth;
                            body_a->y += normal.y * depth;
                            body_a->shape_data.aabb.x = body_a->x;
                            body_a->shape_data.aabb.y = body_a->y;

                            // Apply restitution
                            float dot = body_a->velocity.x * normal.x + body_a->velocity.y * normal.y;
                            body_a->velocity.x -= (1.0f + body_a->restitution) * dot * normal.x;
                            body_a->velocity.y -= (1.0f + body_a->restitution) * dot * normal.y;
                        }
                    }
                }
            }
        }
    }

    uint64_t end_time = get_time_ms();
    g_physics.step_time_ms = (end_time - start_time);
}

void ir_physics_set_gravity(float gx, float gy) {
    g_physics.gravity.x = gx;
    g_physics.gravity.y = gy;
}

void ir_physics_get_gravity(float* gx, float* gy) {
    if (gx) *gx = g_physics.gravity.x;
    if (gy) *gy = g_physics.gravity.y;
}

// ============================================================================
// Collision Callbacks
// ============================================================================

void ir_physics_set_body_callback(IRPhysicsBodyID body_id, IRCollisionCallback callback,
                                   void* user_data) {
    (void)body_id; (void)callback; (void)user_data;
    // TODO: Implement per-body callbacks
}

void ir_physics_set_global_callback(IRCollisionCallback callback, void* user_data) {
    g_physics.global_callback = callback;
    g_physics.global_callback_data = user_data;
}

// ============================================================================
// Queries
// ============================================================================

bool ir_physics_raycast(float x1, float y1, float x2, float y2,
                        IRPhysicsBodyID* hit_body, IRVec2* hit_point, IRVec2* hit_normal) {
    (void)x1; (void)y1; (void)x2; (void)y2;
    (void)hit_body; (void)hit_point; (void)hit_normal;
    // TODO: Implement raycast
    return false;
}

uint32_t ir_physics_query_aabb(float x, float y, float width, float height,
                                IRPhysicsBodyID* out_bodies, uint32_t max_bodies) {
    IRAABB query_aabb = {x, y, width, height};
    uint32_t count = 0;

    for (uint32_t i = 0; i < g_physics.body_count && count < max_bodies; i++) {
        IRPhysicsBody* body = &g_physics.bodies[i];
        if (!body->active) continue;

        if (body->shape == IR_SHAPE_AABB) {
            if (ir_aabb_vs_aabb(&query_aabb, &body->shape_data.aabb)) {
                out_bodies[count++] = body->id;
            }
        }
    }

    return count;
}

uint32_t ir_physics_query_circle(float x, float y, float radius,
                                  IRPhysicsBodyID* out_bodies, uint32_t max_bodies) {
    IRCircle query_circle = {x, y, radius};
    uint32_t count = 0;

    for (uint32_t i = 0; i < g_physics.body_count && count < max_bodies; i++) {
        IRPhysicsBody* body = &g_physics.bodies[i];
        if (!body->active) continue;

        if (body->shape == IR_SHAPE_CIRCLE) {
            if (ir_circle_vs_circle(&query_circle, &body->shape_data.circle)) {
                out_bodies[count++] = body->id;
            }
        }
    }

    return count;
}

bool ir_physics_test_overlap(IRPhysicsBodyID body_a_id, IRPhysicsBodyID body_b_id) {
    IRPhysicsBody* body_a = find_body(body_a_id);
    IRPhysicsBody* body_b = find_body(body_b_id);
    if (!body_a || !body_b) return false;

    if (body_a->shape == IR_SHAPE_AABB && body_b->shape == IR_SHAPE_AABB) {
        return ir_aabb_vs_aabb(&body_a->shape_data.aabb, &body_b->shape_data.aabb);
    } else if (body_a->shape == IR_SHAPE_CIRCLE && body_b->shape == IR_SHAPE_CIRCLE) {
        return ir_circle_vs_circle(&body_a->shape_data.circle, &body_b->shape_data.circle);
    }

    return false;
}

uint32_t ir_physics_get_contacts(IRContact* out_contacts, uint32_t max_contacts) {
    uint32_t count = g_physics.contact_count < max_contacts ?
                     g_physics.contact_count : max_contacts;

    memcpy(out_contacts, g_physics.contacts, count * sizeof(IRContact));
    return count;
}

// ============================================================================
// Debug Rendering
// ============================================================================

void ir_physics_set_debug_draw(bool enabled) {
    g_physics.debug_draw_enabled = enabled;
}

void ir_physics_debug_draw(void) {
    if (!g_physics.debug_draw_enabled || !g_physics.debug_draw_callback) return;

    for (uint32_t i = 0; i < g_physics.body_count; i++) {
        IRPhysicsBody* body = &g_physics.bodies[i];
        if (!body->active) continue;

        g_physics.debug_draw_callback(body, g_physics.debug_draw_user_data);
    }
}

void ir_physics_set_debug_draw_callback(IRPhysicsDebugDrawCallback callback, void* user_data) {
    g_physics.debug_draw_callback = callback;
    g_physics.debug_draw_user_data = user_data;
}

// ============================================================================
// Statistics
// ============================================================================

void ir_physics_get_stats(IRPhysicsStats* stats) {
    if (!stats) return;

    uint32_t active_count = 0;
    uint32_t static_count = 0;
    uint32_t dynamic_count = 0;

    for (uint32_t i = 0; i < g_physics.body_count; i++) {
        IRPhysicsBody* body = &g_physics.bodies[i];
        if (!body->active) continue;

        active_count++;
        if (body->type == IR_BODY_STATIC) static_count++;
        if (body->type == IR_BODY_DYNAMIC) dynamic_count++;
    }

    stats->body_count = g_physics.body_count;
    stats->active_bodies = active_count;
    stats->static_bodies = static_count;
    stats->dynamic_bodies = dynamic_count;
    stats->collision_checks = g_physics.collision_checks;
    stats->collisions = g_physics.collisions;
    stats->step_time_ms = g_physics.step_time_ms;
    stats->backend = g_physics.backend_type;
}

void ir_physics_print_stats(void) {
    IRPhysicsStats stats;
    ir_physics_get_stats(&stats);

    printf("\n=== Physics System Statistics ===\n");
    printf("Backend: %d\n", stats.backend);
    printf("Bodies: %u (%u active)\n", stats.body_count, stats.active_bodies);
    printf("  Static: %u\n", stats.static_bodies);
    printf("  Dynamic: %u\n", stats.dynamic_bodies);
    printf("Collision Checks: %u\n", stats.collision_checks);
    printf("Collisions: %u\n", stats.collisions);
    printf("Step Time: %.2f ms\n", stats.step_time_ms);
    printf("Gravity: (%.1f, %.1f)\n", g_physics.gravity.x, g_physics.gravity.y);
    printf("=================================\n\n");
}

void ir_physics_get_aabb(IRPhysicsBodyID body_id, IRAABB* out_aabb) {
    IRPhysicsBody* body = find_body(body_id);
    if (!body || !out_aabb) return;

    if (body->shape == IR_SHAPE_AABB) {
        *out_aabb = body->shape_data.aabb;
    } else if (body->shape == IR_SHAPE_CIRCLE) {
        IRCircle* circle = &body->shape_data.circle;
        out_aabb->x = circle->x;
        out_aabb->y = circle->y;
        out_aabb->width = circle->radius * 2.0f;
        out_aabb->height = circle->radius * 2.0f;
    }
}
