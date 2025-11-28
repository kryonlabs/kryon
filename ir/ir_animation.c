// IR Animation System - Foundation for animations
// Provides timing, easing, and animation state management

#include "ir_animation.h"
#include "ir_core.h"
#include "ir_builder.h"
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
    IRAnimationType type;
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

IRAnimationState* ir_animation_create(IRComponent* target, IRAnimationType type, float duration) {
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

void ir_animation_set_delay(IRAnimationState* anim, float delay) {
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

        // Apply animation based on type
        if (anim->target && anim->target->style) {
            switch (anim->type) {
                case IR_ANIMATION_FADE:
                    anim->target->style->opacity = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    break;

                case IR_ANIMATION_SCALE:
                    anim->target->style->transform.scale_x = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    anim->target->style->transform.scale_y = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    break;

                case IR_ANIMATION_SLIDE:
                    anim->target->style->transform.translate_x = anim->start_value + (anim->end_value - anim->start_value) * eased;
                    break;

                default:
                    break;
            }

            // Mark component dirty for re-layout
            ir_layout_mark_dirty(anim->target);
        }

        current = &anim->next;
    }
}

uint32_t ir_animation_get_active_count(IRAnimationContext* ctx) {
    return ctx ? ctx->animation_count : 0;
}

// ============================================================================
// Animation Helper Functions
// ============================================================================

IRAnimationState* ir_animation_fade_in(IRComponent* target, float duration) {
    IRAnimationState* anim = ir_animation_create(target, IR_ANIMATION_FADE, duration);
    if (anim && target && target->style) {
        ir_animation_set_property_range(anim, 0.0f, 1.0f, &target->style->opacity);
    }
    return anim;
}

IRAnimationState* ir_animation_fade_out(IRComponent* target, float duration) {
    IRAnimationState* anim = ir_animation_create(target, IR_ANIMATION_FADE, duration);
    if (anim && target && target->style) {
        ir_animation_set_property_range(anim, 1.0f, 0.0f, &target->style->opacity);
    }
    return anim;
}

IRAnimationState* ir_animation_slide_in(IRComponent* target, float from_x, float duration) {
    IRAnimationState* anim = ir_animation_create(target, IR_ANIMATION_SLIDE, duration);
    if (anim && target && target->style) {
        ir_animation_set_property_range(anim, from_x, 0.0f, &target->style->transform.translate_x);
    }
    return anim;
}

IRAnimationState* ir_animation_scale(IRComponent* target, float from_scale, float to_scale, float duration) {
    IRAnimationState* anim = ir_animation_create(target, IR_ANIMATION_SCALE, duration);
    if (anim) {
        ir_animation_set_property_range(anim, from_scale, to_scale, NULL);
    }
    return anim;
}
