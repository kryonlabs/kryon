// IR Animation Builder Module
// Animation builder functions extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_animation_builder.h"
#include "ir_builder.h"
#include "ir_animation.h"
#include "ir_style_builder.h"
#include "ir_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum keyframes per animation (defined in ir_core.h, but need locally)
#ifndef IR_MAX_KEYFRAMES
#define IR_MAX_KEYFRAMES 16
#endif

// Maximum properties per keyframe
#ifndef IR_MAX_KEYFRAME_PROPERTIES
#define IR_MAX_KEYFRAME_PROPERTIES 16
#endif

// Forward declaration for keyframe application (from ir_animation.c)
extern void ir_animation_apply_keyframes(IRComponent* component, IRAnimation* anim, float current_time);

// ============================================================================
// Animation and Keyframe Creation
// ============================================================================

IRAnimation* ir_animation_create_keyframe(const char* name, float duration) {
    IRAnimation* anim = (IRAnimation*)calloc(1, sizeof(IRAnimation));
    if (!anim) return NULL;

    if (name) {
        anim->name = strdup(name);
    }
    anim->duration = duration;
    anim->delay = 0.0f;
    anim->iteration_count = 1;
    anim->alternate = false;
    anim->default_easing = IR_EASING_LINEAR;
    anim->keyframe_count = 0;
    anim->current_time = 0.0f;
    anim->current_iteration = 0;
    anim->is_paused = false;

    return anim;
}

void ir_animation_destroy(IRAnimation* anim) {
    if (!anim) return;
    if (anim->name) {
        free(anim->name);
    }
    free(anim);
}

void ir_animation_set_iterations(IRAnimation* anim, int32_t count) {
    if (anim) {
        anim->iteration_count = count;
    }
}

void ir_animation_set_alternate(IRAnimation* anim, bool alternate) {
    if (anim) {
        anim->alternate = alternate;
    }
}

void ir_animation_set_delay(IRAnimation* anim, float delay) {
    if (anim) {
        anim->delay = delay;
    }
}

void ir_animation_set_default_easing(IRAnimation* anim, IREasingType easing) {
    if (anim) {
        anim->default_easing = easing;
    }
}

// ============================================================================
// Keyframe Management
// ============================================================================

IRKeyframe* ir_animation_add_keyframe(IRAnimation* anim, float offset) {
    if (!anim || anim->keyframe_count >= IR_MAX_KEYFRAMES) return NULL;

    IRKeyframe* kf = &anim->keyframes[anim->keyframe_count];
    kf->offset = offset;
    kf->easing = anim->default_easing;
    kf->property_count = 0;

    // Initialize all properties as not set
    for (int i = 0; i < IR_MAX_KEYFRAME_PROPERTIES; i++) {
        kf->properties[i].is_set = false;
    }

    anim->keyframe_count++;
    return kf;
}

void ir_keyframe_set_property(IRKeyframe* kf, IRAnimationProperty prop, float value) {
    if (!kf || kf->property_count >= IR_MAX_KEYFRAME_PROPERTIES) return;

    // Check if property already exists, update it
    for (uint8_t i = 0; i < kf->property_count; i++) {
        if (kf->properties[i].property == prop) {
            kf->properties[i].value = value;
            kf->properties[i].is_set = true;
            return;
        }
    }

    // Add new property
    kf->properties[kf->property_count].property = prop;
    kf->properties[kf->property_count].value = value;
    kf->properties[kf->property_count].is_set = true;
    kf->property_count++;
}

void ir_keyframe_set_color_property(IRKeyframe* kf, IRAnimationProperty prop, IRColor color) {
    if (!kf || kf->property_count >= IR_MAX_KEYFRAME_PROPERTIES) return;

    // Check if property already exists, update it
    for (uint8_t i = 0; i < kf->property_count; i++) {
        if (kf->properties[i].property == prop) {
            kf->properties[i].color_value = color;
            kf->properties[i].is_set = true;
            return;
        }
    }

    // Add new property
    kf->properties[kf->property_count].property = prop;
    kf->properties[kf->property_count].color_value = color;
    kf->properties[kf->property_count].is_set = true;
    kf->property_count++;
}

void ir_keyframe_set_easing(IRKeyframe* kf, IREasingType easing) {
    if (kf) {
        kf->easing = easing;
    }
}

// ============================================================================
// Attach Animation to Component
// ============================================================================

