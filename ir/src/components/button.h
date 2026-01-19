#ifndef IR_COMPONENT_BUTTON_H
#define IR_COMPONENT_BUTTON_H

#include "../include/ir_core.h"
#include "registry.h"

/**
 * Button Component - Single-Pass Layout
 *
 * Handles layout for interactive button components with text labels.
 * Computes intrinsic size based on text content + padding + button chrome.
 */

// Single-pass layout function for button
void layout_button_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y);

// Component trait
extern const IRLayoutTrait IR_BUTTON_LAYOUT_TRAIT;

// Initialize and register button component trait
void ir_button_component_init(void);

#endif // IR_COMPONENT_BUTTON_H
