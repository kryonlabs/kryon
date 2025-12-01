/*
 * Kryon Particle System Implementation
 *
 * High-performance particle system with pooled allocation
 */

#include "ir_particle.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Global State
// ============================================================================

static IRParticleSystem g_particle_system = {0};

// Particle render callback
static IRParticleRenderCallback g_render_callback = NULL;
static void* g_render_user_data = NULL;

// ============================================================================
// Helper Functions
// ============================================================================

// Generate random float in range [min, max]
static float rand_range(float min, float max) {
    float r = (float)rand() / (float)RAND_MAX;
    return min + r * (max - min);
}

// Interpolate between two values
static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Interpolate between two colors
static uint32_t color_lerp(uint32_t color_a, uint32_t color_b, float t) {
    uint8_t r1 = (color_a >> 24) & 0xFF;
    uint8_t g1 = (color_a >> 16) & 0xFF;
    uint8_t b1 = (color_a >> 8) & 0xFF;
    uint8_t a1 = color_a & 0xFF;

    uint8_t r2 = (color_b >> 24) & 0xFF;
    uint8_t g2 = (color_b >> 16) & 0xFF;
    uint8_t b2 = (color_b >> 8) & 0xFF;
    uint8_t a2 = color_b & 0xFF;

    uint8_t r = (uint8_t)lerp((float)r1, (float)r2, t);
    uint8_t g = (uint8_t)lerp((float)g1, (float)g2, t);
    uint8_t b = (uint8_t)lerp((float)b1, (float)b2, t);
    uint8_t a = (uint8_t)lerp((float)a1, (float)a2, t);

    return (r << 24) | (g << 16) | (b << 8) | a;
}

// Convert degrees to radians
static float deg_to_rad(float degrees) {
    return degrees * (M_PI / 180.0f);
}

// Get emitter by ID
static IRParticleEmitter* get_emitter(IRParticleEmitterID emitter_id) {
    if (emitter_id == IR_INVALID_EMITTER) return NULL;

    for (uint32_t i = 0; i < g_particle_system.emitter_count; i++) {
        if (g_particle_system.emitters[i].id == emitter_id) {
            return &g_particle_system.emitters[i];
        }
    }
    return NULL;
}

// ============================================================================
// Particle Allocation
// ============================================================================

static IRParticle* allocate_particle(void) {
    // Find free particle slot
    uint32_t start_index = g_particle_system.next_particle_index;

    for (uint32_t i = 0; i < IR_MAX_PARTICLES; i++) {
        uint32_t index = (start_index + i) % IR_MAX_PARTICLES;
        if (!g_particle_system.particles[index].active) {
            g_particle_system.next_particle_index = (index + 1) % IR_MAX_PARTICLES;
            g_particle_system.active_count++;
            return &g_particle_system.particles[index];
        }
    }

    return NULL;  // Pool exhausted
}

static void spawn_particle(IRParticleEmitter* emitter) {
    if (!emitter || !emitter->active) return;

    IRParticle* p = allocate_particle();
    if (!p) return;  // No free particles

    // Initialize particle
    memset(p, 0, sizeof(IRParticle));
    p->active = true;
    p->emitter_id = 0;  // TODO: Store proper emitter ID

    // Position (varies by emitter type)
    switch (emitter->type) {
        case IR_EMITTER_POINT:
            p->x = emitter->x;
            p->y = emitter->y;
            break;

        case IR_EMITTER_BOX: {
            float hw = emitter->shape.box.width / 2.0f;
            float hh = emitter->shape.box.height / 2.0f;
            p->x = emitter->x + rand_range(-hw, hw);
            p->y = emitter->y + rand_range(-hh, hh);
            break;
        }

        case IR_EMITTER_CIRCLE: {
            float angle = rand_range(0, 360.0f);
            float rad = deg_to_rad(angle);
            p->x = emitter->x + cosf(rad) * emitter->shape.cone.radius;
            p->y = emitter->y + sinf(rad) * emitter->shape.cone.radius;
            break;
        }

        case IR_EMITTER_CONE:
        case IR_EMITTER_BURST:
            p->x = emitter->x;
            p->y = emitter->y;
            break;
    }

    // Velocity (direction + speed)
    float direction = emitter->direction + rand_range(-emitter->spread / 2, emitter->spread / 2);
    float speed = rand_range(emitter->speed_min, emitter->speed_max);
    float dir_rad = deg_to_rad(direction);

    p->vx = cosf(dir_rad) * speed;
    p->vy = sinf(dir_rad) * speed;

    // Acceleration (starts at zero, gravity applied during update)
    p->ax = 0;
    p->ay = 0;

    // Lifetime
    p->lifetime = rand_range(emitter->lifetime_min, emitter->lifetime_max);
    p->age = 0;

    // Size
    p->size = rand_range(emitter->size_min, emitter->size_max);

    // Color (start color)
    p->color = emitter->color_gradient.start_color;

    // Rotation
    p->rotation = rand_range(emitter->rotation_min, emitter->rotation_max);
    p->angular_vel = rand_range(emitter->angular_vel_min, emitter->angular_vel_max);

    g_particle_system.total_spawned++;
}

