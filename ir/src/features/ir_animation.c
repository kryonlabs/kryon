// IR Animation System - Foundation for animations
// Provides timing, easing, and animation state management

#include "ir_animation.h"
#include "../include/ir_core.h"
#include "../include/ir_builder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Easing Functions
// ============================================================================

float ir_ease_linear(float t) {
    return t;
}

float ir_ease_in_quad(float t) {
    return t * t;
}

float ir_ease_out_quad(float t) {
    return t * (2.0f - t);
}

float ir_ease_in_out_quad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

float ir_ease_in_cubic(float t) {
    return t * t * t;
}

float ir_ease_out_cubic(float t) {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}

float ir_ease_in_out_cubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

float ir_ease_in_bounce(float t) {
    return 1.0f - ir_ease_out_bounce(1.0f - t);
}

float ir_ease_out_bounce(float t) {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= 2.625f / 2.75f;
        return 7.5625f * t * t + 0.984375f;
    }
}

// ============================================================================
// Animation State Management
// ============================================================================

typedef struct IRAnimationState {
    IRComponent* target;
    uint32_t type;  // Animation type (deprecated, use keyframes)
    float duration;
    float elapsed;
    float delay;
    bool loop;
    bool playing;
    IREasingFunction easing;

    // Property animation data
    float start_value;
    float end_value;
    float* target_property;  // Pointer to the property being animated

    struct IRAnimationState* next;
} IRAnimationState;

struct IRAnimationContext {
    IRAnimationState* active_animations;
    uint32_t animation_count;
};

IRAnimationContext* ir_animation_context_create(void) {
    IRAnimationContext* ctx = (IRAnimationContext*)calloc(1, sizeof(IRAnimationContext));
    return ctx;
}

void ir_animation_context_destroy(IRAnimationContext* ctx) {
    if (!ctx) return;

    IRAnimationState* anim = ctx->active_animations;
    while (anim) {
        IRAnimationState* next = anim->next;
        free(anim);
        anim = next;
    }

    free(ctx);
}

IRAnimationState* ir_animation_create(IRComponent* target, uint32_t type, float duration) {
    if (!target) return NULL;

    IRAnimationState* anim = (IRAnimationState*)calloc(1, sizeof(IRAnimationState));
    if (!anim) return NULL;

    anim->target = target;
    anim->type = type;
    anim->duration = duration;
    anim->elapsed = 0.0f;
    anim->delay = 0.0f;
    anim->loop = false;
    anim->playing = false;
    anim->easing = ir_ease_linear;
    anim->start_value = 0.0f;
    anim->end_value = 1.0f;
    anim->target_property = NULL;
    anim->next = NULL;

    return anim;
}

void ir_animation_set_easing(IRAnimationState* anim, IREasingFunction easing) {
    if (anim && easing) {
        anim->easing = easing;
    }
}

void ir_animation_set_delay_legacy(IRAnimationState* anim, float delay) {
    if (anim) {
        anim->delay = delay;
    }
}

void ir_animation_set_loop(IRAnimationState* anim, bool loop) {
    if (anim) {
        anim->loop = loop;
    }
}

void ir_animation_set_property_range(IRAnimationState* anim, float start, float end, float* target_property) {
    if (!anim) return;

    anim->start_value = start;
    anim->end_value = end;
    anim->target_property = target_property;
}

void ir_animation_start(IRAnimationContext* ctx, IRAnimationState* anim) {
    if (!ctx || !anim) return;

    anim->playing = true;
    anim->elapsed = 0.0f;

    // Add to active animations list
    anim->next = ctx->active_animations;
    ctx->active_animations = anim;
    ctx->animation_count++;
}

void ir_animation_stop(IRAnimationState* anim) {
    if (anim) {
        anim->playing = false;
    }
}

