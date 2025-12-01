#ifndef IR_PARTICLE_H
#define IR_PARTICLE_H

#include <stdint.h>
#include <stdbool.h>
#include "ir_sprite.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Kryon Particle System
 *
 * High-performance particle system for effects like fire, smoke, explosions, etc.
 * Features:
 * - Pooled particle allocation (10,000+ particles)
 * - Multiple emitter types (point, cone, box, circle)
 * - Physics simulation (gravity, drag, acceleration)
 * - Color and size interpolation over lifetime
 * - Integration with sprite system for textured particles
 * - Efficient batch rendering
 *
 * Performance target: 10,000 active particles at 60 FPS
 */

// ============================================================================
// Type Definitions
// ============================================================================

typedef uint32_t IRParticleEmitterID;
typedef uint32_t IRParticleSystemID;

#define IR_INVALID_EMITTER 0
#define IR_MAX_EMITTERS 64
#define IR_MAX_PARTICLES 16384  // 16K particles

// Emitter types
typedef enum {
    IR_EMITTER_POINT,    // Emit from a single point
    IR_EMITTER_CONE,     // Emit in a cone direction
    IR_EMITTER_BOX,      // Emit from within a box area
    IR_EMITTER_CIRCLE,   // Emit from a circle perimeter
    IR_EMITTER_BURST     // One-time burst emission
} IREmitterType;

// Particle blend modes
typedef enum {
    IR_PARTICLE_BLEND_NORMAL,    // Standard alpha blending
    IR_PARTICLE_BLEND_ADDITIVE,  // Additive blending (for fire, glow)
    IR_PARTICLE_BLEND_MULTIPLY   // Multiply blending (for smoke)
} IRParticleBlendMode;

// Single particle data
typedef struct {
    // Position and velocity
    float x, y;
    float vx, vy;        // Velocity
    float ax, ay;        // Acceleration

    // Visual properties
    uint32_t color;      // Current RGBA color
    float size;          // Current size
    float rotation;      // Current rotation (degrees)
    float angular_vel;   // Rotation speed (degrees/sec)

    // Lifetime
    float age;           // Current age (seconds)
    float lifetime;      // Max lifetime (seconds)

    // Internal
    bool active;
    uint32_t emitter_id; // Which emitter spawned this particle
} IRParticle;

// Color gradient (interpolate color over lifetime)
typedef struct {
    uint32_t start_color;  // RGBA at birth
    uint32_t end_color;    // RGBA at death
} IRColorGradient;

// Size curve (interpolate size over lifetime)
typedef struct {
    float start_size;
    float end_size;
} IRSizeCurve;

// Emitter configuration
typedef struct {
    // ID
    IRParticleEmitterID id;

    // Emitter type and status
    IREmitterType type;
    bool active;
    bool one_shot;       // If true, emit once then deactivate

    // Position (world space)
    float x, y;

    // Emission rate
    float particles_per_second;
    float emission_accumulator;  // Internal: tracks fractional particles

    // Burst mode
    uint32_t burst_count;  // Number of particles to emit in burst

    // Particle initial properties
    float direction;       // Base direction (degrees, 0=right)
    float spread;          // Angle spread (degrees, 0=no spread)
    float speed_min;       // Minimum initial speed
    float speed_max;       // Maximum initial speed

    float lifetime_min;    // Minimum particle lifetime (seconds)
    float lifetime_max;    // Maximum particle lifetime (seconds)

    float size_min;        // Minimum initial size
    float size_max;        // Maximum initial size

    // Physics
    float gravity_x;       // Gravity acceleration X
    float gravity_y;       // Gravity acceleration Y (positive = down)
    float drag;            // Air resistance (0-1, 0=no drag)

    // Visual properties
    IRColorGradient color_gradient;
    IRSizeCurve size_curve;
    float rotation_min;    // Initial rotation range
    float rotation_max;
    float angular_vel_min; // Rotation speed range
    float angular_vel_max;

    // Emitter shape parameters
    union {
        struct {
            float radius;  // For CIRCLE and CONE
            float height;  // For CONE
        } cone;
        struct {
            float width;
            float height;
        } box;
    } shape;

    // Rendering
    IRSpriteID sprite;     // Sprite to use (0 = simple square)
    IRParticleBlendMode blend_mode;

} IRParticleEmitter;

// Particle system (manages all particles and emitters)
typedef struct {
    // Particle pool
    IRParticle particles[IR_MAX_PARTICLES];
    uint32_t active_count;
    uint32_t next_particle_index;  // Hint for allocation

    // Emitters
    IRParticleEmitter emitters[IR_MAX_EMITTERS];
    uint32_t emitter_count;
    uint32_t next_emitter_id;

    // Statistics
    uint32_t total_spawned;
    uint32_t total_died;

    // Global settings
    bool initialized;
} IRParticleSystem;

// ============================================================================
// Particle System Management
// ============================================================================

/**
 * Initialize particle system
 */
