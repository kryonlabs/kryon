#ifndef IR_TRANSITION_H
#define IR_TRANSITION_H

// _POSIX_C_SOURCE should be defined by the including .c file (typically 200809L)
// to match the rest of the codebase

#include <stdint.h>
#include <stdbool.h>
#include "ir_properties.h"

// Forward declarations
typedef struct IRComponent IRComponent;
typedef struct IRStyle IRStyle;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

// Maximum active transitions per component
#define IR_MAX_ACTIVE_TRANSITIONS 16

// Maximum components with active transitions (global)
#define IR_MAX_TRANSITION_ENTRIES 512

// Pseudo-state flags (matching CSS pseudo-classes)
#define IR_PSEUDO_NONE      0
#define IR_PSEUDO_HOVER     (1 << 0)
#define IR_PSEUDO_ACTIVE    (1 << 1)
#define IR_PSEUDO_FOCUS     (1 << 2)
#define IR_PSEUDO_DISABLED  (1 << 3)
#define IR_PSEUDO_CHECKED   (1 << 4)

// ============================================================================
// Transition State (Runtime)
// ============================================================================

// Active transition - tracks a running transition
typedef struct IRActiveTransition {
    // The transition definition (from style->transitions)
    IRTransition* definition;

    // Property being animated
    IRAnimationProperty property;

    // Start and end values (union for different types)
    union {
        struct { float start, end; } numeric;          // For opacity, rotate
        struct { float start_x, start_y;               // For translate
                 float end_x, end_y; } translate;
        struct { float start_scale, end_scale; } scale; // For scale
        struct { IRColor start, end; } color;          // For colors
    } values;

    // Timing
    float elapsed;        // Time since transition started
    float duration;       // Transition duration
    float delay;          // Delay before starting

    // State
    bool is_active;       // Currently animating
    bool is_delayed;      // In delay phase
    bool completed;       // Transition finished

    // For state tracking
    uint32_t from_state;  // State we're transitioning from
    uint32_t to_state;    // State we're transitioning to
} IRActiveTransition;

// Per-component transition state
typedef struct IRComponentTransitionState {
    // Active transitions
    IRActiveTransition active[IR_MAX_ACTIVE_TRANSITIONS];
    uint8_t active_count;

    // Previous pseudo-state (for detecting changes)
    uint32_t previous_pseudo_state;

    // Snapshot of property values before transition
    struct {
        float opacity;
        float translate_x, translate_y;
        float scale_x, scale_y;
        float rotate;
        IRColor background;
        IRColor border_color;
        IRColor text_fill_color;
    } previous_values;

    bool values_initialized;
    bool in_state_change;  // True during a state change transition
} IRComponentTransitionState;

// Global transition context
typedef struct IRTransitionContext {
    // Component -> transition state mapping
    struct {
        IRComponent* component;
        IRComponentTransitionState state;
    } entries[IR_MAX_TRANSITION_ENTRIES];
    uint32_t count;

    // Statistics
    uint32_t total_transitions_started;
    uint32_t total_transitions_completed;
} IRTransitionContext;

// ============================================================================
// Context Management
// ============================================================================

/**
 * Create a new transition context
 */
IRTransitionContext* ir_transition_context_create(void);

/**
 * Destroy transition context and free all resources
 */
void ir_transition_context_destroy(IRTransitionContext* ctx);

/**
 * Reset the transition context (clear all state)
 */
void ir_transition_context_reset(IRTransitionContext* ctx);

// ============================================================================
// Transition Update (call each frame)
// ============================================================================

/**
 * Update all transitions in the component tree
 * Call this once per frame with the delta time
 *
 * @param ctx Transition context
 * @param root Root component of the tree
 * @param delta_time Time since last update (seconds)
 */
void ir_transition_tree_update(IRTransitionContext* ctx, IRComponent* root, float delta_time);

// ============================================================================
// Component State Access
// ============================================================================

/**
 * Get transition state for a component (creates if needed)
 * Returns NULL if maximum entries reached
 */
IRComponentTransitionState* ir_transition_get_state(IRTransitionContext* ctx, IRComponent* component);

/**
 * Remove transition state for a component (cleanup when no longer needed)
 */
void ir_transition_remove_state(IRTransitionContext* ctx, IRComponent* component);

// ============================================================================
// Transition Control
// ============================================================================

/**
 * Start a transition for a property change
 *
 * @param ctx Transition context
 * @param component Target component
 * @param definition Transition definition (from style)
 * @param from_state State we're transitioning from
 * @param to_state State we're transitioning to
 */
void ir_transition_start(IRTransitionContext* ctx, IRComponent* component,
                        IRTransition* definition, uint32_t from_state, uint32_t to_state);

/**
 * Cancel all active transitions for a component
 */
void ir_transition_cancel_all(IRTransitionContext* ctx, IRComponent* component);

/**
 * Cancel active transition for a specific property
 */
void ir_transition_cancel_property(IRTransitionContext* ctx, IRComponent* component,
                                  IRAnimationProperty property);

// ============================================================================
// Value Access (for getting current "display" values during transitions)
// ============================================================================

/**
 * Get the current display value of opacity (considering active transitions)
 * Returns -1.0f if no active transition for this property
 */
float ir_transition_get_opacity(IRComponent* component);

/**
 * Get the current display value of transform (considering active transitions)
 * Returns false if no active transition for transform
 */
bool ir_transition_get_transform(IRComponent* component, float* x, float* y,
                                float* scale_x, float* scale_y, float* rotate);

// ============================================================================
// Statistics
// ============================================================================

typedef struct {
    uint32_t total_started;
    uint32_t total_completed;
    uint32_t active_components;
    uint32_t active_transitions;
} IRTransitionStats;

/**
 * Get transition statistics
 */
void ir_transition_get_stats(IRTransitionContext* ctx, IRTransitionStats* stats);

// ============================================================================
// Internal API (for renderer integration)
// ============================================================================

/**
 * Snapshot current style values for a component
 * Called before starting transitions to capture "from" values
 */
void ir_transition_snapshot_values(IRComponent* component, IRComponentTransitionState* state);

/**
 * Apply interpolated values from active transitions to the component
 * Called each frame during update
 */
void ir_transition_apply_values(IRComponentTransitionState* state, IRComponent* component);

/**
 * Check if a component has any active transitions
 */
bool ir_transition_has_active(IRComponentTransitionState* state);

/**
 * Get current pseudo-state for a component
 * Combines hover, active, focus, etc. into a bitmask
 */
uint32_t ir_transition_get_pseudo_state(IRComponent* component);

#ifdef __cplusplus
}
#endif

#endif // IR_TRANSITION_H