// ============================================================================
// Initialization
// ============================================================================

void ir_particle_init(void) {
    memset(&g_particle_system, 0, sizeof(IRParticleSystem));
    g_particle_system.initialized = true;
    g_particle_system.next_emitter_id = 1;
}

void ir_particle_shutdown(void) {
    memset(&g_particle_system, 0, sizeof(IRParticleSystem));
}

// ============================================================================
// Update and Rendering
// ============================================================================

void ir_particle_update(float dt) {
    if (!g_particle_system.initialized) return;

    // Update all particles
    for (uint32_t i = 0; i < IR_MAX_PARTICLES; i++) {
        IRParticle* p = &g_particle_system.particles[i];
        if (!p->active) continue;

        // Update lifetime
        p->age += dt;
        if (p->age >= p->lifetime) {
            p->active = false;
            g_particle_system.active_count--;
            g_particle_system.total_died++;
            continue;
        }

        // Lifetime progress (0.0 to 1.0)
        float t = p->age / p->lifetime;

        // Update physics
        p->vx += p->ax * dt;
        p->vy += p->ay * dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;

        // Update rotation
        p->rotation += p->angular_vel * dt;
    }

    // Update all emitters
    for (uint32_t i = 0; i < g_particle_system.emitter_count; i++) {
        IRParticleEmitter* emitter = &g_particle_system.emitters[i];
        if (!emitter->active) continue;

        // Skip burst emitters (manual emission)
        if (emitter->type == IR_EMITTER_BURST) continue;

        // Accumulate particles to spawn
        emitter->emission_accumulator += emitter->particles_per_second * dt;

        // Spawn whole particles
        while (emitter->emission_accumulator >= 1.0f) {
            spawn_particle(emitter);
            emitter->emission_accumulator -= 1.0f;
        }

        // Apply gravity and drag to all particles from this emitter
        for (uint32_t j = 0; j < IR_MAX_PARTICLES; j++) {
            IRParticle* p = &g_particle_system.particles[j];
            if (!p->active) continue;

            // Apply gravity
            p->ax = emitter->gravity_x;
            p->ay = emitter->gravity_y;

            // Apply drag
            if (emitter->drag > 0.0f) {
                p->vx *= (1.0f - emitter->drag * dt);
                p->vy *= (1.0f - emitter->drag * dt);
            }

            // Interpolate color
            float t = p->age / p->lifetime;
            p->color = color_lerp(emitter->color_gradient.start_color,
                                 emitter->color_gradient.end_color, t);

            // Interpolate size
            p->size = lerp(emitter->size_curve.start_size,
                          emitter->size_curve.end_size, t);
        }
    }
}

void ir_particle_set_render_callback(IRParticleRenderCallback callback, void* user_data) {
    g_render_callback = callback;
    g_render_user_data = user_data;
}

void ir_particle_render(void) {
    if (!g_particle_system.initialized) return;
    if (!g_render_callback) return;  // No render callback set

    // Call render callback for each active particle
    for (uint32_t i = 0; i < IR_MAX_PARTICLES; i++) {
        IRParticle* p = &g_particle_system.particles[i];
        if (!p->active) continue;

        g_render_callback(p, g_render_user_data);
    }
}

void ir_particle_get_stats(uint32_t* active_particles,
                           uint32_t* active_emitters,
                           uint32_t* total_spawned,
                           uint32_t* total_died) {
    if (active_particles) *active_particles = g_particle_system.active_count;
    if (active_emitters) {
        uint32_t count = 0;
        for (uint32_t i = 0; i < g_particle_system.emitter_count; i++) {
            if (g_particle_system.emitters[i].active) count++;
        }
        *active_emitters = count;
    }
    if (total_spawned) *total_spawned = g_particle_system.total_spawned;
    if (total_died) *total_died = g_particle_system.total_died;
}

void ir_particle_clear_all(void) {
    for (uint32_t i = 0; i < IR_MAX_PARTICLES; i++) {
        if (g_particle_system.particles[i].active) {
            g_particle_system.particles[i].active = false;
            g_particle_system.active_count--;
        }
    }
}

// ============================================================================
// Emitter Management
// ============================================================================