void ir_particle_init(void);

/**
 * Shutdown particle system
 */
void ir_particle_shutdown(void);

/**
 * Update all particles and emitters
 * dt: Delta time in seconds
 */
void ir_particle_update(float dt);

/**
 * Render callback function type
 * Called for each active particle during rendering
 */
typedef void (*IRParticleRenderCallback)(IRParticle* particle, void* user_data);

/**
 * Set custom particle render callback
 * If not set, particles won't be rendered
 */
void ir_particle_set_render_callback(IRParticleRenderCallback callback, void* user_data);

/**
 * Render all active particles
 * Calls the registered render callback for each particle
 */
void ir_particle_render(void);

/**
 * Get particle system statistics
 */
void ir_particle_get_stats(uint32_t* active_particles,
                           uint32_t* active_emitters,
                           uint32_t* total_spawned,
                           uint32_t* total_died);

/**
 * Clear all particles
 * Emitters remain active
 */
void ir_particle_clear_all(void);

// ============================================================================
// Emitter Management
// ============================================================================

/**
 * Create particle emitter
 * Returns emitter ID or IR_INVALID_EMITTER on failure
 */
IRParticleEmitterID ir_particle_create_emitter(IREmitterType type);

/**
 * Destroy emitter
 * All particles spawned by this emitter will continue until they die naturally
 */
void ir_particle_destroy_emitter(IRParticleEmitterID emitter_id);

/**
 * Get emitter by ID
 */
IRParticleEmitter* ir_particle_get_emitter(IRParticleEmitterID emitter_id);

/**
 * Start/stop emitter
 */
void ir_particle_emitter_start(IRParticleEmitterID emitter_id);
void ir_particle_emitter_stop(IRParticleEmitterID emitter_id);

/**
 * Set emitter position
 */
void ir_particle_emitter_set_position(IRParticleEmitterID emitter_id, float x, float y);

/**
 * Emit burst of particles
 * Useful for one-time effects like explosions
 */
void ir_particle_emit_burst(IRParticleEmitterID emitter_id, uint32_t count);

// ============================================================================
// Emitter Configuration Helpers
// ============================================================================

/**
 * Set emission rate (particles per second)
 */
void ir_particle_emitter_set_rate(IRParticleEmitterID emitter_id, float rate);

/**
 * Set particle lifetime range
 */
void ir_particle_emitter_set_lifetime(IRParticleEmitterID emitter_id,
                                      float min_lifetime, float max_lifetime);

/**
 * Set particle speed range
 */
void ir_particle_emitter_set_speed(IRParticleEmitterID emitter_id,
                                   float min_speed, float max_speed);

/**
 * Set particle size range
 */
void ir_particle_emitter_set_size(IRParticleEmitterID emitter_id,
                                  float min_size, float max_size);

/**
 * Set size curve (interpolate size over lifetime)
 */
void ir_particle_emitter_set_size_curve(IRParticleEmitterID emitter_id,
                                        float start_size, float end_size);

/**
 * Set color gradient (interpolate color over lifetime)
 */
void ir_particle_emitter_set_color_gradient(IRParticleEmitterID emitter_id,
                                            uint32_t start_color, uint32_t end_color);

/**
 * Set emission direction and spread
 * direction: Base direction in degrees (0 = right, 90 = down)
 * spread: Angle spread in degrees (0 = no spread, 360 = all directions)
 */
void ir_particle_emitter_set_direction(IRParticleEmitterID emitter_id,
                                       float direction, float spread);

/**
 * Set gravity
 */
void ir_particle_emitter_set_gravity(IRParticleEmitterID emitter_id,
                                     float gravity_x, float gravity_y);

/**
 * Set air drag (0-1, 0=no drag, 1=full stop)
 */
void ir_particle_emitter_set_drag(IRParticleEmitterID emitter_id, float drag);

/**
 * Set particle sprite
 */
void ir_particle_emitter_set_sprite(IRParticleEmitterID emitter_id, IRSpriteID sprite);

/**
 * Set blend mode
 */
void ir_particle_emitter_set_blend_mode(IRParticleEmitterID emitter_id,
                                        IRParticleBlendMode mode);

// ============================================================================
// Preset Emitters (Quick Setup)
// ============================================================================

/**
 * Create fire emitter preset
 */
IRParticleEmitterID ir_particle_create_fire(float x, float y);

/**
 * Create smoke emitter preset
 */
IRParticleEmitterID ir_particle_create_smoke(float x, float y);

/**
 * Create explosion burst preset
 */
IRParticleEmitterID ir_particle_create_explosion(float x, float y);

/**
 * Create sparkles emitter preset
 */
IRParticleEmitterID ir_particle_create_sparkles(float x, float y);

/**
 * Create rain emitter preset
 */
IRParticleEmitterID ir_particle_create_rain(float x, float y, float width);

#ifdef __cplusplus
}
#endif

#endif // IR_PARTICLE_H