void ir_animation_update(IRAnimationContext* ctx, float delta_time) {
    if (!ctx) return;

    IRAnimationState** current = &ctx->active_animations;

    while (*current) {
        IRAnimationState* anim = *current;

        if (!anim->playing) {
            // Remove from list
            *current = anim->next;
            ctx->animation_count--;
            free(anim);
            continue;
        }

        // Handle delay
        if (anim->delay > 0.0f) {
            anim->delay -= delta_time;
            current = &anim->next;
            continue;
        }

        // Update elapsed time
        anim->elapsed += delta_time;

        // Calculate progress (0.0 to 1.0)
        float progress = anim->elapsed / anim->duration;
        if (progress > 1.0f) {
            if (anim->loop) {
                anim->elapsed = 0.0f;
                progress = 0.0f;
            } else {
                progress = 1.0f;
                anim->playing = false;
            }
        }

        // Apply easing
        float eased = anim->easing(progress);

        // Update property
        if (anim->target_property) {
            float value = anim->start_value + (anim->end_value - anim->start_value) * eased;
            *anim->target_property = value;
        }

        // Apply animation based on type (legacy types: 0=fade, 1=slide, 2=scale)
        if (anim->target && anim->target->style) {
            switch (anim->type) {
                case 0:  // Fade
                    anim->target->style->opacity = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    break;

                case 2:  // Scale
                    anim->target->style->transform.scale_x = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    anim->target->style->transform.scale_y = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    break;

                case 1:  // Slide
                    anim->target->style->transform.translate_x = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    break;

                default:
                    break;
            }

            // Mark component dirty for re-render
            ir_layout_mark_render_dirty(anim->target);
        }

        current = &anim->next;
    }
}

uint32_t ir_animation_get_active_count(IRAnimationContext* ctx) {
    return ctx ? ctx->animation_count : 0;
}

// ============================================================================
// Easing Type to Function Mapping
// ============================================================================

float ir_easing_evaluate(IREasingType type, float t) {
    switch (type) {
        case IR_EASING_LINEAR:
            return ir_ease_linear(t);
        case IR_EASING_EASE_IN:
        case IR_EASING_EASE_IN_CUBIC:
            return ir_ease_in_cubic(t);
        case IR_EASING_EASE_OUT:
        case IR_EASING_EASE_OUT_CUBIC:
            return ir_ease_out_cubic(t);
        case IR_EASING_EASE_IN_OUT:
        case IR_EASING_EASE_IN_OUT_CUBIC:
            return ir_ease_in_out_cubic(t);
        case IR_EASING_EASE_IN_QUAD:
            return ir_ease_in_quad(t);
        case IR_EASING_EASE_OUT_QUAD:
            return ir_ease_out_quad(t);
        case IR_EASING_EASE_IN_OUT_QUAD:
            return ir_ease_in_out_quad(t);
        case IR_EASING_EASE_IN_BOUNCE:
            return ir_ease_in_bounce(t);
        case IR_EASING_EASE_OUT_BOUNCE:
            return ir_ease_out_bounce(t);
        default:
            return t;  // Linear fallback
    }
}

// ============================================================================
// Keyframe Animation
// ============================================================================

// Find two keyframes to interpolate between at given time
static void find_keyframe_range(IRAnimation* anim, float normalized_time,
                               IRKeyframe** out_from, IRKeyframe** out_to, float* out_local_t) {
    if (!anim || anim->keyframe_count < 2) {
        *out_from = NULL;
        *out_to = NULL;
        *out_local_t = 0.0f;
        return;
    }

    // Find the two keyframes we're between
    for (uint8_t i = 0; i < anim->keyframe_count - 1; i++) {
        if (normalized_time >= anim->keyframes[i].offset &&
            normalized_time <= anim->keyframes[i + 1].offset) {
            *out_from = &anim->keyframes[i];
            *out_to = &anim->keyframes[i + 1];

            // Calculate local time between these keyframes
            float range = (*out_to)->offset - (*out_from)->offset;
            *out_local_t = (normalized_time - (*out_from)->offset) / range;
            return;
        }
    }

    // Handle edge cases
    if (normalized_time <= anim->keyframes[0].offset) {
        *out_from = &anim->keyframes[0];
        *out_to = &anim->keyframes[0];
        *out_local_t = 0.0f;
    } else {
        *out_from = &anim->keyframes[anim->keyframe_count - 1];
        *out_to = &anim->keyframes[anim->keyframe_count - 1];
        *out_local_t = 1.0f;
    }
}