IRParticleEmitterID ir_particle_create_emitter(IREmitterType type) {
    if (!g_particle_system.initialized) {
        ir_particle_init();
    }

    if (g_particle_system.emitter_count >= IR_MAX_EMITTERS) {
        return IR_INVALID_EMITTER;
    }

    IRParticleEmitterID id = g_particle_system.next_emitter_id++;

    IRParticleEmitter* emitter = &g_particle_system.emitters[g_particle_system.emitter_count];
    memset(emitter, 0, sizeof(IRParticleEmitter));

    emitter->id = id;
    emitter->type = type;
    emitter->active = false;  // Start inactive
    emitter->particles_per_second = 10.0f;
    emitter->direction = 0.0f;  // Right
    emitter->spread = 30.0f;
    emitter->speed_min = 50.0f;
    emitter->speed_max = 100.0f;
    emitter->lifetime_min = 1.0f;
    emitter->lifetime_max = 2.0f;
    emitter->size_min = 4.0f;
    emitter->size_max = 8.0f;
    emitter->gravity_y = 100.0f;  // Downward gravity
    emitter->drag = 0.1f;
    emitter->color_gradient.start_color = 0xFFFFFFFF;  // White
    emitter->color_gradient.end_color = 0xFFFFFF00;    // Transparent white
    emitter->size_curve.start_size = 8.0f;
    emitter->size_curve.end_size = 0.0f;
    emitter->blend_mode = IR_PARTICLE_BLEND_NORMAL;

    g_particle_system.emitter_count++;

    return id;
}

void ir_particle_destroy_emitter(IRParticleEmitterID emitter_id) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->active = false;
    }
}

IRParticleEmitter* ir_particle_get_emitter(IRParticleEmitterID emitter_id) {
    return get_emitter(emitter_id);
}

void ir_particle_emitter_start(IRParticleEmitterID emitter_id) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->active = true;
    }
}

void ir_particle_emitter_stop(IRParticleEmitterID emitter_id) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->active = false;
    }
}

void ir_particle_emitter_set_position(IRParticleEmitterID emitter_id, float x, float y) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->x = x;
        emitter->y = y;
    }
}

void ir_particle_emit_burst(IRParticleEmitterID emitter_id, uint32_t count) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (!emitter) return;

    for (uint32_t i = 0; i < count; i++) {
        spawn_particle(emitter);
    }
}

// ============================================================================
// Configuration Helpers
// ============================================================================

void ir_particle_emitter_set_rate(IRParticleEmitterID emitter_id, float rate) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) emitter->particles_per_second = rate;
}

void ir_particle_emitter_set_lifetime(IRParticleEmitterID emitter_id,
                                      float min_lifetime, float max_lifetime) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->lifetime_min = min_lifetime;
        emitter->lifetime_max = max_lifetime;
    }
}

void ir_particle_emitter_set_speed(IRParticleEmitterID emitter_id,
                                   float min_speed, float max_speed) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->speed_min = min_speed;
        emitter->speed_max = max_speed;
    }
}

void ir_particle_emitter_set_size(IRParticleEmitterID emitter_id,
                                  float min_size, float max_size) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->size_min = min_size;
        emitter->size_max = max_size;
    }
}

void ir_particle_emitter_set_size_curve(IRParticleEmitterID emitter_id,
                                        float start_size, float end_size) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->size_curve.start_size = start_size;
        emitter->size_curve.end_size = end_size;
    }
}

void ir_particle_emitter_set_color_gradient(IRParticleEmitterID emitter_id,
                                            uint32_t start_color, uint32_t end_color) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->color_gradient.start_color = start_color;
        emitter->color_gradient.end_color = end_color;
    }
}

void ir_particle_emitter_set_direction(IRParticleEmitterID emitter_id,
                                       float direction, float spread) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->direction = direction;
        emitter->spread = spread;
    }
}

void ir_particle_emitter_set_gravity(IRParticleEmitterID emitter_id,
                                     float gravity_x, float gravity_y) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) {
        emitter->gravity_x = gravity_x;
        emitter->gravity_y = gravity_y;
    }
}

void ir_particle_emitter_set_drag(IRParticleEmitterID emitter_id, float drag) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) emitter->drag = drag;
}

void ir_particle_emitter_set_sprite(IRParticleEmitterID emitter_id, IRSpriteID sprite) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) emitter->sprite = sprite;
}

void ir_particle_emitter_set_blend_mode(IRParticleEmitterID emitter_id,
                                        IRParticleBlendMode mode) {
    IRParticleEmitter* emitter = get_emitter(emitter_id);
    if (emitter) emitter->blend_mode = mode;
}

// ============================================================================
// Preset Emitters
// ============================================================================

