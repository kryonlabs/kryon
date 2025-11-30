#ifndef IR_ANIMATION_H
#define IR_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>
#include "ir_core.h"

// Forward declarations
typedef struct IRAnimationState IRAnimationState;
typedef struct IRAnimationContext IRAnimationContext;

// ============================================================================
// Easing Functions
// ============================================================================

typedef float (*IREasingFunction)(float t);

// Linear
float ir_ease_linear(float t);

// Quadratic
float ir_ease_in_quad(float t);
float ir_ease_out_quad(float t);
float ir_ease_in_out_quad(float t);

// Cubic
float ir_ease_in_cubic(float t);
float ir_ease_out_cubic(float t);
float ir_ease_in_out_cubic(float t);

// Bounce
float ir_ease_in_bounce(float t);
float ir_ease_out_bounce(float t);

// ============================================================================
// Animation Context
// ============================================================================

// Create animation context
IRAnimationContext* ir_animation_context_create(void);

// Destroy animation context
void ir_animation_context_destroy(IRAnimationContext* ctx);

// Update all animations (call once per frame)
void ir_animation_update(IRAnimationContext* ctx, float delta_time);

// Get number of active animations
uint32_t ir_animation_get_active_count(IRAnimationContext* ctx);

// ============================================================================
// Animation Management
// ============================================================================

// Create a new animation (deprecated, use keyframe-based animations)
IRAnimationState* ir_animation_create(IRComponent* target, uint32_t type, float duration);

// Set easing function
void ir_animation_set_easing(IRAnimationState* anim, IREasingFunction easing);

// Set delay before animation starts  (for legacy animations)
void ir_animation_set_delay_legacy(IRAnimationState* anim, float delay);

// Set loop mode
void ir_animation_set_loop(IRAnimationState* anim, bool loop);

// Set property range to animate
void ir_animation_set_property_range(IRAnimationState* anim, float start, float end, float* target_property);

// Start animation
void ir_animation_start(IRAnimationContext* ctx, IRAnimationState* anim);

// Stop animation
void ir_animation_stop(IRAnimationState* anim);

// ============================================================================
// Animation Helpers
// ============================================================================

// Fade in animation
IRAnimationState* ir_animation_fade_in(IRComponent* target, float duration);

// Fade out animation
IRAnimationState* ir_animation_fade_out(IRComponent* target, float duration);

// Slide in animation
IRAnimationState* ir_animation_slide_in(IRComponent* target, float from_x, float duration);

// Scale animation
IRAnimationState* ir_animation_scale(IRComponent* target, float from_scale, float to_scale, float duration);

// Keyframe animation application
void ir_animation_apply_keyframes(IRComponent* component, IRAnimation* anim, float current_time);

// Easing evaluation
float ir_easing_evaluate(IREasingType type, float t);

#endif // IR_ANIMATION_H