// Interpolate numeric property value
static float interpolate_property(float from_value, float to_value, float t, IREasingType easing) {
    float eased_t = ir_easing_evaluate(easing, t);
    return from_value + (to_value - from_value) * eased_t;
}

// Interpolate color property
static IRColor interpolate_color(IRColor from, IRColor to, float t, IREasingType easing) {
    float eased_t = ir_easing_evaluate(easing, t);

    // For now, only interpolate solid colors
    if (from.type == IR_COLOR_SOLID && to.type == IR_COLOR_SOLID) {
        IRColor result;
        result.type = IR_COLOR_SOLID;
        result.data.r = (uint8_t)(from.data.r + (to.data.r - from.data.r) * eased_t);
        result.data.g = (uint8_t)(from.data.g + (to.data.g - from.data.g) * eased_t);
        result.data.b = (uint8_t)(from.data.b + (to.data.b - from.data.b) * eased_t);
        result.data.a = (uint8_t)(from.data.a + (to.data.a - from.data.a) * eased_t);
        return result;
    }

    // For other color types, just return target
    return to;
}

// Apply keyframe animation to component
void ir_animation_apply_keyframes(IRComponent* component, IRAnimation* anim, float current_time) {
    if (!component || !anim || !component->style) return;

    // Handle delay
    if (current_time < anim->delay) return;
    float adjusted_time = current_time - anim->delay;

    // Handle iteration and looping
    if (anim->iteration_count >= 0 && adjusted_time >= anim->duration * anim->iteration_count) {
        return;  // Animation finished
    }

    // Calculate normalized time (0.0 to 1.0) within current iteration
    float iteration_time = fmodf(adjusted_time, anim->duration);
    float normalized_time = iteration_time / anim->duration;

    // Handle alternate direction
    int iteration = (int)(adjusted_time / anim->duration);
    if (anim->alternate && (iteration % 2 == 1)) {
        normalized_time = 1.0f - normalized_time;
    }

    // Find keyframe range
    IRKeyframe* from_kf = NULL;
    IRKeyframe* to_kf = NULL;
    float local_t = 0.0f;
    find_keyframe_range(anim, normalized_time, &from_kf, &to_kf, &local_t);

    if (!from_kf || !to_kf) return;

    // Use easing from the 'from' keyframe
    IREasingType easing = from_kf->easing;

    // Apply each property
    for (uint8_t i = 0; i < from_kf->property_count; i++) {
        if (!from_kf->properties[i].is_set) continue;

        IRAnimationProperty prop = from_kf->properties[i].property;

        // Find corresponding property in 'to' keyframe
        float from_value = from_kf->properties[i].value;
        IRColor from_color = from_kf->properties[i].color_value;
        float to_value = from_value;  // Default if not found
        IRColor to_color = from_color;

        for (uint8_t j = 0; j < to_kf->property_count; j++) {
            if (to_kf->properties[j].property == prop && to_kf->properties[j].is_set) {
                to_value = to_kf->properties[j].value;
                to_color = to_kf->properties[j].color_value;
                break;
            }
        }

        // Apply interpolated value to component
        switch (prop) {
            case IR_ANIM_PROP_OPACITY:
                component->style->opacity = interpolate_property(from_value, to_value, local_t, easing);
                break;

            case IR_ANIM_PROP_TRANSLATE_X:
                component->style->transform.translate_x = interpolate_property(from_value, to_value, local_t, easing);
                break;

            case IR_ANIM_PROP_TRANSLATE_Y:
                component->style->transform.translate_y = interpolate_property(from_value, to_value, local_t, easing);
                break;

            case IR_ANIM_PROP_SCALE_X:
                component->style->transform.scale_x = interpolate_property(from_value, to_value, local_t, easing);
                break;

            case IR_ANIM_PROP_SCALE_Y:
                component->style->transform.scale_y = interpolate_property(from_value, to_value, local_t, easing);
                break;

            case IR_ANIM_PROP_ROTATE:
                component->style->transform.rotate = interpolate_property(from_value, to_value, local_t, easing);
                break;

            case IR_ANIM_PROP_BACKGROUND_COLOR:
                component->style->background = interpolate_color(from_color, to_color, local_t, easing);
                break;

            default:
                break;
        }
    }

    // Mark component dirty for re-render
    // Use render-only flag since animations don't affect layout
    ir_layout_mark_render_dirty(component);
}

