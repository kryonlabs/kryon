#ifndef IR_COMPONENT_CHECKBOX_H
#define IR_COMPONENT_CHECKBOX_H

#include "../include/ir_core.h"
#include "../include/ir_component_registry.h"

// Checkbox layout function
void layout_checkbox_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y);

// Checkbox layout trait
extern const IRLayoutTrait IR_CHECKBOX_LAYOUT_TRAIT;

// Initialize checkbox component
void ir_checkbox_component_init(void);

#endif // IR_COMPONENT_CHECKBOX_H