IRParticleEmitterID ir_particle_create_fire(float x, float y) {
    IRParticleEmitterID id = ir_particle_create_emitter(IR_EMITTER_POINT);
    if (id == IR_INVALID_EMITTER) return id;

    IRParticleEmitter* e = ir_particle_get_emitter(id);
    e->x = x;
    e->y = y;
    e->particles_per_second = 50.0f;
    e->direction = -90.0f;  // Up
    e->spread = 30.0f;
    e->speed_min = 20.0f;
    e->speed_max = 50.0f;
    e->lifetime_min = 0.5f;
    e->lifetime_max = 1.5f;
    e->gravity_y = -30.0f;  // Slight upward drift
    e->color_gradient.start_color = 0xFF4400FF;  // Orange
    e->color_gradient.end_color = 0xFF440000;    // Transparent orange
    e->size_curve.start_size = 8.0f;
    e->size_curve.end_size = 2.0f;
    e->blend_mode = IR_PARTICLE_BLEND_ADDITIVE;

    return id;
}

IRParticleEmitterID ir_particle_create_smoke(float x, float y) {
    IRParticleEmitterID id = ir_particle_create_emitter(IR_EMITTER_POINT);
    if (id == IR_INVALID_EMITTER) return id;

    IRParticleEmitter* e = ir_particle_get_emitter(id);
    e->x = x;
    e->y = y;
    e->particles_per_second = 20.0f;
    e->direction = -90.0f;  // Up
    e->spread = 20.0f;
    e->speed_min = 10.0f;
    e->speed_max = 30.0f;
    e->lifetime_min = 2.0f;
    e->lifetime_max = 4.0f;
    e->gravity_y = -10.0f;  // Slow upward drift
    e->drag = 0.3f;
    e->color_gradient.start_color = 0x808080FF;  // Gray
    e->color_gradient.end_color = 0x80808000;    // Transparent gray
    e->size_curve.start_size = 4.0f;
    e->size_curve.end_size = 16.0f;  // Expands over time
    e->blend_mode = IR_PARTICLE_BLEND_NORMAL;

    return id;
}

IRParticleEmitterID ir_particle_create_explosion(float x, float y) {
    IRParticleEmitterID id = ir_particle_create_emitter(IR_EMITTER_BURST);
    if (id == IR_INVALID_EMITTER) return id;

    IRParticleEmitter* e = ir_particle_get_emitter(id);
    e->x = x;
    e->y = y;
    e->direction = 0.0f;
    e->spread = 360.0f;  // All directions
    e->speed_min = 100.0f;
    e->speed_max = 200.0f;
    e->lifetime_min = 0.5f;
    e->lifetime_max = 1.0f;
    e->gravity_y = 200.0f;  // Strong downward gravity
    e->color_gradient.start_color = 0xFFFF00FF;  // Yellow
    e->color_gradient.end_color = 0xFF440000;    // Transparent red
    e->size_curve.start_size = 12.0f;
    e->size_curve.end_size = 2.0f;
    e->blend_mode = IR_PARTICLE_BLEND_ADDITIVE;

    // Emit burst
    ir_particle_emit_burst(id, 100);

    return id;
}

IRParticleEmitterID ir_particle_create_sparkles(float x, float y) {
    IRParticleEmitterID id = ir_particle_create_emitter(IR_EMITTER_POINT);
    if (id == IR_INVALID_EMITTER) return id;

    IRParticleEmitter* e = ir_particle_get_emitter(id);
    e->x = x;
    e->y = y;
    e->particles_per_second = 30.0f;
    e->direction = 0.0f;
    e->spread = 360.0f;  // All directions
    e->speed_min = 30.0f;
    e->speed_max = 60.0f;
    e->lifetime_min = 0.5f;
    e->lifetime_max = 1.5f;
    e->gravity_y = 50.0f;
    e->color_gradient.start_color = 0xFFFF00FF;  // Yellow
    e->color_gradient.end_color = 0xFFFF0000;    // Transparent yellow
    e->size_curve.start_size = 4.0f;
    e->size_curve.end_size = 1.0f;
    e->blend_mode = IR_PARTICLE_BLEND_ADDITIVE;

    return id;
}

IRParticleEmitterID ir_particle_create_rain(float x, float y, float width) {
    IRParticleEmitterID id = ir_particle_create_emitter(IR_EMITTER_BOX);
    if (id == IR_INVALID_EMITTER) return id;

    IRParticleEmitter* e = ir_particle_get_emitter(id);
    e->x = x;
    e->y = y;
    e->shape.box.width = width;
    e->shape.box.height = 10.0f;
    e->particles_per_second = 100.0f;
    e->direction = 90.0f;  // Down
    e->spread = 5.0f;
    e->speed_min = 200.0f;
    e->speed_max = 300.0f;
    e->lifetime_min = 2.0f;
    e->lifetime_max = 3.0f;
    e->gravity_y = 400.0f;  // Fast downward
    e->color_gradient.start_color = 0x8888FFFF;  // Light blue
    e->color_gradient.end_color = 0x8888FF80;    // Semi-transparent
    e->size_curve.start_size = 2.0f;
    e->size_curve.end_size = 2.0f;  // Constant size
    e->blend_mode = IR_PARTICLE_BLEND_NORMAL;

    return id;
}
