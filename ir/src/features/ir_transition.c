// IR Transition System - CSS-style automatic transitions
//
// This module provides runtime execution of CSS-style transitions.
// When style properties change (via pseudo-states or direct changes),
// transitions automatically animate from old to new values.

#define _POSIX_C_SOURCE 199309L

#include "ir_transition.h"
#include "../include/ir_core.h"
#include "../include/ir_builder.h"
#include "ir_animation.h"
#include "../utils/ir_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declarations from animation module
extern float ir_easing_evaluate(IREasingType type, float t);

// ============================================================================
// Context Management
// ============================================================================

IRTransitionContext* ir_transition_context_create(void) {
    IRTransitionContext* ctx = (IRTransitionContext*)calloc(1, sizeof(IRTransitionContext));
    if (!ctx) return NULL;

    ctx->count = 0;
    ctx->total_transitions_started = 0;
    ctx->total_transitions_completed = 0;

    return ctx;
}

void ir_transition_context_destroy(IRTransitionContext* ctx) {
    if (!ctx) return;
    free(ctx);
}

void ir_transition_context_reset(IRTransitionContext* ctx) {
    if (!ctx) return;

    ctx->count = 0;
    ctx->total_transitions_started = 0;
    ctx->total_transitions_completed = 0;
}

// ============================================================================
// Component State Access
// ============================================================================

IRComponentTransitionState* ir_transition_get_state(IRTransitionContext* ctx, IRComponent* component) {
    if (!ctx || !component) return NULL;

    // Find existing entry
    for (uint32_t i = 0; i < ctx->count; i++) {
        if (ctx->entries[i].component == component) {
            return &ctx->entries[i].state;
        }
    }

    // Create new entry
    if (ctx->count >= IR_MAX_TRANSITION_ENTRIES) {
        return NULL;  // Max reached
    }

    ctx->entries[ctx->count].component = component;
    IRComponentTransitionState* state = &ctx->entries[ctx->count].state;

    // Initialize state
    memset(state, 0, sizeof(IRComponentTransitionState));
    state->active_count = 0;
    state->previous_pseudo_state = 0;
    state->values_initialized = false;
    state->in_state_change = false;

    ctx->count++;
    return state;
}

void ir_transition_remove_state(IRTransitionContext* ctx, IRComponent* component) {
    if (!ctx || !component) return;

    for (uint32_t i = 0; i < ctx->count; i++) {
        if (ctx->entries[i].component == component) {
            // Shift remaining entries
            memmove(&ctx->entries[i], &ctx->entries[i + 1],
                   (ctx->count - i - 1) * sizeof(ctx->entries[0]));
            ctx->count--;
            return;
        }
    }
}

// ============================================================================
// Pseudo-State Detection
// ============================================================================

uint32_t ir_transition_get_pseudo_state(IRComponent* component) {
    if (!component || !component->style) {
        return IR_PSEUDO_NONE;
    }

    // The style already tracks current pseudo-states via current_pseudo_states
    // This function maps those to our flags
    uint32_t state = IR_PSEUDO_NONE;

    // Check if component has hover state set
    if (component->style->current_pseudo_states & (1 << 0)) {
        state |= IR_PSEUDO_HOVER;
    }
    if (component->style->current_pseudo_states & (1 << 1)) {
        state |= IR_PSEUDO_ACTIVE;
    }
    if (component->style->current_pseudo_states & (1 << 2)) {
        state |= IR_PSEUDO_FOCUS;
    }
    if (component->style->current_pseudo_states & (1 << 3)) {
        state |= IR_PSEUDO_DISABLED;
    }
    if (component->style->current_pseudo_states & (1 << 4)) {
        state |= IR_PSEUDO_CHECKED;
    }

    return state;
}

// ============================================================================
// Value Snapshotting
// ============================================================================

void ir_transition_snapshot_values(IRComponent* component, IRComponentTransitionState* state) {
    if (!component || !component->style) return;

    IRStyle* style = component->style;

    // Snapshot current style values
    state->previous_values.opacity = style->opacity;
    state->previous_values.translate_x = style->transform.translate_x;
    state->previous_values.translate_y = style->transform.translate_y;
    state->previous_values.scale_x = style->transform.scale_x;
    state->previous_values.scale_y = style->transform.scale_y;
    state->previous_values.rotate = style->transform.rotate;

    // Deep copy colors
    state->previous_values.background = style->background;
    state->previous_values.border_color = style->border_color;
    state->previous_values.text_fill_color = style->text_fill_color;

    state->values_initialized = true;
}

