#ifndef IR_COMPONENT_TEXT_H
#define IR_COMPONENT_TEXT_H

#include "../ir_core.h"
#include "ir_component_registry.h"

/**
 * Text Component - Single-Pass Layout Implementation
 *
 * Handles text measurement and layout with proper wrapping support.
 * Uses backend text measurement callback for accurate multi-line text sizing.
 */

// Single-pass layout function for Text components
void layout_text_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                             float parent_x, float parent_y);

// Text component trait (registered in registry)
extern const IRLayoutTrait IR_TEXT_LAYOUT_TRAIT;

// Initialize and register Text component trait
void ir_text_component_init(void);

#endif // IR_COMPONENT_TEXT_H
