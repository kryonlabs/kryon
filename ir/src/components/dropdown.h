#ifndef IR_COMPONENT_DROPDOWN_H
#define IR_COMPONENT_DROPDOWN_H

#include "../include/ir_core.h"
#include "../include/ir_component_registry.h"

// Dropdown layout function
void layout_dropdown_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y);

// Dropdown layout trait
extern const IRLayoutTrait IR_DROPDOWN_LAYOUT_TRAIT;

// Initialize dropdown component
void ir_dropdown_component_init(void);

#endif // IR_COMPONENT_DROPDOWN_H
