#ifndef IR_COMPONENT_INPUT_H
#define IR_COMPONENT_INPUT_H

#include "../../include/ir_core.h"
#include "registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Layout function for Input component
void layout_input_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y);

// Input layout trait (extern for registration)
extern const IRLayoutTrait IR_INPUT_LAYOUT_TRAIT;

// Initialize Input component layout trait
void ir_input_component_init(void);

#ifdef __cplusplus
}
#endif

#endif // IR_COMPONENT_INPUT_H