// ============================================================================
// Transition Interpolation
// ============================================================================

static IRColor interpolate_color(IRColor from, IRColor to, float t) {
    IRColor result;
    result.type = IR_COLOR_SOLID;
    result.data.r = (uint8_t)(from.data.r + (to.data.r - from.data.r) * t);
    result.data.g = (uint8_t)(from.data.g + (to.data.g - from.data.g) * t);
    result.data.b = (uint8_t)(from.data.b + (to.data.b - from.data.b) * t);
    result.data.a = (uint8_t)(from.data.a + (to.data.a - from.data.a) * t);
    result.var_name = NULL;
    return result;
}

static void apply_transition_to_component(IRActiveTransition* active, IRComponent* component, float t) {
    IRStyle* style = component->style;
    if (!style) return;

    float eased_t = ir_easing_evaluate(active->definition->easing, t);

    switch (active->property) {
        case IR_ANIM_PROP_OPACITY: {
            float value = active->values.numeric.start +
                         (active->values.numeric.end - active->values.numeric.start) * eased_t;
            style->opacity = value;
            break;
        }

        case IR_ANIM_PROP_TRANSLATE_X: {
            float value = active->values.translate.start_x +
                         (active->values.translate.end_x - active->values.translate.start_x) * eased_t;
            style->transform.translate_x = value;
            break;
        }

        case IR_ANIM_PROP_TRANSLATE_Y: {
            float value = active->values.translate.start_y +
                         (active->values.translate.end_y - active->values.translate.start_y) * eased_t;
            style->transform.translate_y = value;
            break;
        }

        case IR_ANIM_PROP_SCALE_X:
        case IR_ANIM_PROP_SCALE_Y: {
            // Scale uses unified scale for both x and y in transitions
            float value = active->values.scale.start_scale +
                         (active->values.scale.end_scale - active->values.scale.start_scale) * eased_t;
            if (active->property == IR_ANIM_PROP_SCALE_X) {
                style->transform.scale_x = value;
            } else {
                style->transform.scale_y = value;
            }
            break;
        }

        case IR_ANIM_PROP_ROTATE: {
            float value = active->values.numeric.start +
                         (active->values.numeric.end - active->values.numeric.start) * eased_t;
            style->transform.rotate = value;
            break;
        }

        case IR_ANIM_PROP_BACKGROUND_COLOR: {
            IRColor value = interpolate_color(active->values.color.start,
                                            active->values.color.end, eased_t);
            style->background = value;
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// Apply All Active Transitions
// ============================================================================

void ir_transition_apply_values(IRComponentTransitionState* state, IRComponent* component) {
    if (!state || !component || !component->style) return;

    // For each active transition, apply interpolated value
    // We apply in order, with later transitions overriding earlier ones
    // (last transition for a property wins)
    for (uint8_t i = 0; i < IR_MAX_ACTIVE_TRANSITIONS; i++) {
        IRActiveTransition* active = &state->active[i];
        if (!active->is_active || active->is_delayed || active->completed) {
            continue;
        }

        // Calculate progress
        float progress = active->elapsed / active->duration;
        if (progress > 1.0f) progress = 1.0f;

        apply_transition_to_component(active, component, progress);
    }
}

// ============================================================================
// Transition Update
// ============================================================================

bool ir_transition_has_active(IRComponentTransitionState* state) {
    if (!state) return false;

    for (uint8_t i = 0; i < IR_MAX_ACTIVE_TRANSITIONS; i++) {
        if (state->active[i].is_active && !state->active[i].completed) {
            return true;
        }
    }
    return false;
}

static void update_active_transitions(IRTransitionContext* ctx,
                                     IRComponentTransitionState* state,
                                     IRComponent* component,
                                     float delta_time) {
    if (!state || !component || !component->style) return;

    for (uint8_t i = 0; i < IR_MAX_ACTIVE_TRANSITIONS; i++) {
        IRActiveTransition* active = &state->active[i];
        if (!active->is_active || active->completed) {
            continue;
        }

        // Handle delay
        if (active->is_delayed) {
            active->delay -= delta_time;
            if (active->delay <= 0.0f) {
                active->is_delayed = false;
                active->delay = 0.0f;
            } else {
                continue;  // Still in delay phase
            }
        }

        // Update elapsed time
        active->elapsed += delta_time;

        // Calculate progress
        float progress = active->elapsed / active->duration;
        if (progress >= 1.0f) {
            progress = 1.0f;
            active->completed = true;
            active->is_active = false;
            ctx->total_transitions_completed++;
        }

        // Apply interpolated value
        apply_transition_to_component(active, component, progress);

        // Mark for re-render if still animating
        if (!active->completed) {
            ir_layout_mark_render_dirty(component);
        }
    }
}

static void detect_and_start_transitions(IRTransitionContext* ctx,
                                        IRComponent* component,
                                        IRComponentTransitionState* state) {
    if (!ctx || !component || !state) return;
    if (!component->style) return;

    uint32_t current_state = ir_transition_get_pseudo_state(component);
    uint32_t previous_state = state->previous_pseudo_state;

    if (current_state == previous_state && state->values_initialized) {
        return;  // No change
    }

    // Snapshot current values before any state change
    if (!state->values_initialized) {
        ir_transition_snapshot_values(component, state);
        state->previous_pseudo_state = current_state;
        return;
    }

    IRStyle* style = component->style;
    if (!style || style->transition_count == 0) {
        state->previous_pseudo_state = current_state;
        return;
    }

    // State changed - start transitions for defined properties
    for (uint32_t i = 0; i < style->transition_count; i++) {
        IRTransition* trans = style->transitions[i];
        if (!trans) continue;

        // Check if this transition should trigger
        // trigger_state == 0 means "always trigger on any state change"
        uint32_t trigger = trans->trigger_state;
        bool should_trigger = (trigger == 0) ||
                             ((current_state & trigger) != (previous_state & trigger));

        if (should_trigger) {
            ir_transition_start(ctx, component, trans, previous_state, current_state);
        }
    }

    // Update previous state
    state->previous_pseudo_state = current_state;
}

// ============================================================================
// Tree Update (Main Entry Point)
// ============================================================================

void ir_transition_tree_update(IRTransitionContext* ctx, IRComponent* root, float delta_time) {
    if (!ctx || !root) return;

    // Skip if component has no style or no transitions defined
    if (!root->style || root->style->transition_count == 0) {
        // Still process children recursively
        if (root->children) {
            for (uint32_t i = 0; i < root->child_count; i++) {
                if (root->children[i]) {
                    ir_transition_tree_update(ctx, root->children[i], delta_time);
                }
            }
        }
        return;
    }

    // Skip subtrees without any registered transitions
    if (ctx->count == 0) return;

    // Process this component
    IRComponentTransitionState* state = ir_transition_get_state(ctx, root);
    if (state) {
        // Detect and start new transitions
        detect_and_start_transitions(ctx, root, state);

        // Update active transitions
        update_active_transitions(ctx, state, root, delta_time);
    }

    // Recursively process children
    if (root->children) {
        for (uint32_t i = 0; i < root->child_count; i++) {
            if (root->children[i]) {
                ir_transition_tree_update(ctx, root->children[i], delta_time);
            }
        }
    }
}

// ============================================================================
// Transition Start
// ============================================================================

void ir_transition_start(IRTransitionContext* ctx, IRComponent* component,
                        IRTransition* definition, uint32_t from_state, uint32_t to_state) {
    if (!ctx || !component || !definition) return;

    IRStyle* style = component->style;
    if (!style) return;

    IRComponentTransitionState* state = ir_transition_get_state(ctx, component);
    if (!state) return;

    // Ensure we have a snapshot of current values
    if (!state->values_initialized) {
        ir_transition_snapshot_values(component, state);
    }

    // Find an available slot for the new transition
    IRActiveTransition* active = NULL;
    for (uint8_t i = 0; i < IR_MAX_ACTIVE_TRANSITIONS; i++) {
        if (!state->active[i].is_active || state->active[i].completed) {
            active = &state->active[i];
            break;
        }
    }

    if (!active) return;  // No available slots

    // Cancel any existing transition for the same property
    ir_transition_cancel_property(ctx, component, definition->property);

    // Find a free slot again (in case the cancelled one freed up)
    for (uint8_t i = 0; i < IR_MAX_ACTIVE_TRANSITIONS; i++) {
        if (!state->active[i].is_active || state->active[i].completed) {
            active = &state->active[i];
            break;
        }
    }
    if (!active) return;

    // Initialize the transition
    memset(active, 0, sizeof(IRActiveTransition));
    active->definition = definition;
    active->property = definition->property;
    active->elapsed = 0.0f;
    active->duration = definition->duration;
    active->delay = definition->delay;
    active->is_active = true;
    active->is_delayed = (definition->delay > 0);
    active->completed = false;
    active->from_state = from_state;
    active->to_state = to_state;

    // Set start and end values based on property
    switch (definition->property) {
        case IR_ANIM_PROP_OPACITY:
            active->values.numeric.start = state->previous_values.opacity;
            active->values.numeric.end = style->opacity;
            break;

        case IR_ANIM_PROP_TRANSLATE_X:
            active->values.translate.start_x = state->previous_values.translate_x;
            active->values.translate.start_y = style->transform.translate_y;  // Keep Y
            active->values.translate.end_x = style->transform.translate_x;
            active->values.translate.end_y = style->transform.translate_y;
            break;

        case IR_ANIM_PROP_TRANSLATE_Y:
            active->values.translate.start_x = style->transform.translate_x;  // Keep X
            active->values.translate.start_y = state->previous_values.translate_y;
            active->values.translate.end_x = style->transform.translate_x;
            active->values.translate.end_y = style->transform.translate_y;
            break;

        case IR_ANIM_PROP_SCALE_X:
            active->values.scale.start_scale = state->previous_values.scale_x;
            active->values.scale.end_scale = style->transform.scale_x;
            break;

        case IR_ANIM_PROP_SCALE_Y:
            active->values.scale.start_scale = state->previous_values.scale_y;
            active->values.scale.end_scale = style->transform.scale_y;
            break;

        case IR_ANIM_PROP_ROTATE:
            active->values.numeric.start = state->previous_values.rotate;
            active->values.numeric.end = style->transform.rotate;
            break;

        case IR_ANIM_PROP_BACKGROUND_COLOR:
            active->values.color.start = state->previous_values.background;
            active->values.color.end = style->background;
            break;

        default:
            // Unsupported property
            active->is_active = false;
            return;
    }

    state->active_count++;
    ctx->total_transitions_started++;

    // Mark component as having active animations
    component->has_active_animations = true;
    IRComponent* ancestor = component->parent;
    while (ancestor) {
        ancestor->has_active_animations = true;
        ancestor = ancestor->parent;
    }
}

// ============================================================================
// Transition Cancellation
// ============================================================================

void ir_transition_cancel_all(IRTransitionContext* ctx, IRComponent* component) {
    if (!ctx || !component) return;

    IRComponentTransitionState* state = ir_transition_get_state(ctx, component);
    if (!state) return;

    for (uint8_t i = 0; i < IR_MAX_ACTIVE_TRANSITIONS; i++) {
        state->active[i].is_active = false;
        state->active[i].completed = true;
    }
    state->active_count = 0;
}

void ir_transition_cancel_property(IRTransitionContext* ctx, IRComponent* component,
                                  IRAnimationProperty property) {
    if (!ctx || !component) return;

    IRComponentTransitionState* state = ir_transition_get_state(ctx, component);
    if (!state) return;

    for (uint8_t i = 0; i < IR_MAX_ACTIVE_TRANSITIONS; i++) {
        if (state->active[i].is_active && state->active[i].property == property) {
            state->active[i].is_active = false;
            state->active[i].completed = true;
            state->active_count--;
        }
    }
}

// ============================================================================
// Value Access (for external queries)
// ============================================================================

float ir_transition_get_opacity(IRComponent* component) {
    if (!component || !component->style) return -1.0f;
    return component->style->opacity;
}

bool ir_transition_get_transform(IRComponent* component, float* x, float* y,
                                float* scale_x, float* scale_y, float* rotate) {
    if (!component || !component->style) return false;

    IRTransform* t = &component->style->transform;
    if (x) *x = t->translate_x;
    if (y) *y = t->translate_y;
    if (scale_x) *scale_x = t->scale_x;
    if (scale_y) *scale_y = t->scale_y;
    if (rotate) *rotate = t->rotate;
    return true;
}

// ============================================================================
// Statistics
// ============================================================================

void ir_transition_get_stats(IRTransitionContext* ctx, IRTransitionStats* stats) {
    if (!stats) return;

    if (!ctx) {
        memset(stats, 0, sizeof(IRTransitionStats));
        return;
    }

    stats->total_started = ctx->total_transitions_started;
    stats->total_completed = ctx->total_transitions_completed;
    stats->active_components = 0;
    stats->active_transitions = 0;

    for (uint32_t i = 0; i < ctx->count; i++) {
        if (ir_transition_has_active(&ctx->entries[i].state)) {
            stats->active_components++;
            stats->active_transitions += ctx->entries[i].state.active_count;
        }
    }
}
