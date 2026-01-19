#ifndef IR_ANIMATION_BUILDER_H
#define IR_ANIMATION_BUILDER_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/ir_core.h"

// Forward declarations
typedef struct IRAnimation IRAnimation;
typedef struct IRKeyframe IRKeyframe;
typedef struct IRTransition IRTransition;

// ============================================================================
// Animation and Keyframe Creation
// ============================================================================

IRAnimation* ir_animation_create_keyframe(const char* name, float duration);
void ir_animation_destroy(IRAnimation* anim);

void ir_animation_set_iterations(IRAnimation* anim, int32_t count);  // -1 = infinite
void ir_animation_set_alternate(IRAnimation* anim, bool alternate);
void ir_animation_set_delay(IRAnimation* anim, float delay);
void ir_animation_set_default_easing(IRAnimation* anim, IREasingType easing);

// ============================================================================
// Keyframe Management
// ============================================================================

IRKeyframe* ir_animation_add_keyframe(IRAnimation* anim, float offset);  // offset 0.0-1.0
void ir_keyframe_set_property(IRKeyframe* kf, IRAnimationProperty prop, float value);
void ir_keyframe_set_color_property(IRKeyframe* kf, IRAnimationProperty prop, IRColor color);
void ir_keyframe_set_easing(IRKeyframe* kf, IREasingType easing);

// ============================================================================
// Attach Animation to Component
// ============================================================================

void ir_component_add_animation(IRComponent* component, IRAnimation* anim);

// ============================================================================
// Animation Flag Propagation
// ============================================================================

// Re-propagate animation flags after tree construction
// Call this after all components are created and added to fix flag propagation
void ir_animation_propagate_flags(IRComponent* root);

// General component subtree finalization (post-construction propagation)
// Call this after adding children (especially from static loops) to ensure all
// post-construction propagation steps are performed
void ir_component_finalize_subtree(IRComponent* component);

// ============================================================================
// Transition Creation
// ============================================================================

IRTransition* ir_transition_create(IRAnimationProperty property, float duration);
void ir_transition_destroy(IRTransition* transition);

void ir_transition_set_easing(IRTransition* transition, IREasingType easing);
void ir_transition_set_delay(IRTransition* transition, float delay);
void ir_transition_set_trigger(IRTransition* transition, uint32_t state_mask);

// ============================================================================
// Attach Transition to Component
// ============================================================================

void ir_component_add_transition(IRComponent* component, IRTransition* transition);

// ============================================================================
// Animation Tree Update
// ============================================================================

// Apply all animations to a component tree (call each frame from renderer)
void ir_animation_tree_update(IRComponent* root, float current_time);

#endif // IR_ANIMATION_BUILDER_H