void ir_component_add_animation(IRComponent* component, IRAnimation* anim) {
    if (!component || !anim) return;

    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    // Reallocate animations array (array of pointers)
    IRAnimation** new_anims = (IRAnimation**)realloc(style->animations,
                                                     (style->animation_count + 1) * sizeof(IRAnimation*));
    if (!new_anims) return;

    style->animations = new_anims;
    style->animations[style->animation_count] = anim;  // Store pointer
    style->animation_count++;

    // OPTIMIZATION: Set animation flag and propagate to ancestors
    component->has_active_animations = true;
    IRComponent* ancestor = component->parent;
    while (ancestor) {
        ancestor->has_active_animations = true;
        ancestor = ancestor->parent;
    }
}

// ============================================================================
// Animation Flag Propagation
// ============================================================================

// Re-propagate has_active_animations flags from children to ancestors
// Call this after the component tree is fully constructed
// FIX: Animations were attached before components were added to parents,
//      so the flag propagation in ir_component_add_animation didn't work
void ir_animation_propagate_flags(IRComponent* root) {
    if (!root) return;

    // Reset flag for this component
    root->has_active_animations = false;

    // Check if this component has animations (stored in style)
    IRStyle* style = ir_get_style(root);
    if (style && style->animation_count > 0) {
        root->has_active_animations = true;
    }

    // Recursively check children and propagate upward
    for (uint32_t i = 0; i < root->child_count; i++) {
        ir_animation_propagate_flags(root->children[i]);

        // If any child has animations, propagate to parent
        if (root->children[i]->has_active_animations) {
            root->has_active_animations = true;
        }
    }
}

// General component subtree finalization
// Call this after components are added (especially from static loops) to ensure
// all post-construction propagation steps are performed
void ir_component_finalize_subtree(IRComponent* component) {
    if (!component) return;

    // Propagate animation flags for this subtree
    ir_animation_propagate_flags(component);

    // Future finalization steps can be added here:
    // - Validate event handlers are properly registered
    // - Propagate style inheritance flags
    // - Initialize layout constraint caches
    // - etc.
}

// ============================================================================
// Transition Creation
// ============================================================================

IRTransition* ir_transition_create(IRAnimationProperty property, float duration) {
    IRTransition* trans = (IRTransition*)calloc(1, sizeof(IRTransition));
    if (!trans) return NULL;

    trans->property = property;
    trans->duration = duration;
    trans->delay = 0.0f;
    trans->easing = IR_EASING_EASE_IN_OUT;
    trans->trigger_state = 0;  // All states by default

    return trans;
}

void ir_transition_destroy(IRTransition* transition) {
    free(transition);
}

void ir_transition_set_easing(IRTransition* transition, IREasingType easing) {
    if (transition) {
        transition->easing = easing;
    }
}

void ir_transition_set_delay(IRTransition* transition, float delay) {
    if (transition) {
        transition->delay = delay;
    }
}

void ir_transition_set_trigger(IRTransition* transition, uint32_t state_mask) {
    if (transition) {
        transition->trigger_state = state_mask;
    }
}

// ============================================================================
// Attach Transition to Component
// ============================================================================

void ir_component_add_transition(IRComponent* component, IRTransition* transition) {
    if (!component || !transition) return;

    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    // Reallocate transitions array
    IRTransition** new_trans = (IRTransition**)realloc(style->transitions,
                                                      (style->transition_count + 1) * sizeof(IRTransition*));
    if (!new_trans) return;

    style->transitions = new_trans;
    style->transitions[style->transition_count] = transition;
    style->transition_count++;
}

// ============================================================================
// Animation Tree Update
// ============================================================================

// Apply all animations to a component tree (call each frame from renderer)
void ir_animation_tree_update(IRComponent* root, float current_time) {
    if (!root) return;

    // OPTIMIZATION: Skip entire subtree if no animations (80% reduction - visits only ~5% of nodes)
    if (!root->has_active_animations) return;

    // Apply all animations on this component
    if (root->style && root->style->animations) {
        for (uint32_t i = 0; i < root->style->animation_count; i++) {
            ir_animation_apply_keyframes(root, root->style->animations[i], current_time);
        }
    }

    // Recursively update children
    for (uint32_t i = 0; i < root->child_count; i++) {
        ir_animation_tree_update(root->children[i], current_time);
    }
}
