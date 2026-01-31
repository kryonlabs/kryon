/**
 * Kryon IR - Layout Helper Utilities
 *
 * Shared utilities for component layout to reduce code duplication.
 * Provides common layout state initialization, style access, and constraint logic.
 */

#ifndef LAYOUT_HELPERS_H
#define LAYOUT_HELPERS_H

#include "../include/ir_core.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Layout State Management
// ============================================================================

/**
 * Ensure a component has layout state allocated.
 * Allocates layout_state if it doesn't exist.
 *
 * @param c Component to ensure layout state for
 * @return Allocated layout state, or NULL on allocation failure
 */
IRLayoutState* layout_ensure_state(IRComponent* c);

/**
 * Initialize layout state with default values.
 *
 * @param state Layout state to initialize
 */
void layout_state_init(IRLayoutState* state);

/**
 * Mark layout as valid and computed as valid.
 *
 * @param state Layout state to mark valid
 */
void layout_mark_valid(IRLayoutState* state);

// ============================================================================
// Style Access Helpers
// ============================================================================

/**
 * Get font size from component style with default fallback.
 *
 * @param c Component
 * @param default_size Default font size if not specified
 * @return Font size in pixels
 */
float layout_get_font_size(IRComponent* c, float default_size);

/**
 * Get padding from component style.
 *
 * @param c Component
 * @return Padding structure (zeroed if no style)
 */
IRSpacing layout_get_padding(IRComponent* c);

/**
 * Get margin from component style.
 *
 * @param c Component
 * @return Margin structure (zeroed if no style)
 */
IRSpacing layout_get_margin(IRComponent* c);

/**
 * Get explicit width from component style if set.
 *
 * @param c Component
 * @param out_width Output for width value
 * @return true if explicit width is set, false otherwise
 */
bool layout_get_explicit_width(IRComponent* c, float* out_width);

/**
 * Get explicit height from component style if set.
 *
 * @param c Component
 * @param out_height Output for height value
 * @return true if explicit height is set, false otherwise
 */
bool layout_get_explicit_height(IRComponent* c, float* out_height);

// ============================================================================
// Constraint Application
// ============================================================================

/**
 * Apply layout constraints to dimensions.
 * Ensures dimensions are within min/max bounds.
 *
 * @param width Width to constrain (updated in place)
 * @param height Height to constrain (updated in place)
 * @param constraints Constraints to apply
 */
void layout_apply_constraints(float* width, float* height,
                               IRLayoutConstraints constraints);

/**
 * Apply only min constraints to dimensions.
 *
 * @param width Width to constrain (updated in place)
 * @param height Height to constrain (updated in place)
 * @param constraints Constraints to apply
 */
void layout_apply_min_constraints(float* width, float* height,
                                   IRLayoutConstraints constraints);

/**
 * Apply only max constraints to dimensions.
 *
 * @param width Width to constrain (updated in place)
 * @param height Height to constrain (updated in place)
 * @param constraints Constraints to apply
 */
void layout_apply_max_constraints(float* width, float* height,
                                   IRLayoutConstraints constraints);

// ============================================================================
// Final Layout Computation
// ============================================================================

/**
 * Set final computed layout for a component.
 * Sets position and dimensions, marks layout as valid.
 *
 * @param c Component
 * @param x X position
 * @param y Y position
 * @param width Width
 * @param height Height
 */
void layout_set_final(IRComponent* c, float x, float y, float width, float height);

/**
 * Set final computed layout for a component with parent offset.
 * Adds parent position to computed position.
 *
 * @param c Component
 * @param parent_x Parent X position
 * @param parent_y Parent Y position
 * @param width Width
 * @param height Height
 */
void layout_set_final_with_parent(IRComponent* c, float parent_x, float parent_y,
                                  float width, float height);

/**
 * Set final dimensions only (keeps current position).
 *
 * @param c Component
 * @param width Width
 * @param height Height
 */
void layout_set_final_dimensions(IRComponent* c, float width, float height);

// ============================================================================
// Debug Logging
// ============================================================================

/**
 * Check if layout debug mode is enabled for a component.
 * Checks environment variable KRYON_DEBUG_<COMPONENT>.
 *
 * @param component_name Component name (e.g., "Button", "Text")
 * @return true if debug enabled, false otherwise
 */
bool layout_is_debug_enabled(const char* component_name);

/**
 * Log final layout for a component if debug is enabled.
 *
 * @param component_name Component name
 * @param c Component with layout state
 * @param extra_info Optional extra info string (can be NULL)
 */
void layout_debug_log(const char* component_name, IRComponent* c, const char* extra_info);

// ============================================================================
// Intrinsic Size Computation Helpers
// ============================================================================

/**
 * Compute intrinsic size with text measurement.
 * Measures text width and adds padding.
 *
 * @param text Text to measure (can be NULL)
 * @param font_size Font size in pixels
 * @param padding Padding to add
 * @param min_width Minimum width
 * @param out_width Output for computed width
 * @param out_height Output for computed height (single line)
 */
void layout_compute_text_size(const char* text, float font_size,
                               IRSpacing padding, float min_width,
                               float* out_width, float* out_height);

/**
 * Compute size with explicit dimensions taking precedence.
 * Uses intrinsic size, but overrides with explicit dimensions if set.
 *
 * @param c Component
 * @param intrinsic_width Intrinsic width
 * @param intrinsic_height Intrinsic height
 * @param out_width Output for final width
 * @param out_height Output for final height
 */
void layout_compute_with_explicit(IRComponent* c,
                                  float intrinsic_width, float intrinsic_height,
                                  float* out_width, float* out_height);

/**
 * Complete layout computation pipeline.
 * Combines all steps: ensure state, get font size/padding,
 * compute intrinsic size, apply explicit dimensions, apply constraints,
 * set final layout with parent position.
 *
 * @param c Component
 * @param constraints Layout constraints
 * @param parent_x Parent X position
 * @param parent_y Parent Y position
 * @param intrinsic_width Intrinsic width (before explicit/constraints)
 * @param intrinsic_height Intrinsic height (before explicit/constraints)
 */
void layout_compute_full_pipeline(IRComponent* c,
                                   IRLayoutConstraints constraints,
                                   float parent_x, float parent_y,
                                   float intrinsic_width,
                                   float intrinsic_height);

// ============================================================================
// Default Constants
// ============================================================================

/** Default font size for most components */
#define LAYOUT_DEFAULT_FONT_SIZE 14.0f

/** Default padding for components */
#define LAYOUT_DEFAULT_PADDING 8.0f

/** Minimum button width */
#define LAYOUT_MIN_BUTTON_WIDTH 50.0f

/** Minimum input width */
#define LAYOUT_MIN_INPUT_WIDTH 150.0f

/** Chrome padding for form controls */
#define LAYOUT_FORM_CHROME_PADDING 12.0f

#ifdef __cplusplus
}
#endif

#endif // LAYOUT_HELPERS_H