// ============================================================================
// Animation Helper Functions
// ============================================================================

IRAnimationState* ir_animation_fade_in(IRComponent* target, float duration) {
    IRAnimationState* anim = ir_animation_create(target, 0, duration);  // 0 = fade
    if (anim && target && target->style) {
        ir_animation_set_property_range(anim, 0.0f, 1.0f, &target->style->opacity);
    }
    return anim;
}

IRAnimationState* ir_animation_fade_out(IRComponent* target, float duration) {
    IRAnimationState* anim = ir_animation_create(target, 0, duration);  // 0 = fade
    if (anim && target && target->style) {
        ir_animation_set_property_range(anim, 1.0f, 0.0f, &target->style->opacity);
    }
    return anim;
}

IRAnimationState* ir_animation_slide_in(IRComponent* target, float from_x, float duration) {
    IRAnimationState* anim = ir_animation_create(target, 1, duration);  // 1 = slide
    if (anim && target && target->style) {
        ir_animation_set_property_range(anim, from_x, 0.0f, &target->style->transform.translate_x);
    }
    return anim;
}

IRAnimationState* ir_animation_scale(IRComponent* target, float from_scale, float to_scale, float duration) {
    IRAnimationState* anim = ir_animation_create(target, 2, duration);  // 2 = scale
    if (anim) {
        ir_animation_set_property_range(anim, from_scale, to_scale, NULL);
    }
    return anim;
}

// ============================================================================
// Preset Animations
// ============================================================================

IRAnimation* ir_animation_fade_in_out(float duration) {
    IRAnimation* anim = ir_animation_create_keyframe("fadeInOut", duration);
    if (!anim) return NULL;

    IRKeyframe* kf0 = ir_animation_add_keyframe(anim, 0.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_OPACITY, 0.0f);

    IRKeyframe* kf1 = ir_animation_add_keyframe(anim, 0.5f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_OPACITY, 1.0f);

    IRKeyframe* kf2 = ir_animation_add_keyframe(anim, 1.0f);
    ir_keyframe_set_property(kf2, IR_ANIM_PROP_OPACITY, 0.0f);

    return anim;
}

IRAnimation* ir_animation_pulse(float duration) {
    IRAnimation* anim = ir_animation_create_keyframe("pulse", duration);
    if (!anim) return NULL;

    IRKeyframe* kf0 = ir_animation_add_keyframe(anim, 0.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_SCALE_X, 1.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_SCALE_Y, 1.0f);

    IRKeyframe* kf1 = ir_animation_add_keyframe(anim, 0.5f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_SCALE_X, 1.1f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_SCALE_Y, 1.1f);
    ir_keyframe_set_easing(kf1, IR_EASING_EASE_IN_OUT);

    IRKeyframe* kf2 = ir_animation_add_keyframe(anim, 1.0f);
    ir_keyframe_set_property(kf2, IR_ANIM_PROP_SCALE_X, 1.0f);
    ir_keyframe_set_property(kf2, IR_ANIM_PROP_SCALE_Y, 1.0f);

    ir_animation_set_iterations(anim, -1);  // Loop forever
    return anim;
}

IRAnimation* ir_animation_slide_in_left(float duration) {
    IRAnimation* anim = ir_animation_create_keyframe("slideInLeft", duration);
    if (!anim) return NULL;

    IRKeyframe* kf0 = ir_animation_add_keyframe(anim, 0.0f);
    ir_keyframe_set_property(kf0, IR_ANIM_PROP_TRANSLATE_X, -300.0f);

    IRKeyframe* kf1 = ir_animation_add_keyframe(anim, 1.0f);
    ir_keyframe_set_property(kf1, IR_ANIM_PROP_TRANSLATE_X, 0.0f);
    ir_keyframe_set_easing(kf1, IR_EASING_EASE_OUT);

    return anim;
}
